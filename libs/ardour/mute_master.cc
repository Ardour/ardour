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

#include "ardour/types.h"
#include "ardour/mute_master.h"
#include "ardour/session.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace std;

const MuteMaster::MutePoint MuteMaster::AllPoints = MutePoint (MuteMaster::PreFader|
							       MuteMaster::PostFader|
							       MuteMaster::Listen|
							       MuteMaster::Main);

MuteMaster::MuteMaster (Session& s, const std::string&)
	: SessionHandleRef (s) 
        , _mute_point (AllPoints)
        , _self_muted (false)
        , _muted_by_others (0)
{
}

void
MuteMaster::mute_at (MutePoint mp)
{
	if ((_mute_point & mp) != mp) {
		_mute_point = MutePoint (_mute_point | mp);
                cerr << "Mute point set, now " << _mute_point << endl;
		MutePointChanged (); // EMIT SIGNAL
	}
}

void
MuteMaster::unmute_at (MutePoint mp)
{
	if ((_mute_point & mp) == mp) {
		_mute_point = MutePoint (_mute_point & ~mp);
                cerr << "Mute point unset, now " << _mute_point << endl;
		MutePointChanged (); // EMIT SIGNAL
	}
}

void
MuteMaster::clear_muted_by_others ()
{
        _muted_by_others = 0;
}

void
MuteMaster::mod_muted_by_others (int32_t delta)
{
	if (delta < 0) {
		if (_muted_by_others >= (uint32_t) abs (delta)) {
			_muted_by_others += delta;
		} else {
			_muted_by_others = 0;
		}
	} else {
		_muted_by_others += delta;
	}
}

void
MuteMaster::set_solo_level (SoloLevel l)
{
        _solo_level = l;
}

gain_t
MuteMaster::mute_gain_at (MutePoint mp) const
{
        gain_t gain;
        const SoloLevel l = _solo_level;

        // cerr << "solo level = " << _solo_level << " selfmuted " <<  self_muted_at (mp) << " omute " << muted_by_others_at (mp) << endl;
        
        if (Config->get_solo_mute_override()) {
                if ((l == SelfSoloed) || (l == DownstreamSoloed)) { 
                        gain = 1.0;
                } else if (self_muted_at (mp)) { // self-muted 
                        gain = Config->get_solo_mute_gain ();
                } else if (l == UpstreamSoloed) {
                        gain = 1.0;
                } else if (muted_by_others_at (mp)) { // muted by others
                        gain = Config->get_solo_mute_gain ();
                } else {
                        gain = 1.0;
                }
        } else {
                if (self_muted_at (mp)) { // self-muted 
                        gain = Config->get_solo_mute_gain ();
                } else if ((l == SelfSoloed) || (l == DownstreamSoloed)) {
                        gain = 1.0;
                } else if (muted_by_others_at (mp)) { // muted by others
                        gain = Config->get_solo_mute_gain ();
                } else if (l == UpstreamSoloed) { // soloed by others
                        gain = 1.0;
                } else {
                        gain = 1.0;
                }
        }

        // cerr << "\tgain = " << gain << endl;
        
        return gain;
}

void
MuteMaster::set_mute_points (const std::string& mute_point)
{
        MutePoint old = _mute_point;

	_mute_point = (MutePoint) string_2_enum (mute_point, _mute_point);
        cerr << "Mute point set from string, now " << _mute_point << endl;
        
        if (old != _mute_point) {
                MutePointChanged(); /* EMIT SIGNAL */
        }
}

void
MuteMaster::set_mute_points (MutePoint mp) 
{
        if (_mute_point != mp) {
                _mute_point = mp;
                cerr << "Mute point set from mp, now " << _mute_point << endl;
                MutePointChanged (); /* EMIT SIGNAL */
        }
}

int
MuteMaster::set_state (const XMLNode& node, int /*version*/)
{
	const XMLProperty* prop;

	if ((prop = node.property ("mute-point")) != 0) {
		_mute_point = (MutePoint) string_2_enum (prop->value(), _mute_point);
                cerr << "Mute point set from STATE string, now " << _mute_point << endl;
	}

	if ((prop = node.property ("muted")) != 0) {
		_self_muted = string_is_affirmative (prop->value());
	} else {
                _self_muted = (_mute_point != MutePoint (0));
        }

        if ((prop = node.property ("muted-by-others")) != 0) {
                if (sscanf (prop->value().c_str(), "%u", &_muted_by_others) != 1) {
                        _muted_by_others = 0;
                }
        } else {
                _muted_by_others = 0;
        }

	return 0;
}

XMLNode&
MuteMaster::get_state()
{
	XMLNode* node = new XMLNode (X_("MuteMaster"));
	node->add_property ("mute-point", enum_2_string (_mute_point));
	node->add_property ("muted", (_self_muted ? X_("yes") : X_("no")));

        char buf[32];
        snprintf (buf, sizeof (buf), "%u", _muted_by_others);
	node->add_property ("muted-by-others", buf);

	return *node;
}
