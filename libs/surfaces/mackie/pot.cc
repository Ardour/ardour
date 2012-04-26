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

#include "pot.h"
#include "surface.h"
#include "control_group.h"

using namespace Mackie;

int const Pot::External = 0x2e; /* specific ID for "vpot" representing external control */
int const Pot::ID       = 0x10; /* base value for v-pot IDs */

Control*
Pot::factory (Surface& surface, int id, const char* name, Group& group)
{
	Pot* p = new Pot (id, name, group);
	surface.pots[id] = p;
	surface.controls.push_back (p);
	group.add (*p);
	return p;
}

MidiByteArray
Pot::set (float val, bool onoff, Mode mode)
{
	// TODO do an exact calc for 0.50? To allow manually re-centering the port.

	// center on if val is "very close" to 0.50
	MIDI::byte msg =  (val > 0.48 && val < 0.58 ? 1 : 0) << 6;
	
	// Pot/LED mode
	msg |=  (mode << 4);
	
	// val, but only if off hasn't explicitly been set
	if (onoff) {
		if (mode == spread) {
			msg |=  (lrintf (val * 6) + 1) & 0x0f; // 0b00001111
		} else {
			msg |=  (lrintf (val * 10.0) + 1) & 0x0f; // 0b00001111
		}
	}

	/* outbound LED message requires 0x20 to be added to the LED's id
	 */

	return MidiByteArray (3, 0xb0, 0x20 + id(), msg);

}
	
