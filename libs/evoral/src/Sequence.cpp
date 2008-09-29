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
#include <evoral/TypeMap.hpp>

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

struct null_ostream : public std::ostream {
	null_ostream(): std::ios(0), std::ostream(0) {}
};
static null_ostream nullout;

//static ostream& debugout = cout;
static ostream& debugout = nullout;
static ostream& errorout = cerr;

// Read iterator (const_iterator)

Sequence::const_iterator::const_iterator(const Sequence& seq, EventTime t)
	: _seq(&seq)
	, _is_end( (t == DBL_MAX) || seq.empty() )
	, _locked( !_is_end )
{
	debugout << "Created Iterator @ " << t << " (is end: " << _is_end << ")" << endl;

	if (_is_end) {
		return;
	}

	seq.read_lock();

	// find first note which begins after t
	_note_iter = seq.notes().end();
	for (Sequence::Notes::const_iterator i = seq.notes().begin(); i != seq.notes().end(); ++i) {
		if ((*i)->time() >= t) {
			_note_iter = i;
			break;
		}
	}

	ControlIterator earliest_control(boost::shared_ptr<ControlList>(), DBL_MAX, 0.0);

	_control_iters.reserve(seq._controls.size());
	
	// find the earliest control event available
	for (Controls::const_iterator i = seq._controls.begin(); i != seq._controls.end(); ++i) {
		debugout << "Iterator: control: " << seq._type_map.to_symbol(i->first) << endl;
		double x, y;
		bool ret = i->second->list()->rt_safe_earliest_event_unlocked(t, DBL_MAX, x, y);
		if (!ret) {
			debugout << "Iterator: CC " << i->first.id() << " (size " << i->second->list()->size()
				<< ") has no events past " << t << endl;
			continue;
		}

		assert(x >= 0);

		/*if (y < i->first.min() || y > i->first.max()) {
			errorout << "ERROR: Controller " << i->first.symbol() << " value " << y
				<< " out of range [" << i->first.min() << "," << i->first.max()
				<< "], event ignored" << endl;
			continue;
		}*/

		const ControlIterator new_iter(i->second->list(), x, y);

		debugout << "Iterator: CC " << i->first.id() << " added (" << x << ", " << y << ")" << endl;
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

	if (_note_iter != seq.notes().end()
			&& (*_note_iter)->on_event().time() >= t
			&& (!earliest_control.list
				|| (*_note_iter)->on_event().time() < earliest_control.x)) {
		debugout << "Reading note on event @ " << (*_note_iter)->on_event().time() << endl;
		_event = boost::shared_ptr<Event>(new Event((*_note_iter)->on_event(), true));
		_active_notes.push(*_note_iter);
		++_note_iter;
		_control_iter = _control_iters.end();
	} else if (earliest_control.list) {
		debugout << "Reading control event @ " << earliest_control.x << endl;
		seq.control_to_midi_event(_event, earliest_control);
	}

	if ( (! _event.get()) || _event->size() == 0) {
		debugout << "New iterator @ " << t << " is at end." << endl;
		_is_end = true;

		// eliminate possible race condition here (ugly)
		static Glib::Mutex mutex;
		Glib::Mutex::Lock lock(mutex);
		if (_locked) {
			_seq->read_unlock();
			_locked = false;
		}
	} else {
		debugout << "New Iterator = " << _event->event_type();
		debugout << " : " << hex << (int)((MIDIEvent*)_event.get())->type();
		debugout << " @ " <<  _event->time() << endl;
	}

	//assert(_is_end || (_event->buffer() && _event->buffer()[0] != '\0'));
}

Sequence::const_iterator::~const_iterator()
{
	if (_locked) {
		_seq->read_unlock();
	}
}

const
Sequence::const_iterator& Sequence::const_iterator::operator++()
{
	if (_is_end) {
		throw std::logic_error("Attempt to iterate past end of Sequence");
	}
	
	debugout << "Iterator ++" << endl;
	assert(_event->buffer() && _event->size() > 0);
	
	const MIDIEvent& ev = *((MIDIEvent*)_event.get());

	//debugout << "const_iterator::operator++: " << _event->to_string() << endl;

	if (! (ev.is_note() || ev.is_cc() || ev.is_pgm_change()
				|| ev.is_pitch_bender() || ev.is_channel_pressure()) ) {
		errorout << "Unknown event type: " << hex << int(ev.buffer()[0])
			<< int(ev.buffer()[1]) << int(ev.buffer()[2]) << endl;
	}
	assert((ev.is_note() || ev.is_cc() || ev.is_pgm_change() || ev.is_pitch_bender() || ev.is_channel_pressure()));

	// Increment past current control event
	if (!ev.is_note() && _control_iter != _control_iters.end() && _control_iter->list.get()) {
		double x = 0.0, y = 0.0;
		const bool ret = _control_iter->list->rt_safe_earliest_event_unlocked(
				_control_iter->x, DBL_MAX, x, y, false);

		assert(!ret || x > _control_iter->x);

		if (ret) {
			_control_iter->x = x;
			_control_iter->y = y;
		} else {
			_control_iter->list.reset();
			_control_iter->x = DBL_MAX;
			_control_iter->y = DBL_MAX;
		}
	}

	_control_iter = _control_iters.begin();

	// find the _control_iter with the earliest event time
	for (ControlIterators::iterator i = _control_iters.begin(); i != _control_iters.end(); ++i) {
		if (i->x < _control_iter->x) {
			_control_iter = i;
		}
	}

	enum Type {NIL, NOTE_ON, NOTE_OFF, CONTROL};

	Type type = NIL;
	EventTime t = 0;

	// Next earliest note on
	if (_note_iter != _seq->notes().end()) {
		type = NOTE_ON;
		t = (*_note_iter)->time();
	}

	// Use the next earliest note off iff it's earlier than the note on
	if (!_seq->percussive() && (! _active_notes.empty())) {
		if (type == NIL || _active_notes.top()->end_time() <= t) {
			type = NOTE_OFF;
			t = _active_notes.top()->end_time();
		}
	}

	// Use the next earliest controller iff it's earlier than the note event
	if (_control_iter != _control_iters.end() && _control_iter->x != DBL_MAX) {
		if (type == NIL || _control_iter->x < t) {
			type = CONTROL;
		}
	}

	if (type == NOTE_ON) {
		debugout << "Iterator = note on" << endl;
		*_event = (*_note_iter)->on_event();
		_active_notes.push(*_note_iter);
		++_note_iter;
	} else if (type == NOTE_OFF) {
		debugout << "Iterator = note off" << endl;
		*_event = _active_notes.top()->off_event();
		_active_notes.pop();
	} else if (type == CONTROL) {
		debugout << "Iterator = control" << endl;
		_seq->control_to_midi_event(_event, *_control_iter);
	} else {
		debugout << "Iterator = End" << endl;
		_is_end = true;
	}

	assert(_is_end || (_event->size() > 0 && _event->buffer() && _event->buffer()[0] != '\0'));

	return *this;
}

bool
Sequence::const_iterator::operator==(const const_iterator& other) const
{
	if (_is_end || other._is_end) {
		return (_is_end == other._is_end);
	} else {
		return (_event == other._event);
	}
}

Sequence::const_iterator&
Sequence::const_iterator::operator=(const const_iterator& other)
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
	
	if (!_is_end && other._event) {
		if (_event) {
			*_event = *other._event.get();
		} else {
			_event = boost::shared_ptr<Event>(new Event(*other._event, true));
		}
	} else {
		if (_event) {
			_event->clear();
		}
	}

	return *this;
}

