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

#include "ardour/audioengine.h"
#include "ardour/audio_backend.h"
#include "ardour/slave.h"

using namespace std;
using namespace ARDOUR;

Engine_Slave::Engine_Slave (AudioEngine& e)
	: engine (e)
{
	double x;
	samplepos_t p;
	/* call this to initialize things */
	speed_and_position (x, p);
}

Engine_Slave::~Engine_Slave ()
{
}

bool
Engine_Slave::locked() const
{
	return true;
}

bool
Engine_Slave::ok() const
{
	return true;
}

bool
Engine_Slave::speed_and_position (double& sp, samplepos_t& position)
{
	boost::shared_ptr<AudioBackend> backend = engine.current_backend();

	if (backend) {
		_starting = backend->speed_and_position (sp, position);
	} else {
		_starting = false;
	}

	return true;
}
