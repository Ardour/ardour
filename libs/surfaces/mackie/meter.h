/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_mackie_control_protocol_meter_h__
#define __ardour_mackie_control_protocol_meter_h__

#include "controls.h"
#include "midi_byte_array.h"

namespace ArdourSurface {

namespace Mackie {

class SurfacePort;

class Meter : public Control
{
public:
	Meter (int id, std::string name, Group & group)
		: Control  (id, name, group)
		, _enabled (false)
		, overload_on (false) {}


	void send_update (Surface&, float dB);
	bool enabled () const { return _enabled; }

	MidiByteArray zero();
	static std::pair<bool, float> calculate_meter_over_and_deflection (float dB);

	static Control* factory (Surface&, int id, const char*, Group&);

	void notify_metering_state_changed(Surface& surface, bool transport_is_rolling, bool metering_active);

  private:
	bool _enabled;
	bool overload_on;
};

} // Mackie namespace
} // ArdourSurface namespace

#endif /* __ardour_mackie_control_protocol_meter_h__ */
