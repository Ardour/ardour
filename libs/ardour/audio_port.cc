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
#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/data_type.h"
#include "ardour/audio_buffer.h"

using namespace ARDOUR;
using namespace std;

AudioPort::AudioPort (const std::string& name, Flags flags)
	: Port (name, DataType::AUDIO, flags)
	, _buffer_data_set (false)
	, _buffer (new AudioBuffer (0))
{
	assert (name.find_first_of (':') == string::npos);
}

AudioPort::~AudioPort ()
{
	delete _buffer;
}

void
AudioPort::cycle_start (nframes_t nframes, nframes_t offset)
{
	/* caller must hold process lock */

	/* get_buffer() must only be run on outputs here in cycle_start().

	   Inputs must be done in the correct processing order, which 
	   requires interleaving with route processing. that will 
	   happen when Port::get_buffer() is called.
	*/

	if (sends_output() && !_buffer_data_set) {
		
		_buffer->set_data ((Sample *) jack_port_get_buffer (_jack_port, nframes) + offset, nframes);
		_buffer_data_set = true;
		
	}

	if (receives_input()) {
		_buffer_data_set = false;
	} else {
		_buffer->silence (nframes, offset);
	}
}

AudioBuffer &
AudioPort::get_audio_buffer (nframes_t nframes, nframes_t offset)
{
	/* caller must hold process lock */

	if (receives_input () && !_buffer_data_set) {

		_buffer->set_data ((Sample *) jack_port_get_buffer (_jack_port, nframes) + offset, nframes);
		
	} 
	
	return *_buffer;
}

void
AudioPort::cycle_end (nframes_t nframes, nframes_t offset)
{
       _buffer_data_set = false;
}
