/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * Copyright (C) 2000-2008 Paul Davis
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#define __STDC_LIMIT_MACROS 1

#include <iostream>
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <stdint.h>
#include <evoral/Sequence.hpp>
#include <evoral/ControlList.hpp>
#include <evoral/Control.hpp>
#include <evoral/ControlSet.hpp>
#include <evoral/EventSink.hpp>
#include <evoral/MIDIParameters.hpp>

using namespace std;

namespace Evoral {

void Sequence::write_lock() {
	_lock.writer_lock();
	_control_lock.lock();
}

void Sequence::write_unlock() {
	_lock.writer_unlock();
	_control_lock.unlock();
}

void Sequence::read_lock() const {
	_lock.reader_lock();
}

void Sequence::read_unlock() const {
	_lock.reader_unlock();
}

// Read iterator (const_iterator)

Sequence::const_iterator::const_iterator(const Sequence& seq, double t)
	: _seq(&seq)
	, _is_end( (t == DBL_MAX) || seq.empty() )
	, _locked( !_is_end )
{
	//cerr << "Created MIDI iterator @ " << t << " (is end: " << _is_end << ")" << endl;

	if (_is_end) {
		return;
	}

	seq.read_lock();

	_note_iter = seq.notes().end();
	// find first note which begins after t
	for (Sequence::Notes::const_iterator i = seq.notes().begin(); i != seq.notes().end(); ++i) {
		if ((*i)->time() >= t) {
			_note_iter = i;
			break;
		}
	}

	ControlIterator earliest_control(boost::shared_ptr<ControlList>(), DBL_MAX, 0.0);

	_control_iters.reserve(seq.controls().size());
	
	// find the earliest control event available
	for (Controls::const_iterator i = seq.controls().begin(); i != seq.controls().end(); ++i) {
		double x, y;
		bool ret = i->second->list()->rt_safe_earliest_event_unlocked(t, DBL_MAX, x, y);
		if (!ret) {
			//cerr << "MIDI Iterator: CC " << i->first.id() << " (size " << i->second->list()->size()
			//	<< ") has no events past " << t << endl;
			continue;
		}

		assert(x >= 0);

		if (y < i->first.min() || y > i->first.max()) {
			cerr << "ERROR: Controller (" << i->first.type() << ") value '" << y
				<< "' out of range [" << i->first.min() << "," << i->first.max()
				<< "], event ignored" << endl;
			continue;
		}

		const ControlIterator new_iter(i->second->list(), x, y);

		//cerr << "MIDI Iterator: CC " << i->first.id() << " added (" << x << ", " << y << ")" << endl;
		_control_iters.push_back(new_iter);

		// if the x of the current control is less than earliest_control
		// we have a new earliest_control
		if (x < earliest_control.x) {
			earliest_control = new_iter;
			_control_iter = _control_iters.end();
			--_control_iter;
			// now _control_iter points to the last Element in _control_iters
		}
	}

	if (_note_iter != seq.notes().end()) {
		_event = boost::shared_ptr<Event>(new Event((*_note_iter)->on_event(), true));
	}

	double time = DBL_MAX;
	// in case we have no notes in the region, we still want to get controller messages
	if (_event.get()) {
		time = _event->time();
		// if the note is going to make it this turn, advance _note_iter
		if (earliest_control.x > time) {
			_active_notes.push(*_note_iter);
			++_note_iter;
		}
	}
	
	// <=, because we probably would want to send control events first 
	if (earliest_control.list.get() && earliest_control.x <= time) {
		seq.control_to_midi_event(_event, earliest_control);
	} else {
		_control_iter = _control_iters.end();
	}

	if ( (! _event.get()) || _event->size() == 0) {
		//cerr << "Created MIDI iterator @ " << t << " is at end." << endl;
		_is_end = true;

		// eliminate possible race condition here (ugly)
		static Glib::Mutex mutex;
		Glib::Mutex::Lock lock(mutex);
		if (_locked) {
			_seq->read_unlock();
			_locked = false;
		}
	} else {
		//printf("New MIDI Iterator = %X @ %lf\n", _event->type(), _event->time());
	}

	assert(_is_end || (_event->buffer() && _event->buffer()[0] != '\0'));
}

Sequence::const_iterator::~const_iterator()
{
	if (_locked) {
		_seq->read_unlock();
	}
}

const Sequence::const_iterator& Sequence::const_iterator::operator++()
{
	if (_is_end) {
		throw std::logic_error("Attempt to iterate past end of Sequence");
	}
	
	assert(_event->buffer() && _event->buffer()[0] != '\0');

	/*cerr << "const_iterator::operator++: " << _event->to_string() << endl;*/

	if (! (_event->is_note() || _event->is_cc() || _event->is_pgm_change() || _event->is_pitch_bender() || _event->is_channel_aftertouch()) ) {
		cerr << "FAILED event buffer: " << hex << int(_event->buffer()[0]) << int(_event->buffer()[1]) << int(_event->buffer()[2]) << endl;
	}
	assert((_event->is_note() || _event->is_cc() || _event->is_pgm_change() || _event->is_pitch_bender() || _event->is_channel_aftertouch()));

	// Increment past current control event
	if (!_event->is_note() && _control_iter != _control_iters.end() && _control_iter->list.get()) {
		double x = 0.0, y = 0.0;
		const bool ret = _control_iter->list->rt_safe_earliest_event_unlocked(
				_control_iter->x, DBL_MAX, x, y, false);

		if (ret) {
			_control_iter->x = x;
			_control_iter->y = y;
		} else {
			_control_iter->list.reset();
			_control_iter->x = DBL_MAX;
		}
	}

	const std::vector<ControlIterator>::iterator old_control_iter = _control_iter;
	_control_iter = _control_iters.begin();

	// find the _control_iter with the earliest event time
	for (std::vector<ControlIterator>::iterator i = _control_iters.begin();
			i != _control_iters.end(); ++i) {
		if (i->x < _control_iter->x) {
			_control_iter = i;
		}
	}

	enum Type {NIL, NOTE_ON, NOTE_OFF, AUTOMATION};

	Type type = NIL;
	double t = 0;

	// Next earliest note on
	if (_note_iter != _seq->notes().end()) {
		type = NOTE_ON;
		t = (*_note_iter)->time();
	}

	// Use the next earliest note off iff it's earlier than the note on
	if (!_seq->percussive() && (! _active_notes.empty())) {
		if (type == NIL || _active_notes.top()->end_time() <= (*_note_iter)->time()) {
			type = NOTE_OFF;
			t = _active_notes.top()->end_time();
		}
	}

	// Use the next earliest controller iff it's earlier than the note event
	if (_control_iter != _control_iters.end() && _control_iter->x != DBL_MAX /*&& _control_iter != old_control_iter */) {
		if (type == NIL || _control_iter->x < t) {
			type = AUTOMATION;
		}
	}

	if (type == NOTE_ON) {
		//cerr << "********** MIDI Iterator = note on" << endl;
		*_event = (*_note_iter)->on_event();
		_active_notes.push(*_note_iter);
		++_note_iter;
	} else if (type == NOTE_OFF) {
		//cerr << "********** MIDI Iterator = note off" << endl;
		*_event = _active_notes.top()->off_event();
		_active_notes.pop();
	} else if (type == AUTOMATION) {
		//cerr << "********** MIDI Iterator = Automation" << endl;
		_seq->control_to_midi_event(_event, *_control_iter);
	} else {
		//cerr << "********** MIDI Iterator = End" << endl;
		_is_end = true;
	}

	assert(_is_end || _event->size() > 0);

	return *this;
}

bool Sequence::const_iterator::operator==(const const_iterator& other) const
{
	if (_is_end || other._is_end) {
		return (_is_end == other._is_end);
	} else {
		return (_event == other._event);
	}
}

Sequence::const_iterator& Sequence::const_iterator::operator=(const const_iterator& other)
{
	if (_locked && _seq != other._seq) {
		_seq->read_unlock();
	}

	_seq           = other._seq;
	_active_notes  = other._active_notes;
	_is_end        = other._is_end;
	_locked        = other._locked;
	_note_iter     = other._note_iter;
	_control_iters = other._control_iters;
	size_t index   = other._control_iter - other._control_iters.begin();
	_control_iter  = _control_iters.begin() + index;
	
	if (!_is_end) {
		_event =  boost::shared_ptr<Event>(new Event(*other._event, true));
	}

	return *this;
}

// Sequence

Sequence::Sequence(size_t size)
	: _read_iter(*this, DBL_MAX)
	, _edited(false)
	, _notes(size)
	, _writing(false)
	, _end_iter(*this, DBL_MAX)
	, _next_read(UINT32_MAX)
	, _percussive(false)
{
	assert(_end_iter._is_end);
	assert( ! _end_iter._locked);
}

/** Read events in frame range \a start .. \a start+cnt into \a dst,
 * adding \a offset to each event's timestamp.
 * \return number of events written to \a dst
 */
size_t Sequence::read(EventSink& dst, timestamp_t start, timestamp_t nframes, timedur_t offset) const
{
	//cerr << this << " MM::read @ " << start << " frames: " << nframes << " -> " << stamp_offset << endl;
	//cerr << this << " MM # notes: " << n_notes() << endl;

	size_t read_events = 0;

	if (start != _next_read) {
		_read_iter = const_iterator(*this, (double)start);
		//cerr << "Repositioning iterator from " << _next_read << " to " << start << endl;
	} else {
		//cerr << "Using cached iterator at " << _next_read << endl;
	}

	_next_read = (nframes_t) floor (start + nframes);

	while (_read_iter != end() && _read_iter->time() < start + nframes) {
		assert(_read_iter->size() > 0);
		assert(_read_iter->buffer());
		dst.write(_read_iter->time() + offset,
		          _read_iter->size(), 
		          _read_iter->buffer());
		
		 /*cerr << this << " Sequence::read event @ " << _read_iter->time()  
		 << " type: " << hex << int(_read_iter->type()) << dec 
		 << " note: " << int(_read_iter->note()) 
		 << " velocity: " << int(_read_iter->velocity()) 
		 << endl;*/
		
		++_read_iter;
		++read_events;
	}

	return read_events;
}

/** Write the controller event pointed to by \a iter to \a ev.
 * The buffer of \a ev will be allocated or resized as necessary.
 * \return true on success
 */
bool
Sequence::control_to_midi_event(boost::shared_ptr<Event>& ev, const ControlIterator& iter) const
{
	assert(iter.list.get());
	if (!ev) {
		ev = boost::shared_ptr<Event>(new Event(0, 3, NULL, true));
	}
	
	switch (iter.list->parameter().type()) {
	case midi_cc_type:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.list->parameter().id() <= INT8_MAX);
		assert(iter.y <= INT8_MAX);
		
		ev->time() = iter.x;
		ev->realloc(3);
		ev->buffer()[0] = MIDI_CMD_CONTROL + iter.list->parameter().channel();
		ev->buffer()[1] = (uint8_t)iter.list->parameter().id();
		ev->buffer()[2] = (uint8_t)iter.y;
		break;

	case midi_pc_type:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.list->parameter().id() == 0);
		assert(iter.y <= INT8_MAX);
		
