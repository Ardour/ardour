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

#define __STDC_LIMIT_MACROS 1

#include <iostream>
#include <algorithm>
#include <stdint.h>
#include <pbd/enumwriter.h>
#include <ardour/midi_model.h>
#include <ardour/midi_events.h>
#include <ardour/midi_source.h>
#include <ardour/types.h>
#include <ardour/session.h>

using namespace std;
using namespace ARDOUR;


// Read iterator (const_iterator)

MidiModel::const_iterator::const_iterator(MidiModel& model, double t)
	: _model(model)
{
	model.read_lock();

	_note_iter = model.notes().end();
	for (MidiModel::Notes::iterator i = model.notes().begin(); i != model.notes().end(); ++i) {
		if ((*i).time() >= t) {
			_note_iter = i;
			break;
		}
	}

	MidiControlIterator earliest_control = make_pair(boost::shared_ptr<AutomationList>(), make_pair(DBL_MAX, 0.0));

	_control_iters.reserve(model.controls().size());
	for (Automatable::Controls::const_iterator i = model.controls().begin();
			i != model.controls().end(); ++i) {
		double x, y;
		i->second->list()->rt_safe_earliest_event(t, DBL_MAX, x, y);
		
		const MidiControlIterator new_iter = make_pair(i->second->list(), make_pair(x, y));

		if (x < earliest_control.second.first)
			earliest_control = new_iter;

		_control_iters.push_back(new_iter);
	}

	if (_note_iter != model.notes().end())
		_event = MidiEvent(_note_iter->on_event(), false);

	if (earliest_control.first != 0 && earliest_control.second.first < _event.time())
		model.control_to_midi_event(_event, earliest_control);

}


MidiModel::const_iterator::~const_iterator()
{
	_model.read_unlock();
}


// MidiModel

