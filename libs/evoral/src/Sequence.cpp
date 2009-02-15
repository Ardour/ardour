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
#include <algorithm>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <stdint.h>
#include "evoral/Control.hpp"
#include "evoral/ControlList.hpp"
#include "evoral/ControlSet.hpp"
#include "evoral/EventSink.hpp"
#include "evoral/MIDIParameters.hpp"
#include "evoral/Sequence.hpp"
#include "evoral/TypeMap.hpp"

using namespace std;

namespace Evoral {

template<typename Time>
void Sequence<Time>::write_lock() {
	_lock.writer_lock();
	_control_lock.lock();
}

template<typename Time>
void Sequence<Time>::write_unlock() {
	_lock.writer_unlock();
	_control_lock.unlock();
}

template<typename Time>
void Sequence<Time>::read_lock() const {
	_lock.reader_lock();
}

template<typename Time>
void Sequence<Time>::read_unlock() const {
	_lock.reader_unlock();
}

struct null_ostream : public std::ostream {
	null_ostream(): std::ios(0), std::ostream(0) {}
};
static null_ostream nullout;

#ifdef DEBUG_SEQUENCE
static ostream& debugout = cout;
#endif

static ostream& errorout = cerr;

// Read iterator (const_iterator)

template<typename Time>
Sequence<Time>::const_iterator::const_iterator()
	: _seq(NULL)
	, _is_end(true)
	, _locked(false)
{
	_event = boost::shared_ptr< Event<Time> >(new Event<Time>());
}

template<typename Time>
Sequence<Time>::const_iterator::const_iterator(const Sequence<Time>& seq, Time t)
	: _seq(&seq)
	, _is_end( (t == DBL_MAX) || seq.empty() )
	, _locked( !_is_end )
{
	#ifdef DEBUG_SEQUENCE
	debugout << "Created Iterator @ " << t << " (is end: " << _is_end << ")" << endl;
	#endif
	
	if (_is_end) {
		return;
	}

	seq.read_lock();

	// find first note which begins after t
	_note_iter = seq.notes().end();
	for (typename Sequence<Time>::Notes::const_iterator i = seq.notes().begin();
			i != seq.notes().end(); ++i) {
		if ((*i)->time() >= t) {
			_note_iter = i;
			break;
		}
	}
	assert(_note_iter == seq.notes().end() || (*_note_iter)->time() >= t);
	
	// find first sysex which begins after t
	_sysex_iter = seq.sysexes().end();
	for (typename Sequence<Time>::SysExes::const_iterator i = seq.sysexes().begin();
			i != seq.sysexes().end(); ++i) {
		if ((*i)->time() >= t) {
			_sysex_iter = i;
			break;
		}
	}
	assert(_sysex_iter == seq.sysexes().end() || (*_sysex_iter)->time() >= t);

	ControlIterator earliest_control(boost::shared_ptr<ControlList>(), DBL_MAX, 0.0);

	_control_iters.reserve(seq._controls.size());
	
	// find the earliest control event available
	for (Controls::const_iterator i = seq._controls.begin(); i != seq._controls.end(); ++i) {
		#ifdef DEBUG_SEQUENCE		
		debugout << "Iterator: control: " << seq._type_map.to_symbol(i->first) << endl;
		#endif
		double x, y;
		bool ret = i->second->list()->rt_safe_earliest_event_unlocked(t, DBL_MAX, x, y);
		if (!ret) {
			#ifdef DEBUG_SEQUENCE
			debugout << "Iterator: CC " << i->first.id() << " (size " << i->second->list()->size()
				<< ") has no events past " << t << endl;
			#endif
			continue;
		}

		assert(x >= 0);

		/*
		if (y < i->first.min() || y > i->first.max()) {
			errorout << "ERROR: Controller " << i->first.symbol() << " value " << y
				<< " out of range [" << i->first.min() << "," << i->first.max()
				<< "], event ignored" << endl;
			continue;
		}
		*/

		const ControlIterator new_iter(i->second->list(), x, y);

		#ifdef DEBUG_SEQUENCE
		debugout << "Iterator: CC " << i->first.id() << " added (" << x << ", " << y << ")" << endl;
		#endif
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
	
#define MAKE_SURE_ADDING_SYSEXES_PRESERVES_OLD_SEMANTICS 1
#if MAKE_SURE_ADDING_SYSEXES_PRESERVES_OLD_SEMANTICS
	MIDIMessageType original_type       = NIL;
	assert (!earliest_control.list || earliest_control.x >= t);
	
	       if (_note_iter != seq.notes().end()
	                       && (*_note_iter)->on_event().time() >= t
	                       && (!earliest_control.list
	                               || (*_note_iter)->on_event().time() < earliest_control.x)) {
	    	   original_type = NOTE_ON;
	       } else {
	    	   original_type = CONTROL;
	       }
#endif
	
	MIDIMessageType type       = NIL;
	Time            earliest_t = t;

	// if the note comes before anything else set the iterator to the note
	if (_note_iter != seq.notes().end() && (*_note_iter)->on_event().time() >= t) {
		type = NOTE_ON;
		earliest_t = (*_note_iter)->on_event().time();
	}
	     
	if (earliest_control.list && 
	    earliest_control.x >= t &&
	    earliest_control.x <= earliest_t) {
		type = CONTROL;
		earliest_t = earliest_control.x;
	}
	
	if (_sysex_iter != seq.sysexes().end() &&
	    (*_sysex_iter)->time() >= t &&
	    (*_sysex_iter)->time() <= earliest_t) {
		type = SYSEX;
		earliest_t = (*_sysex_iter)->time();
	}
	
#if MAKE_SURE_ADDING_SYSEXES_PRESERVES_OLD_SEMANTICS
	assert (type == original_type || type == SYSEX);
#endif
	
	if (type == NOTE_ON) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Reading note on event @ " << earliest_t << endl;
		#endif
		// initialize the event pointer with a new event
		_event = boost::shared_ptr< Event<Time> >(new Event<Time>((*_note_iter)->on_event(), true));		
		_active_notes.push(*_note_iter);
		++_note_iter;
		_control_iter = _control_iters.end();
	} else if (type == CONTROL) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Reading control event @ " << earliest_t << endl;
		#endif
		seq.control_to_midi_event(_event, earliest_control);
	} else if (type == SYSEX) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Reading system exclusive event @ " << earliest_t << endl;
		#endif
		// initialize the event pointer with a new event
		_event = boost::shared_ptr< Event<Time> >(new Event<Time>(*(*_sysex_iter), true));
		++_sysex_iter;
		_control_iter = _control_iters.end();		
	}

	if ( (! _event.get()) || _event->size() == 0) {
		#ifdef DEBUG_SEQUENCE
		debugout << "New iterator @ " << t << " is at end." << endl;
		#endif
		_is_end = true;

		// eliminate possible race condition here (ugly)
		static Glib::Mutex mutex;
		Glib::Mutex::Lock lock(mutex);
		if (_locked) {
			_seq->read_unlock();
			_locked = false;
		}
	} else {
		#ifdef DEBUG_SEQUENCE
		debugout << "New Iterator = " << _event->event_type();
		debugout << " : " << hex << (int)((MIDIEvent<Time>*)_event.get())->type();
		debugout << " @ " <<  _event->time() << endl;
		#endif
	}
	
	assert(_event && _event->size() > 0);

	//assert(_is_end || (_event->buffer() && _event->buffer()[0] != '\0'));
}

