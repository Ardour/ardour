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

#include "led.h"
#include "surface.h"
#include "control_group.h"

using namespace ArdourSurface;
using namespace US2400;

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
	if (new_state == last_state) {
		if (new_state == llast_state) {
			return MidiByteArray ();
		}
	}
	llast_state = last_state;
	last_state = new_state;
	
	MIDI::byte msg = 0;

	switch  (new_state.state()) {
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