		ev->time() = iter.x;
		ev->realloc(2);
		ev->buffer()[0] = MIDI_CMD_PGM_CHANGE + iter.list->parameter().channel();
		ev->buffer()[1] = (uint8_t)iter.y;
		break;

	case midi_pb_type:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.list->parameter().id() == 0);
		assert(iter.y < (1<<14));
		
		ev->time() = iter.x;
		ev->realloc(3);
		ev->buffer()[0] = MIDI_CMD_BENDER + iter.list->parameter().channel();
		ev->buffer()[1] = uint16_t(iter.y) & 0x7F; // LSB
		ev->buffer()[2] = (uint16_t(iter.y) >> 7) & 0x7F; // MSB
		break;

	case midi_ca_type:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.list->parameter().id() == 0);
		assert(iter.y <= INT8_MAX);

		ev->time() = iter.x;
		ev->realloc(2);
		ev->buffer()[0] = MIDI_CMD_CHANNEL_PRESSURE + iter.list->parameter().channel();
		ev->buffer()[1] = (uint8_t)iter.y;
		break;

	default:
		return false;
	}

	return true;
}


/** Clear all events from the model.
 */
void Sequence::clear()
{
	_lock.writer_lock();
	_notes.clear();
	for (Controls::iterator li = _controls.begin(); li != _controls.end(); ++li)
		li->second->list()->clear();
	_next_read = 0;
	_read_iter = end();
	_lock.writer_unlock();
}