template<typename Time>
Sequence<Time>::const_iterator::~const_iterator()
{
	if (_locked) {
		_seq->read_unlock();
	}
}

template<typename Time>
const typename Sequence<Time>::const_iterator&
Sequence<Time>::const_iterator::operator++()
{
	if (_is_end) {
		throw std::logic_error("Attempt to iterate past end of Sequence");
	}
	
	#ifdef DEBUG_SEQUENCE
	debugout << "Iterator ++" << endl;
	#endif
	assert(_event && _event->buffer() && _event->size() > 0);
	
	const MIDIEvent<Time>& ev = *((MIDIEvent<Time>*)_event.get());

	if (! (ev.is_note() || ev.is_cc() || ev.is_pgm_change()
				|| ev.is_pitch_bender() || ev.is_channel_pressure() || ev.is_sysex()) ) {
		errorout << "Unknown event type: " << hex << int(ev.buffer()[0])
			<< int(ev.buffer()[1]) << int(ev.buffer()[2]) << endl;
	}
	
	assert((
	        ev.is_note() || 
	        ev.is_cc() || 
	        ev.is_pgm_change() || 
	        ev.is_pitch_bender() || 
	        ev.is_channel_pressure() ||
	        ev.is_sysex()
	      ));

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

	MIDIMessageType type       = NIL;
	Time            earliest_t = 0;

	// Next earliest note on
	if (_note_iter != _seq->notes().end()) {
		type = NOTE_ON;
		earliest_t = (*_note_iter)->time();
	}

	// Use the next earliest note off iff it's earlier than the note on
	if (!_seq->percussive() && (! _active_notes.empty())) {
		if (type == NIL || _active_notes.top()->end_time() <= earliest_t) {
			type = NOTE_OFF;
			earliest_t = _active_notes.top()->end_time();
		}
	}

	// Use the next earliest controller iff it's earlier than the note event
	if (_control_iter != _control_iters.end() && _control_iter->x != DBL_MAX) {
		if (type == NIL || _control_iter->x <= earliest_t) {
			type = CONTROL;
			earliest_t = _control_iter->x;
		}
	}
	
	// Use the next earliest SysEx iff it's earlier than the controller
	if (_sysex_iter != _seq->sysexes().end()) {
		if (type == NIL || (*_sysex_iter)->time() <= earliest_t) {
			type = SYSEX;
			earliest_t = (*_sysex_iter)->time();
		}
	}

	if (type == NOTE_ON) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Iterator = note on" << endl;
		#endif
		*_event = (*_note_iter)->on_event();
		_active_notes.push(*_note_iter);
		++_note_iter;
	} else if (type == NOTE_OFF) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Iterator = note off" << endl;
		#endif
		*_event = _active_notes.top()->off_event();
		_active_notes.pop();
	} else if (type == CONTROL) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Iterator = control" << endl;
		#endif
		_seq->control_to_midi_event(_event, *_control_iter);
	} else if (type == SYSEX) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Iterator = SysEx" << endl;
		#endif
		*_event =*(*_sysex_iter);
		++_sysex_iter;
	} else {
		#ifdef DEBUG_SEQUENCE
		debugout << "Iterator = End" << endl;
		#endif
		_is_end = true;
	}

	assert(_is_end || (_event->size() > 0 && _event->buffer() && _event->buffer()[0] != '\0'));

	return *this;
}