// Sequence

Sequence::Sequence(const TypeMap& type_map, size_t size)
	: _read_iter(*this, DBL_MAX)
	, _edited(false)
	, _type_map(type_map)
	, _notes(size)
	, _writing(false)
	, _end_iter(*this, DBL_MAX)
	, _next_read(UINT32_MAX)
	, _percussive(false)
	, _lowest_note(127)
	, _highest_note(0)
{
	debugout << "Sequence (size " << size << ") constructed: " << this << endl;
	assert(_end_iter._is_end);
	assert( ! _end_iter._locked);
}

/** Read events in frame range \a start .. \a start+cnt into \a dst,
 * adding \a offset to each event's timestamp.
 * \return number of events written to \a dst
 */
size_t
Sequence::read(EventSink& dst, timestamp_t start, timedur_t nframes, timestamp_t offset) const
{
	debugout << this << " read @ " << start << " * " << nframes << " + " << offset << endl;
	debugout << this << " # notes: " << n_notes() << endl;
	debugout << this << " # controls: " << _controls.size() << endl;

	size_t read_events = 0;

	if (start != _next_read) {
		debugout << "Repositioning iterator from " << _next_read << " to " << start << endl;
		_read_iter = const_iterator(*this, (double)start);
	} else {
		debugout << "Using cached iterator at " << _next_read << endl;
	}

	_next_read = (nframes_t) floor (start + nframes);

	while (_read_iter != end() && _read_iter->time() < start + nframes) {
		assert(_read_iter->size() > 0);
		assert(_read_iter->buffer());
		dst.write(_read_iter->time() + offset,
		          _read_iter->event_type(),
		          _read_iter->size(), 
		          _read_iter->buffer());
		
		 debugout << this << " read event type " << _read_iter->event_type()
			 << " @ " << _read_iter->time() << " : ";
		 for (size_t i = 0; i < _read_iter->size(); ++i)
			 debugout << hex << (int)_read_iter->buffer()[i];
		 debugout << endl;
		
		++_read_iter;
		++read_events;
	}

	return read_events;
}