/** Begin a write of events to the model.
 *
 * If \a mode is Sustained, complete notes with duration are constructed as note
 * on/off events are received.  Otherwise (Percussive), only note on events are
 * stored; note off events are discarded entirely and all contained notes will
 * have duration 0.
 */
void Sequence::start_write()
{
	//cerr << "MM " << this << " START WRITE, PERCUSSIVE = " << _percussive << endl;
	write_lock();
	_writing = true;
	for (int i = 0; i < 16; ++i)
		_write_notes[i].clear();
	
	_dirty_controls.clear();
	write_unlock();
}

/** Finish a write of events to the model.
 *
 * If \a delete_stuck is true and the current mode is Sustained, note on events
 * that were never resolved with a corresonding note off will be deleted.
 * Otherwise they will remain as notes with duration 0.
 */
void Sequence::end_write(bool delete_stuck)
{
	write_lock();
	assert(_writing);

	//cerr << "MM " << this << " END WRITE: " << _notes.size() << " NOTES\n";

	if (!_percussive && delete_stuck) {
		for (Notes::iterator n = _notes.begin(); n != _notes.end() ;) {
			if ((*n)->duration() == 0) {
				cerr << "WARNING: Stuck note lost: " << (*n)->note() << endl;
				n = _notes.erase(n);
				// we have to break here because erase invalidates the iterator
				break;
			} else {
				++n;
			}
		}
	}

	for (int i = 0; i < 16; ++i) {
		if (!_write_notes[i].empty()) {
			cerr << "WARNING: Sequence::end_write: Channel " << i << " has "
					<< _write_notes[i].size() << " stuck notes" << endl;
		}
		_write_notes[i].clear();
	}

	for (ControlLists::const_iterator i = _dirty_controls.begin(); i != _dirty_controls.end(); ++i) {
		(*i)->mark_dirty();
	}
	
	_writing = false;
	write_unlock();
}

