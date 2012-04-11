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

#ifndef __ardour_mackie_control_protocol_meter_h__
#define __ardour_mackie_control_protocol_meter_h__

#include "controls.h"
#include "midi_byte_array.h"

namespace Mackie {

class SurfacePort;

class Meter : public Control
{
public:
	Meter (int id, std::string name, Group & group)
		: Control  (id, name, group)
		, last_segment_value_sent (-1)
		, overload_on (false) {}
	
	MidiByteArray update_message (float dB);

	MidiByteArray zero() { return update_message (-99999999.0); }

	static Control* factory (Surface&, int id, const char*, Group&);
	
	int last_segment_value_sent;

  private:
	bool overload_on;
};

}

#endif /* __ardour_mackie_control_protocol_meter_h__ */
