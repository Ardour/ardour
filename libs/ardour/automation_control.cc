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
#include "ardour/automation_control.h"
#include "ardour/automation_watch.h"
#include "ardour/event_type_map.h"
#include "ardour/session.h"

#include "pbd/memento_command.h"
#include "pbd/stacktrace.h"

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

double
AutomationControl::get_masters_value_locked () const
{
	gain_t v = 1.0;

	for (Masters::const_iterator mr = _masters.begin(); mr != _masters.end(); ++mr) {
		/* get current master value, scale by our current ratio with that master */
		v *= mr->second.master()->get_value () * mr->second.ratio();
	}

	return min (_desc.upper, v);
}

double
AutomationControl::get_value_locked() const
{
	/* read or write masters lock must be held */

	if (_masters.empty()) {
		return Control::get_double (false, _session.transport_frame());
	}

	return get_masters_value_locked ();
}

/** Get the current effective `user' value based on automation state */
double
AutomationControl::get_value() const
{
	bool from_list = _list && ((AutomationList*)_list.get())->automation_playback();

	if (!from_list) {
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		return get_value_locked ();
	} else {
		return Control::get_double (from_list, _session.transport_frame());
	}
}

/** Set the value and do the right thing based on automation state
 *  (e.g. record if necessary, etc.)
 *  @param value `user' value
 */
void
AutomationControl::set_value (double value, PBD::Controllable::GroupControlDisposition gcd)
{
	bool to_list = _list && ((AutomationList*)_list.get())->automation_write();

	Control::set_double (value, _session.transport_frame(), to_list);

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
AutomationControl::add_master (boost::shared_ptr<AutomationControl> m)
{
	double current_value;
	double new_value;
	std::pair<Masters::iterator,bool> res;

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		current_value = get_value_locked ();

		/* ratio will be recomputed below */

		res = _masters.insert (make_pair<PBD::ID,MasterRecord> (m->id(), MasterRecord (m, 1.0)));

		if (res.second) {

			recompute_masters_ratios (current_value);

			/* note that we bind @param m as a weak_ptr<AutomationControl>, thus
			   avoiding holding a reference to the control in the binding
			   itself.
			*/

			m->DropReferences.connect_same_thread (masters_connections, boost::bind (&AutomationControl::master_going_away, this, m));

			/* Store the connection inside the MasterRecord, so that when we destroy it, the connection is destroyed
			   and we no longer hear about changes to the AutomationControl.

			   Note that we fix the "from_self" argument that will
			   be given to our own Changed signal to "false",
			   because the change came from the master.
			*/


			m->Changed.connect_same_thread (res.first->second.connection, boost::bind (&AutomationControl::master_changed, this, _1, _2));
		}

		new_value = get_value_locked ();
	}

	if (res.second) {
		/* this will notify everyone that we're now slaved to the master */
		MasterStatusChange (); /* EMIT SIGNAL */
	}

	if (new_value != current_value) {
		/* force a call to to ::master_changed() to carry the
		 * consequences that would occur if the master assumed
		 * its current value WHILE we were slaved.
		 */
		master_changed (false, Controllable::NoGroup);
		/* effective value changed by master */
		Changed (false, Controllable::NoGroup);
	}

}

void
AutomationControl::master_changed (bool /*from_self*/, GroupControlDisposition gcd)
{
	/* our value has (likely) changed, but not because we were
	 * modified. Just the master.
	 */

	Changed (false, gcd); /* EMIT SIGNAL */
}

void
AutomationControl::master_going_away (boost::weak_ptr<AutomationControl> wm)
{
	boost::shared_ptr<AutomationControl> m = wm.lock();
	if (m) {
		remove_master (m);
	}
}

void
AutomationControl::remove_master (boost::shared_ptr<AutomationControl> m)
{
	double current_value;
	double new_value;
	Masters::size_type erased = 0;

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		current_value = get_value_locked ();
		erased = _masters.erase (m->id());
		if (erased) {
			recompute_masters_ratios (current_value);
		}
		new_value = get_value_locked ();
	}

	if (erased) {
		MasterStatusChange (); /* EMIT SIGNAL */
	}

	if (new_value != current_value) {
		Changed (false, Controllable::NoGroup);
	}
}

void
AutomationControl::clear_masters ()
{
	double current_value;
	double new_value;
	bool had_masters = false;

	{
		Glib::Threads::RWLock::WriterLock lm (master_lock);
		current_value = get_value_locked ();
		if (!_masters.empty()) {
			had_masters = true;
		}
		_masters.clear ();
		new_value = get_value_locked ();
	}

	if (had_masters) {
		MasterStatusChange (); /* EMIT SIGNAL */
	}

	if (new_value != current_value) {
		Changed (false, Controllable::NoGroup);
	}

}

bool
AutomationControl::slaved_to (boost::shared_ptr<AutomationControl> m) const
{
	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	return _masters.find (m->id()) != _masters.end();
}

bool
AutomationControl::slaved () const
{
	Glib::Threads::RWLock::ReaderLock lm (master_lock);
	return !_masters.empty();
}
