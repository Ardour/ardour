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
	enum base_id_t {
		recenable_base_id = 0x0,
		solo_base_id = 0x08,
		mute_base_id = 0x10,
		select_base_id = 0x18,
		vselect_base_id = 0x20,
		fader_touch_base_id = 0xe0,
	};

	enum ButtonID {
		Io = 0x28,
		Sends = 0x29,
		Pan = 0x2a,
		Plugin = 0x2b,
		Eq = 0x2c,
		Dyn = 0x2d,
		Left = 0x2e,
		Right = 0x2f,
		ChannelLeft = 0x30,
		ChannelRight = 0x31,
		Flip = 0x32,
		Edit = 0x33,
		NameValue = 0x34,
		TimecodeBeats = 0x35,
		F1 = 0x36,
		F2 = 0x37,
		F3 = 0x38,
		F4 = 0x39,
		F5 = 0x3a,
		F6 = 0x3b,
		F7 = 0x3c,
		F8 = 0x3d,
		F9 = 0x3e,
		F10 = 0x3f,
		F11 = 0x40,
		F12 = 0x41,
		F13 = 0x42,
		F14 = 0x43,
		F15 = 0x44,
		F16 = 0x45,
		Shift = 0x46,
		Option = 0x47,
		Ctrl = 0x48,
		CmdAlt = 0x49,
		On = 0x4a,
		RecReady = 0x4b,
		Undo = 0x4c,
		Save = 0x4d,
		Touch = 0x4e,
		Redo = 0x4f,
		Marker = 0x50,
		Enter = 0x51,
		Cancel = 0x52,
		Mixer = 0x53,
		FrmLeft = 0x54,
		FrmRight = 0x55,
		Loop = 0x56,
		PunchIn = 0x57,
		PunchOut = 0x58,
		Home = 0x59,
		End = 0x5a,
		Rewind = 0x5b,
		Ffwd = 0x5c,
		Stop = 0x5d,
		Play = 0x5e,
		Record = 0x5f,
		CursorUp = 0x60,
		CursorDown = 0x61,
		CursorLeft = 0x62,
		CursorRight = 0x63,
		Zoom = 0x64,
		Scrub = 0x65,
		UserA = 0x66,
		UserB = 0x67,
	};

	Button (int id, std::string name, Group & group)
		: Control (id, name, group)
		, _led  (id, name + "_led", group) {}
	
	MidiByteArray zero() { return _led.zero (); }
	MidiByteArray set_state (LedState ls) { return _led.set_state (ls); }

	static Control* factory (Surface&, int id, const char*, Group&);
	
private:
	Led _led;
};

}

#endif
