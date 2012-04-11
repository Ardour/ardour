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

#ifndef __ardour_mackie_control_protocol_led_h__
#define __ardour_mackie_control_protocol_led_h__

#include "controls.h"
#include "midi_byte_array.h"
#include "types.h"

namespace Mackie {

class Led : public Control
{
public:
	Led (int id, std::string name, Group & group)
		: Control (id, name, group)
		, state (off)
	{
	}
	
	Led & led() { return *this; }
	MidiByteArray set_state (LedState);

	MidiByteArray zero() { return set_state (off); }
	
	static Control* factory (Surface&, int id, const char*, Group&);

  private:
	LedState state;
};

}

#endif /* __ardour_mackie_control_protocol_led_h__ */
