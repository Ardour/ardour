/*
    Copyright (C) 2007 Paul Davis 
	Written by Dave Robillard, 2007

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

#include <iostream>
#include <ardour/midi_model.h>
#include <ardour/midi_events.h>
#include <ardour/types.h>

using namespace std;
using namespace ARDOUR;


MidiModel::MidiModel(size_t size)
	: _notes(size)
{
}


/** Begin a write of events to the model.
 *
 * As note on and off events are written, complete notes with duration are
 * constructed
 */
void
MidiModel::start_write()
{
	_write_notes.clear();
}



/** Finish a write of events to the model.
 *
 * If \a delete_stuck is true, note on events that were never resolved with
 * a corresonding note off will be deleted.  Otherwise they will remain as
 * notes with duration 0.
 */
void
MidiModel::end_write(bool delete_stuck)
{
	if (delete_stuck) {
		_write_notes.clear();
	} else {
		cerr << "FIXME: Stuck notes lost" << endl;
		_write_notes.clear();
		/* Merge _write_events into _events */
		/*size_t ev_index = 0
		size_t write_index = 0;
		while ( ! _write_events.empty()) {
			// do stuff
		}*/
	}
}


/** Append contents of \a buf to model.  NOT realtime safe.
 *
 * Timestamps of events in \a buf are expected to be relative to
 * the start of this model (t=0) and MUST be monotonically increasing
 * and MUST be >= the latest event currently in the model.
 *
 * Events in buf are deep copied.
 */
void
MidiModel::append(const MidiBuffer& buf)
{
	for (size_t i=0; i < buf.size(); ++i) {
		const MidiEvent& ev = buf[i];
		
		assert(_write_notes.empty() || ev.time >= _write_notes.back().start);

		if (ev.type() == MIDI_CMD_NOTE_ON)
			append_note_on(ev.time, ev.note(), ev.velocity());
		else if (ev.type() == MIDI_CMD_NOTE_OFF)
			append_note_off(ev.time, ev.note());
	}
}


/** Append \a in_event to model.  NOT realtime safe.
 *
 * Timestamps of events in \a buf are expected to be relative to
 * the start of this model (t=0) and MUST be monotonically increasing
 * and MUST be >= the latest event currently in the model.
 */
void
MidiModel::append(double time, size_t size, Byte* buf)
{
	assert(_write_notes.empty() || time >= _write_notes.back().start);

	if ((buf[0] & 0xF0) == MIDI_CMD_NOTE_ON)
		append_note_on(time, buf[1], buf[2]);
	else if ((buf[0] & 0xF0) == MIDI_CMD_NOTE_OFF)
		append_note_off(time, buf[1]);
}


void
MidiModel::append_note_on(double time, uint8_t note, uint8_t velocity)
{
	_write_notes.push_back(Note(time, 0, note, velocity));
}


void
MidiModel::append_note_off(double time, uint8_t note_num)
{
	/* _write_notes (active notes) is presumably small enough for linear
	 * search to be a good idea.  maybe not with instruments (percussion)
	 * that don't send note off at all though.... FIXME? */

	/* FIXME: note off velocity for that one guy out there who actually has
	 * keys that send it */

	for (size_t i=0; i < _write_notes.size(); ++i) {
		Note& note = _write_notes[i];
		if (note.note == note_num) {
			assert(time > note.start);
			note.duration = time - note.start;
			cerr << "MidiModel resolved note, duration: " << note.duration << endl;
			break;
		}
	}
}

