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

#include <cmath>

#include "pbd/compose.h"
#include "ardour/debug.h"

#include "meter.h"
#include "surface.h"
#include "surface_port.h"
#include "control_group.h"
#include "mackie_control_protocol.h"

using namespace PBD;
using namespace ArdourSurface;
using namespace Mackie;

Control*
Meter::factory (Surface& surface, int id, const char* name, Group& group)
{
	Meter* m = new Meter (id, name, group);
	surface.meters[id] = m;
	surface.controls.push_back (m);
	group.add (*m);
	return m;
}

void
Meter::notify_metering_state_changed(Surface& surface, bool transport_is_rolling, bool metering_active)
{
	MidiByteArray msg;

	// sysex header
	msg << surface.sysex_hdr();

	// code for Channel Meter Enable Message
	msg << 0x20;

	// Channel identification
	msg << id();

	// Enable (0x07) / Disable (0x00) level meter on LCD, peak hold display on horizontal meter and signal LED
	_enabled = ((surface.mcp().device_info().has_separate_meters() || transport_is_rolling) && metering_active);
	msg << (_enabled ? 0x07 : 0x00);

	// sysex trailer
	msg << MIDI::eox;

	surface.write (msg);
}

void
Meter::send_update (Surface& surface, float dB)
{
	std::pair<bool,float> result = calculate_meter_over_and_deflection(dB);

	MidiByteArray msg;

	if (result.first) {
		if (!overload_on) {
			overload_on = true;
			surface.write (MidiByteArray (2, 0xd0, (id() << 4) | 0xe));

		}
	} else {
		if (overload_on) {
			overload_on = false;
			surface.write (MidiByteArray (2, 0xd0, (id() << 4) | 0xf));
		}
	}

	/* we can use up to 13 segments */

	int segment = lrintf ((result.second/115.0) * 13.0);

	surface.write (MidiByteArray (2, 0xd0, (id()<<4) | segment));
}

MidiByteArray
Meter::zero ()
{
	return MidiByteArray (2, 0xD0, (id()<<4 | 0));
}

std::pair<bool, float>
Meter::calculate_meter_over_and_deflection (float dB) 
{
	float def = 0.0f; /* Meter deflection %age */
	bool over = false;

	if (dB < -70.0f) {
		def = 0.0f;
	} else if (dB < -60.0f) {
		def = (dB + 70.0f) * 0.25f;
	} else if (dB < -50.0f) {
		def = (dB + 60.0f) * 0.5f + 2.5f;
	} else if (dB < -40.0f) {
		def = (dB + 50.0f) * 0.75f + 7.5f;
	} else if (dB < -30.0f) {
		def = (dB + 40.0f) * 1.5f + 15.0f;
	} else if (dB < -20.0f) {
		def = (dB + 30.0f) * 2.0f + 30.0f;
	} else if (dB < 6.0f) {
		def = (dB + 20.0f) * 2.5f + 50.0f;
	} else {
		def = 115.0f;
	}

	/* 115 is the deflection %age that would be
	   when dB=6.0. this is an arbitrary
	   endpoint for our scaling.
	*/

	if (def > 100.0f) {
			over = true;
	}
	return std::make_pair (over, def);
}
