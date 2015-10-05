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

/* The routines in here should know absolutely nothing about how io is actually done */

#include <tranzport_control_protocol.h>

int
TranzportControlProtocol::flush ()
{
	int pending = 0;

// Always write the lights first
	if(!(pending = lights_flush())) {
		pending = screen_flush();
	}

#if DEBUG_TRANZPORT_BITS > 9
	int s;
	if(s = (screen_invalid.count())) { //  + lights_invalid.count())) {
		printf("VALID  : %s %s\n",
		       screen_invalid.to_string().c_str(),
		       lights_invalid.to_string().c_str());
		printf("CURR   : %s %s\n",
		       screen_invalid.to_string().c_str(),
		       lights_current.to_string().c_str());
		printf("PENDING  : %s %s\n",
		       screen_invalid.to_string().c_str(),
		       lights_pending.to_string().c_str());
#if DEBUG_TRANZPORT_BITS > 10
		printf("invalid bits: %d\n",s);
#endif
	}
#endif
	return pending;
}


int
TranzportControlProtocol::lights_flush ()
{
	std::bitset<LIGHTS> light_state;
	light_state = lights_pending ^ lights_current;
	if ( (light_state.none() || lights_invalid.none()))
	{
		return (0);
	}

#if DEBUG_TRANZPORT_LIGHTS
	printf("LPEND   : %s\n", lights_pending.to_string().c_str());
	printf("LCURR   : %s\n", lights_current.to_string().c_str());
#endif

	// if ever we thread reads/writes STATUS_OK will have to move into the loop
	int i;

	if ( _device_status == STATUS_OK || _device_status == STATUS_ONLINE) {
		for (i = 0; i<LIGHTS; i++) {
			if(light_state[i]) {
				if(light_set ((LightID)i,lights_pending[i])) {
#if DEBUG_TRANZPORT_LIGHTS > 2
					printf("Did %d light writes\n",i);
#endif
					return light_state.count();
				} else {
					light_state[i] = 0;
				}

			}
		}
	}
	light_state = lights_pending ^ lights_current;
#if DEBUG_TRANZPORT_LIGHTS > 2
	printf("Did %d light writes, left: %d\n",i, light_state.count());
#endif

	return light_state.count();
}
