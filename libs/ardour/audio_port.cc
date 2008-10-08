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
#include <ardour/jack_audio_port.h>
#include <ardour/audioengine.h>
#include <ardour/data_type.h>

using namespace ARDOUR;
using namespace std;

AudioPort::AudioPort (const std::string& name, Flags flags, bool external, nframes_t capacity)
	: Port (name, flags)
	, BaseAudioPort (name, flags)
	, PortFacade (name, flags)
	, _has_been_mixed_down( false )
{
	if (!external || receives_input()) {

		/* internal-only and input ports need their own buffers.
		   external output ports use the external port buffer.
		*/

		_buffer = new AudioBuffer (capacity);
		_own_buffer = true;
	}

	if (!external) {

		_ext_port = 0;
		set_name (name);

	} else {
		
		/* make the JackAudioPort create its own buffer. For input,
		   we will copy from it during cycle_start(). For output,
		   we will set up our buffer to point to its buffer, which
		   will in turn be using the JACK port buffer for data.
		*/

		_ext_port = new JackAudioPort (name, flags, 0);

		//if (sends_output()) {
		//	_buffer = &dynamic_cast<JackAudioPort*>(_ext_port)->get_audio_buffer( nframes, offset );
		//} 

		Port::set_name (_ext_port->name());
	}

	reset ();
}

AudioPort::~AudioPort()
{
	if (_ext_port) {
		delete _ext_port;
		_ext_port = 0;
	}
}

void
AudioPort::reset()
{
	BaseAudioPort::reset();

	if (_ext_port) {
		_ext_port->reset ();
	}
}


void
AudioPort::cycle_start (nframes_t nframes, nframes_t offset)
{
	/* caller must hold process lock */

	if (_ext_port) {
		_ext_port->cycle_start (nframes, offset);
	}
	_has_been_mixed_down = false;
}

AudioBuffer &
AudioPort::get_audio_buffer( nframes_t nframes, nframes_t offset ) {

	if (_has_been_mixed_down)	
		return *_buffer;

	if (_flags & IsInput) {

		if (_ext_port) {
			_buffer->read_from (dynamic_cast<BaseAudioPort*>(_ext_port)->get_audio_buffer (nframes, offset), nframes, offset);

			if (!_connections.empty()) {
				(*_mixdown) (_connections, _buffer, nframes, offset, false);
			}

		} else {
		
			if (_connections.empty()) {
				_buffer->silence (nframes, offset);
			} else {
				(*_mixdown) (_connections, _buffer, nframes, offset, true);
			}
		}

	} else {
		
		// XXX if we could get the output stage to not purely mix into, but also
		// to initially overwrite the buffer, we could avoid this silence step.
		if (_ext_port) {
			_buffer = & (dynamic_cast<BaseAudioPort*>(_ext_port)->get_audio_buffer( nframes, offset ));
		}
		if (nframes)
			_buffer->silence (nframes, offset);
	}
	if (nframes)
		_has_been_mixed_down = true;

	return *_buffer;
}

void
AudioPort::cycle_end (nframes_t nframes, nframes_t offset)
{
	_has_been_mixed_down=false;
}
