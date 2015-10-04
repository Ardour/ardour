/*
 *   Copyright (C) 2006 Paul Davis
 *   Copyright (C) 2007 Michael Taht
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 *
 *   */

#include <tranzport_control_protocol.h>

// Lights are buffered, and arguably these functions should be eliminated or inlined

void
TranzportControlProtocol::lights_on ()
{
	lights_pending.set();
}

void
TranzportControlProtocol::lights_off ()
{
	lights_pending.reset();
}

int
TranzportControlProtocol::light_on (LightID light)
{
	lights_pending.set(light);
	return 0;
}

int
TranzportControlProtocol::light_off (LightID light)
{
	lights_pending.reset(light);
	return 0;
}

void TranzportControlProtocol::lights_init()
{
	lights_invalid.set();
	lights_flash = lights_pending = lights_current.reset();
}


// Now that all this is bitsets, I don't see much
// need for these 4 to remain in the API

void TranzportControlProtocol::light_validate (LightID light)
{
	lights_invalid.reset(light);
}

void TranzportControlProtocol::light_invalidate (LightID light)
{
	lights_invalid.set(light);
}

void TranzportControlProtocol::lights_validate ()
{
	lights_invalid.reset();
}

void TranzportControlProtocol::lights_invalidate ()
{
	lights_invalid.set();
}

int
TranzportControlProtocol::light_set (LightID light, bool offon)
{
	uint8_t cmd[8];
	cmd[0] = 0x00;  cmd[1] = 0x00;  cmd[2] = light;  cmd[3] = offon;
	cmd[4] = 0x00;  cmd[5] = 0x00;  cmd[6] = 0x00;  cmd[7] = 0x00;

	if (write (cmd) == 0) {
		lights_current[light] = offon;
		lights_invalid.reset(light);
		return 0;
	} else {
		return 1;
	}
}
