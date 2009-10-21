/*
    Copyright (C) 2006-2008 Paul Davis
    Author: Torben Hohn

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <iostream>
#include "ardour/event_type_map.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_state_tracker.h"

using namespace std;
using namespace ARDOUR;


MidiStateTracker::MidiStateTracker ()
{
	reset ();
}

void
MidiStateTracker::reset ()
{
	memset (_active_notes, 0, sizeof (_active_notes));
}

void
MidiStateTracker::track_note_onoffs (const Evoral::MIDIEvent<MidiBuffer::TimeType>& event)
{
	if (event.is_note_on()) {
		_active_notes [event.note() + 128 * event.channel()]++;
	} else if (event.is_note_off()){
		if (_active_notes[event.note() + 128 * event.channel()]) {
			_active_notes [event.note() + 128 * event.channel()]--;
		}
	}
}

void
MidiStateTracker::add (uint8_t note, uint8_t chn)
{
	++_active_notes[note + 128 * chn];
}

void
MidiStateTracker::remove (uint8_t note, uint8_t chn)
{
	if (_active_notes[note + 128 * chn]) {
		--_active_notes[note + 128 * chn];
	}
}

void
MidiStateTracker::track (const MidiBuffer::iterator &from, const MidiBuffer::iterator &to, bool& looped)
{
	looped = false;

	for (MidiBuffer::iterator i = from; i != to; ++i) {
		const Evoral::MIDIEvent<MidiBuffer::TimeType> ev(*i, false);
		if (ev.event_type() == LoopEventType) {
			looped = true;
			continue;
		}

		track_note_onoffs (ev);
	}
}

void
MidiStateTracker::resolve_notes (MidiBuffer &dst, nframes64_t time)
{
	for (int channel = 0; channel < 16; ++channel) {
		for (int note = 0; note < 128; ++note) {
			while (_active_notes[channel * 128 + note]) {
				uint8_t buffer[3] = { MIDI_CMD_NOTE_OFF | channel, note, 0 };
				Evoral::MIDIEvent<MidiBuffer::TimeType> noteoff
					(time, MIDI_CMD_NOTE_OFF, 3, buffer, false);
				dst.push_back (noteoff);

				_active_notes[channel * 128 + note]--;
			}
		}
	}
}

void
MidiStateTracker::resolve_notes (MidiRingBuffer<nframes_t> &dst, nframes64_t time)
{
	uint8_t buf[3];
	for (int channel = 0; channel < 16; ++channel) {
		for (int note = 0; note < 128; ++note) {
			while (_active_notes[channel * 128 + note]) {
				buf[0] = MIDI_CMD_NOTE_OFF|channel;
				buf[1] = note;
				buf[2] = 0;
				dst.write (time, EventTypeMap::instance().midi_event_type (buf[0]), 3, buf);
				_active_notes[channel * 128 + note]--;
			}
		}
	}
}

void
MidiStateTracker::dump (ostream& o)
{
	o << "******\n";
	for (int c = 0; c < 16; ++c) {
		for (int x = 0; x < 128; ++x) {
			if (_active_notes[c * 128 + x]) {
				o << "Channel " << c+1 << " Note " << x << " is on ("
				  << (int) _active_notes[c*128+x] <<  "times)\n";
			}
		}
	}
	o << "+++++\n";
}