template<typename Time>
bool
Sequence<Time>::const_iterator::operator==(const const_iterator& other) const
{
	if (_is_end || other._is_end) {
		return (_is_end == other._is_end);
	} else {
		return (_event == other._event);
	}
}

template<typename Time>
typename Sequence<Time>::const_iterator&
Sequence<Time>::const_iterator::operator=(const const_iterator& other)
{
	if (_seq != other._seq) {
		if (_locked) {
			_seq->read_unlock();
		}
		if (other._locked) {
		   other._seq->read_lock();
		}
	}

	_seq           = other._seq;
	_active_notes  = other._active_notes;
	_is_end        = other._is_end;
	_locked        = other._locked;
	_note_iter     = other._note_iter;
	_sysex_iter    = other._sysex_iter;
	_control_iters = other._control_iters;
	size_t index   = other._control_iter - other._control_iters.begin();
	_control_iter  = _control_iters.begin() + index;
	
	if (!_is_end && other._event) {
		if (_event) {
			*_event = *other._event.get();
		} else {
			_event = boost::shared_ptr< Event<Time> >(new Event<Time>(*other._event, true));
		}
	} else {
		if (_event) {
			_event->clear();
		}
	}

	return *this;
}

// Sequence

template<typename Time>
Sequence<Time>::Sequence(const TypeMap& type_map, size_t size)
	: _read_iter(*this, DBL_MAX)
	, _edited(false)
	, _type_map(type_map)
	, _notes(size)
	, _writing(false)
	, _end_iter(*this, DBL_MAX)
//	, _next_read(UINT32_MAX)
	, _percussive(false)
	, _lowest_note(127)
	, _highest_note(0)
{
	#ifdef DEBUG_SEQUENCE
	debugout << "Sequence (size " << size << ") constructed: " << this << endl;
	#endif
	assert(_end_iter._is_end);
	assert( ! _end_iter._locked);
}

#if 0
/** Read events in frame range \a start .. \a (start + dur) into \a dst,
 * adding \a offset to each event's timestamp.
 * \return number of events written to \a dst
 */
