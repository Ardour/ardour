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
	if (name == "IO") { return IO; }
	if (name == "Sends") { return Sends; }
	if (name == "Pan") { return Pan; }
	if (name == "Plugin") { return Plugin; }
	if (name == "Eq") { return Eq; }
	if (name == "Dyn") { return Dyn; }
	if (name == "Left") { return Left; }
	if (name == "Right") { return Right; }
	if (name == "ChannelLeft") { return ChannelLeft; }
	if (name == "ChannelRight") { return ChannelRight; }
	if (name == "Flip") { return Flip; }
	if (name == "Edit") { return Edit; }
	if (name == "NameValue") { return NameValue; }
	if (name == "TimecodeBeats") { return TimecodeBeats; }
	if (name == "F1") { return F1; }
	if (name == "F2") { return F2; }
	if (name == "F3") { return F3; }
	if (name == "F4") { return F4; }
	if (name == "F5") { return F5; }
	if (name == "F6") { return F6; }
	if (name == "F7") { return F7; }
	if (name == "F8") { return F8; }
	if (name == "F9") { return F9; }
	if (name == "F10") { return F10; }
	if (name == "F11") { return F11; }
	if (name == "F12") { return F12; }
	if (name == "F13") { return F13; }
	if (name == "F14") { return F14; }
	if (name == "F15") { return F15; }
	if (name == "F16") { return F16; }
	if (name == "Shift") { return Shift; }
	if (name == "Option") { return Option; }
	if (name == "Ctrl") { return Ctrl; }
	if (name == "CmdAlt") { return CmdAlt; }
	if (name == "On") { return On; }
	if (name == "RecReady") { return RecReady; }
	if (name == "Undo") { return Undo; }
	if (name == "Save") { return Save; }
	if (name == "Touch") { return Touch; }
	if (name == "Redo") { return Redo; }
	if (name == "Marker") { return Marker; }
	if (name == "Enter") { return Enter; }
	if (name == "Cancel") { return Cancel; }
	if (name == "Mixer") { return Mixer; }
	if (name == "FrmLeft") { return FrmLeft; }
	if (name == "FrmRight") { return FrmRight; }
	if (name == "Loop") { return Loop; }
	if (name == "PunchIn") { return PunchIn; }
	if (name == "PunchOut") { return PunchOut; }
	if (name == "Home") { return Home; }
	if (name == "End") { return End; }
	if (name == "Rewind") { return Rewind; }
	if (name == "Ffwd") { return Ffwd; }
	if (name == "Stop") { return Stop; }
	if (name == "Play") { return Play; }
	if (name == "Record") { return Record; }
	if (name == "CursorUp") { return CursorUp; }
	if (name == "CursorDown") { return CursorDown; }
	if (name == "CursorLeft") { return CursorLeft; }
	if (name == "CursorRight") { return CursorRight; }
	if (name == "Zoom") { return Zoom; }
	if (name == "Scrub") { return Scrub; }
	if (name == "UserA") { return UserA; }
	if (name == "UserB") { return UserB; }
	if (name == "Snapshot") { return Snapshot; }
	if (name == "Read") { return Read; }
	if (name == "Write") { return Write; }
	if (name == "FdrGroup") { return FdrGroup; }
	if (name == "ClearSolo") { return ClearSolo; }
	if (name == "Track") { return Track; }
	if (name == "Send") { return Send; }
	if (name == "MidiTracks") { return MidiTracks; }
	if (name == "Inputs") { return Inputs; }
	if (name == "AudioTracks") { return AudioTracks; }
	if (name == "AudioInstruments") { return AudioInstruments; }
	if (name == "Aux") { return Aux; }
	if (name == "Busses") { return Busses; }
	if (name == "Outputs") { return Outputs; }
	if (name == "User") { return User; }
	if (name == "Trim") { return Trim; }
	if (name == "Latch") { return Latch; }
	if (name == "Grp") { return Grp; }
	if (name == "Nudge") { return Nudge; }
	if (name == "Drop") { return Drop; }
	if (name == "Replace") { return Replace; }
	if (name == "Click") { return Click; }
	if (name == "View") { return View; }
		
		/* Strip buttons */
		
	if (name == "RecEnable") { return RecEnable; }
	if (name == "Solo") { return Solo; }
	if (name == "Mute") { return Mute; }
	if (name == "Select") { return Select; }
	if (name == "VSelect") { return VSelect; }
	if (name == "FaderTouch") { return FaderTouch; }

	return -1;
}
