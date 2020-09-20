/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Nick Mainsbridge <mainsbridge@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <math.h>
#include <iostream>

#include "pbd/memento_command.h"

#include "ardour/audioengine.h"
#include "ardour/automation_control.h"
#include "ardour/automation_watch.h"
#include "ardour/control_group.h"
#include "ardour/event_type_map.h"
#include "ardour/session.h"
#include "ardour/selection.h"
#include "ardour/value_as_string.h"

#include "pbd/i18n.h"

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
                                     const string&                             name,
                                     Controllable::Flag                        flags)

	: Controllable (name.empty() ? EventTypeMap::instance().to_symbol(parameter) : name, flags)
	, Evoral::Control(parameter, desc, list)
	, SessionHandleRef (session)
	, _desc(desc)
	, _no_session(false)
{
	if (_desc.toggled) {
		set_flags (Controllable::Toggle);
	}
	boost::shared_ptr<AutomationList> al = alist();
	if (al) {
		al->StateChanged.connect_same_thread (_state_changed_connection, boost::bind (&Session::set_dirty, &_session));
	}
}

AutomationControl::~AutomationControl ()
{
	if (!_no_session && !_session.deletion_in_progress ()) {
		_session.selection().remove_control_by_id (id());
		DropReferences (); /* EMIT SIGNAL */
	}
}

void
AutomationControl::session_going_away ()
{
	SessionHandleRef::session_going_away ();
	DropReferences (); /* EMIT SIGNAL */
	_no_session = true;
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
	bool from_list = alist() && alist()->automation_playback();
	return Control::get_double (from_list, timepos_t (_session.transport_sample()));
}

double
AutomationControl::get_save_value() const
{
	/* save user-value, not incl masters */
	return Control::get_double ();
}

void
AutomationControl::pre_realtime_queue_stuff (double val, PBD::Controllable::GroupControlDisposition gcd)
{
	if (_group && _group->use_me (gcd)) {
		_group->pre_realtime_queue_stuff (val);
	} else {
		do_pre_realtime_queue_stuff (val);
	}
}

void
AutomationControl::set_value (double val, PBD::Controllable::GroupControlDisposition gcd)
{
	if (!writable()) {
		return;
	}

	if (_list && !touching () && alist()->automation_state() == Latch && _session.transport_rolling ()) {
		start_touch (timepos_t (_session.transport_sample ()));
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
		_group->set_group_value (boost::dynamic_pointer_cast<AutomationControl>(shared_from_this()), val);
	} else {
		actually_set_value (val, gcd);
	}
}

ControlList
AutomationControl::grouped_controls () const
{
	if (_group && _group->use_me (PBD::Controllable::UseGroup)) {
		return _group->controls ();
	} else {
		return ControlList ();
	}
}

void
AutomationControl::automation_run (samplepos_t start, pframes_t nframes)
{
	if (!automation_playback ()) {
		return;
	}

	assert (_list);
	bool valid = false;
	double val = _list->rt_safe_eval (timepos_t (start), valid);
	if (!valid) {
		return;
	}
	if (toggled ()) {
		const double thresh = .5 * (_desc.upper - _desc.lower);
		set_value_unchecked (val >= thresh ? _desc.upper : _desc.lower);
	} else {
		set_value_unchecked (val);
	}
}

/** Set the value and do the right thing based on automation state
 *  (e.g. record if necessary, etc.)
 *  @param value `user' value
 */
