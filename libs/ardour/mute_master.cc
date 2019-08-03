/*
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include "pbd/enumwriter.h"
#include "pbd/xml++.h"
#include "pbd/enum_convert.h"

#include "ardour/types.h"
#include "ardour/mute_master.h"
#include "ardour/session.h"

#include "pbd/i18n.h"

namespace PBD {
	DEFINE_ENUM_CONVERT(ARDOUR::MuteMaster::MutePoint);
}

using namespace ARDOUR;
using namespace std;

const string MuteMaster::xml_node_name (X_("MuteMaster"));

const MuteMaster::MutePoint MuteMaster::AllPoints = MuteMaster::MutePoint(
	PreFader|PostFader|Listen|Main);

MuteMaster::MuteMaster (Session& s, Muteable& m, const std::string&)
	: SessionHandleRef (s)
	, _muteable (&m)
	, _mute_point (MutePoint (0))
	, _muted_by_self (false)
	, _soloed_by_self (false)
	, _soloed_by_others (false)
	, _muted_by_masters (0)
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

gain_t
MuteMaster::mute_gain_at (MutePoint mp) const
{
	gain_t gain;

	if (Config->get_solo_mute_override()) {
		if (_soloed_by_self) {
			gain = GAIN_COEFF_UNITY;
		} else if (muted_by_self_at (mp) || muted_by_masters_at (mp)) {
			gain = GAIN_COEFF_ZERO;
		} else {
			if (!_soloed_by_others && muted_by_others_soloing_at (mp)) {
				gain = Config->get_solo_mute_gain ();
			} else {
				gain = GAIN_COEFF_UNITY;
			}
		}
	} else {
		if (muted_by_self_at (mp) || muted_by_masters_at (mp)) {
			gain = GAIN_COEFF_ZERO;
		} else if (_soloed_by_self || _soloed_by_others) {
			gain = GAIN_COEFF_UNITY;
		} else {
			if (muted_by_others_soloing_at (mp)) {
				gain = Config->get_solo_mute_gain ();
			} else {
				gain = GAIN_COEFF_UNITY;
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
	node.get_property ("mute-point", _mute_point);

	if (!node.get_property ("muted", _muted_by_self)) {
		_muted_by_self = (_mute_point != MutePoint (0));
	}

	return 0;
}

XMLNode&
MuteMaster::get_state()
{
	XMLNode* node = new XMLNode (xml_node_name);
	node->set_property ("mute-point", _mute_point);
	node->set_property ("muted", _muted_by_self);
	return *node;
}

bool
MuteMaster::muted_by_others_soloing_at (MutePoint mp) const
{
	return _muteable->muted_by_others_soloing() && (_mute_point & mp);
}

void
MuteMaster::set_muted_by_masters (bool yn)
{
	_muted_by_masters = yn;
}
