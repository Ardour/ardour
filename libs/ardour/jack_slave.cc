/*
    Copyright (C) 2004 Paul Davis 

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

#include <iostream>

#include <errno.h>
#include <jack/jack.h>
#include <jack/transport.h>

#include <ardour/slave.h>
#include <ardour/session.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace sigc;

JACK_Slave::JACK_Slave (jack_client_t* j)
	: jack (j)
{
	float x;
	nframes_t p;
	/* call this to initialize things */
	speed_and_position (x, p);
}

JACK_Slave::~JACK_Slave ()
{
}

bool
JACK_Slave::locked() const
{
	return true;
}

bool
JACK_Slave::ok() const
{
	return true;
}

bool 
JACK_Slave::speed_and_position (float& sp, nframes_t& position) 
{
	jack_position_t pos;
	jack_transport_state_t state;

	state = jack_transport_query (jack, &pos);

	switch (state) {
	case JackTransportStopped:
		speed = 0;
		_starting = false;
		break;
	case JackTransportRolling:
		speed = 1.0;
		_starting = false;
		break;
	case JackTransportLooping:
		speed = 1.0;
		_starting = false;
		break;
	case JackTransportStarting:
		_starting = true;
		// don't adjust speed here, just leave it as it was
		break;
	}

	sp = speed;
	position = pos.frame;
	return true;
}
