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
#include <ardour/midi_port.h>
#include <ardour/data_type.h>
#include <iostream>

using namespace ARDOUR;
using namespace std;

MidiPort::MidiPort(jack_port_t* p)
	: Port(p)
	, _buffer(4096) // FIXME FIXME FIXME Jack needs to tell us this
	, _nframes_this_cycle(0)
{
	DataType dt(_type);
	assert(dt == DataType::MIDI);

	reset();


}


MidiPort::~MidiPort()
{
}

void
MidiPort::cycle_start (jack_nframes_t nframes)
{
	_buffer.clear();
	assert(_buffer.size() == 0);

	_nframes_this_cycle = nframes;

	if (_flags & JackPortIsOutput) {
		_buffer.silence(nframes);
		assert(_buffer.size() == 0);
		return;
	}

	// We're an input - copy Jack events to internal buffer
	
	void* jack_buffer = jack_port_get_buffer(_port, nframes);

	const jack_nframes_t event_count
		= jack_midi_port_get_info(jack_buffer, nframes)->event_count;

	assert(event_count < _buffer.capacity());

	MidiEvent ev;

	// FIXME: too slow, event struct is copied twice (here and MidiBuffer::push_back)
	for (jack_nframes_t i=0; i < event_count; ++i) {

		// This will fail to compile if we change MidiEvent to our own class
		jack_midi_event_get(static_cast<jack_midi_event_t*>(&ev), jack_buffer, i, nframes);

		_buffer.push_back(ev);
		// Convert note ons with velocity 0 to proper note offs
		// FIXME: Jack MIDI should guarantee this - does it?
		//if (ev->buffer[0] == MIDI_CMD_NOTE_ON && ev->buffer[2] == 0)
		//	ev->buffer[0] = MIDI_CMD_NOTE_OFF;
	}

	assert(_buffer.size() == event_count);

	//if (_buffer.size() > 0)
	//	cerr << "MIDIPort got " << event_count << " events." << endl;
}

void
MidiPort::cycle_end()
{
	if (_flags & JackPortIsInput) {
		_nframes_this_cycle = 0;
		return;
	}

	// We're an output - copy events from internal buffer to Jack buffer
	
	void* jack_buffer = jack_port_get_buffer(_port, _nframes_this_cycle);

	const jack_nframes_t event_count = _buffer.size();
	
	//if (event_count > 0)
	//	cerr << "MIDIPort writing " << event_count << " events." << endl;

	jack_midi_clear_buffer(jack_buffer, _nframes_this_cycle);
	for (jack_nframes_t i=0; i < event_count; ++i) {
		const jack_midi_event_t& ev = _buffer[i];
		assert(ev.time < _nframes_this_cycle);
		jack_midi_event_write(jack_buffer, ev.time, ev.buffer, ev.size, _nframes_this_cycle);
	}
	
	_nframes_this_cycle = 0;
}
