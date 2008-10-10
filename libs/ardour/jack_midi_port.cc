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
#include <ardour/jack_midi_port.h>

using namespace ARDOUR;
JackMidiPort::JackMidiPort (const std::string& name, Flags flgs, MidiBuffer* buf)
	: Port (name, flgs)
	, JackPort (name, DataType::MIDI, flgs)
	, BaseMidiPort (name, flgs) 
	, _has_been_mixed_down (false)
{
	// MIDI ports always need a buffer since jack buffer format is different
	assert(buf);

	_buffer = buf;
	_own_buffer = false;
}

void
JackMidiPort::cycle_start (nframes_t nframes, nframes_t offset)
{
	/* FIXME: offset */

	_buffer->clear();
	assert(_buffer->size() == 0);

	if (_flags & IsInput) {
		return;
	}

	// We're an output - delete the midi_events.
	
	void* jack_buffer = jack_port_get_buffer (_port, nframes);

	jack_midi_clear_buffer (jack_buffer);
}

MidiBuffer &
JackMidiPort::get_midi_buffer( nframes_t nframes, nframes_t offset ) {

	if (_has_been_mixed_down)
	    return *_buffer;

	if (_flags & IsOutput) {
		return *_buffer;
	}

	// We're an input - copy Jack events to internal buffer
	
	void* jack_buffer = jack_port_get_buffer(_port, nframes);
	const nframes_t event_count = jack_midi_get_event_count(jack_buffer);

	assert(event_count < _buffer->capacity());

	jack_midi_event_t ev;

	for (nframes_t i=0; i < event_count; ++i) {

		jack_midi_event_get (&ev, jack_buffer, i);

		// i guess this should do but i leave it off to test the rest first.
		//if (ev.time > offset && ev.time < offset+nframes)
			_buffer->push_back (ev);
	}

	assert(_buffer->size() == event_count);

	/*if (_buffer->size() > 0)
		cerr << "JackMIDIPort got " << event_count << " events (buf " << _buffer << ")" << endl;*/
	if (nframes)
		_has_been_mixed_down = true;

	return *_buffer;
}

void
JackMidiPort::cycle_end (nframes_t nframes, nframes_t offset)
{
	/* FIXME: offset */

	_has_been_mixed_down = false;

#if 0
	if (_flags & IsInput) {
		return;
	}

	// We're an output - copy events from source buffer to Jack buffer
	
	void* jack_buffer = jack_port_get_buffer (_port, nframes);

	jack_midi_clear_buffer (jack_buffer);

	for (MidiBuffer::iterator i = _buffer->begin(); i != _buffer->end(); ++i) {
		const Evoral::Event& ev = *i;

		// event times should be frames, relative to cycle start
		assert(ev.time() >= 0);
		assert(ev.time() < nframes);
		jack_midi_event_write (jack_buffer, (jack_nframes_t) ev.time(), ev.buffer(), ev.size());
	}
#endif
}

void
JackMidiPort::flush_buffers (nframes_t nframes, nframes_t offset)
{
	/* FIXME: offset */

	if (_flags & IsInput) {
		return;
	}

	void* jack_buffer = jack_port_get_buffer (_port, nframes);

	for (MidiBuffer::iterator i = _buffer->begin(); i != _buffer->end(); ++i) {
		const Evoral::Event& ev = *i;
		// event times should be frames, relative to cycle start
		assert(ev.time() >= 0);
		assert(ev.time() < (nframes+offset));
		if (ev.time() >= offset)
			jack_midi_event_write (jack_buffer, (jack_nframes_t) ev.time(), ev.buffer(), ev.size());
	}
}
