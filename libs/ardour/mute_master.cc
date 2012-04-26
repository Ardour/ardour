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

#include "pbd/enumwriter.h"
#include "pbd/xml++.h"
#include "pbd/convert.h"

#include "ardour/types.h"
#include "ardour/mute_master.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

MuteMaster::MuteMaster (Session& s, const std::string&)
	: SessionHandleRef (s)
	, _mute_point (MutePoint (0))
        , _muted_by_self (false)
        , _soloed (false)
        , _solo_ignore (false)
{

	if (Config->get_mute_affects_pre_fader ()) {
		_mute_point = MutePoint (_mute_point | PreFader);
	}

	if (Config->get_mute_affects_post_fader ()) {
		_mute_point = MutePoint (_mute_point | PostFader);
	}

	if (Config->get_mute_affects_control_outs ()) {
		_mute_point = MutePoint (_mute_point | Listen);
	}

	if (Config->get_mute_affects_main_outs ()) {
		_mute_point = MutePoint (_mute_point | Main);
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
MuteMaster::set_soloed (bool yn)
{
        _soloed = yn;
}

gain_t
MuteMaster::mute_gain_at (MutePoint mp) const
{
        gain_t gain;

        if (Config->get_solo_mute_override()) {
                if (_soloed) {
                        gain = 1.0;
                } else if (muted_by_self_at (mp)) {
                        gain = 0.0;
                } else {
                        if (muted_by_others_at (mp)) {
                                gain = Config->get_solo_mute_gain ();
                        } else {
                                gain = 1.0;
                        }
                }
        } else {
                if (muted_by_self_at (mp)) {
                        gain = 0.0;
                } else if (_soloed) {
                        gain = 1.0;
                } else {
                        if (muted_by_others_at (mp)) {
                                gain = Config->get_solo_mute_gain ();
                        } else {
                                gain = 1.0;
                        }
                }
        }

        return gain;
}

void
MuteMaster::set_mute_points (const std::string& mute_point)
{
        MutePoint old = _mute_point;

	_mute_point = (MutePoint) string_2_enum (mute_point, _mute_point);

        if (old != _mute_point) {
                MutePointChanged(); /* EMIT SIGNAL */
        }
}

void
MuteMaster::set_mute_points (MutePoint mp)
{
        if (_mute_point != mp) {
                _mute_point = mp;
                MutePointChanged (); /* EMIT SIGNAL */
        }
}

int
MuteMaster::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;

	if ((prop = node.property ("mute-point")) != 0) {
		_mute_point = (MutePoint) string_2_enum (prop->value(), _mute_point);
	}

	if ((prop = node.property ("muted")) != 0) {
		_muted_by_self = PBD::string_is_affirmative (prop->value());
	} else {
                _muted_by_self = (_mute_point != MutePoint (0));
        }

	return 0;
}

XMLNode&
MuteMaster::get_state()
{
	XMLNode* node = new XMLNode (X_("MuteMaster"));
	node->add_property ("mute-point", enum_2_string (_mute_point));
	node->add_property ("muted", (_muted_by_self ? X_("yes") : X_("no")));
	return *node;
}

bool
MuteMaster::muted_by_others_at (MutePoint mp) const
{
	return (!_solo_ignore && _session.soloing() && (_mute_point & mp));
}

