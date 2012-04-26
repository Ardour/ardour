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

#include "led.h"
#include "surface.h"
#include "control_group.h"

using namespace Mackie;

const int Led::FaderTouch = 0x70;
const int Led::Timecode = 0x71;
const int Led::Beats = 0x72;
const int Led::RudeSolo = 0x73;
const int Led::RelayClick = 0x74;

Control*
Led::factory (Surface& surface, int id, const char* name, Group& group)
{
	Led* l = new Led (id, name, group);
	surface.leds[id] = l;
	surface.controls.push_back (l);
	group.add (*l);
	return l;
}

MidiByteArray 
Led::set_state (LedState new_state)
{
	state = new_state;

	MIDI::byte msg = 0;

	switch  (state.state()) {
	case LedState::on:			
		msg = 0x7f; 
		break;
	case LedState::off:			
		msg = 0x00; 
		break;
	case LedState::flashing:	
		msg = 0x01; 
		break;
	case LedState::none:			
		return MidiByteArray ();
	}
	
	return MidiByteArray  (3, 0x90, id(), msg);
}
