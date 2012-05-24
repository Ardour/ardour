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
#include <cerrno>

#include <jack/jack.h>
#include <jack/transport.h>

#include "ardour/slave.h"

using namespace std;
using namespace ARDOUR;

JACK_Slave::JACK_Slave (jack_client_t* j)
	: jack (j)
{
	double x;
	framepos_t p;
	/* call this to initialize things */
	speed_and_position (x, p);
}

JACK_Slave::~JACK_Slave ()
{
}

void
JACK_Slave::reset_client (jack_client_t* j)
{
	jack = j;
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
JACK_Slave::speed_and_position (double& sp, framepos_t& position)
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
	default:
		cerr << "WARNING: Unknown JACK transport state: " << state << endl;
	}

	sp = speed;
	position = pos.frame;
	return true;
}
