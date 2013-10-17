/*
    Copyright (C) 2013 Paul Davis

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

#include "jack_connection.h"
#include "jack_audiobackend.h"

using namespace ARDOUR;

static boost::shared_ptr<JACKAudioBackend> backend;
static boost::shared_ptr<JackConnection> jack_connection;

static boost::shared_ptr<AudioBackend>
backend_factory (AudioEngine& ae)
{
	if (!jack_connection) {
		return boost::shared_ptr<AudioBackend>();
	}

	if (!backend) {
		backend.reset (new JACKAudioBackend (ae, jack_connection));
	}

	return backend;
}

static int
instantiate (const std::string& arg1, const std::string& arg2)
{
	try {
		jack_connection.reset (new JackConnection (arg1, arg2));
	} catch (...) {
		return -1;
	}

	return 0;
}

static int 
deinstantiate ()
{
	backend.reset ();
	jack_connection.reset ();

	return 0;
}

static bool
already_configured ()
{
	return !JackConnection::in_control ();
}

static ARDOUR::AudioBackendInfo _descriptor = {
	"JACK",
	instantiate,
	deinstantiate,
	backend_factory,
	already_configured,
};

extern "C" ARDOURBACKEND_API ARDOUR::AudioBackendInfo* descriptor() { return &_descriptor; }