/** Write the controller event pointed to by \a iter to \a ev.
 * The buffer of \a ev will be allocated or resized as necessary.
 * The event_type of \a ev should be set to the expected output type.
 * \return true on success
 */
bool
Sequence::control_to_midi_event(boost::shared_ptr<Event>& ev, const ControlIterator& iter) const
{
	assert(iter.list.get());
	const uint32_t event_type = iter.list->parameter().type();
	if (!ev) {
		ev = boost::shared_ptr<Event>(new Event(event_type, 0, 3, NULL, true));
	}
	
	uint8_t midi_type = _type_map.parameter_midi_type(iter.list->parameter());
	ev->set_event_type(_type_map.midi_event_type(midi_type));
	switch (midi_type) {
	case MIDI_CMD_CONTROL:
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

	case MIDI_CMD_PGM_CHANGE:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.y <= INT8_MAX);
		
		ev->time() = iter.x;
		ev->realloc(2);
		ev->buffer()[0] = MIDI_CMD_PGM_CHANGE + iter.list->parameter().channel();
		ev->buffer()[1] = (uint8_t)iter.y;
		break;

	case MIDI_CMD_BENDER:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.y < (1<<14));
		
		ev->time() = iter.x;
		ev->realloc(3);
		ev->buffer()[0] = MIDI_CMD_BENDER + iter.list->parameter().channel();
		ev->buffer()[1] = uint16_t(iter.y) & 0x7F; // LSB
		ev->buffer()[2] = (uint16_t(iter.y) >> 7) & 0x7F; // MSB
		break;

	case MIDI_CMD_CHANNEL_PRESSURE:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
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
void
Sequence::clear()
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
 * If \a mode is Sustained, complete notes with length are constructed as note
 * on/off events are received.  Otherwise (Percussive), only note on events are
 * stored; note off events are discarded entirely and all contained notes will
 * have length 0.
 */
