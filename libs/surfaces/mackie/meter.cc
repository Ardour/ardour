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

#include <cmath>

#include "pbd/compose.h"
#include "ardour/debug.h"

#include "meter.h"
#include "surface.h"
#include "surface_port.h"
#include "control_group.h"

using namespace Mackie;
using namespace PBD;

Control*
Meter::factory (Surface& surface, int id, int ordinal, const char* name, Group& group)
{
	Meter* m = new Meter (id, ordinal, name, group);
	surface.meters[id] = m;
	surface.controls.push_back (m);
	group.add (*m);
	return m;
}

MidiByteArray
Meter::update_message (float dB)
{
	float def = 0.0f; /* Meter deflection %age */

	DEBUG_TRACE (DEBUG::MackieControl, string_compose ("Meter ID %1 dB %2\n", raw_id(), dB));
	
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

	MidiByteArray msg;
	
	if (def > 100.0f) {
		if (!overload_on) {
			overload_on = true;
			msg << MidiByteArray (2, 0xd0, (raw_id() << 4) | 0xe);
		}
	} else {
		if (overload_on) {
			overload_on = false;
			msg << MidiByteArray (2, 0xd0, (raw_id() << 4) | 0xf);
		}
	}
	
	/* we can use up to 13 segments */

	int segment = lrintf ((def/115.0) * 13.0);
	
	if (last_segment_value_sent != segment) {
		last_segment_value_sent = segment;
		msg << MidiByteArray (2, 0xD0, (raw_id()<<4) | segment);
	}

	return msg;
}