void
AutomationControl::actually_set_value (double value, PBD::Controllable::GroupControlDisposition gcd)
{
	boost::shared_ptr<AutomationList> al = alist ();
	const samplepos_t pos = _session.transport_sample();
	bool to_list;

	/* We cannot use ::get_value() here since that is virtual, and intended
	   to return a scalar value that in some way reflects the state of the
	   control (with semantics defined by the control itself, since it's
	   internal state may be more complex than can be fully represented by
	   a single scalar).

	   This method's only job is to set the "user_double()" value of the
	   underlying Evoral::Control object, and so we should compare the new
	   value we're being given to the current user_double().

	   Unless ... we're doing automation playback, in which case the
	   current effective value of the control (used to determine if
	   anything has changed) is the one derived from the automation event
	   list.
	*/
	float old_value = Control::user_double();

	if (al && al->automation_write ()) {
		to_list = true;
	} else {
		to_list = false;
	}

	Control::set_double (value, timepos_t (pos), to_list);

	if (old_value != (float)value) {
#if 0
		AutomationType at = (AutomationType) _parameter.type();
		std::cerr << "++++ Changed (" << enum_2_string (at) << ", " << enum_2_string (gcd) << ") = " << value
		<< " (was " << old_value << ") @ " << this << std::endl;
#endif

		Changed (true, gcd);
		if (!al || !al->automation_playback ()) {
			_session.set_dirty ();
		}
	}
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
	if (flags() & NotAutomatable) {
		return;
	}
	if (alist() && as != alist()->automation_state()) {

		const double val = get_value ();

		alist()->set_automation_state (as);

		if (as == Write) {
			AutomationWatch::instance().add_automation_watch (boost::dynamic_pointer_cast<AutomationControl>(shared_from_this()));
		} else if (as & (Touch | Latch)) {
#warning NUTEMPO fixme timestamps here are always in samples ... should match list time domain
			if (alist()->empty()) {
				Control::set_double (val, timepos_t (_session.current_start_sample ()), true);
				Control::set_double (val, timepos_t (_session.current_end_sample ()), true);
				Changed (true, Controllable::NoGroup);
			}
			if (!touching()) {
				AutomationWatch::instance().remove_automation_watch (boost::dynamic_pointer_cast<AutomationControl>(shared_from_this()));
			} else {
				/* this seems unlikely, but the combination of
				 * a control surface and the mouse could make
				 * it possible to put the control into Touch
				 * mode *while* touching it.
				 */
				AutomationWatch::instance().add_automation_watch (boost::dynamic_pointer_cast<AutomationControl>(shared_from_this()));
			}
		} else {
			AutomationWatch::instance().remove_automation_watch (boost::dynamic_pointer_cast<AutomationControl>(shared_from_this()));
			Changed (false, Controllable::NoGroup);
		}
	}
}

void
AutomationControl::start_touch (timepos_t const & when)
{
	if (!_list || touching ()) {
		return;
	}

	ControlTouched (boost::dynamic_pointer_cast<PBD::Controllable>(shared_from_this())); /* EMIT SIGNAL */

	if (alist()->automation_state() & (Touch | Latch)) {
		/* subtle. aligns the user value with the playback and
		 * use take actual value (incl masters).
		 *
		 * Touch + hold writes inverse curve of master-automation
		 * using AutomationWatch::timer ()
		 */
		AutomationControl::actually_set_value (get_value (), Controllable::NoGroup);
		alist()->start_touch (when);
		AutomationWatch::instance().add_automation_watch (boost::dynamic_pointer_cast<AutomationControl>(shared_from_this()));
		set_touching (true);
	}
}

void
AutomationControl::stop_touch (timepos_t const & when)
{
	if (!_list || !touching ()) {
		return;
	}

	if (alist()->automation_state() == Latch && _session.transport_rolling ()) {
		return;
	}
	if (alist()->automation_state() == Touch && _session.transport_rolling () && _desc.toggled) {
		/* Toggle buttons always latch */
		return;
	}

	set_touching (false);

	if (alist()->automation_state() & (Touch | Latch)) {
		alist()->stop_touch (when);
		AutomationWatch::instance().remove_automation_watch (boost::dynamic_pointer_cast<AutomationControl>(shared_from_this()));
	}
}

void
AutomationControl::commit_transaction (bool did_write)
{
	if (did_write) {
		XMLNode* before = alist ()->before ();
		if (before) {
			_session.begin_reversible_command (string_compose (_("record %1 automation"), name ()));
			_session.commit_reversible_command (alist ()->memento_command (before, &alist ()->get_state ()));
		}
	} else {
		alist ()->clear_history ();
	}
}

/* take control-value and return UI range [0..1] */
double
AutomationControl::internal_to_interface (double val, bool rotary) const
{
	// XXX maybe optimize. _desc.from_interface() has
	// a switch-statement depending on AutomationType.
	return _desc.to_interface (val, rotary);
}

/* map GUI range [0..1] to control-value */
double
AutomationControl::interface_to_internal (double val, bool rotary) const
{
	if (!isfinite_local (val)) {
		assert (0);
		val = 0;
	}
	// XXX maybe optimize. see above.
	return _desc.from_interface (val, rotary);
}

std::string
AutomationControl::get_user_string () const
{
	return ARDOUR::value_as_string (_desc, get_value());
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
		_session.set_control (boost::dynamic_pointer_cast<AutomationControl>(shared_from_this()), val, gcd);
		return true;
	}

	return false;
}