/** Append \a ev to model.  NOT realtime safe.
 *
 * Timestamps of events in \a buf are expected to be relative to
 * the start of this model (t=0) and MUST be monotonically increasing
 * and MUST be >= the latest event currently in the model.
 */
void Sequence::append(const Event& ev)
{
	write_lock();
	_edited = true;

	assert(_notes.empty() || ev.time() >= _notes.back()->time());
	assert(_writing);

	if (ev.is_note_on()) {
		append_note_on_unlocked(ev.channel(), ev.time(), ev.note(),
				ev.velocity());
	} else if (ev.is_note_off()) {
		append_note_off_unlocked(ev.channel(), ev.time(), ev.note());
	} else if (ev.is_cc()) {
		append_control_unlocked(
				Evoral::MIDI::ContinuousController(midi_cc_type, ev.cc_number(), ev.channel()),
				ev.time(), ev.cc_value());
	} else if (ev.is_pgm_change()) {
		append_control_unlocked(
				Evoral::MIDI::ProgramChange(midi_pc_type, ev.channel()),
				ev.time(), ev.pgm_number());
	} else if (ev.is_pitch_bender()) {
		append_control_unlocked(
				Evoral::MIDI::PitchBender(midi_pb_type, ev.channel()),
				ev.time(), double(  (0x7F & ev.pitch_bender_msb()) << 7
				                  | (0x7F & ev.pitch_bender_lsb()) ));
	} else if (ev.is_channel_aftertouch()) {
		append_control_unlocked(
				Evoral::MIDI::ChannelAftertouch(midi_ca_type, ev.channel()),
				ev.time(), ev.channel_aftertouch());
	} else {
		printf("WARNING: Sequence: Unknown event type %X\n", ev.type());
	}

	write_unlock();
}

