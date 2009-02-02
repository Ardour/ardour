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
#include <ardour/event_type_map.h>
#include <ardour/midi_state_tracker.h>

using namespace std;
using namespace ARDOUR;


MidiStateTracker::MidiStateTracker ()
{
	_active_notes.reset();
}

void
MidiStateTracker::track_note_onoffs (const Evoral::MIDIEvent<MidiBuffer::TimeType>& event)
{
	if (event.is_note_on()) {
		_active_notes [event.note() + 128 * event.channel()] = true;
	} else if (event.is_note_off()){
		_active_notes [event.note() + 128 * event.channel()] = false;
	}
}

bool
MidiStateTracker::track (const MidiBuffer::iterator &from, const MidiBuffer::iterator &to)
{
	bool ret = false;

	for (MidiBuffer::iterator i = from; i != to; ++i) {
		const Evoral::MIDIEvent<MidiBuffer::TimeType> ev(*i, false);
		if (ev.event_type() == LoopEventType) {
			ret = true;
			continue;
		}

		track_note_onoffs (ev);
	}
	return ret;
}

void
MidiStateTracker::resolve_notes (MidiBuffer &dst, nframes_t time)
{
	// Dunno if this is actually faster but at least it fills our cacheline.
	if (_active_notes.none ())
		return;

	for (int channel = 0; channel < 16; ++channel) {
		for (int note = 0; note < 128; ++note) {
			if (_active_notes[channel * 128 + note]) {
				uint8_t buffer[3] = { MIDI_CMD_NOTE_OFF | channel, note, 0 };
				Evoral::MIDIEvent<MidiBuffer::TimeType> noteoff
						(time, MIDI_CMD_NOTE_OFF, 3, buffer, false);

				dst.push_back (noteoff);	

				_active_notes [channel * 128 + note] = false;
			}
		}
	}
}

