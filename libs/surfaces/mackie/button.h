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

#ifndef __ardour_mackie_control_protocol_button_h__
#define __ardour_mackie_control_protocol_button_h__

#include "controls.h"
#include "led.h"

namespace Mackie {

class Surface;

class Button : public Control
{
public:
/* These values uniquely identify each possible button that an MCP device may
   send. Each DeviceInfo object contains its own set of button definitions that
   define what device ID will be sent for each button, and there is no reason
   for them to be the same.  */

	enum ID {
		/* Global Buttons */
		
		IO,
		Sends,
		Pan,
		Plugin,
		Eq,
		Dyn,
		Left,
		Right,
		ChannelLeft,
		ChannelRight,
		Flip,
		Edit,
		NameValue,
		TimecodeBeats,
		F1,
		F2,
		F3,
		F4,
		F5,
		F6,
		F7,
		F8,
		F9,
		F10,
		F11,
		F12,
		F13,
		F14,
		F15,
		F16,
		Shift,
		Option,
		Ctrl,
		CmdAlt,
		On,
		RecReady,
		Undo,
		Save,
		Touch,
		Redo,
		Marker,
		Enter,
		Cancel,
		Mixer,
		FrmLeft,
		FrmRight,
		Loop,
		PunchIn,
		PunchOut,
		Home,
		End,
		Rewind,
		Ffwd,
		Stop,
		Play,
		Record,
		CursorUp,
		CursorDown,
		CursorLeft,
		CursorRight,
		Zoom,
		Scrub,
		UserA,
		UserB,
		Snapshot,
		Read,
		Write,
		FdrGroup,
		ClearSolo,
		Track,
		Send,
		MidiTracks,
		Inputs,
		AudioTracks,
		AudioInstruments,
		Aux,
		Busses,
		Outputs,
		User,
		Trim,
		Latch,
		Grp,
		Nudge, 
		Drop,
		Replace,
		Click,
		View,

		/* Strip buttons */
		
		RecEnable,
		Solo,
		Mute,
		Select,
		VSelect,
		FaderTouch,
	};


	Button (ID bid, int did, std::string name, Group & group)
		: Control (did, name, group)
		, _bid (bid)
		, _led  (did, name + "_led", group) {}
	
	MidiByteArray zero() { return _led.zero (); }
	MidiByteArray set_state (LedState ls) { return _led.set_state (ls); }
	
	ID bid() const { return _bid; }
	
	static Control* factory (Surface& surface, Button::ID bid, int id, const std::string&, Group& group);
	static int name_to_id (const std::string& name);

private:
	ID  _bid; /* device independent button ID */
	Led _led;
};

}

#endif
