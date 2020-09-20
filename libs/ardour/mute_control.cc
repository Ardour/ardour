/*
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#include "evoral/ControlList.h"

#include "ardour/mute_master.h"
#include "ardour/session.h"
#include "ardour/mute_control.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;


#warning NUTEMPO QUESTION what time domain shoudl this really use?
MuteControl::MuteControl (Session& session, std::string const & name, Muteable& m)
	: SlavableAutomationControl (session, MuteAutomation, ParameterDescriptor (MuteAutomation),
	                             boost::shared_ptr<AutomationList> (new AutomationList (Evoral::Parameter (MuteAutomation), Temporal::AudioTime)),
	                             name)
	, _muteable (m)
{
	_list->set_interpolation (Evoral::ControlList::Discrete);
	/* mute changes must be synchronized by the process cycle */
	set_flag (Controllable::RealTime);
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
			_muteable.mute_master()->set_muted_by_masters (true);
			Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
		}
	}
}

void
MuteControl::pre_remove_master (boost::shared_ptr<AutomationControl> m)
{
	if (!m) {
		/* null control ptr means we're removing all masters */
		_muteable.mute_master()->set_muted_by_masters (false);
		/* Changed will be emitted in SlavableAutomationControl::clear_masters() */
		return;
	}

	if (m->get_value() && get_boolean_masters() == 1) {
		_muteable.mute_master()->set_muted_by_masters (false);
		if (!muted_by_self()) {
			Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
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

	SlavableAutomationControl::actually_set_value (val, gcd);
}

bool
MuteControl::handle_master_change (boost::shared_ptr<AutomationControl> m)
{
	bool send_signal = false;
	boost::shared_ptr<MuteControl> mc = boost::dynamic_pointer_cast<MuteControl> (m);
	if (!mc) {
		return false;
	}

	if (m->get_value()) {
		/* this master is now enabled */
		if (get_boolean_masters() == 0) {
			_muteable.mute_master()->set_muted_by_masters (true);
			if (!muted_by_self()) {
				send_signal = true;
			}
		}
	} else {
		/* this master is disabled and there was only 1 enabled before */
		if (get_boolean_masters() == 1) {
			_muteable.mute_master()->set_muted_by_masters (false);
			if (!muted_by_self()) {
				send_signal = true;
			}
		}
	}
	return send_signal;
}

double
MuteControl::get_value () const
{
	if (slaved ()) {
		return muted_by_self() || muted_by_masters ();
	}

	if (_list && boost::dynamic_pointer_cast<AutomationList>(_list)->automation_playback()) {
		// Playing back automation, get the value from the list
		return AutomationControl::get_value();
	}

	return muted();
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
	/* have to get (self-muted) value from somewhere. could be our own
	   Control, or the Muteable that we sort-of proxy for. Since this
	   method is called by ::get_value(), use the latter to avoid recursion.
	*/
	return _muteable.mute_master()->muted_by_self() || muted_by_masters ();
}

bool
MuteControl::muted_by_self () const
{
	return _muteable.mute_master()->muted_by_self();
}

bool
MuteControl::muted_by_masters () const
{
	return _muteable.mute_master()->muted_by_masters ();
}

bool
MuteControl::muted_by_others_soloing () const
{
	return _muteable.muted_by_others_soloing ();
}

void
MuteControl::automation_run (samplepos_t start, pframes_t len)
{
	boolean_automation_run (start, len);

	bool valid = false;
	bool mute  = false;

	if (list() && automation_playback()) {
		mute = list()->rt_safe_eval (timepos_t (start), valid) >= 0.5;
	}

	if (!valid) {
		return;
	}

	if (muted_by_masters ()) {
		/* already muted, no need to check further,
		 * except we need to up update implicit/explict mute
		 */
		if (muted_by_self () != mute) {
			set_value_unchecked (mute ? 1. : 0.);
			Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
		}
		return;
	}

	if (mute && !muted()) {
		set_value_unchecked (1.0);  // mute
		Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
	} else if (!mute && muted()) {
		set_value_unchecked (0.0);  // unmute
		Changed (false, Controllable::NoGroup); /* EMIT SIGNAL */
	}
}
