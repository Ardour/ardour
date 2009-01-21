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
#include <ardour/audioengine.h>
#include <ardour/data_type.h>
#include <ardour/audio_buffer.h>

using namespace ARDOUR;
using namespace std;

AudioPort::AudioPort (const std::string& name, Flags flags, bool ext, nframes_t capacity)
	: Port (name, DataType::AUDIO, flags, ext)
	, _has_been_mixed_down (false)
	, _buffer (0)
{
	assert (name.find_first_of (':') == string::npos);
	
	if (external ()) {
		
		/* external ports use the external port buffer */
		_buffer = new AudioBuffer (0);

	} else {

		/* internal ports need their own buffers */
		_buffer = new AudioBuffer (capacity);
		
	}
	
}

AudioPort::~AudioPort()
{
	delete _buffer;
}

void
AudioPort::cycle_start (nframes_t nframes, nframes_t offset)
{
	/* caller must hold process lock */

	_has_been_mixed_down = false;

	if (external ()) {
		/* external ports use JACK's memory */
		_buffer->set_data ((Sample *) jack_port_get_buffer (_jack_port, nframes), nframes + offset);
	}
}

AudioBuffer &
AudioPort::get_audio_buffer (nframes_t nframes, nframes_t offset)
{
	/* caller must hold process lock */

	if (_has_been_mixed_down) {
		return *_buffer;
	}

	if (receives_input ()) {

		/* INPUT */

		/* If we're external (), we have some data in our buffer set up by JACK;
		   otherwise, we have an undefined buffer.  In either case we mix down
		   our non-JACK inputs; either accumulating into the JACK data or
		   overwriting the undefined data */
		   
		mixdown (nframes, offset, !external ());
		
	} else {

		/* OUTPUT */

		if (!external ()) {
			/* start internal output buffers with silence */
			_buffer->silence (nframes, offset);
		}
		
	}
	
	if (nframes) {
		_has_been_mixed_down = true;
	}

	return *_buffer;
}

void
AudioPort::cycle_end (nframes_t nframes, nframes_t offset)
{
	_has_been_mixed_down = false;
}

void
AudioPort::mixdown (nframes_t cnt, nframes_t offset, bool first_overwrite)
{
	if (_connections.empty()) {
		if (first_overwrite) {
			_buffer->silence (cnt, offset);
		}
		return;
	}
	
	set<Port*>::const_iterator p = _connections.begin();

	if (first_overwrite) {
		_buffer->read_from (dynamic_cast<AudioPort*>(*p)->get_audio_buffer (cnt, offset), cnt, offset);
		++p;
	}

	for (; p != _connections.end (); ++p) {
		_buffer->accumulate_from (dynamic_cast<AudioPort*>(*p)->get_audio_buffer (cnt, offset), cnt, offset);
	}
}

void
AudioPort::reset ()
{
	Port::reset ();
	
	if (_buffer->capacity () != 0) {
		_buffer->resize (_engine->frames_per_cycle ());
		_buffer->clear ();
	}
}
