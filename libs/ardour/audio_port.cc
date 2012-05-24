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

#include "pbd/stacktrace.h"

#include "ardour/audio_buffer.h"
#include "ardour/audio_port.h"
#include "ardour/data_type.h"

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
AudioPort::cycle_start (pframes_t nframes)
{
	/* caller must hold process lock */

        Port::cycle_start (nframes);

	if (sends_output()) {
		_buffer->prepare ();
	}
}

void
AudioPort::cycle_end (pframes_t)
{
        if (sends_output() && !_buffer->written()) {
                /* we can't use nframes here because the current buffer capacity may
                   be shorter than the full buffer size if we split the cycle.
                */
		if (_buffer->capacity () > 0) {
			_buffer->silence (_buffer->capacity());
		}
	}
}

void
AudioPort::cycle_split ()
{
}

AudioBuffer&
AudioPort::get_audio_buffer (pframes_t nframes)
{
	/* caller must hold process lock */
       _buffer->set_data ((Sample *) jack_port_get_buffer (_jack_port, _cycle_nframes) +
                          _global_port_buffer_offset + _port_buffer_offset, nframes);
	return *_buffer;
}



