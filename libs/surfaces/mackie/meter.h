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
		, overload_on (false) {}
	
	void send_update (Surface&, float dB);

	MidiByteArray zero();

	static Control* factory (Surface&, int id, const char*, Group&);
  
	void update_transport_rolling(Surface& surface);

  private:
	bool overload_on;
	bool _transport_is_rolling;
};

}

#endif /* __ardour_mackie_control_protocol_meter_h__ */
