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
MuteControl::master_changed (bool self_change, Controllable::GroupControlDisposition gcd)
{
	double m = get_masters_value ();
	const int32_t old_muted_by_others = _muteable.mute_master()->muted_by_others ();
	std::cerr << "master " << (self_change ? " self " : " not-self") << " changed to " << m << " old others = " << old_muted_by_others << std::endl;

	_muteable.mute_master()->mod_muted_by_others (m ? 1 : -1);

	if (m) {
		/* master(s) are now muted. If we are self-muted, this
		   doesn't change our status. If we are not self-muted,
		   then it changes our status if either:

		   - the master had its own self-muted status changed OR
		   - the total number of masters that are muted used to be zero
		*/

		if (!muted_by_self()) {
			if (self_change || old_muted_by_others == 0) {
				/* note false as the first argument - our own
				   value was not changed
				*/
				Changed (false, gcd);
			} else {
				cerr << " no Change signal\n";
			}
		} else {
			cerr << "muted by self, not relevant\n";
		}
	} else {
		/* no master(s) are now muted. If we are self-muted, this
		   doesn't change our status. If we are not self-muted,
		   then it changes our status if either:

		   - the master had its own self-muted status changed OR
		   - the total number of masters that are muted used to be non-zero
		*/

		if (!muted_by_self()) {
			if (self_change || old_muted_by_others != 0) {
				Changed (false, gcd);
			} else {
				cerr << " No change signal\n";
			}
		} else {
			cerr << "muted by self, not relevant\n";
		}
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
	return _muteable.mute_master()->muted_by_self() || _muteable.mute_master()->muted_by_others();
}

bool
MuteControl::muted_by_self () const
{
	return _muteable.mute_master()->muted_by_self();
}

bool
MuteControl::muted_by_others () const
{
	return _muteable.mute_master()->muted_by_others ();
}
