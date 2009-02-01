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

#include <ardour/midi_port.h>
#include <ardour/data_type.h>

using namespace ARDOUR;
using namespace std;

MidiPort::MidiPort (const std::string& name, Flags flags, bool ext, nframes_t capacity)
        : Port (name, DataType::MIDI, flags, ext)
	, _has_been_mixed_down (false)
{
	// FIXME: size kludge (see BufferSet::ensure_buffers)
	// Jack needs to tell us this
	_buffer = new MidiBuffer (capacity * 32);
}

MidiPort::~MidiPort()
{
	delete _buffer;
}


void
MidiPort::cycle_start (nframes_t nframes, nframes_t offset)
{
	if (external ()) {
		_buffer->clear ();
		assert (_buffer->size () == 0);

		if (sends_output ()) {
			jack_midi_clear_buffer (jack_port_get_buffer (_jack_port, nframes));
		}
	}
}

MidiBuffer &
MidiPort::get_midi_buffer (nframes_t nframes, nframes_t offset)
{
	if (_has_been_mixed_down) {
	    return *_buffer;
	}

	if (receives_input ()) {
			
		if (external ()) {
		
			void* jack_buffer = jack_port_get_buffer (_jack_port, nframes);
			const nframes_t event_count = jack_midi_get_event_count(jack_buffer);

			assert (event_count < _buffer->capacity());

			jack_midi_event_t ev;

			for (nframes_t i = 0; i < event_count; ++i) {

				jack_midi_event_get (&ev, jack_buffer, i);

				// i guess this should do but i leave it off to test the rest first.
				//if (ev.time > offset && ev.time < offset+nframes)
				_buffer->push_back (ev);
			}

			if (nframes) {
				_has_been_mixed_down = true;
			}

			if (!_connections.empty()) {
				mixdown (nframes, offset, false);
			}

		} else {
		
			if (_connections.empty()) {
				_buffer->silence (nframes, offset);
			} else {
				mixdown (nframes, offset, true);
			}
		}

	} else {
		_buffer->silence (nframes, offset);
	}
	
	if (nframes) {
		_has_been_mixed_down = true;
	}

	return *_buffer;
}

	
void
MidiPort::cycle_end (nframes_t nframes, nframes_t offset)
{
#if 0

	if (external () && sends_output ()) {
		/* FIXME: offset */

		// We're an output - copy events from source buffer to Jack buffer
		
		void* jack_buffer = jack_port_get_buffer (_jack_port, nframes);
		
		jack_midi_clear_buffer (jack_buffer);
		
		for (MidiBuffer::iterator i = _buffer->begin(); i != _buffer->end(); ++i) {
			const Evoral::Event& ev = *i;

			// event times should be frames, relative to cycle start
			assert(ev.time() >= 0);
			assert(ev.time() < nframes);
			jack_midi_event_write (jack_buffer, (jack_nframes_t) ev.time(), ev.buffer(), ev.size());
		}
	}
#endif

	_has_been_mixed_down = false;
}

void
MidiPort::flush_buffers (nframes_t nframes, nframes_t offset)
{
	/* FIXME: offset */
	
	if (external () && sends_output ()) {
		
		void* jack_buffer = jack_port_get_buffer (_jack_port, nframes);

		for (MidiBuffer::iterator i = _buffer->begin(); i != _buffer->end(); ++i) {
			const Evoral::Event& ev = *i;
			// event times should be frames, relative to cycle start
			assert(ev.time() >= 0);
			assert(ev.time() < (nframes+offset));
			if (ev.time() >= offset) {
				jack_midi_event_write (jack_buffer, (jack_nframes_t) ev.time(), ev.buffer(), ev.size());
			}
		}
	}
}

void
MidiPort::mixdown (nframes_t cnt, nframes_t offset, bool first_overwrite)
{
	set<Port*>::const_iterator p = _connections.begin();

	if (first_overwrite) {
		_buffer->read_from ((dynamic_cast<MidiPort*>(*p))->get_midi_buffer (cnt, offset), cnt, offset);
		++p;
	}

	// XXX DAVE: this is just a guess

	for (; p != _connections.end(); ++p) {
		_buffer->merge (*_buffer, (dynamic_cast<MidiPort*>(*p))->get_midi_buffer (cnt, offset));
	}
}