void
Sequence::start_write()
{
	debugout << this << " START WRITE, PERCUSSIVE = " << _percussive << endl;
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
 * Otherwise they will remain as notes with length 0.
 */
void
Sequence::end_write(bool delete_stuck)
{
	write_lock();
	assert(_writing);

	debugout << this << " END WRITE: " << _notes.size() << " NOTES\n";

	if (!_percussive && delete_stuck) {
		for (Notes::iterator n = _notes.begin(); n != _notes.end() ;) {
			if ((*n)->length() == 0) {
				errorout << "WARNING: Stuck note lost: " << (*n)->note() << endl;
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
			errorout << "WARNING: Sequence::end_write: Channel " << i << " has "
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
void
Sequence::append(const Event& event)
{
	write_lock();
	_edited = true;
	
	const MIDIEvent& ev = (const MIDIEvent&)event;

	assert(_notes.empty() || ev.time() >= _notes.back()->time());
	assert(_writing);

	if (ev.is_note_on()) {
		append_note_on_unlocked(ev.channel(), ev.time(), ev.note(),
				ev.velocity());
	} else if (ev.is_note_off()) {
		append_note_off_unlocked(ev.channel(), ev.time(), ev.note());
	} else if (!_type_map.type_is_midi(ev.event_type())) {
		printf("WARNING: Sequence: Unknown event type %X\n", ev.event_type());
	} else if (ev.is_cc()) {
		append_control_unlocked(
				Evoral::MIDI::ContinuousController(ev.event_type(), ev.channel(), ev.cc_number()),
				ev.time(), ev.cc_value());
	} else if (ev.is_pgm_change()) {
		append_control_unlocked(
				Evoral::MIDI::ProgramChange(ev.event_type(), ev.channel()),
				ev.time(), ev.pgm_number());
	} else if (ev.is_pitch_bender()) {
		append_control_unlocked(
				Evoral::MIDI::PitchBender(ev.event_type(), ev.channel()),
				ev.time(), double(  (0x7F & ev.pitch_bender_msb()) << 7
					| (0x7F & ev.pitch_bender_lsb()) ));
	} else if (ev.is_channel_pressure()) {
		append_control_unlocked(
				Evoral::MIDI::ChannelPressure(ev.event_type(), ev.channel()),
				ev.time(), ev.channel_pressure());
	} else {
		printf("WARNING: Sequence: Unknown MIDI event type %X\n", ev.type());
	}

	write_unlock();
}

void
Sequence::append_note_on_unlocked(uint8_t chan, EventTime time, uint8_t note_num, uint8_t velocity)
{
	debugout << this << " c" << (int)chan << " note " << (int)note_num << " off @ " << time << endl;
	assert(note_num <= 127);
	assert(chan < 16);
	assert(_writing);
	_edited = true;

	if (note_num < _lowest_note)
		_lowest_note = note_num;
	if (note_num > _highest_note)
		_highest_note = note_num;

	boost::shared_ptr<Note> new_note(new Note(chan, time, 0, note_num, velocity));
	_notes.push_back(new_note);
	if (!_percussive) {
		debugout << "Sustained: Appending active note on " << (unsigned)(uint8_t)note_num << endl;
		_write_notes[chan].push_back(_notes.size() - 1);
	} else {
	 	debugout << "Percussive: NOT appending active note on" << endl;
	 }
}

void
Sequence::append_note_off_unlocked(uint8_t chan, EventTime time, uint8_t note_num)
{
	debugout << this << " c" << (int)chan << " note " << (int)note_num << " off @ " << time << endl;
	assert(note_num <= 127);
	assert(chan < 16);
	assert(_writing);
	_edited = true;

	if (_percussive) {
		debugout << "Sequence Ignoring note off (percussive mode)" << endl;
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
			note.set_length(time - note.time());
			_write_notes[chan].erase(n);
			debugout << "resolved note, length: " << note.length() << endl;
			resolved = true;
			break;
		}
	}

	if (!resolved) {
		errorout << this << " spurious note off chan " << (int)chan
				<< ", note " << (int)note_num << " @ " << time << endl;
	}
}

void
Sequence::append_control_unlocked(const Parameter& param, EventTime time, double value)
{
	debugout << this << " " << _type_map.to_symbol(param) << " @ " << time << " \t= \t" << value
			<< " # controls: " << _controls.size() << endl;
	control(param, true)->list()->rt_add(time, value);
}


void
Sequence::add_note_unlocked(const boost::shared_ptr<Note> note)
{
	debugout << this << " add note " << (int)note->note() << " @ " << note->time() << endl;
	_edited = true;
	Notes::iterator i = upper_bound(_notes.begin(), _notes.end(), note,
			note_time_comparator);
	_notes.insert(i, note);
}

void
Sequence::remove_note_unlocked(const boost::shared_ptr<const Note> note)
{
	_edited = true;
	debugout << this << " remove note " << (int)note->note() << " @ " << note->time() << endl;
	for (Notes::iterator n = _notes.begin(); n != _notes.end(); ++n) {
		Note& _n = *(*n);
		const Note& _note = *note;
		// TODO: There is still the issue, that after restarting ardour
		// persisted undo does not work, because of rounding errors in the
		// event times after saving/restoring to/from MIDI files
		/*cerr << "======================================= " << endl;
		cerr << int(_n.note()) << "@" << int(_n.time()) << "[" << int(_n.channel()) << "] --" << int(_n.length()) << "-- #" << int(_n.velocity()) << endl;
		cerr << int(_note.note()) << "@" << int(_note.time()) << "[" << int(_note.channel()) << "] --" << int(_note.length()) << "-- #" << int(_note.velocity()) << endl;
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
bool
Sequence::is_sorted() const {
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

