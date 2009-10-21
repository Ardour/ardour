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
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <cstdio>
#include "evoral/Control.hpp"
#include "evoral/ControlList.hpp"
#include "evoral/ControlSet.hpp"
#include "evoral/EventSink.hpp"
#include "evoral/MIDIParameters.hpp"
#include "evoral/Sequence.hpp"
#include "evoral/TypeMap.hpp"
#include "evoral/midi_util.h"

// #define DEBUG_SEQUENCE 1
#ifdef DEBUG_SEQUENCE
	#include <boost/format.hpp>
	using boost::format;
	#define DUMP(x) cerr << (x);
#else
	#define DUMP(x)
#endif

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


// Read iterator (const_iterator)

template<typename Time>
Sequence<Time>::const_iterator::const_iterator()
	: _seq(NULL)
	, _is_end(true)
	, _locked(false)
	, _control_iter(_control_iters.end())
{
	_event = boost::shared_ptr< Event<Time> >(new Event<Time>());
}

template<typename Time>
Sequence<Time>::const_iterator::const_iterator(const Sequence<Time>& seq, Time t)
	: _seq(&seq)
	, _type(NIL)
	, _is_end((t == DBL_MAX) || seq.empty())
	, _locked(!_is_end)
	, _note_iter(seq.notes().end())
	, _sysex_iter(seq.sysexes().end())
	, _control_iter(_control_iters.end())
{
	DUMP(format("Created Iterator @ %1% (is end: %2%)\n)") % t % _is_end);

	if (_is_end) {
		return;
	}

	seq.read_lock();

	// Find first note which begins at or after t
	_note_iter = seq.note_lower_bound(t);

	// Find first sysex event at or after t
	for (typename Sequence<Time>::SysExes::const_iterator i = seq.sysexes().begin();
			i != seq.sysexes().end(); ++i) {
		if ((*i)->time() >= t) {
			_sysex_iter = i;
			break;
		}
	}
	assert(_sysex_iter == seq.sysexes().end() || (*_sysex_iter)->time() >= t);

	// Find first control event after t
	ControlIterator earliest_control(boost::shared_ptr<ControlList>(), DBL_MAX, 0.0);
	_control_iters.reserve(seq._controls.size());
	bool   found                  = false;
	size_t earliest_control_index = 0;
	for (Controls::const_iterator i = seq._controls.begin(); i != seq._controls.end(); ++i) {
		DUMP(format("Iterator: control: %1%\n") % seq._type_map.to_symbol(i->first));
		double x, y;
		bool ret = i->second->list()->rt_safe_earliest_event_unlocked(t, DBL_MAX, x, y);
		if (!ret) {
			DUMP(format("Iterator: CC %1% (size %2%) has no events past %3%\n")
					% i->first.id() % i->second->list()->size() % t);
			continue;
		}

		assert(x >= 0);

		if (y < i->first.min() || y > i->first.max()) {
			cerr << "ERROR: Controller value " << y
				<< " out of range [" << i->first.min() << "," << i->first.max()
				<< "], event ignored" << endl;
			continue;
		}

		DUMP(format("Iterator: CC %1% added (%2%, %3%)\n") % i->first.id() % x % y);

		const ControlIterator new_iter(i->second->list(), x, y);
		_control_iters.push_back(new_iter);

		// Found a new earliest_control
		if (x < earliest_control.x) {
			earliest_control = new_iter;
			earliest_control_index = _control_iters.size() - 1;
			found = true;
		}
	}

	if (found) {
		_control_iter = _control_iters.begin() + earliest_control_index;
	} else {
		_control_iter = _control_iters.end();
	}

	// Now find the earliest event overall and point to it
	Time earliest_t = t;

	if (_note_iter != seq.notes().end()) {
		_type = NOTE_ON;
		earliest_t = (*_note_iter)->time();
	}

	if (_sysex_iter != seq.sysexes().end() && (*_sysex_iter)->time() < earliest_t) {
		_type = SYSEX;
		earliest_t = (*_sysex_iter)->time();
	}

	if (_control_iter != _control_iters.end()
			&& earliest_control.list && earliest_control.x >= t
			&& earliest_control.x < earliest_t) {
		_type = CONTROL;
		earliest_t = earliest_control.x;
	}

	switch (_type) {
	case NOTE_ON:
		DUMP(format("Starting at note on event @ %1%\n") % earliest_t);
		_event = boost::shared_ptr< Event<Time> >(
				new Event<Time>((*_note_iter)->on_event(), true));
		_active_notes.push(*_note_iter);
		break;
	case SYSEX:
		DUMP(format("Starting at sysex event @ %1%\n") % earliest_t);
		_event = boost::shared_ptr< Event<Time> >(
				new Event<Time>(*(*_sysex_iter), true));
		break;
	case CONTROL:
		DUMP(format("Starting at control event @ %1%\n") % earliest_t);
		seq.control_to_midi_event(_event, earliest_control);
		break;
	default:
		break;
	}

	if (_type == NIL || !_event || _event->size() == 0) {
		DUMP(format("Starting at end @ %1%\n") % t);
		_type   = NIL;
		_is_end = true;
		_locked = false;
		_seq->read_unlock();
	} else {
		DUMP(printf("New iterator = 0x%x : 0x%x @ %f\n",
			    (int)_event->event_type(),
			    (int)((MIDIEvent<Time>*)_event.get())->type(),
			    _event->time()));
		assert(midi_event_is_valid(_event->buffer(), _event->size()));
	}
}