void Sequence::append_note_on_unlocked(uint8_t chan, double time,
		uint8_t note_num, uint8_t velocity)
{
	/*cerr << "Sequence " << this << " chan " << (int)chan <<
	 " note " << (int)note_num << " on @ " << time << endl;*/

	assert(note_num <= 127);
	assert(chan < 16);
	assert(_writing);
	_edited = true;

	boost::shared_ptr<Note> new_note(new Note(chan, time, 0, note_num, velocity));
	_notes.push_back(new_note);
	if (!_percussive) {
		//cerr << "MM Sustained: Appending active note on " << (unsigned)(uint8_t)note_num << endl;
		_write_notes[chan].push_back(_notes.size() - 1);
	}/* else {
	 cerr << "MM Percussive: NOT appending active note on" << endl;
	 }*/
}

void Sequence::append_note_off_unlocked(uint8_t chan, double time,
		uint8_t note_num)
{
	/*cerr << "Sequence " << this << " chan " << (int)chan <<
	 " note " << (int)note_num << " off @ " << time << endl;*/

	assert(note_num <= 127);
	assert(chan < 16);
	assert(_writing);
	_edited = true;

	if (_percussive) {
		cerr << "Sequence Ignoring note off (percussive mode)" << endl;
		return;
	}

	/* FIXME: make _write_notes fixed size (127 noted) for speed */

	/* FIXME: note off velocity for that one guy out there who actually has
	 * keys that send it */

	bool resolved = false;

	for (WriteNotes::iterator n = _write_notes[chan].begin(); n
			!= _write_notes[chan].end(); ++n) {
		Note& note = *_notes[*n].get();
		if (note.note() == note_num) {
			assert(time >= note.time());
			note.set_duration(time - note.time());
			_write_notes[chan].erase(n);
			//cerr << "MM resolved note, duration: " << note.duration() << endl;
			resolved = true;
			break;
		}
	}

	if (!resolved) {
		cerr << "Sequence " << this << " spurious note off chan " << (int)chan
				<< ", note " << (int)note_num << " @ " << time << endl;
	}
}

void Sequence::append_control_unlocked(const Parameter& param, double time, double value)
{
	control(param, true)->list()->rt_add(time, value);
}


void Sequence::add_note_unlocked(const boost::shared_ptr<Note> note)
{
	//cerr << "Sequence " << this << " add note " << (int)note.note() << " @ " << note.time() << endl;
	_edited = true;
	Notes::iterator i = upper_bound(_notes.begin(), _notes.end(), note,
			note_time_comparator);
	_notes.insert(i, note);
}

void Sequence::remove_note_unlocked(const boost::shared_ptr<const Note> note)
{
	_edited = true;
	//cerr << "Sequence " << this << " remove note " << (int)note.note() << " @ " << note.time() << endl;
	for (Notes::iterator n = _notes.begin(); n != _notes.end(); ++n) {
		Note& _n = *(*n);
		const Note& _note = *note;
		// TODO: There is still the issue, that after restarting ardour
		// persisted undo does not work, because of rounding errors in the
		// event times after saving/restoring to/from MIDI files
		/*cerr << "======================================= " << endl;
		cerr << int(_n.note()) << "@" << int(_n.time()) << "[" << int(_n.channel()) << "] --" << int(_n.duration()) << "-- #" << int(_n.velocity()) << endl;
		cerr << int(_note.note()) << "@" << int(_note.time()) << "[" << int(_note.channel()) << "] --" << int(_note.duration()) << "-- #" << int(_note.velocity()) << endl;
		cerr << "Equal: " << bool(_n == _note) << endl;
		cerr << endl << endl;*/
		if (_n == _note) {
			_notes.erase(n);
			// we have to break here, because erase invalidates all iterators, ie. n itself
			break;
		}
	}
}

/** Slow!  for debugging only. */
#ifndef NDEBUG
bool Sequence::is_sorted() const {
	bool t = 0;
	for (Notes::const_iterator n = _notes.begin(); n != _notes.end(); ++n)
		if ((*n)->time() < t)
			return false;
		else
			t = (*n)->time();

	return true;
}
#endif

} // namespace Evoral

