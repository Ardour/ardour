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
	enum ButtonID {
		ButtonIo = 0x28,
		ButtonSends = 0x29,
		ButtonPan = 0x2a,
		ButtonPlugin = 0x2b,
		ButtonEq = 0x2c,
		ButtonDyn = 0x2d,
		ButtonLeft = 0x2e,
		ButtonRight = 0x2f,
		ButtonChannelLeft = 0x30,
		ButtonChannelRight = 0x31,
		ButtonFlip = 0x32,
		ButtonEdit = 0x33,
		ButtonNameValue = 0x34,
		ButtonTimecodeBeats = 0x35,
		ButtonF1 = 0x36,
		ButtonF2 = 0x37,
		ButtonF3 = 0x38,
		ButtonF4 = 0x39,
		ButtonF5 = 0x3a,
		ButtonF6 = 0x3b,
		ButtonF7 = 0x3c,
		ButtonF8 = 0x3d,
		ButtonF9 = 0x3e,
		ButtonF10 = 0x3f,
		ButtonF11 = 0x40,
		ButtonF12 = 0x41,
		ButtonF13 = 0x42,
		ButtonF14 = 0x43,
		ButtonF15 = 0x44,
		ButtonF16 = 0x45,
		ButtonShift = 0x46,
		ButtonOption = 0x47,
		ButtonControl = 0x48,
		ButtonCmdAlt = 0x49,
		ButtonOn = 0x4a,
		ButtonRecReady = 0x4b,
		ButtonUndo = 0x4c,
		ButtonSnapshot = 0x4d,
		ButtonTouch = 0x4e,
		ButtonRedo = 0x4f,
		ButtonMarker = 0x50,
		ButtonEnter = 0x51,
		ButtonCancel = 0x52,
		ButtonMixer = 0x53,
		ButtonFrmLeft = 0x54,
		ButtonFrmRight = 0x55,
		ButtonLoop = 0x56,
		ButtonPunchIn = 0x57,
		ButtonPunchOut = 0x58,
		ButtonHome = 0x59,
		ButtonEnd = 0x5a,
		ButtonRewind = 0x5b,
		ButtonFfwd = 0x5c,
		ButtonStop = 0x5d,
		ButtonPlay = 0x5e,
		ButtonRecord = 0x5f,
		ButtonCursorUp = 0x60,
		ButtonCursorDown = 0x61,
		ButtonCursorLeft = 0x62,
		ButtonCursorRight = 0x63,
		ButtonZoom = 0x64,
		ButtonScrub = 0x65,
		ButtonUserA = 0x66,
		ButtonUserB = 0x67,
	};

	Button (int id, int ordinal, std::string name, Group & group)
		: Control (id,  ordinal, name, group)
		, _led  (id, ordinal, name + "_led", group) {}
	
	virtual const Led & led() const  { return _led; }
	
	virtual type_t type() const { return type_button; };

	static Control* factory (Surface&, int id, int ordinal, const char*, Group&);
	
private:
	Led _led;
};

}

#endif
