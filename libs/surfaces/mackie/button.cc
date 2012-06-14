/*
	Copyright (C) 2006,2007 John Anderson
	Copyright (C) 2012 Paul Davis

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

#include <glib.h>

#include "button.h"
#include "surface.h"
#include "control_group.h"

using namespace Mackie;

Control*
Button::factory (Surface& surface, Button::ID bid, int id, const std::string& name, Group& group)
{
	Button* b = new Button (bid, id, name, group);
	/* store button with the device-specific ID */
	surface.buttons[id] = b;
	surface.controls.push_back (b);
	group.add (*b);
	return b;
}

int
Button::name_to_id (const std::string& name)
{
	if (!g_ascii_strcasecmp (name.c_str(), "IO")) { return IO; }
	if (!g_ascii_strcasecmp (name.c_str(), "Sends")) { return Sends; }
	if (!g_ascii_strcasecmp (name.c_str(), "Pan")) { return Pan; }
	if (!g_ascii_strcasecmp (name.c_str(), "Plugin")) { return Plugin; }
	if (!g_ascii_strcasecmp (name.c_str(), "Eq")) { return Eq; }
	if (!g_ascii_strcasecmp (name.c_str(), "Dyn")) { return Dyn; }
	if (!g_ascii_strcasecmp (name.c_str(), "Left")) { return Left; }
	if (!g_ascii_strcasecmp (name.c_str(), "Right")) { return Right; }
	if (!g_ascii_strcasecmp (name.c_str(), "ChannelLeft")) { return ChannelLeft; }
	if (!g_ascii_strcasecmp (name.c_str(), "ChannelRight")) { return ChannelRight; }
	if (!g_ascii_strcasecmp (name.c_str(), "Flip")) { return Flip; }
	if (!g_ascii_strcasecmp (name.c_str(), "Edit")) { return Edit; }
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
	if (!g_ascii_strcasecmp (name.c_str(), "F9")) { return F9; }
	if (!g_ascii_strcasecmp (name.c_str(), "F10")) { return F10; }
	if (!g_ascii_strcasecmp (name.c_str(), "F11")) { return F11; }
	if (!g_ascii_strcasecmp (name.c_str(), "F12")) { return F12; }
	if (!g_ascii_strcasecmp (name.c_str(), "F13")) { return F13; }
	if (!g_ascii_strcasecmp (name.c_str(), "F14")) { return F14; }
	if (!g_ascii_strcasecmp (name.c_str(), "F15")) { return F15; }
	if (!g_ascii_strcasecmp (name.c_str(), "F16")) { return F16; }
	if (!g_ascii_strcasecmp (name.c_str(), "Shift")) { return Shift; }
	if (!g_ascii_strcasecmp (name.c_str(), "Option")) { return Option; }
	if (!g_ascii_strcasecmp (name.c_str(), "Ctrl")) { return Ctrl; }
	if (!g_ascii_strcasecmp (name.c_str(), "CmdAlt")) { return CmdAlt; }
	if (!g_ascii_strcasecmp (name.c_str(), "On")) { return On; }
	if (!g_ascii_strcasecmp (name.c_str(), "RecReady")) { return RecReady; }
	if (!g_ascii_strcasecmp (name.c_str(), "Undo")) { return Undo; }
	if (!g_ascii_strcasecmp (name.c_str(), "Save")) { return Save; }
	if (!g_ascii_strcasecmp (name.c_str(), "Touch")) { return Touch; }
	if (!g_ascii_strcasecmp (name.c_str(), "Redo")) { return Redo; }
	if (!g_ascii_strcasecmp (name.c_str(), "Marker")) { return Marker; }
	if (!g_ascii_strcasecmp (name.c_str(), "Enter")) { return Enter; }
	if (!g_ascii_strcasecmp (name.c_str(), "Cancel")) { return Cancel; }
	if (!g_ascii_strcasecmp (name.c_str(), "Mixer")) { return Mixer; }
	if (!g_ascii_strcasecmp (name.c_str(), "FrmLeft")) { return FrmLeft; }
	if (!g_ascii_strcasecmp (name.c_str(), "FrmRight")) { return FrmRight; }
	if (!g_ascii_strcasecmp (name.c_str(), "Loop")) { return Loop; }
	if (!g_ascii_strcasecmp (name.c_str(), "PunchIn")) { return PunchIn; }
	if (!g_ascii_strcasecmp (name.c_str(), "PunchOut")) { return PunchOut; }
	if (!g_ascii_strcasecmp (name.c_str(), "Home")) { return Home; }
	if (!g_ascii_strcasecmp (name.c_str(), "End")) { return End; }
	if (!g_ascii_strcasecmp (name.c_str(), "Rewind")) { return Rewind; }
	if (!g_ascii_strcasecmp (name.c_str(), "Ffwd")) { return Ffwd; }
	if (!g_ascii_strcasecmp (name.c_str(), "Stop")) { return Stop; }
	if (!g_ascii_strcasecmp (name.c_str(), "Play")) { return Play; }
	if (!g_ascii_strcasecmp (name.c_str(), "Record")) { return Record; }
	if (!g_ascii_strcasecmp (name.c_str(), "CursorUp")) { return CursorUp; }
	if (!g_ascii_strcasecmp (name.c_str(), "CursorDown")) { return CursorDown; }
	if (!g_ascii_strcasecmp (name.c_str(), "CursorLeft")) { return CursorLeft; }
	if (!g_ascii_strcasecmp (name.c_str(), "CursorRight")) { return CursorRight; }
	if (!g_ascii_strcasecmp (name.c_str(), "Zoom")) { return Zoom; }
	if (!g_ascii_strcasecmp (name.c_str(), "Scrub")) { return Scrub; }
	if (!g_ascii_strcasecmp (name.c_str(), "UserA")) { return UserA; }
	if (!g_ascii_strcasecmp (name.c_str(), "UserB")) { return UserB; }
	if (!g_ascii_strcasecmp (name.c_str(), "Snapshot")) { return Snapshot; }
	if (!g_ascii_strcasecmp (name.c_str(), "Read")) { return Read; }
	if (!g_ascii_strcasecmp (name.c_str(), "Write")) { return Write; }
	if (!g_ascii_strcasecmp (name.c_str(), "FdrGroup")) { return FdrGroup; }
	if (!g_ascii_strcasecmp (name.c_str(), "ClearSolo")) { return ClearSolo; }
	if (!g_ascii_strcasecmp (name.c_str(), "Track")) { return Track; }
	if (!g_ascii_strcasecmp (name.c_str(), "Send")) { return Send; }
	if (!g_ascii_strcasecmp (name.c_str(), "MidiTracks")) { return MidiTracks; }
	if (!g_ascii_strcasecmp (name.c_str(), "Inputs")) { return Inputs; }
	if (!g_ascii_strcasecmp (name.c_str(), "AudioTracks")) { return AudioTracks; }
	if (!g_ascii_strcasecmp (name.c_str(), "AudioInstruments")) { return AudioInstruments; }
	if (!g_ascii_strcasecmp (name.c_str(), "Aux")) { return Aux; }
	if (!g_ascii_strcasecmp (name.c_str(), "Busses")) { return Busses; }
	if (!g_ascii_strcasecmp (name.c_str(), "Outputs")) { return Outputs; }
	if (!g_ascii_strcasecmp (name.c_str(), "User")) { return User; }
	if (!g_ascii_strcasecmp (name.c_str(), "Trim")) { return Trim; }
	if (!g_ascii_strcasecmp (name.c_str(), "Latch")) { return Latch; }
	if (!g_ascii_strcasecmp (name.c_str(), "Grp")) { return Grp; }
	if (!g_ascii_strcasecmp (name.c_str(), "Nudge")) { return Nudge; }
	if (!g_ascii_strcasecmp (name.c_str(), "Drop")) { return Drop; }
	if (!g_ascii_strcasecmp (name.c_str(), "Replace")) { return Replace; }
	if (!g_ascii_strcasecmp (name.c_str(), "Click")) { return Click; }
	if (!g_ascii_strcasecmp (name.c_str(), "View")) { return View; }
		
		/* Strip buttons */
		
	if (!g_ascii_strcasecmp (name.c_str(), "RecEnable")) { return RecEnable; }
	if (!g_ascii_strcasecmp (name.c_str(), "Solo")) { return Solo; }
	if (!g_ascii_strcasecmp (name.c_str(), "Mute")) { return Mute; }
	if (!g_ascii_strcasecmp (name.c_str(), "Select")) { return Select; }
	if (!g_ascii_strcasecmp (name.c_str(), "VSelect")) { return VSelect; }
	if (!g_ascii_strcasecmp (name.c_str(), "FaderTouch")) { return FaderTouch; }

	/* Master Fader button */

	if (!g_ascii_strcasecmp (name.c_str(), "MasterFaderTouch")) { return MasterFaderTouch; }

	return -1;
}

