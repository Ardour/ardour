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
#include <ardour/audioengine.h>
#include <ardour/jack_audio_port.h>

using namespace ARDOUR;

JackAudioPort::JackAudioPort (const std::string& name, Flags flgs, AudioBuffer* buf)
	: Port (name, flgs)
	, JackPort (name, DataType::AUDIO, flgs)
	, BaseAudioPort (name, flgs)
	, _has_been_mixed_down( false )
{
	if (buf) {

		_buffer = buf;
		_own_buffer = false;

	} else {
		
		/* data space will be provided by JACK */

		_buffer = new AudioBuffer (0);
		_own_buffer = true;
	}
}

int
JackAudioPort::reestablish ()
{
	int ret = JackPort::reestablish ();
	
	if (ret == 0 && _flags & IsOutput) {
		_buffer->clear ();
	}

	return ret;
}

AudioBuffer&
JackAudioPort::get_audio_buffer (nframes_t nframes, nframes_t offset) {
	assert (_buffer);

	if (_has_been_mixed_down)
		return *_buffer;

	if( _flags & IsInput )
		_buffer->set_data ((Sample*) jack_port_get_buffer (_port, nframes), nframes+offset);


	if (nframes)
		_has_been_mixed_down = true;

	return *_buffer;
}

void
JackAudioPort::cycle_start (nframes_t nframes, nframes_t offset) {
	if( _flags & IsOutput )
		_buffer->set_data ((Sample*) jack_port_get_buffer (_port, nframes), nframes+offset);
}
void
JackAudioPort::cycle_end (nframes_t nframes, nframes_t offset) {
	_has_been_mixed_down=false;
}
