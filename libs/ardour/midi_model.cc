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
#include <ardour/session.h>

using namespace std;
using namespace ARDOUR;


MidiModel::MidiModel(Session& s, size_t size)
	: _session(s)
	, _notes(size)
	, _command(NULL)
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
		for (Notes::iterator n = _notes.begin(); n != _notes.end() ; ) {
			if (n->duration == 0) {
				cerr << "WARNING: Stuck note lost: " << n->note << endl;
				n = _notes.erase(n);
			} else {
				++n;
			}
		}
	}

	_write_notes.clear();
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
		
		assert(_notes.empty() || ev.time >= _notes.back().start);

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
MidiModel::append(double time, size_t size, const Byte* buf)
{
	assert(_notes.empty() || time >= _notes.back().start);

	if ((buf[0] & 0xF0) == MIDI_CMD_NOTE_ON)
		append_note_on(time, buf[1], buf[2]);
	else if ((buf[0] & 0xF0) == MIDI_CMD_NOTE_OFF)
		append_note_off(time, buf[1]);
}


void
MidiModel::append_note_on(double time, uint8_t note_num, uint8_t velocity)
{
	_notes.push_back(Note(time, 0, note_num, velocity));
	_write_notes.push_back(_notes.size() - 1);
}


void
MidiModel::append_note_off(double time, uint8_t note_num)
{
	/* _write_notes (active notes) is presumably small enough for linear
	 * search to be a good idea.  maybe not with instruments (percussion)
	 * that don't send note off at all though.... FIXME? */

	/* FIXME: note off velocity for that one guy out there who actually has
	 * keys that send it */

	for (WriteNotes::iterator n = _write_notes.begin(); n != _write_notes.end(); ++n) {
		Note& note = _notes[*n];
		if (note.note == note_num) {
			assert(time > note.start);
			note.duration = time - note.start;
			_write_notes.erase(n);
			//cerr << "MidiModel resolved note, duration: " << note.duration << endl;
			break;
		}
	}
}


void
MidiModel::add_note(const Note& note)
{
	Notes::iterator i = upper_bound(_notes.begin(), _notes.end(), note, note_time_comparator);
	_notes.insert(i, note);
	if (_command)
		_command->add_note(note);
}


void
MidiModel::remove_note(const Note& note)
{
	Notes::iterator n = find(_notes.begin(), _notes.end(), note);
	if (n != _notes.end())
		_notes.erase(n);
	
	if (_command)
		_command->remove_note(note);
}



void
MidiModel::begin_command()
{
	assert(!_command);
	_session.begin_reversible_command("midi edit");
	_command = new MidiEditCommand(*this);
}


void
MidiModel::finish_command()
{
	_session.commit_reversible_command(_command);
	_command = NULL;
}


// MidiEditCommand


void
MidiModel::MidiEditCommand::add_note(const Note& note)
{
	//cerr << "MEC: apply" << endl;

	_removed_notes.remove(note);
	_added_notes.push_back(note);
}


void
MidiModel::MidiEditCommand::remove_note(const Note& note)
{
	//cerr << "MEC: remove" << endl;

	_added_notes.remove(note);
	_removed_notes.push_back(note);
}

		
void 
MidiModel::MidiEditCommand::operator()()
{
	//cerr << "MEC: apply" << endl;
	assert(!_model.current_command());
	
	for (std::list<Note>::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i)
		_model.add_note(*i);
	
	for (std::list<Note>::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i)
		_model.remove_note(*i);

	_model.ContentsChanged(); /* EMIT SIGNAL */
}


void
MidiModel::MidiEditCommand::undo()
{
	//cerr << "MEC: undo" << endl;
	assert(!_model.current_command());

	for (std::list<Note>::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i)
		_model.remove_note(*i);
	
	for (std::list<Note>::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i)
		_model.add_note(*i);
	
	_model.ContentsChanged(); /* EMIT SIGNAL */
}