std::string
Button::id_to_name (Button::ID id)
{
	if (id == IO)  { return "IO"; }
	if (id == Sends) { return "Sends"; }
	if (id == Pan) { return "Pan"; }
	if (id == Plugin) { return "Plugin"; }
	if (id == Eq) { return "Eq"; }
	if (id == Dyn) { return "Dyn"; }
	if (id == Left) { return "Bank Left"; }
	if (id == Right) { return "Bank Right"; }
	if (id == ChannelLeft) { return "Channel Left"; }
	if (id == ChannelRight) { return "Channel Right"; }
	if (id == Flip) { return "Flip"; }
	if (id == Edit) { return "Edit"; }
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
	if (id == F9) { return "F9"; }
	if (id == F10) { return "F10"; }
	if (id == F11) { return "F11"; }
	if (id == F12) { return "F12"; }
	if (id == F13) { return "F13"; }
	if (id == F14) { return "F14"; }
	if (id == F15) { return "F15"; }
	if (id == F16) { return "F16"; }
	if (id == Shift) { return "Shift"; }
	if (id == Option) { return "Option"; }
	if (id == Ctrl) { return "Ctrl"; }
	if (id == CmdAlt) { return "CmdAlt"; }
	if (id == On) { return "On"; }
	if (id == RecReady) { return "Record"; }
	if (id == Undo) { return "Undo"; }
	if (id == Save) { return "Save"; }
	if (id == Touch) { return "Touch"; }
	if (id == Redo) { return "Redo"; }
	if (id == Marker) { return "Marker"; }
	if (id == Enter) { return "Enter"; }
	if (id == Cancel) { return "Cancel"; }
	if (id == Mixer) { return "Mixer"; }
	if (id == FrmLeft) { return "Frm Left"; }
	if (id == FrmRight) { return "Frm Right"; }
	if (id == Loop) { return "Loop"; }
	if (id == PunchIn) { return "Punch In"; }
	if (id == PunchOut) { return "Punch Out"; }
	if (id == Home) { return "Home"; }
	if (id == End) { return "End"; }
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
	if (id == Snapshot) { return "Snapshot"; }
	if (id == Read) { return "Read"; }
	if (id == Write) { return "Write"; }
	if (id == FdrGroup) { return "Fader Group"; }
	if (id == ClearSolo) { return "Clear Solo"; }
	if (id == Track) { return "Track"; }
	if (id == Send) { return "Send"; }
	if (id == MidiTracks) { return "Midi Tracks"; }
	if (id == Inputs) { return "Inputs"; }
	if (id == AudioTracks) { return "Audio Tracks"; }
	if (id == AudioInstruments) { return "Audio Instruments"; }
	if (id == Aux) { return "Aux"; }
	if (id == Busses) { return "Busses"; }
	if (id == Outputs) { return "Outputs"; }
	if (id == User) { return "User"; }
	if (id == Trim) { return "Trim"; }
	if (id == Latch) { return "Latch"; }
	if (id == Grp) { return "Group"; }
	if (id == Nudge) { return "Nudge"; }
	if (id == Drop) { return "Drop"; }
	if (id == Replace) { return "Replace"; }
	if (id == Click) { return "Click"; }
	if (id == View) { return "View"; }

	if (id == RecEnable) { return "Record Enable"; }
	if (id == Solo) { return "Solo"; }
	if (id == Mute) { return "Mute"; }
	if (id == Select) { return "Select"; }
	if (id == VSelect) { return "V-Pot"; }
	if (id == FaderTouch) { return "Fader Touch"; }

	if (id == MasterFaderTouch) { return "Master Fader Touch"; }

	return "???";
}
