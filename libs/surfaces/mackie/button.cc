/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2015 Paul Davis <paul@linuxaudiosystems.com>
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
using namespace Mackie;

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
	if (!g_ascii_strcasecmp (name.c_str(), "Track")) { return Track; }
	if (!g_ascii_strcasecmp (name.c_str(), "Send")) { return Send; }
	if (!g_ascii_strcasecmp (name.c_str(), "Pan")) { return Pan; }
	if (!g_ascii_strcasecmp (name.c_str(), "Plugin")) { return Plugin; }
	if (!g_ascii_strcasecmp (name.c_str(), "Eq")) { return Eq; }
	if (!g_ascii_strcasecmp (name.c_str(), "Dyn")) { return Dyn; }
	if (!g_ascii_strcasecmp (name.c_str(), "Bank Left")) { return Left; }
	if (!g_ascii_strcasecmp (name.c_str(), "Bank Right")) { return Right; }
	if (!g_ascii_strcasecmp (name.c_str(), "Channel Left")) { return ChannelLeft; }
	if (!g_ascii_strcasecmp (name.c_str(), "Channel Right")) { return ChannelRight; }
	if (!g_ascii_strcasecmp (name.c_str(), "Flip")) { return Flip; }
	if (!g_ascii_strcasecmp (name.c_str(), "View")) { return View; }
	if (!g_ascii_strcasecmp (name.c_str(), "Name/Value")) { return NameValue; }
	if (!g_ascii_strcasecmp (name.c_str(), "Timecode/Beats")) { return TimecodeBeats; }
	if (!g_ascii_strcasecmp (name.c_str(), "F1")) { return F1; }
	if (!g_ascii_strcasecmp (name.c_str(), "F2")) { return F2; }
	if (!g_ascii_strcasecmp (name.c_str(), "F3")) { return F3; }
	if (!g_ascii_strcasecmp (name.c_str(), "F4")) { return F4; }
	if (!g_ascii_strcasecmp (name.c_str(), "F5")) { return F5; }
	if (!g_ascii_strcasecmp (name.c_str(), "F6")) { return F6; }
	if (!g_ascii_strcasecmp (name.c_str(), "F7")) { return F7; }
	if (!g_ascii_strcasecmp (name.c_str(), "F8")) { return F8; }
	if (!g_ascii_strcasecmp (name.c_str(), "Midi Tracks")) { return MidiTracks; }
	if (!g_ascii_strcasecmp (name.c_str(), "Inputs")) { return Inputs; }
	if (!g_ascii_strcasecmp (name.c_str(), "Audio Tracks")) { return AudioTracks; }
	if (!g_ascii_strcasecmp (name.c_str(), "Audio Instruments")) { return AudioInstruments; }
	if (!g_ascii_strcasecmp (name.c_str(), "Aux")) { return Aux; }
	if (!g_ascii_strcasecmp (name.c_str(), "Busses")) { return Busses; }
	if (!g_ascii_strcasecmp (name.c_str(), "Outputs")) { return Outputs; }
	if (!g_ascii_strcasecmp (name.c_str(), "User")) { return User; }
	if (!g_ascii_strcasecmp (name.c_str(), "UserA")) { return UserA; }
	if (!g_ascii_strcasecmp (name.c_str(), "UserB")) { return UserB; }
	if (!g_ascii_strcasecmp (name.c_str(), "Shift")) { return Shift; }
	if (!g_ascii_strcasecmp (name.c_str(), "Option")) { return Option; }
	if (!g_ascii_strcasecmp (name.c_str(), "Ctrl")) { return Ctrl; }
	if (!g_ascii_strcasecmp (name.c_str(), "CmdAlt")) { return CmdAlt; }
	if (!g_ascii_strcasecmp (name.c_str(), "Read")) { return Read; }
	if (!g_ascii_strcasecmp (name.c_str(), "Write")) { return Write; }
	if (!g_ascii_strcasecmp (name.c_str(), "Trim")) { return Trim; }
	if (!g_ascii_strcasecmp (name.c_str(), "Touch")) { return Touch; }
	if (!g_ascii_strcasecmp (name.c_str(), "Latch")) { return Latch; }
	if (!g_ascii_strcasecmp (name.c_str(), "Group")) { return Grp; }
	if (!g_ascii_strcasecmp (name.c_str(), "Save")) { return Save; }
	if (!g_ascii_strcasecmp (name.c_str(), "Undo")) { return Undo; }
	if (!g_ascii_strcasecmp (name.c_str(), "Cancel")) { return Cancel; }
	if (!g_ascii_strcasecmp (name.c_str(), "Enter")) { return Enter; }
	if (!g_ascii_strcasecmp (name.c_str(), "Marker")) { return Marker; }
	if (!g_ascii_strcasecmp (name.c_str(), "Nudge")) { return Nudge; }
	if (!g_ascii_strcasecmp (name.c_str(), "Loop")) { return Loop; }
	if (!g_ascii_strcasecmp (name.c_str(), "Drop")) { return Drop; }
	if (!g_ascii_strcasecmp (name.c_str(), "Replace")) { return Replace; }
	if (!g_ascii_strcasecmp (name.c_str(), "Click")) { return Click; }
	if (!g_ascii_strcasecmp (name.c_str(), "Clear Solo")) { return ClearSolo; }
	if (!g_ascii_strcasecmp (name.c_str(), "Rewind")) { return Rewind; }
	if (!g_ascii_strcasecmp (name.c_str(), "Ffwd")) { return Ffwd; }
	if (!g_ascii_strcasecmp (name.c_str(), "Stop")) { return Stop; }
	if (!g_ascii_strcasecmp (name.c_str(), "Play")) { return Play; }
	if (!g_ascii_strcasecmp (name.c_str(), "Record")) { return Record; }
	if (!g_ascii_strcasecmp (name.c_str(), "Cursor Up")) { return CursorUp; }
	if (!g_ascii_strcasecmp (name.c_str(), "Cursor Down")) { return CursorDown; }
	if (!g_ascii_strcasecmp (name.c_str(), "Cursor Left")) { return CursorLeft; }
	if (!g_ascii_strcasecmp (name.c_str(), "Cursor Right")) { return CursorRight; }
	if (!g_ascii_strcasecmp (name.c_str(), "Zoom")) { return Zoom; }
	if (!g_ascii_strcasecmp (name.c_str(), "Scrub")) { return Scrub; }
	if (!g_ascii_strcasecmp (name.c_str(), "User A")) { return UserA; }
	if (!g_ascii_strcasecmp (name.c_str(), "User B")) { return UserB; }

		/* Strip buttons */

	if (!g_ascii_strcasecmp (name.c_str(), "Record Enable")) { return RecEnable; }
	if (!g_ascii_strcasecmp (name.c_str(), "Solo")) { return Solo; }
	if (!g_ascii_strcasecmp (name.c_str(), "Mute")) { return Mute; }
	if (!g_ascii_strcasecmp (name.c_str(), "Select")) { return Select; }
	if (!g_ascii_strcasecmp (name.c_str(), "V-Pot")) { return VSelect; }
	if (!g_ascii_strcasecmp (name.c_str(), "Fader Touch")) { return FaderTouch; }

	/* Master Fader button */

	if (!g_ascii_strcasecmp (name.c_str(), "Master Fader Touch")) { return MasterFaderTouch; }

	return -1;
}