template<typename Time>
Sequence<Time>::const_iterator::~const_iterator()
{
	if (_locked) {
		_seq->read_unlock();
	}
}

template<typename Time>
void
Sequence<Time>::const_iterator::invalidate()
{
	while (!_active_notes.empty()) {
		_active_notes.pop();
	}
	_type = NIL;
	_is_end = true;
	if (_seq) {
		_note_iter = _seq->notes().end();
		_sysex_iter = _seq->sysexes().end();
	}
	_control_iter = _control_iters.end();
	if (_locked) {
		_seq->read_unlock();
		_locked = false;
	}
}

template<typename Time>
const typename Sequence<Time>::const_iterator&
Sequence<Time>::const_iterator::operator++()
{
	if (_is_end) {
		throw std::logic_error("Attempt to iterate past end of Sequence");
	}

	DUMP("Sequence::const_iterator++\n");
	assert(_event && _event->buffer() && _event->size() > 0);

	const MIDIEvent<Time>& ev = *((MIDIEvent<Time>*)_event.get());

	if (!(     ev.is_note()
			|| ev.is_cc()
			|| ev.is_pgm_change()
			|| ev.is_pitch_bender()
			|| ev.is_channel_pressure()
			|| ev.is_sysex()) ) {
		cerr << "WARNING: Unknown event (type " << _type << "): " << hex
			<< int(ev.buffer()[0]) << int(ev.buffer()[1]) << int(ev.buffer()[2]) << endl;
	}

	double x   = 0.0;
	double y   = 0.0;
	bool   ret = false;

	// Increment past current event
	switch (_type) {
	case NOTE_ON:
		++_note_iter;
		break;
	case NOTE_OFF:
		break;
	case CONTROL:
		// Increment current controller iterator
		ret = _control_iter->list->rt_safe_earliest_event_unlocked(
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

		// Find the controller with the next earliest event time
		_control_iter = _control_iters.begin();
		for (ControlIterators::iterator i = _control_iters.begin();
				i != _control_iters.end(); ++i) {
			if (i->x < _control_iter->x) {
				_control_iter = i;
			}
		}
		break;
	case SYSEX:
		++_sysex_iter;
		break;
	default:
		assert(false);
	}

	// Now find the earliest event overall and point to it
	_type = NIL;
	Time earliest_t = std::numeric_limits<Time>::max();

	// Next earliest note on
	if (_note_iter != _seq->notes().end()) {
		_type = NOTE_ON;
		earliest_t = (*_note_iter)->time();
	}

	// Use the next note off iff it's earlier or the same time as the note on
	if (!_seq->percussive() && (!_active_notes.empty())) {
		if (_type == NIL || _active_notes.top()->end_time() <= earliest_t) {
			_type = NOTE_OFF;
			earliest_t = _active_notes.top()->end_time();
		}
	}

	// Use the next earliest controller iff it's earlier than the note event
	if (_control_iter != _control_iters.end() && _control_iter->x != DBL_MAX) {
		if (_type == NIL || _control_iter->x < earliest_t) {
			_type = CONTROL;
			earliest_t = _control_iter->x;
		}
	}

	// Use the next earliest SysEx iff it's earlier than the controller
	if (_sysex_iter != _seq->sysexes().end()) {
		if (_type == NIL || (*_sysex_iter)->time() < earliest_t) {
			_type = SYSEX;
			earliest_t = (*_sysex_iter)->time();
		}
	}

	// Set event to reflect new position
	switch (_type) {
	case NOTE_ON:
		DUMP("iterator = note on\n");
		*_event = (*_note_iter)->on_event();
		_active_notes.push(*_note_iter);
		break;
	case NOTE_OFF:
		DUMP("iterator = note off\n");
		assert(!_active_notes.empty());
		*_event = _active_notes.top()->off_event();
		_active_notes.pop();
		break;
	case CONTROL:
		DUMP("iterator = control\n");
		_seq->control_to_midi_event(_event, *_control_iter);
		break;
	case SYSEX:
		DUMP("iterator = sysex\n");
		*_event = *(*_sysex_iter);
		break;
	default:
		DUMP("iterator = end\n");
		_is_end = true;
	}

	assert(_is_end || (_event->size() > 0 && _event->buffer() && _event->buffer()[0] != '\0'));

	return *this;
}

template<typename Time>
bool
Sequence<Time>::const_iterator::operator==(const const_iterator& other) const
{
	if (_seq != other._seq) {
		return false;
	} else if (_is_end || other._is_end) {
		return (_is_end == other._is_end);
	} else if (_type != other._type) {
		return false;
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
	} else if (!_locked && other._locked) {
		_seq->read_lock();
	}

	_seq           = other._seq;
	_event         = other._event;
	_active_notes  = other._active_notes;
	_type          = other._type;
	_is_end        = other._is_end;
	_locked        = other._locked;
	_note_iter     = other._note_iter;
	_sysex_iter    = other._sysex_iter;
	_control_iters = other._control_iters;

	if (other._control_iter == other._control_iters.end()) {
		_control_iter = _control_iters.end();
	} else {
		const size_t index = other._control_iter - other._control_iters.begin();
		_control_iter  = _control_iters.begin() + index;
	}

	return *this;
}

// Sequence

template<typename Time>
Sequence<Time>::Sequence(const TypeMap& type_map)
	: _edited(false)
	, _type_map(type_map)
	, _writing(false)
	, _end_iter(*this, DBL_MAX)
	, _percussive(false)
	, _lowest_note(127)
	, _highest_note(0)
{
	DUMP(format("Sequence (size %1%) constructed: %2%\n") % size % this);
	assert(_end_iter._is_end);
	assert( ! _end_iter._locked);
}

/** Write the controller event pointed to by \a iter to \a ev.
 * The buffer of \a ev will be allocated or resized as necessary.
 * The event_type of \a ev should be set to the expected output type.
 * \return true on success
 */
template<typename Time>
bool
Sequence<Time>::control_to_midi_event(
		boost::shared_ptr< Event<Time> >& ev,
		const ControlIterator&            iter) const
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
	DUMP(format("%1% : start_write (percussive = %2%)\n") % this % _percussive);
	write_lock();
	_writing = true;
	for (int i = 0; i < 16; ++i) {
		_write_notes[i].clear();
	}
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

	if (!_writing) {
		write_unlock();
		return;
	}

	DUMP(format("%1% : end_write (%2% notes)\n") % this % _notes.size());

	if (!_percussive && delete_stuck) {
		for (typename Notes::iterator n = _notes.begin(); n != _notes.end() ;) {
			typename Notes::iterator next = n;
			++next;
			if ((*n)->length() == 0) {
				cerr << "WARNING: Stuck note lost: " << (*n)->note() << endl;
				_notes.erase(n);
			}
			n = next;
		}
	}

	for (int i = 0; i < 16; ++i) {
		if (!_write_notes[i].empty()) {
			cerr << "WARNING: Sequence<Time>::end_write: Channel " << i << " has "
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

	assert(_notes.empty() || ev.time() >= (*_notes.rbegin())->time());
	assert(_writing);

	if (!midi_event_is_valid(ev.buffer(), ev.size())) {
		cerr << "WARNING: Sequence ignoring illegal MIDI event" << endl;
		write_unlock();
		return;
	}

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
	DUMP(format("%1% c=%2% note %3% on @ %4% v=%5%\n")
			% this % (int)chan % (int)note_num % time % (int)velocity);
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
	_notes.insert(new_note);
	if (!_percussive) {
		DUMP(format("Sustained: Appending active note on %1% channel %2%\n")
				% (unsigned)(uint8_t)note_num % chan);
		_write_notes[chan].insert(new_note);
	} else {
		DUMP("Percussive: NOT appending active note on\n");
	}
}

template<typename Time>
void
Sequence<Time>::append_note_off_unlocked(uint8_t chan, Time time, uint8_t note_num)
{
	DUMP(format("%1% c=%2% note %3% off @ %4%\n")
			% this % (int)chan % (int)note_num % time);
	assert(note_num <= 127);
	assert(chan < 16);
	assert(_writing);
	_edited = true;

	if (_percussive) {
		DUMP("Sequence Ignoring note off (percussive mode)\n");
		return;
	}

	// TODO: support note off velocity

	bool resolved = false;
	for (typename WriteNotes::iterator n = _write_notes[chan].begin();
			n != _write_notes[chan].end(); ++n) {
		boost::shared_ptr< Note<Time> > note = *n;
		if (note->note() == note_num) {
			assert(time >= note->time());
			note->set_length(time - note->time());
			_write_notes[chan].erase(n);
			DUMP(format("resolved note, length: %1%\n") % note.length());
			resolved = true;
			break;
		}
	}

	if (!resolved) {
		cerr << this << " spurious note off chan " << (int)chan
				<< ", note " << (int)note_num << " @ " << time << endl;
	}
}

template<typename Time>
void
Sequence<Time>::append_control_unlocked(const Parameter& param, Time time, double value)
{
	DUMP(format("%1% %2% @ %3%\t=\t%4% # controls: %5%\n")
			% this % _type_map.to_symbol(param) % time % value % _controls.size());
	boost::shared_ptr<Control> c = control(param, true);
	c->list()->rt_add(time, value);
}

template<typename Time>
void
Sequence<Time>::append_sysex_unlocked(const MIDIEvent<Time>& ev)
{
	#ifdef DEBUG_SEQUENCE
	cerr << this << " SysEx @ " << ev.time() << " \t= \t [ " << hex;
	for (size_t i=0; i < ev.size(); ++i) {
		cerr << int(ev.buffer()[i]) << " ";
	} cerr << "]" << endl;
	#endif

	boost::shared_ptr<MIDIEvent<Time> > event(new MIDIEvent<Time>(ev, true));
	_sysexes.push_back(event);
}

template<typename Time>
void
Sequence<Time>::add_note_unlocked(const boost::shared_ptr< Note<Time> > note)
{
	DUMP(format("%1% add note %2% @ %3%\n") % this % (int)note->note() % note->time());
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
	DUMP(format("%1% remove note %2% @ %3%\n") % this % (int)note->note() % note->time());
	for (typename Notes::iterator n = _notes.begin(); n != _notes.end(); ++n) {
		if (*(*n) == *note) {
			_notes.erase(n);
			break;
		}
	}
}

template<typename Time>
void
Sequence<Time>::set_notes (const Sequence<Time>::Notes& n)
{
	_notes = n;
}

/** Return the earliest note with time >= t */
template<typename Time>
typename Sequence<Time>::Notes::const_iterator
Sequence<Time>::note_lower_bound (Time t) const
{
	boost::shared_ptr< Note<Time> > search_note(new Note<Time>(0, t, 0, 0, 0));
	typename Sequence<Time>::Notes::const_iterator i = _notes.lower_bound(search_note);
	assert(i == _notes.end() || (*i)->time() >= t);
	return i;
}

template class Sequence<Evoral::MusicalTime>;

} // namespace Evoral

