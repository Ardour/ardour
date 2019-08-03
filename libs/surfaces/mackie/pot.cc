/*
 * Copyright (C) 2006-2007 John Anderson
 * Copyright (C) 2012-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pot.h"
#include "surface.h"
#include "control_group.h"

using namespace ArdourSurface;
using namespace Mackie;

int const Pot::External = 0x2e; /* specific ID for "vpot" representing external control */
int const Pot::ID       = 0x10; /* base value for v-pot IDs */

Control*
Pot::factory (Surface& surface, int id, const char* name, Group& group)
{
	Pot* p = new Pot (id, name, group);
	surface.pots[id] = p;
	p->is_qcon = surface.get_qcon_flag();
	surface.controls.push_back (p);
	group.add (*p);
	return p;
}

MidiByteArray
Pot::set (float val, bool onoff, Mode mode)
{
	// TODO do an exact calc for 0.50? To allow manually re-centering the port.

	MIDI::byte msg;

	// center on if val is "very close" to 0.50
	if( !is_qcon ) {
		// center the position and shift bits for standard mackie surface
		msg =  (val > 0.48 && val < 0.58 ? 1 : 0) << 6; 
	} else {	//center the position and don't shift anything for qcon - TODO: on center position lit the center LED on the ring
		if(val > 0.48 && val < 0.58) {
			val = 0.50;		
		}

		// set msg
		msg = val;
	}	


	// Pot/LED mode
	if( !is_qcon ) {
		// Mackie mode - Supports all ring modes
		msg |=  (mode << 4);
	} else {	
		// Qcon rotary mode - Only "DOT" mode? - TODO: Investigate how to proper set vpot rings to different modes on qcon
		msg |=  (0 << 4);
	}

	/*
	 * Even though a width value may be negative, there is
	 * technically still width there, it is just reversed,
	 * so make sure to show it on the LED ring appropriately.
	 */
	if (val < 0){
	  val = val * -1;
	}

	// val, but only if off hasn't explicitly been set
	if (onoff) {
		if (mode == spread) {
			msg |=  (lrintf (val * 6)) & 0x0f; // 0b00001111
		} else {
			msg |=  (lrintf (val * 10.0) + 1) & 0x0f; // 0b00001111
		}
	}

	/* outbound LED message requires 0x20 to be added to the LED's id
	 */

	return MidiByteArray (3, 0xb0, 0x20 + id(), msg);

}

