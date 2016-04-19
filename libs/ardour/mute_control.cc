/*
    Copyright (C) 2016 Paul Davis

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "evoral/ControlList.hpp"

#include "ardour/mute_master.h"
#include "ardour/session.h"
#include "ardour/mute_control.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;


MuteControl::MuteControl (Session& session, std::string const & name, Muteable& m)
	: SlavableAutomationControl (session, MuteAutomation, ParameterDescriptor (MuteAutomation),
	                             boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (MuteAutomation))),
	                             name)
	, _muteable (m)
{
	_list->set_interpolation (Evoral::ControlList::Discrete);
	/* mute changes must be synchronized by the process cycle */
	set_flags (Controllable::Flag (flags() | Controllable::RealTime));
}

void
MuteControl::post_add_master (boost::shared_ptr<AutomationControl> m)
{
	if (m->get_value()) {

		/* boolean masters records are not updated until AFTER
		 * ::post_add_master() is called, so we can use them to check
		 * on whether any master was already enabled before the new
		 * one was added.
		 */

		if (!muted_by_self() && !get_boolean_masters()) {
			Changed (false, Controllable::NoGroup);
		}
	}
}

void
MuteControl::pre_remove_master (boost::shared_ptr<AutomationControl> m)
{
	if (!m) {
		/* null control ptr means we're removing all masters */
		_muteable.mute_master()->set_muted_by_others (false);
		/* Changed will be emitted in SlavableAutomationControl::clear_masters() */
		return;
	}

	if (m->get_value()) {
		if (!muted_by_self() && (muted_by_others() == 1)) {
			Changed (false, Controllable::NoGroup);
		}
	}
}

void
MuteControl::actually_set_value (double val, Controllable::GroupControlDisposition gcd)
{
	if (muted_by_self() != bool (val)) {
		_muteable.mute_master()->set_muted_by_self (val);

		/* allow the Muteable to respond to the mute change
		   before anybody else knows about it.
		*/
		_muteable.act_on_mute ();
	}

	AutomationControl::actually_set_value (val, gcd);
}

void
MuteControl::master_changed (bool self_change, Controllable::GroupControlDisposition gcd, boost::shared_ptr<AutomationControl> m)
{
	bool send_signal = false;
	const double changed_master_value = m->get_value();
	boost::shared_ptr<MuteControl> mc = boost::dynamic_pointer_cast<MuteControl> (m);

	if (m) {
		cerr << "master changed, self ? " << self_change << " self muted = "
		     << mc->muted_by_self() << " others " << mc->muted_by_others()
		     << endl;
	}

	if (changed_master_value) {
		/* this master is now enabled */
		if (!muted_by_self() && get_boolean_masters() == 0) {
			send_signal = true;
		}
	} else {
		if (!muted_by_self() && get_boolean_masters() == 1) {
			send_signal = true;
		}
	}

	update_boolean_masters_records (m);

	if (send_signal) {
		Changed (false, Controllable::NoGroup);
	}
}

double
MuteControl::get_value () const
{
	if (slaved ()) {
		Glib::Threads::RWLock::ReaderLock lm (master_lock);
		return get_masters_value_locked ();
	}

	if (_list && boost::dynamic_pointer_cast<AutomationList>(_list)->automation_playback()) {
		// Playing back automation, get the value from the list
		return AutomationControl::get_value();
	}

	return muted() ? 1.0 : 0.0;
}

void
MuteControl::set_mute_points (MuteMaster::MutePoint mp)
{
	_muteable.mute_master()->set_mute_points (mp);
	_muteable.mute_points_changed (); /* EMIT SIGNAL */

	if (_muteable.mute_master()->muted_by_self()) {
		Changed (true, Controllable::UseGroup); /* EMIT SIGNAL */
	}
}

MuteMaster::MutePoint
MuteControl::mute_points () const
{
	return _muteable.mute_master()->mute_points ();
}

bool
MuteControl::muted () const
{
	return muted_by_self() || muted_by_others();
}

bool
MuteControl::muted_by_self () const
{
	return _muteable.mute_master()->muted_by_self();
}

bool
MuteControl::muted_by_others () const
{
	return get_masters_value ();
}