template<typename Time>
size_t
Sequence<Time>::read(EventSink<Time>& dst, Time start, Time dur, Time offset) const
{
	#ifdef DEBUG_SEQUENCE
	debugout << this << " read @ " << start << " * " << dur << " + " << offset << endl;
	debugout << this << " # notes: " << n_notes() << endl;
	debugout << this << " # controls: " << _controls.size() << endl;
	#endif

	size_t read_events = 0;

	if (start != _next_read) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Repositioning iterator from " << _next_read << " to " << start << endl;
		#endif
		_read_iter = const_iterator(*this, start);
	} else {
		#ifdef DEBUG_SEQUENCE
		debugout << "Using cached iterator at " << _next_read << endl;
		#endif
	}

	_next_read = start + dur;

	while (_read_iter != end() && _read_iter->time() < start + dur) {
		assert(_read_iter->size() > 0);
		assert(_read_iter->buffer());
		dst.write(_read_iter->time() + offset,
		          _read_iter->event_type(),
		          _read_iter->size(), 
		          _read_iter->buffer());
		
		 #ifdef DEBUG_SEQUENCE
		 debugout << this << " read event type " << _read_iter->event_type()
			 << " @ " << _read_iter->time() << " : ";
		 for (size_t i = 0; i < _read_iter->size(); ++i)
			 debugout << hex << (int)_read_iter->buffer()[i];
		 debugout << endl;
		 #endif
		
		++_read_iter;
		++read_events;
	}

	return read_events;
}
#endif

/** Write the controller event pointed to by \a iter to \a ev.
 * The buffer of \a ev will be allocated or resized as necessary.
 * The event_type of \a ev should be set to the expected output type.
 * \return true on success
 */
