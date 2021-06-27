/*
 * Copyright (C) 2017 Ben Loftis <ben@harrisonconsoles.com>
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

#include <glib.h>

#include "ardour/ardour.h"

#include "button.h"
#include "surface.h"
#include "control_group.h"

using namespace ArdourSurface;
using namespace US2400;

Control*
Button::factory (Surface& surface, Button::ID bid, int id, const std::string& name, Group& group)
{
	Button* b = new Button (surface, bid, id, name, group);
	/* store button with the device-specific ID */
	surface.buttons[id] = b;
	surface.controls.push_back (b);
	group.add (*b);
	return b;
}

void
Button::pressed ()
{
	press_time = PBD::get_microseconds ();
}

void
Button::released ()
{
	press_time = 0;
}

int32_t
Button::long_press_count ()
{
	if (press_time == 0) {
		return -1; /* button is not pressed */
	}

	const PBD::microseconds_t delta = PBD::get_microseconds () - press_time;

	if (delta < 500000) {
		return 0;
	} else if (delta < 1000000) {
		return 1;
	}

	return 2;
}
int
Button::name_to_id (const std::string& name)
{
	if (!g_ascii_strcasecmp (name.c_str(), "Send")) { return Send; }
	if (!g_ascii_strcasecmp (name.c_str(), "Pan")) { return Pan; }
	if (!g_ascii_strcasecmp (name.c_str(), "Bank Left")) { return Left; }
	if (!g_ascii_strcasecmp (name.c_str(), "Bank Right")) { return Right; }
	if (!g_ascii_strcasecmp (name.c_str(), "Flip")) { return Flip; }
	if (!g_ascii_strcasecmp (name.c_str(), "Mstr Select")) { return MstrSelect; }
	if (!g_ascii_strcasecmp (name.c_str(), "F1")) { return F1; }
	if (!g_ascii_strcasecmp (name.c_str(), "F2")) { return F2; }
	if (!g_ascii_strcasecmp (name.c_str(), "F3")) { return F3; }
	if (!g_ascii_strcasecmp (name.c_str(), "F4")) { return F4; }
	if (!g_ascii_strcasecmp (name.c_str(), "F5")) { return F5; }
	if (!g_ascii_strcasecmp (name.c_str(), "F6")) { return F6; }
	if (!g_ascii_strcasecmp (name.c_str(), "Shift")) { return Shift; }
	if (!g_ascii_strcasecmp (name.c_str(), "Drop")) { return Drop; }
	if (!g_ascii_strcasecmp (name.c_str(), "Clear Solo")) { return ClearSolo; }
	if (!g_ascii_strcasecmp (name.c_str(), "Rewind")) { return Rewind; }
	if (!g_ascii_strcasecmp (name.c_str(), "Ffwd")) { return Ffwd; }
	if (!g_ascii_strcasecmp (name.c_str(), "Stop")) { return Stop; }
	if (!g_ascii_strcasecmp (name.c_str(), "Play")) { return Play; }
	if (!g_ascii_strcasecmp (name.c_str(), "Record")) { return Record; }
	if (!g_ascii_strcasecmp (name.c_str(), "Scrub")) { return Scrub; }

		/* Strip buttons */

	if (!g_ascii_strcasecmp (name.c_str(), "Solo")) { return Solo; }
	if (!g_ascii_strcasecmp (name.c_str(), "Mute")) { return Mute; }
	if (!g_ascii_strcasecmp (name.c_str(), "Select")) { return Select; }
	if (!g_ascii_strcasecmp (name.c_str(), "Fader Touch")) { return FaderTouch; }

	/* Master Fader button */

	if (!g_ascii_strcasecmp (name.c_str(), "Master Fader Touch")) { return MasterFaderTouch; }

	return -1;
}

std::string
Button::id_to_name (Button::ID id)
{
	if (id == Send) { return "Send"; }
	if (id == Pan) { return "Pan"; }
	if (id == Left) { return "Bank Left"; }
	if (id == Right) { return "Bank Right"; }
	if (id == Flip) { return "Flip"; }
	if (id == MstrSelect) { return "Mstr Select"; }
	if (id == F1) { return "F1"; }
	if (id == F2) { return "F2"; }
	if (id == F3) { return "F3"; }
	if (id == F4) { return "F4"; }
	if (id == F5) { return "F5"; }
	if (id == F6) { return "F6"; }
	if (id == Shift) { return "Shift"; }
	if (id == Drop) { return "Drop"; }
	if (id == ClearSolo) { return "Clear Solo"; }
	if (id == Rewind) { return "Rewind"; }
	if (id == Ffwd) { return "FFwd"; }
	if (id == Stop) { return "Stop"; }
	if (id == Play) { return "Play"; }
	if (id == Record) { return "Record"; }
	if (id == Scrub) { return "Scrub"; }

	if (id == Solo) { return "Solo"; }
	if (id == Mute) { return "Mute"; }
	if (id == Select) { return "Select"; }
	if (id == FaderTouch) { return "Fader Touch"; }

	if (id == MasterFaderTouch) { return "Master Fader Touch"; }

	return "???";
}