std::string
Button::id_to_name (Button::ID id)
{
	if (id == Track) { return "Track"; }
	if (id == Send) { return "Send"; }
	if (id == Pan) { return "Pan"; }
	if (id == Plugin) { return "Plugin"; }
	if (id == Eq) { return "Eq"; }
	if (id == Dyn) { return "Dyn"; }
	if (id == Left) { return "Bank Left"; }
	if (id == Right) { return "Bank Right"; }
	if (id == ChannelLeft) { return "Channel Left"; }
	if (id == ChannelRight) { return "Channel Right"; }
	if (id == Flip) { return "Flip"; }
	if (id == View) { return "View"; }
	if (id == NameValue) { return "Name/Value"; }
	if (id == TimecodeBeats) { return "Timecode/Beats"; }
	if (id == F1) { return "F1"; }
	if (id == F2) { return "F2"; }
	if (id == F3) { return "F3"; }
	if (id == F4) { return "F4"; }
	if (id == F5) { return "F5"; }
	if (id == F6) { return "F6"; }
	if (id == F7) { return "F7"; }
	if (id == F8) { return "F8"; }
	if (id == MidiTracks) { return "Midi Tracks"; }
	if (id == Inputs) { return "Inputs"; }
	if (id == AudioTracks) { return "Audio Tracks"; }
	if (id == AudioInstruments) { return "Audio Instruments"; }
	if (id == Aux) { return "Aux"; }
	if (id == Busses) { return "Busses"; }
	if (id == Outputs) { return "Outputs"; }
	if (id == User) { return "User"; }
	if (id == Shift) { return "Shift"; }
	if (id == Option) { return "Option"; }
	if (id == Ctrl) { return "Ctrl"; }
	if (id == CmdAlt) { return "CmdAlt"; }
	if (id == Read) { return "Read"; }
	if (id == Write) { return "Write"; }
	if (id == Trim) { return "Trim"; }
	if (id == Touch) { return "Touch"; }
	if (id == Latch) { return "Latch"; }
	if (id == Grp) { return "Group"; }
	if (id == Save) { return "Save"; }
	if (id == Undo) { return "Undo"; }
	if (id == Cancel) { return "Cancel"; }
	if (id == Enter) { return "Enter"; }
	if (id == Marker) { return "Marker"; }
	if (id == Nudge) { return "Nudge"; }
	if (id == Loop) { return "Loop"; }
	if (id == Drop) { return "Drop"; }
	if (id == Replace) { return "Replace"; }
	if (id == Click) { return "Click"; }
	if (id == ClearSolo) { return "Clear Solo"; }
	if (id == Rewind) { return "Rewind"; }
	if (id == Ffwd) { return "FFwd"; }
	if (id == Stop) { return "Stop"; }
	if (id == Play) { return "Play"; }
	if (id == Record) { return "Record"; }
	if (id == CursorUp) { return "Cursor Up"; }
	if (id == CursorDown) { return "Cursor Down"; }
	if (id == CursorLeft) { return "Cursor Left"; }
	if (id == CursorRight) { return "Cursor Right"; }
	if (id == Zoom) { return "Zoom"; }
	if (id == Scrub) { return "Scrub"; }
	if (id == UserA) { return "User A"; }
	if (id == UserB) { return "User B"; }

	if (id == RecEnable) { return "Record Enable"; }
	if (id == Solo) { return "Solo"; }
	if (id == Mute) { return "Mute"; }
	if (id == Select) { return "Select"; }
	if (id == VSelect) { return "V-Pot"; }
	if (id == FaderTouch) { return "Fader Touch"; }

	if (id == MasterFaderTouch) { return "Master Fader Touch"; }

	return "???";
}
