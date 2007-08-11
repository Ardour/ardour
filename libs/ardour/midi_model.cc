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
#include <algorithm>
#include <pbd/enumwriter.h>
#include <ardour/midi_model.h>
#include <ardour/midi_events.h>
#include <ardour/types.h>
#include <ardour/session.h>

using namespace std;
using namespace ARDOUR;

// Note

MidiModel::Note::Note(double t, double d, uint8_t n, uint8_t v)
	: _on_event(t, 3, NULL, true)
	, _off_event(t + d, 3, NULL, true)
{
	_on_event.buffer()[0] = MIDI_CMD_NOTE_ON;
	_on_event.buffer()[1] = n;
	_on_event.buffer()[2] = v;
	
	_off_event.buffer()[0] = MIDI_CMD_NOTE_OFF;
	_off_event.buffer()[1] = n;
	_off_event.buffer()[2] = 0x40;
	
	assert(time() == t);
	assert(duration() == d);
	assert(note() == n);
	assert(velocity() == v);
}


MidiModel::Note::Note(const Note& copy)
	: _on_event(copy._on_event, true)
	, _off_event(copy._off_event, true)
{
	/*
	assert(copy._on_event.size == 3);
	_on_event.buffer = _on_event_buffer;
	memcpy(_on_event_buffer, copy._on_event_buffer, 3);
	
	assert(copy._off_event.size == 3);
	_off_event.buffer = _off_event_buffer;
	memcpy(_off_event_buffer, copy._off_event_buffer, 3);
	*/

	assert(time() == copy.time());
	assert(end_time() == copy.end_time());
	assert(note() == copy.note());
	assert(velocity() == copy.velocity());
	assert(duration() == copy.duration());
}


const MidiModel::Note&
MidiModel::Note::operator=(const Note& copy)
{
	_on_event = copy._on_event;
	_off_event = copy._off_event;
	/*_on_event.time = copy._on_event.time;
	assert(copy._on_event.size == 3);
	memcpy(_on_event_buffer, copy._on_event_buffer, 3);
	
	_off_event.time = copy._off_event.time;
	assert(copy._off_event.size == 3);
	memcpy(_off_event_buffer, copy._off_event_buffer, 3);
	*/
	
	assert(time() == copy.time());
	assert(end_time() == copy.end_time());
	assert(note() == copy.note());
	assert(velocity() == copy.velocity());
	assert(duration() == copy.duration());

	return *this;
}

// MidiModel

MidiModel::MidiModel(Session& s, size_t size)
	: _session(s)
	, _notes(size)
	, _note_mode(Sustained)
	, _writing(false)
	, _active_notes(LaterNoteEndComparator())
{
}


/** Read events in frame range \a start .. \a start+cnt into \a dst,
 * adding \a stamp_offset to each event's timestamp.
 * \return number of events written to \a dst
 */
size_t
MidiModel::read (MidiRingBuffer& dst, nframes_t start, nframes_t nframes, nframes_t stamp_offset) const
{
	size_t read_events = 0;

	//cerr << "MM READ @ " << start << " + " << nframes << endl;

	/* FIXME: cache last lookup value to avoid the search */

	if (_note_mode == Sustained) {

		/* FIXME: cache last lookup value to avoid the search */
		for (Notes::const_iterator n = _notes.begin(); n != _notes.end(); ++n) {

			//cerr << "MM ON " << n->time() << endl;

			while ( ! _active_notes.empty() ) {
				const Note* const earliest_off = _active_notes.top();
				const MidiEvent& ev = earliest_off->off_event();
				if (ev.time() < start + nframes && ev.time() <= n->time()) {
					dst.write(ev.time() + stamp_offset, ev.size(), ev.buffer());
					_active_notes.pop();
					++read_events;
				} else {
					break;
				}
			}

			if (n->time() >= start + nframes)
				break;

			// Note on
			if (n->time() >= start) {
				const MidiEvent& ev = n->on_event();
				dst.write(ev.time() + stamp_offset, ev.size(), ev.buffer());
				_active_notes.push(&(*n));
				++read_events;
			}

		}

	// Percussive
	} else {
		for (Notes::const_iterator n = _notes.begin(); n != _notes.end(); ++n) {
			// Note on
			if (n->time() >= start) {
				if (n->time() < start + nframes) {
					const MidiEvent& ev = n->on_event();
					dst.write(ev.time() + stamp_offset, ev.size(), ev.buffer());
					++read_events;
				} else {
					break;
				}
			}
		}
	}

	//if (read_events > 0)
	//	cerr << "MM READ " << read_events << " EVENTS" << endl;

	return read_events;
}


/** Begin a write of events to the model.
 *
 * If \a mode is Sustained, complete notes with duration are constructed as note
 * on/off events are received.  Otherwise (Percussive), only note on events are
 * stored; note off events are discarded entirely and all contained notes will
 * have duration 0.
 */
void
MidiModel::start_write()
{
	//cerr << "MM START WRITE, MODE = " << enum_2_string(_note_mode) << endl;
	_write_notes.clear();
	_writing = true;
}



/** Finish a write of events to the model.
 *
 * If \a delete_stuck is true and the current mode is Sustained, note on events
 * that were never resolved with a corresonding note off will be deleted.
 * Otherwise they will remain as notes with duration 0.
 */
