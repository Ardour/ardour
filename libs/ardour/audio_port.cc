/*
    Copyright (C) 2006 Paul Davis 

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

#include <cassert>
#include <ardour/audio_port.h>
#include <ardour/data_type.h>

using namespace ARDOUR;
using namespace std;

jack_nframes_t AudioPort::_short_over_length = 2;
jack_nframes_t AudioPort::_long_over_length = 10;

AudioPort::AudioPort(jack_port_t* p)
	: Port(p)
	, _buffer(0)
{
	DataType dt(_type);
	assert(dt == DataType::AUDIO);
	
	reset();
}

void
AudioPort::reset()
{
	Port::reset();
	if (_flags & JackPortIsOutput) {
		_buffer.clear();
		_silent = true;
	}
	
	_metering = 0;
	reset_meters ();
}

void
AudioPort::cycle_start (jack_nframes_t nframes)
{
	if (_flags & JackPortIsOutput) {
		// FIXME: do nothing, we can cache the value (but capacity needs to be set)
		_buffer.set_data((Sample*)jack_port_get_buffer (_port, nframes), nframes);
	} else {
		_buffer.set_data((Sample*)jack_port_get_buffer (_port, nframes), nframes);
	}
}

void
AudioPort::cycle_end()
{
	// whatever...
}
