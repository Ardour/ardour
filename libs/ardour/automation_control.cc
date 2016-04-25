/*
    Copyright (C) 2007 Paul Davis
    Author: David Robillard

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <math.h>
#include <iostream>

#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"

#include "ardour/audioengine.h"
#include "ardour/automation_control.h"
#include "ardour/automation_watch.h"
#include "ardour/control_group.h"
#include "ardour/event_type_map.h"
#include "ardour/session.h"

#include "i18n.h"

#ifdef COMPILER_MSVC
#include <float.h>
// C99 'isfinite()' is not available in MSVC.
#define isfinite_local(val) (bool)_finite((double)val)
#else
#define isfinite_local isfinite
#endif

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AutomationControl::AutomationControl(ARDOUR::Session&                          session,
                                     const Evoral::Parameter&                  parameter,
                                     const ParameterDescriptor&                desc,
                                     boost::shared_ptr<ARDOUR::AutomationList> list,
                                     const string&                             name)
	: Controllable (name.empty() ? EventTypeMap::instance().to_symbol(parameter) : name)
	, Evoral::Control(parameter, desc, list)
	, _session(session)
	, _desc(desc)
{
	if (_desc.toggled) {
		set_flags (Controllable::Toggle);
	}
}

AutomationControl::~AutomationControl ()
{
	DropReferences (); /* EMIT SIGNAL */
}

bool
AutomationControl::writable() const
{
	boost::shared_ptr<AutomationList> al = alist();
	if (al) {
		return al->automation_state() != Play;
	}
	return true;
}

/** Get the current effective `user' value based on automation state */
double
AutomationControl::get_value() const
{
	bool from_list = _list && boost::dynamic_pointer_cast<AutomationList>(_list)->automation_playback();
	return Control::get_double (from_list, _session.transport_frame());
}

void
AutomationControl::set_value (double val, PBD::Controllable::GroupControlDisposition gcd)
{
	if (!writable()) {
		return;
	}

	/* enforce strict double/boolean value mapping */

	if (_desc.toggled) {
		if (val != 0.0) {
			val = 1.0;
		}
	}

	if (check_rt (val, gcd)) {
		/* change has been queued to take place in an RT context */
		return;
	}

	if (_group && _group->use_me (gcd)) {
		_group->set_group_value (shared_from_this(), val);
	} else {
		actually_set_value (val, gcd);
	}
}

/** Set the value and do the right thing based on automation state
 *  (e.g. record if necessary, etc.)
 *  @param value `user' value
 */
void
AutomationControl::actually_set_value (double value, PBD::Controllable::GroupControlDisposition gcd)
{
	bool to_list = _list && boost::dynamic_pointer_cast<AutomationList>(_list)->automation_write();
	const double old_value = Control::user_double ();

	Control::set_double (value, _session.transport_frame(), to_list);

	AutomationType at = (AutomationType) _parameter.type();

	std::cerr << "++++ Changed (" << enum_2_string (at) << ", " << enum_2_string (gcd) << ") = " << value 
	          << " (was " << old_value << ") @ " << this << std::endl;
	Changed (true, gcd);
}

void
AutomationControl::set_list (boost::shared_ptr<Evoral::ControlList> list)
{
	Control::set_list (list);
	Changed (true, Controllable::NoGroup);
}

void
AutomationControl::set_automation_state (AutoState as)
{
	if (_list && as != alist()->automation_state()) {

		alist()->set_automation_state (as);
		if (_desc.toggled) {
			return;  // No watch for boolean automation
		}

		if (as == Write) {
			AutomationWatch::instance().add_automation_watch (shared_from_this());
		} else if (as == Touch) {
			if (!touching()) {
				AutomationWatch::instance().remove_automation_watch (shared_from_this());
			} else {
				/* this seems unlikely, but the combination of
				 * a control surface and the mouse could make
				 * it possible to put the control into Touch
				 * mode *while* touching it.
				 */
				AutomationWatch::instance().add_automation_watch (shared_from_this());
			}
		} else {
			AutomationWatch::instance().remove_automation_watch (shared_from_this());
		}
	}
}

void
AutomationControl::set_automation_style (AutoStyle as)
{
	if (!_list) return;
	alist()->set_automation_style (as);
}

void
AutomationControl::start_touch(double when)
{
	if (!_list) {
		return;
	}

	if (!touching()) {

		if (alist()->automation_state() == Touch) {
			/* subtle. aligns the user value with the playback */
			set_value (get_value (), Controllable::NoGroup);
			alist()->start_touch (when);
			if (!_desc.toggled) {
				AutomationWatch::instance().add_automation_watch (shared_from_this());
			}
		}
		set_touching (true);
	}
}

void
AutomationControl::stop_touch(bool mark, double when)
{
	if (!_list) return;
	if (touching()) {
		set_touching (false);

		if (alist()->automation_state() == Touch) {
			alist()->stop_touch (mark, when);
			if (!_desc.toggled) {
				AutomationWatch::instance().remove_automation_watch (shared_from_this());

			}
		}
	}
}

void
AutomationControl::commit_transaction (bool did_write)
{
	if (did_write) {
		if (alist ()->before ()) {
			_session.begin_reversible_command (string_compose (_("record %1 automation"), name ()));
			_session.commit_reversible_command (new MementoCommand<AutomationList> (*alist ().get (), alist ()->before (), &alist ()->get_state ()));
		}
	} else {
		alist ()->clear_history ();
	}
}

double
AutomationControl::internal_to_interface (double val) const
{
	if (_desc.integer_step) {
		// both upper and lower are inclusive.
		val =  (val - lower()) / (1 + upper() - lower());
	} else {
		val =  (val - lower()) / (upper() - lower());
	}

	if (_desc.logarithmic) {
		if (val > 0) {
			val = pow (val, 1./2.0);
		} else {
			val = 0;
		}
	}

	return val;
}

double
AutomationControl::interface_to_internal (double val) const
{
	if (!isfinite_local (val)) {
		val = 0;
	}
	if (_desc.logarithmic) {
		if (val <= 0) {
			val = 0;
		} else {
			val = pow (val, 2.0);
		}
	}

	if (_desc.integer_step) {
		val =  lower() + val * (1 + upper() - lower());
	} else {
		val =  lower() + val * (upper() - lower());
	}

	if (val < lower()) val = lower();
	if (val > upper()) val = upper();

	return val;
}

void
AutomationControl::set_group (boost::shared_ptr<ControlGroup> cg)
{
	/* this method can only be called by a ControlGroup. We do not need
	   to ensure consistency by calling ControlGroup::remove_control(),
	   since we are guaranteed that the ControlGroup will take care of that
	   for us.
	*/

	_group = cg;
}

bool
AutomationControl::check_rt (double val, Controllable::GroupControlDisposition gcd)
{
	if (!_session.loading() && (flags() & Controllable::RealTime) && !AudioEngine::instance()->in_process_thread()) {
		/* queue change in RT context */
		_session.set_control (shared_from_this(), val, gcd);
		return true;
	}

	return false;
}