void
MidiModel::end_write(bool delete_stuck)
{
	assert(_writing);
	
	//cerr << "MM END WRITE\n";

	if (_note_mode == Sustained && delete_stuck) {
		for (Notes::iterator n = _notes.begin(); n != _notes.end() ; ) {
			if (n->duration() == 0) {
				cerr << "WARNING: Stuck note lost: " << n->note() << endl;
				n = _notes.erase(n);
			} else {
				++n;
			}
		}
	}

	_write_notes.clear();
	_writing = false;
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
	assert(_writing);

	for (MidiBuffer::const_iterator i = buf.begin(); i != buf.end(); ++i) {
		const MidiEvent& ev = *i;
		
		assert(_notes.empty() || ev.time() >= _notes.back().time());

		if (ev.type() == MIDI_CMD_NOTE_ON)
			append_note_on(ev.time(), ev.note(), ev.velocity());
		else if (ev.type() == MIDI_CMD_NOTE_OFF)
			append_note_off(ev.time(), ev.note());
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
	assert(_notes.empty() || time >= _notes.back().time());
	assert(_writing);

	if ((buf[0] & 0xF0) == MIDI_CMD_NOTE_ON)
		append_note_on(time, buf[1], buf[2]);
	else if ((buf[0] & 0xF0) == MIDI_CMD_NOTE_OFF)
		append_note_off(time, buf[1]);
}


void
MidiModel::append_note_on(double time, uint8_t note_num, uint8_t velocity)
{
	assert(_writing);
	_notes.push_back(Note(time, 0, note_num, velocity));
	if (_note_mode == Sustained) {
		//cerr << "MM Appending note on " << (unsigned)(uint8_t)note_num << endl;
		_write_notes.push_back(_notes.size() - 1);
	} else {
		//cerr << "MM NOT appending note on" << endl;
	}
}


void
MidiModel::append_note_off(double time, uint8_t note_num)
{
	assert(_writing);
	if (_note_mode == Percussive) {
		//cerr << "MM Ignoring note off (percussive mode)" << endl;
		return;
	} else {
		//cerr << "MM Attempting to resolve note off " << (unsigned)(uint8_t)note_num << endl;
	}

	/* FIXME: make _write_notes fixed size (127 noted) for speed */
	
	/* FIXME: note off velocity for that one guy out there who actually has
	 * keys that send it */

	for (WriteNotes::iterator n = _write_notes.begin(); n != _write_notes.end(); ++n) {
		Note& note = _notes[*n];
		//cerr << (unsigned)(uint8_t)note.note() << " ? " << (unsigned)note_num << endl;
		if (note.note() == note_num) {
			assert(time > note.time());
			note.set_duration(time - note.time());
			_write_notes.erase(n);
			//cerr << "MidiModel resolved note, duration: " << note.duration() << endl;
			break;
		}
	}
}


void
MidiModel::add_note(const Note& note)
{
	Notes::iterator i = upper_bound(_notes.begin(), _notes.end(), note, note_time_comparator);
	_notes.insert(i, note);
}


void
MidiModel::remove_note(const Note& note)
{
	Notes::iterator n = find(_notes.begin(), _notes.end(), note);
	if (n != _notes.end())
		_notes.erase(n);
}

/** Slow!  for debugging only. */
bool
MidiModel::is_sorted() const
{
	bool t = 0;
	for (Notes::const_iterator n = _notes.begin(); n != _notes.end(); ++n)
		if (n->time() < t)
			return false;
		else
			t = n->time();

	return true;
}


/** Start a new command.
 *
 * This has no side-effects on the model or Session, the returned command
 * can be held on to for as long as the caller wishes, or discarded without
 * formality, until apply_command is called and ownership is taken.
 */
MidiModel::DeltaCommand*
MidiModel::new_delta_command(const string name)
{
	DeltaCommand* cmd =  new DeltaCommand(*this, name);
	return cmd;
}


/** Apply a command.
 *
 * Ownership of cmd is taken, it must not be deleted by the caller.
 * The command will constitute one item on the undo stack.
 */
void
MidiModel::apply_command(Command* cmd)
{
	_session.begin_reversible_command(cmd->name());
	(*cmd)();
	assert(is_sorted());
	_session.commit_reversible_command(cmd);
}


// MidiEditCommand


void
MidiModel::DeltaCommand::add(const Note& note)
{
	//cerr << "MEC: apply" << endl;

	_removed_notes.remove(note);
	_added_notes.push_back(note);
}


void
MidiModel::DeltaCommand::remove(const Note& note)
{
	//cerr << "MEC: remove" << endl;

	_added_notes.remove(note);
	_removed_notes.push_back(note);
}

		
void 
MidiModel::DeltaCommand::operator()()
{
	// This could be made much faster by using a priority_queue for added and
	// removed notes (or sort here), and doing a single iteration over _model
	
	for (std::list<Note>::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i)
		_model.add_note(*i);
	
	for (std::list<Note>::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i)
		_model.remove_note(*i);
	
	_model.ContentsChanged(); /* EMIT SIGNAL */
}


void
MidiModel::DeltaCommand::undo()
{
	// This could be made much faster by using a priority_queue for added and
	// removed notes (or sort here), and doing a single iteration over _model

	for (std::list<Note>::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i)
		_model.remove_note(*i);
	
	for (std::list<Note>::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i)
		_model.add_note(*i);
	
	_model.ContentsChanged(); /* EMIT SIGNAL */
}


bool
MidiModel::write_new_source(const std::string& path)
{
	cerr << "Writing model to " << path << endl;

#if 0
		SourceFactory::createWritable (region->data_type(), session, path, false, session.frame_rate());

		catch (failed_constructor& err) {
			error << string_compose (_("filter: error creating new file %1 (%2)"), path, strerror (errno)) << endmsg;
			return -1;
		}
	}
#endif

	return true;
}