MidiModel::MidiModel(Session& s, size_t size)
	: Automatable(s, "midi model")
	, _notes(size)
	, _note_mode(Sustained)
	, _writing(false)
	, _edited(false)
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

	/* FIXME: cache last lookup value to avoid O(n) search every time */

	if (_note_mode == Sustained) {

		for (Notes::const_iterator n = _notes.begin(); n != _notes.end(); ++n) {

			while ( ! _active_notes.empty() ) {
				const Note* const earliest_off = _active_notes.top();
				const MidiEvent&  off_ev       = earliest_off->off_event();
				if (off_ev.time() < start + nframes && off_ev.time() <= n->time()) {
					dst.write(off_ev.time() + stamp_offset, off_ev.size(), off_ev.buffer());
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
				const MidiEvent& on_ev = n->on_event();
				dst.write(on_ev.time() + stamp_offset, on_ev.size(), on_ev.buffer());
				_active_notes.push(&(*n));
				++read_events;
			}

		}
			
		// Write any trailing note offs
		while ( ! _active_notes.empty() ) {
			const Note* const earliest_off = _active_notes.top();
			const MidiEvent&  off_ev       = earliest_off->off_event();
			if (off_ev.time() < start + nframes) {
				dst.write(off_ev.time() + stamp_offset, off_ev.size(), off_ev.buffer());
				_active_notes.pop();
				++read_events;
			} else {
				break;
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

	return read_events;
}
	

bool
MidiModel::control_to_midi_event(MidiEvent& ev, const MidiControlIterator& iter)
{
	if (iter.first->parameter().type() == MidiCCAutomation) {
		if (!ev.owns_buffer() || ev.size() < 3)
			ev = MidiEvent(iter.second.first, 3, (Byte*)malloc(3), true);

		assert(iter.first);
		assert(iter.first->parameter().id() <= INT8_MAX);
		assert(iter.second.second <= INT8_MAX);
		ev.buffer()[0] = MIDI_CMD_CONTROL;
		ev.buffer()[1] = (Byte)iter.first->parameter().id();
		ev.buffer()[2] = (Byte)iter.second.second;
		ev.time() = iter.second.first; // x
		ev.size() = 3;
		return true;
	} else {
		return false;
	}
}

	
/** Return the earliest MIDI event in the given range.
 *
 * \return true if \a output has been set to the earliest event in the given range.
 */
#if 0
bool
MidiModel::earliest_note_event(MidiEvent& output, nframes_t start, nframes_t nframes) const
{
	/* FIXME: cache last lookup value to avoid O(n) search every time */
		
	const Note*      const earliest_on  = NULL;
	const Note*      const earliest_off = NULL;
	const MidiEvent* const earliest_cc = NULL;

	/* Notes */

	if (_note_mode == Sustained) {
				
		for (Notes::const_iterator n = _notes.begin(); n != _notes.end(); ++n) {

			if ( ! _active_notes.empty() ) {
				const Note* const earliest_off = _active_notes.top();
				const MidiEvent&  off_ev       = earliest_off->off_event();
				if (off_ev.time() < start + nframes && off_ev.time() <= n->time()) {
					output = off_ev;
					//dst.write(off_ev.time() + stamp_offset, off_ev.size(), off_ev.buffer());
					_active_notes.pop();
					return true;
				}
			}

			if (n->time() >= start + nframes)
				break;

			// Note on
			if (n->time() >= start) {
				earliest_on = &n->on_event();
				//dst.write(on_ev.time() + stamp_offset, on_ev.size(), on_ev.buffer());
				_active_notes.push(&(*n));
				return true;
			}

		}
			
		// Write any trailing note offs
		while ( ! _active_notes.empty() ) {
			const Note* const earliest_off = _active_notes.top();
			const MidiEvent&  off_ev       = earliest_off->off_event();
			if (off_ev.time() < start + nframes) {
				dst.write(off_ev.time() + stamp_offset, off_ev.size(), off_ev.buffer());
				_active_notes.pop();
				++read_events;
			} else {
				break;
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

	return read_events;
}
#endif

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
	//cerr << "MM " << this << " START WRITE, MODE = " << enum_2_string(_note_mode) << endl;
	write_lock();
	_writing = true;
	_write_notes.clear();
	write_unlock();
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
	write_lock();
	assert(_writing);
	
	//cerr << "MM " << this << " END WRITE: " << _notes.size() << " NOTES\n";

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
	write_unlock();
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
	write_lock();

	assert(_writing);

	for (MidiBuffer::const_iterator i = buf.begin(); i != buf.end(); ++i) {
		assert(_notes.empty() || (*i).time() >= _notes.back().time());
		append(*i);
	}
	
	write_unlock();
}


/** Append \a in_event to model.  NOT realtime safe.
 *
 * Timestamps of events in \a buf are expected to be relative to
 * the start of this model (t=0) and MUST be monotonically increasing
 * and MUST be >= the latest event currently in the model.
 */
void
MidiModel::append(const MidiEvent& ev)
{
	write_lock();

	assert(_notes.empty() || ev.time() >= _notes.back().time());
	assert(_writing);

	if (ev.is_note_on())
		append_note_on_unlocked(ev.time(), ev.note(), ev.velocity());
	else if (ev.is_note_off())
		append_note_off_unlocked(ev.time(), ev.note());
	else if (ev.is_cc())
		append_cc_unlocked(ev.time(), ev.cc_number(), ev.cc_value());
	else
		printf("MM Unknown event type %X\n", ev.type());
	
	write_unlock();
}


void
MidiModel::append_note_on_unlocked(double time, uint8_t note_num, uint8_t velocity)
{
	//cerr << "MidiModel " << this << " note " << (int)note_num << " on @ " << time << endl;

	assert(_writing);
	_notes.push_back(Note(time, 0, note_num, velocity));
	if (_note_mode == Sustained) {
		//cerr << "MM Sustained: Appending active note on " << (unsigned)(uint8_t)note_num << endl;
		_write_notes.push_back(_notes.size() - 1);
	} else {
		//cerr << "MM Percussive: NOT appending active note on" << endl;
	}
}


void
MidiModel::append_note_off_unlocked(double time, uint8_t note_num)
{
	//cerr << "MidiModel " << this << " note " << (int)note_num << " off @ " << time << endl;

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
			//cerr << "MM resolved note, duration: " << note.duration() << endl;
			break;
		}
	}
}


void
MidiModel::append_cc_unlocked(double time, uint8_t number, uint8_t value)
{
	Parameter param(MidiCCAutomation, number);

	//cerr << "MidiModel " << this << " add CC " << (int)number << " = " << (int)value
	//	<< " @ " << time << endl;
	
	boost::shared_ptr<AutomationControl> control = Automatable::control(param, true);
	control->list()->fast_simple_add(time, (double)value);
}


void
MidiModel::add_note_unlocked(const Note& note)
{
	//cerr << "MidiModel " << this << " add note " << (int)note.note() << " @ " << note.time() << endl;
	Notes::iterator i = upper_bound(_notes.begin(), _notes.end(), note, note_time_comparator);
	_notes.insert(i, note);
}


void
MidiModel::remove_note_unlocked(const Note& note)
{
	//cerr << "MidiModel " << this << " remove note " << (int)note.note() << " @ " << note.time() << endl;
	Notes::iterator n = find(_notes.begin(), _notes.end(), note);
	if (n != _notes.end())
		_notes.erase(n);
}

/** Slow!  for debugging only. */
#ifndef NDEBUG
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
#endif

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
	_edited = true;
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
	
	_model.write_lock();
	
	for (std::list<Note>::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i)
		_model.add_note_unlocked(*i);
	
	for (std::list<Note>::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i)
		_model.remove_note_unlocked(*i);
	
	_model.write_unlock();
	
	_model.ContentsChanged(); /* EMIT SIGNAL */
}


void
MidiModel::DeltaCommand::undo()
{
	// This could be made much faster by using a priority_queue for added and
	// removed notes (or sort here), and doing a single iteration over _model
	
	_model.write_lock();

	for (std::list<Note>::iterator i = _added_notes.begin(); i != _added_notes.end(); ++i)
		_model.remove_note_unlocked(*i);
	
	for (std::list<Note>::iterator i = _removed_notes.begin(); i != _removed_notes.end(); ++i)
		_model.add_note_unlocked(*i);
	
	_model.write_unlock();
	
	_model.ContentsChanged(); /* EMIT SIGNAL */
}


bool
MidiModel::write_to(boost::shared_ptr<MidiSource> source)
{
	//cerr << "Writing model to " << source->name() << endl;

	/* This could be done using a temporary MidiRingBuffer and using
	 * MidiModel::read and MidiSource::write, but this is more efficient
	 * and doesn't require any buffer size assumptions (ie it's worth
	 * the code duplication).
	 *
	 * This is also different from read in that note off events are written
	 * regardless of the track mode.  This is so the user can switch a
	 * recorded track (with note durations from some instrument) to percussive,
	 * save, reload, then switch it back to sustained preserving the original
	 * note durations.
	 */

	/* Percussive 
	for (Notes::const_iterator n = _notes.begin(); n != _notes.end(); ++n) {
		const MidiEvent& ev = n->on_event();
		source->append_event_unlocked(ev);
	}*/

	read_lock();

	LaterNoteEndComparator cmp;
	ActiveNotes active_notes(cmp);
		
	// Foreach note
	for (Notes::const_iterator n = _notes.begin(); n != _notes.end(); ++n) {

		// Write any pending note offs earlier than this note on
		while ( ! active_notes.empty() ) {
			const Note* const earliest_off = active_notes.top();
			const MidiEvent&  off_ev       = earliest_off->off_event();
			if (off_ev.time() <= n->time()) {
				source->append_event_unlocked(off_ev);
				active_notes.pop();
			} else {
				break;
			}
		}

		// Write this note on
		source->append_event_unlocked(n->on_event());
		if (n->duration() > 0)
			active_notes.push(&(*n));
	}
		
	// Write any trailing note offs
	while ( ! active_notes.empty() ) {
		source->append_event_unlocked(active_notes.top()->off_event());
		active_notes.pop();
	}

	_edited = false;
	
	read_unlock();

	return true;
}

XMLNode&
MidiModel::get_state()
{
	XMLNode *node = new XMLNode("MidiModel");
	return *node;
}

