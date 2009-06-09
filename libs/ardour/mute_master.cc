/*

    Copyright (C) 2009 Paul Davis 

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

#include "ardour/mute_master.h"
#include "ardour/rc_configuration.h"

#include "i18n.h"

using namespace ARDOUR;

MuteMaster::MuteMaster (Session& s, const std::string& name)
	: AutomationControl (s, Evoral::Parameter (MuteAutomation), boost::shared_ptr<AutomationList>(), name)
	, _mute_point (MutePoint (0))
{
	// default range for parameter is fine

	_automation = new AutomationList (MuteAutomation);
	set_list (boost::shared_ptr<AutomationList>(_automation));
}

void
MuteMaster::clear_mute ()
{
	if (_mute_point != MutePoint (0)) {
		_mute_point = MutePoint (0);
		MutePointChanged (); // EMIT SIGNAL
	}
}

void
MuteMaster::mute_at (MutePoint mp)
{
	if ((_mute_point & mp) != mp) {
		_mute_point = MutePoint (_mute_point | mp);
		MutePointChanged (); // EMIT SIGNAL
	}
}

void
MuteMaster::unmute_at (MutePoint mp)
{
	if ((_mute_point & mp) == mp) {
		_mute_point = MutePoint (_mute_point & ~mp);
		MutePointChanged (); // EMIT SIGNAL
	}
}

void
MuteMaster::mute (bool yn)
{
	/* convenience wrapper around AutomationControl method */

	if (yn) {
		set_value (1.0f);
	} else {
		set_value (0.0f);
	}
}

gain_t
MuteMaster::mute_gain_at (MutePoint mp) const
{
	if (_mute_point & mp) {
		return Config->get_solo_mute_gain ();
	} else {
		return 1.0;
	}
}

void
MuteMaster::set_value (float f)
{
	mute_at ((MutePoint) ((int) rint (f)));
}

float
MuteMaster::get_value () const
{
	return (float) _mute_point;
}

int
MuteMaster::set_state (const XMLNode& node)
{
	return 0;
}

XMLNode&
MuteMaster::get_state()
{
	return *(new XMLNode (X_("MuteMaster")));
}
