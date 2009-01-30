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
	, _internal_buffer (false)
{
	assert (name.find_first_of (':') == string::npos);

	if (external ()) {
		
		/* external ports use the external port buffer */
		_buffer = new AudioBuffer (0);

	} else {

		/* internal ports need their own buffers */
		_buffer = new AudioBuffer (capacity);
	}

	check_buffer_status ();
	
}

AudioPort::~AudioPort()
{
	delete _buffer;
}

void
AudioPort::cycle_start (nframes_t nframes, nframes_t offset)
{
	/* caller must hold process lock */

	/* For external (JACK) ports, get_buffer() must only be run 
	   on outputs here in cycle_start().

	   Inputs must be done in the correct processing order, which 
	   requires interleaving with route processing. that will 
	   happen when Port::get_buffer() is called.
	*/
	
	if (!receives_input() && external ()) {
		_buffer->set_data ((Sample *) jack_port_get_buffer (_jack_port, nframes) + offset, nframes);
	}

	if (receives_input()) {
		_has_been_mixed_down = false;
	} else {
		_buffer->silence (nframes, offset);
	}
}

AudioBuffer &
AudioPort::get_audio_buffer (nframes_t nframes, nframes_t offset)
{
	/* caller must hold process lock */

	if (receives_input () && !_has_been_mixed_down) {

		/* external ports use JACK's memory unless otherwise noted */
		
		if (external()) {
			if (!using_internal_data()) {
				_buffer->set_data ((Sample *) jack_port_get_buffer (_jack_port, nframes) + offset, nframes);
			} else {
				_buffer->silence (nframes, offset);
			}
		}

		mixdown (nframes, offset, !external ());
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
	/* note: this is only called for input ports */

	if (_connections.empty()) {

		/* no internal mixing to do, so for internal ports
		   just make sure the buffer is silent.
		*/
		
		if (!external()) {
			_buffer->silence (cnt, offset);
		} 

	} else {

		set<Port*>::const_iterator p = _connections.begin();

		/* mix in internally-connected ports. if this is an external port
		   then it may already have data present from JACK. in that case, we 
		   do not want to overwrite that data, so we skip the initial ::read_from()
		   call and do everything with accumulate_from()
		 */

		if (!external()) {
			_buffer->read_from (dynamic_cast<AudioPort*>(*p)->get_audio_buffer (cnt, offset), cnt, offset);
			++p;
			
		}

		for (; p != _connections.end (); ++p) {
			_buffer->accumulate_from (dynamic_cast<AudioPort*>(*p)->get_audio_buffer (cnt, offset), cnt, offset);

		}
	}

	/* XXX horrible heuristic designed to check that we worked the whole buffer.
	   Needs fixing but its a hard problem.
	*/

	if (cnt && offset == 0) {
		_has_been_mixed_down = true;
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

bool
AudioPort::using_internal_data () const
{
	return _internal_buffer;
}

void
AudioPort::use_internal_data ()
{
	_buffer->replace_data (_buffer->capacity());
	_internal_buffer = true;
}

void
AudioPort::use_external_data ()
{
	_internal_buffer = false;
	_buffer->drop_data ();
}
