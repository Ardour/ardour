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
{
	if (buf) {

		cout << name << " BUFFER" << endl;

		_buffer = buf;
		_own_buffer = false;

	} else {

		cout << name << " NO BUFFER" << endl;

		/* data space will be provided by JACK */
		_buffer = new MidiBuffer (0);
		_own_buffer = true;
	}
}

void
JackMidiPort::cycle_start (nframes_t nframes, nframes_t offset_ignored_but_probably_should_not_be)
{
	_buffer->clear();
	assert(_buffer->size() == 0);

	if (_flags & IsOutput) {
		// no buffer, nothing to do
		return;
	}

	// We're an input - copy Jack events to internal buffer
	
	void* jack_buffer = jack_port_get_buffer(_port, nframes);
	const nframes_t event_count = jack_midi_get_event_count(jack_buffer);

	assert(event_count < _buffer->capacity());

	jack_midi_event_t ev;

	for (nframes_t i=0; i < event_count; ++i) {

		jack_midi_event_get (&ev, jack_buffer, i);

		_buffer->push_back (ev);
	}

	assert(_buffer->size() == event_count);

	/*if (_buffer->size() > 0)
		cerr << "JackMIDIPort got " << event_count << " events (buf " << _buffer << ")" << endl;*/
}

void
JackMidiPort::cycle_end (nframes_t nframes, nframes_t offset_ignored_but_probably_should_not_be)
{
	if (_flags & IsInput) {
		return;
	}

	// We're an output - copy events from source buffer to Jack buffer
	
	void* jack_buffer = jack_port_get_buffer (_port, nframes);

	jack_midi_clear_buffer (jack_buffer);

	for (MidiBuffer::iterator i = _buffer->begin(); i != _buffer->end(); ++i) {
		const MidiEvent& ev = *i;
		// event times should be frames, relative to cycle start
		assert(ev.time() >= 0);
		assert(ev.time() < nframes);
		jack_midi_event_write (jack_buffer, (jack_nframes_t) ev.time(), ev.buffer(), ev.size());
	}
}
