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
#include <iostream>

#include "ardour/midi_port.h"
#include "ardour/data_type.h"

using namespace ARDOUR;
using namespace std;

MidiPort::MidiPort (const std::string& name, Flags flags)
	: Port (name, DataType::MIDI, flags)
	, _has_been_mixed_down (false)
{
	_buffer = new MidiBuffer (raw_buffer_size(0));
}

MidiPort::~MidiPort()
{
	delete _buffer;
}

void
MidiPort::cycle_start (nframes_t nframes)
{
	_buffer->clear ();
	assert (_buffer->size () == 0);
	
	if (sends_output ()) {
		jack_midi_clear_buffer (jack_port_get_buffer (_jack_port, nframes));
	}
}

MidiBuffer &
MidiPort::get_midi_buffer (nframes_t nframes, nframes_t offset)
{
	if (_has_been_mixed_down) {
		return *_buffer;
	}

	if (receives_input ()) {

		void* jack_buffer = jack_port_get_buffer (_jack_port, nframes);
		const nframes_t event_count = jack_midi_get_event_count(jack_buffer);
		
		assert (event_count < _buffer->capacity());

		/* suck all relevant MIDI events from the JACK MIDI port buffer
		   into our MidiBuffer
		*/
		
		nframes_t off = offset + _port_offset;

		for (nframes_t i = 0; i < event_count; ++i) {
			
			jack_midi_event_t ev;

			jack_midi_event_get (&ev, jack_buffer, i);
			
			if (ev.time > off && ev.time < off+nframes) {
				_buffer->push_back (ev);
			}
		}
		
		if (nframes) {
			_has_been_mixed_down = true;
		}
		
	} else {
		_buffer->silence (nframes);
	}
	
	if (nframes) {
		_has_been_mixed_down = true;
	}

	return *_buffer;
}

	
void
MidiPort::cycle_end (nframes_t /*nframes*/)
{
	_has_been_mixed_down = false;
}

void
MidiPort::cycle_split ()
{
	_has_been_mixed_down = false;
}

void
MidiPort::flush_buffers (nframes_t nframes, nframes_t offset)
{
	if (sends_output ()) {
		
		void* jack_buffer = jack_port_get_buffer (_jack_port, nframes);

		for (MidiBuffer::iterator i = _buffer->begin(); i != _buffer->end(); ++i) {
			const Evoral::Event<nframes_t>& ev = *i;

			// event times are in frames, relative to cycle start

			// XXX split cycle start or cycle start?

			assert(ev.time() < (nframes+offset+_port_offset));

			if (ev.time() >= offset + _port_offset) {
				jack_midi_event_write (jack_buffer, (jack_nframes_t) ev.time(), ev.buffer(), ev.size());
			}
		}
	}
}

size_t
MidiPort::raw_buffer_size (nframes_t nframes) const
{
	return jack_midi_max_event_size(jack_port_get_buffer(_jack_port, nframes));
}

