/*
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
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

#include "ardour/pannable.h"
#include "ardour/panner.h"
#include "ardour/pan_controllable.h"

using namespace ARDOUR;

void
PanControllable::actually_set_value (double v, Controllable::GroupControlDisposition group_override)
{
	v = std::min (upper (), std::max (lower (), v));

	if (!owner || !owner->panner()) {
		/* no panner: just do it */
		AutomationControl::actually_set_value (v, group_override);
		return;
	}

	boost::shared_ptr<Panner> p = owner->panner();

	bool can_set = false;

	switch (parameter().type()) {
		case PanWidthAutomation:
			can_set = p->clamp_width (v);
			break;
		case PanAzimuthAutomation:
			can_set = p->clamp_position (v);
			break;
		case PanElevationAutomation:
			can_set = p->clamp_elevation (v);
			break;
		default:
			break;
	}

	if (can_set) {
		AutomationControl::actually_set_value (v, group_override);
	}
}

std::string
PanControllable::get_user_string () const
{
	if (!owner || !owner->panner()) {
		/* assume PanAzimuthAutomation, 0..1 */
		float v = get_value ();
		char buf[32];
		snprintf(buf, sizeof(buf), "%.0f%%", 100.f * v);
		return buf;
	}
	return owner->panner()->value_as_string (boost::dynamic_pointer_cast<const AutomationControl>(shared_from_this()));
}