template<typename Time>
bool
Sequence<Time>::control_to_midi_event(boost::shared_ptr< Event<Time> >& ev, const ControlIterator& iter) const
{
	assert(iter.list.get());
	const uint32_t event_type = iter.list->parameter().type();
	
	// initialize the event pointer with a new event, if necessary
	if (!ev) {
		ev = boost::shared_ptr< Event<Time> >(new Event<Time>(event_type, 0, 3, NULL, true));
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
template<typename Time>
void
Sequence<Time>::clear()
{
	_lock.writer_lock();
	_notes.clear();
	for (Controls::iterator li = _controls.begin(); li != _controls.end(); ++li)
		li->second->list()->clear();
//	_next_read = 0;
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
template<typename Time>
void
Sequence<Time>::start_write()
{
	#ifdef DEBUG_SEQUENCE
	debugout << this << " START WRITE, PERCUSSIVE = " << _percussive << endl;
	#endif
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
template<typename Time>
void
Sequence<Time>::end_write(bool delete_stuck)
{
	write_lock();
	assert(_writing);

	#ifdef DEBUG_SEQUENCE
	debugout << this << " END WRITE: " << _notes.size() << " NOTES\n";
	#endif

	if (!_percussive && delete_stuck) {
		for (typename Notes::iterator n = _notes.begin(); n != _notes.end() ;) {
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
			errorout << "WARNING: Sequence<Time>::end_write: Channel " << i << " has "
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
template<typename Time>
void
Sequence<Time>::append(const Event<Time>& event)
{
	write_lock();
	_edited = true;
	
	const MIDIEvent<Time>& ev = (const MIDIEvent<Time>&)event;

	assert(_notes.empty() || ev.time() >= _notes.back()->time());
	assert(_writing);

	if (ev.is_note_on()) {
		append_note_on_unlocked(ev.channel(), ev.time(), ev.note(), ev.velocity());
	} else if (ev.is_note_off()) {
		append_note_off_unlocked(ev.channel(), ev.time(), ev.note());
	} else if (ev.is_sysex()) {
		append_sysex_unlocked(ev);
	} else if (!_type_map.type_is_midi(ev.event_type())) {
		printf("WARNING: Sequence: Unknown event type %X: ", ev.event_type());
		for (size_t i=0; i < ev.size(); ++i) {
			printf("%X ", ev.buffer()[i]);
		}
		printf("\n");
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

template<typename Time>
void
Sequence<Time>::append_note_on_unlocked(uint8_t chan, Time time, uint8_t note_num, uint8_t velocity)
{
	#ifdef DEBUG_SEQUENCE
	debugout << this << " c=" << (int)chan << " note " << (int)note_num
		<< " on @ " << time << " v=" << (int)velocity << endl;
	#endif
	assert(note_num <= 127);
	assert(chan < 16);
	assert(_writing);
	_edited = true;

	if (velocity == 0) {
		append_note_off_unlocked(chan, time, note_num);
		return;
	}

	if (note_num < _lowest_note)
		_lowest_note = note_num;
	if (note_num > _highest_note)
		_highest_note = note_num;

	boost::shared_ptr< Note<Time> > new_note(new Note<Time>(chan, time, 0, note_num, velocity));
	_notes.push_back(new_note);
	if (!_percussive) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Sustained: Appending active note on " << (unsigned)(uint8_t)note_num
			<< " channel " << chan << endl;
		#endif
		_write_notes[chan].push_back(_notes.size() - 1);
	} else {
	 	#ifdef DEBUG_SEQUENCE
	 	debugout << "Percussive: NOT appending active note on" << endl;
		#endif
	 }
}

template<typename Time>
void
Sequence<Time>::append_note_off_unlocked(uint8_t chan, Time time, uint8_t note_num)
{
	#ifdef DEBUG_SEQUENCE
	debugout << this << " c=" << (int)chan << " note " << (int)note_num
		<< " off @ " << time << endl;
	#endif
	assert(note_num <= 127);
	assert(chan < 16);
	assert(_writing);
	_edited = true;

	if (_percussive) {
		#ifdef DEBUG_SEQUENCE
		debugout << "Sequence Ignoring note off (percussive mode)" << endl;
		#endif
		return;
	}

	/* FIXME: make _write_notes fixed size (127 noted) for speed */

	/* FIXME: note off velocity for that one guy out there who actually has
	 * keys that send it */

	bool resolved = false;

	for (WriteNotes::iterator n = _write_notes[chan].begin(); n
			!= _write_notes[chan].end(); ++n) {
		Note<Time>& note = *_notes[*n].get();
		if (note.note() == note_num) {
			assert(time >= note.time());
			note.set_length(time - note.time());
			_write_notes[chan].erase(n);
			#ifdef DEBUG_SEQUENCE
			debugout << "resolved note, length: " << note.length() << endl;
			#endif
			resolved = true;
			break;
		}
	}

	if (!resolved) {
		errorout << this << " spurious note off chan " << (int)chan
				<< ", note " << (int)note_num << " @ " << time << endl;
	}
}

template<typename Time>
void
Sequence<Time>::append_control_unlocked(const Parameter& param, Time time, double value)
{
	#ifdef DEBUG_SEQUENCE
	debugout << this << " " << _type_map.to_symbol(param) << " @ " << time << " \t= \t" << value
			<< " # controls: " << _controls.size() << endl;
	#endif
	boost::shared_ptr<Control> c = control(param, true);
	c->list()->rt_add(time, value);
}

template<typename Time>
void
Sequence<Time>::append_sysex_unlocked(const MIDIEvent<Time>& ev)
{
	#ifdef DEBUG_SEQUENCE
	debugout << this << " SysEx @ " << ev.time() << " \t= \t [ " << hex;
	for (size_t i=0; i < ev.size(); ++i) {
		debugout << int(ev.buffer()[i]) << " ";
	}
	debugout << "]" << endl;
	#endif

	boost::shared_ptr<MIDIEvent<Time> > event(new MIDIEvent<Time>(ev, true));
	_sysexes.push_back(event);
}

template<typename Time>
void
Sequence<Time>::add_note_unlocked(const boost::shared_ptr< Note<Time> > note)
{
	#ifdef DEBUG_SEQUENCE
	debugout << this << " add note " << (int)note->note() << " @ " << note->time() << endl;
	#endif
	_edited = true;
	typename Notes::iterator i = upper_bound(_notes.begin(), _notes.end(), note,
			note_time_comparator);
	_notes.insert(i, note);
}

template<typename Time>
void
Sequence<Time>::remove_note_unlocked(const boost::shared_ptr< const Note<Time> > note)
{
	_edited = true;
	#ifdef DEBUG_SEQUENCE
	debugout << this << " remove note " << (int)note->note() << " @ " << note->time() << endl;
	#endif
	for (typename Notes::iterator n = _notes.begin(); n != _notes.end(); ++n) {
		Note<Time>& _n = *(*n);
		const Note<Time>& _note = *note;
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
template<typename Time>
bool
Sequence<Time>::is_sorted() const {
	bool t = 0;
	for (typename Notes::const_iterator n = _notes.begin(); n != _notes.end(); ++n)
		if ((*n)->time() < t)
			return false;
		else
			t = (*n)->time();

	return true;
}
#endif

template class Sequence<double>;

} // namespace Evoral

