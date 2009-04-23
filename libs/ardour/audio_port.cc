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
	, _buffer (new AudioBuffer (0))
{
	assert (name.find_first_of (':') == string::npos);
}

AudioPort::~AudioPort ()
{
	delete _buffer;
}

void
AudioPort::cycle_start (nframes_t nframes)
{
	/* caller must hold process lock */

	/* get_buffer() must only be run on outputs here in cycle_start().

	   Inputs must be done in the correct processing order, which 
	   requires interleaving with route processing. that will 
	   happen when Port::get_buffer() is called.
	*/

	if (sends_output()) {

		/* Notice that cycle_start() is always run with the *entire* process cycle frame count,
		   so we do not bother to apply _port_offset here - we always want the address of the
		   entire JACK port buffer. We are not collecting data here - just noting the
		   address where we will write data later in the process cycle.
		*/

		_buffer->set_data ((Sample *) jack_port_get_buffer (_jack_port, nframes), nframes);
		_buffer->prepare ();
	}
}

void
AudioPort::cycle_end (nframes_t nframes)
{
	if (sends_output() && !_buffer->written()) {
		_buffer->silence (nframes);
	}
}

void
AudioPort::cycle_split ()
{
}

AudioBuffer&
AudioPort::get_audio_buffer (nframes_t nframes, nframes_t offset)
{
	/* caller must hold process lock */

	if (receives_input ()) {

		/* Get a pointer to the audio data @ offset + _port_offset within the JACK port buffer and store
		   it in our _buffer member.

		   Note that offset is expected to be zero in almost all cases.
		*/

		_buffer->set_data ((Sample *) jack_port_get_buffer (_jack_port, nframes) + offset + _port_offset, nframes);
	} 
	
	/* output ports set their _buffer data information during ::cycle_start()
	 */

	return *_buffer;
}

