/*
    Copyright (C) 2002-2006 Paul Davis 

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

#include <pbd/error.h>

#include <ardour/jack_port.h>
#include <ardour/audioengine.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

AudioEngine* JackPort::engine = 0;

JackPort::JackPort (const std::string& name, DataType type, Flags flgs) 
	: Port (flgs), _port (0)
{
	_port = jack_port_register (engine->jack(), name.c_str(), type.to_jack_type(), flgs, 0);

	if (_port == 0) {
		throw failed_constructor();
	}
	
	_flags = flgs;
	_type  = type;
	_name = jack_port_name (_port);
}

JackPort::~JackPort ()
{
	cerr << "deleting jack port " << _name << endl;

	jack_port_unregister (engine->jack(), _port);
}

int 
JackPort::set_name (string str)
{
	int ret;

	if ((ret = jack_port_set_name (_port, str.c_str())) == 0) {
		_name = str;
	}
	
	return ret;
}

int
JackPort::disconnect ()
{
	return jack_port_disconnect (engine->jack(), _port);
}	

void
JackPort::set_engine (AudioEngine* e)
{
	engine = e;
}

nframes_t
JackPort::total_latency () const
{
	return jack_port_get_total_latency (engine->jack(), _port);
}

int
JackPort::reestablish ()
{
	string short_name;
	
	short_name = _name.substr (_name.find_last_of (':') + 1);

	_port = jack_port_register (engine->jack(), short_name.c_str(), type().to_jack_type(), _flags, 0);

	if (_port == 0) {
		error << string_compose (_("could not reregister %1"), _name) << endmsg;
		return -1;
	}

	reset ();
	

	return 0;
}

void
JackPort::recompute_total_latency () const
{
#ifdef HAVE_JACK_RECOMPUTE_LATENCY
	jack_recompute_total_latency (engine->jack(), _port);
#endif
}


