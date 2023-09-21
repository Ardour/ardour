/*
 * Copyright (C) 2000-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2021 Robin Gareus <robin@gareus.org>
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

#include "pbd/controllable.h"

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/solo_mute_release.h"
#include "ardour/utils.h"

using namespace PBD;
using namespace ARDOUR;

SoloMuteRelease::SoloMuteRelease (bool was_active)
	: active (was_active)
	, exclusive (false)
{
}

void
SoloMuteRelease::set_exclusive (bool e)
{
	exclusive = e;
}

void
SoloMuteRelease::set (std::shared_ptr<Stripable> r)
{
	std::shared_ptr<StripableList> sl (new StripableList);
	if (active) {
		sl->push_back (r);
		routes_on = sl;
	} else {
		sl->push_back (r);
		routes_off = sl;
	}
}

void
SoloMuteRelease::set (std::shared_ptr<StripableList const> rl)
{
	if (active) {
		routes_on = rl;
	} else {
		routes_off = rl;
	}
}

void
SoloMuteRelease::set (std::shared_ptr<StripableList const> on, std::shared_ptr<StripableList const> off)
{
	routes_on = on;
	routes_off = off;
}

void
SoloMuteRelease::set (std::shared_ptr<std::list<std::string> > pml)
{
	port_monitors = pml;
}

void
SoloMuteRelease::release (Session* s, bool mute) const
{
	if (mute) {
		s->set_controls (stripable_list_to_control_list (routes_off, &Stripable::mute_control), 0.0, exclusive ? Controllable::NoGroup : Controllable::NoGroup);
		s->set_controls (stripable_list_to_control_list (routes_on,  &Stripable::mute_control), 1.0, exclusive ? Controllable::NoGroup : Controllable::NoGroup);
	} else {
		s->set_controls (stripable_list_to_control_list (routes_off, &Stripable::solo_control), 0.0, exclusive ? Controllable::NoGroup : Controllable::NoGroup);
		s->set_controls (stripable_list_to_control_list (routes_on,  &Stripable::solo_control), 1.0, exclusive ? Controllable::NoGroup : Controllable::NoGroup);

		if (port_monitors && s->monitor_out ()) {
			s->engine().monitor_port().set_active_monitors (*port_monitors);
		}
	}
}
