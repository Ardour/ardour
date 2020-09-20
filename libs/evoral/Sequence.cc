/*
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <cstdio>

#if __clang__
#include "evoral/Note.h"
#endif

#include "pbd/compose.h"
#include "pbd/error.h"

#include "temporal/beats.h"

#include "evoral/Control.h"
#include "evoral/ControlList.h"
#include "evoral/ControlSet.h"
#include "evoral/EventSink.h"
#include "evoral/ParameterDescriptor.h"
#include "evoral/Sequence.h"
#include "evoral/TypeMap.h"
#include "evoral/midi_util.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;

/** Minimum time between MIDI outputs from a single interpolated controller,
    expressed in beats.  This is to limit the rate at which MIDI messages
    are generated.  It is only applied to interpolated controllers.

    XXX: This is a hack.  The time should probably be expressed in
    seconds rather than beats, and should be configurable etc. etc.
*/
static Temporal::Beats const time_between_interpolated_controller_outputs = Temporal::Beats::ticks (256);

namespace Evoral {

// Read iterator (const_iterator)

template<typename Time>
Sequence<Time>::const_iterator::const_iterator()
	: _seq(NULL)
	, _event(boost::shared_ptr< Event<Time> >(new Event<Time>()))
	, _active_patch_change_message (NO_EVENT)
	, _type(NIL)
	, _is_end(true)
	, _control_iter(_control_iters.end())
	, _force_discrete(false)
{
}

/** @param force_discrete true to force ControlLists to use discrete evaluation, otherwise false to get them to use their configured mode */
template<typename Time>
Sequence<Time>::const_iterator::const_iterator(const Sequence<Time>&               seq,
                                               Time                                t,
                                               bool                                force_discrete,
                                               const std::set<Evoral::Parameter>&  filtered,
                                               std::set<WeakNotePtr> const *                 active_notes)
	: _seq(&seq)
	, _active_patch_change_message (0)
	, _type(NIL)
	, _is_end((t == DBL_MAX) || seq.empty())
	, _note_iter(seq.notes().end())
	, _sysex_iter(seq.sysexes().end())
	, _patch_change_iter(seq.patch_changes().end())
	, _control_iter(_control_iters.end())
	, _force_discrete (force_discrete)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("Created Iterator @ %1 (is end: %2)\n)", t, _is_end));

	if (_is_end) {
		return;
	}

	_lock = seq.read_lock();

	// Add currently active notes, if given
	if (active_notes) {
		for (typename std::set<WeakNotePtr>::const_iterator i = active_notes->begin(); i != active_notes->end(); ++i) {
			NotePtr note = i->lock();
			if (note && note->time() <= t && note->end_time() > t) {
				_active_notes.push(note);
			}
		}
	}

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

	// Find first patch event at or after t
	for (typename Sequence<Time>::PatchChanges::const_iterator i = seq.patch_changes().begin(); i != seq.patch_changes().end(); ++i) {
		if ((*i)->time() >= t) {
			_patch_change_iter = i;
			break;
		}
	}
	assert (_patch_change_iter == seq.patch_changes().end() || (*_patch_change_iter)->time() >= t);

	// Find first control event after t
	_control_iters.reserve(seq._controls.size());
	bool   found                  = false;
	size_t earliest_control_index = 0;
	Temporal::timepos_t earliest_control_x = std::numeric_limits<Temporal::timepos_t>::max();

	for (Controls::const_iterator i = seq._controls.begin(); i != seq._controls.end(); ++i) {

		if (filtered.find (i->first) != filtered.end()) {
			/* this parameter is filtered, so don't bother setting up an iterator for it */
			continue;
		}

		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Iterator: control: %1\n", seq._type_map.to_symbol(i->first)));
		Temporal::timepos_t xtime (Temporal::AudioTime); /* domain may change */
		double y;
		bool ret;
		if (_force_discrete || i->second->list()->interpolation() == ControlList::Discrete) {
			ret = i->second->list()->rt_safe_earliest_event_discrete_unlocked (Temporal::timepos_t (t), xtime, y, true);
		} else {
			ret = i->second->list()->rt_safe_earliest_event_linear_unlocked(Temporal::timepos_t (t), xtime, y, true);
		}

		if (!ret) {
			DEBUG_TRACE (DEBUG::Sequence, string_compose ("Iterator: CC %1 (size %2) has no events past %3\n",
			                                              i->first.id(), i->second->list()->size(), t));
			continue;
		}

		const ParameterDescriptor& desc = seq.type_map().descriptor(i->first);
		if (y < desc.lower || y > desc.upper) {
			cerr << "ERROR: Controller value " << y
			     << " out of range [" << desc.lower << "," << desc.upper
			     << "], event ignored" << endl;
			continue;
		}

		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Iterator: CC %1 added (%2, %3)\n", i->first.id(), xtime, y));

		const ControlIterator new_iter(i->second->list(), xtime, y);
		_control_iters.push_back(new_iter);

		// Found a new earliest_control
		if (xtime < earliest_control_x) {
			earliest_control_x     = xtime;
			earliest_control_index = _control_iters.size() - 1;
			found                  = true;
		}
	}

	if (found) {
		_control_iter = _control_iters.begin() + earliest_control_index;
		assert(_control_iter != _control_iters.end());
		assert(_control_iter->list);
	} else {
		_control_iter = _control_iters.end();
	}

	// Choose the earliest event overall to point to
	choose_next(t);

	// Allocate a new event for storing the current event in MIDI format
	_event = boost::shared_ptr< Event<Time> >(
		new Event<Time>(NO_EVENT, Time(), 4, NULL, true));

	// Set event from chosen sub-iterator
	set_event();

	if (_is_end) {
		DEBUG_TRACE(DEBUG::Sequence,
		            string_compose("Starting at end @ %1\n", t));
	} else {
		DEBUG_TRACE(DEBUG::Sequence,
		            string_compose("Starting at type 0x%1 : 0x%2 @ %3\n",
		                           (int)_event->event_type(),
		                           (int)_event->buffer()[0],
		                           _event->time()));
	}
}

template<typename Time>
void
Sequence<Time>::const_iterator::get_active_notes (std::set<WeakNotePtr>& active_notes) const
{
	/* can't iterate over a std::priority_queue<> such as ActiveNotes */
	ActiveNotes copy (_active_notes);
	while (!copy.empty()) {
		active_notes.insert (copy.top());
		copy.pop ();
	}
}

template<typename Time>
void
Sequence<Time>::const_iterator::invalidate(bool preserve_active_notes)
{
	if (!preserve_active_notes) {
		_active_notes = ActiveNotes();
	}
	_type = NIL;
	_is_end = true;
	if (_seq) {
		_note_iter = _seq->notes().end();
		_sysex_iter = _seq->sysexes().end();
		_patch_change_iter = _seq->patch_changes().end();
		_active_patch_change_message = 0;
	}
	_control_iters.clear();
	_control_iter = _control_iters.end();
	_lock.reset();
}

template<typename Time>
Time
Sequence<Time>::const_iterator::choose_next(Time earliest_t)
{
	_type = NIL;

	// Next earliest note on, if any
	if (_note_iter != _seq->notes().end()) {
		_type      = NOTE_ON;
		earliest_t = (*_note_iter)->time();
	}

	/* Use the next earliest patch change iff it is earlier or coincident with the note-on.
	 * A patch-change with the same time-stamp applies to the concurrent note-on */
	if (_patch_change_iter != _seq->patch_changes().end()) {
		if (_type == NIL || (*_patch_change_iter)->time() <= earliest_t) {
			_type      = PATCH_CHANGE;
			earliest_t = (*_patch_change_iter)->time();
		}
	}

	/* Use the next earliest controller iff it's earlier or coincident with the note-on
	 * or patch-change. Bank-select (CC0, CC32) needs to be sent before the PGM. */
	if (_control_iter != _control_iters.end() &&
	    _control_iter->list && _control_iter->x != std::numeric_limits<Temporal::timepos_t>::max()) {
		if (_type == NIL || _control_iter->x <= earliest_t) {
			_type      = CONTROL;
			earliest_t = _control_iter->x.beats();
		}
	}

	/* .. but prefer to send any Note-off first */
	if ((!_active_notes.empty())) {
		if (_type == NIL || _active_notes.top()->end_time() <= earliest_t) {
			_type      = NOTE_OFF;
			earliest_t = _active_notes.top()->end_time();
		}
	}

	/* SysEx is last, always sent after any other concurrent 3 byte event */
	if (_sysex_iter != _seq->sysexes().end()) {
		if (_type == NIL || (*_sysex_iter)->time() < earliest_t) {
			_type      = SYSEX;
			earliest_t = (*_sysex_iter)->time();
		}
	}

	return earliest_t;
}

template<typename Time>
void
Sequence<Time>::const_iterator::set_event()
{
	switch (_type) {
	case NOTE_ON:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = note on\n");
		_event->assign ((*_note_iter)->on_event());
		_active_notes.push(*_note_iter);
		break;
	case NOTE_OFF:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = note off\n");
		assert(!_active_notes.empty());
		_event->assign (_active_notes.top()->off_event());
		// We don't pop the active note until we increment past it
		break;
	case SYSEX:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = sysex\n");
		_event->assign (*(*_sysex_iter));
		break;
	case CONTROL:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = control\n");
		_seq->control_to_midi_event(_event, *_control_iter);
		break;
	case PATCH_CHANGE:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = program change\n");
		_event->assign ((*_patch_change_iter)->message (_active_patch_change_message));
		break;
	default:
		_is_end = true;
		break;
	}

	if (_type == NIL || !_event || _event->size() == 0) {
		DEBUG_TRACE(DEBUG::Sequence, "iterator = end\n");
		_type   = NIL;
		_is_end = true;
	} else {
		assert(midi_event_is_valid(_event->buffer(), _event->size()));
	}
}

template<typename Time>
const typename Sequence<Time>::const_iterator&
Sequence<Time>::const_iterator::operator++()
{
	if (_is_end) {
		throw std::logic_error("Attempt to iterate past end of Sequence");
	}

	assert(_event && _event->buffer() && _event->size() > 0);

	const Event<Time>& ev = *_event.get();

	if (!(     ev.is_note()
	           || ev.is_cc()
	           || ev.is_pgm_change()
	           || ev.is_pitch_bender()
	           || ev.is_channel_pressure()
	           || ev.is_poly_pressure()
	           || ev.is_sysex()) ) {
		cerr << "WARNING: Unknown event (type " << _type << "): " << hex
		     << int(ev.buffer()[0]) << int(ev.buffer()[1]) << int(ev.buffer()[2]) << endl;
	}

	Temporal::timepos_t x (Temporal::AudioTime);
	Temporal::timepos_t xtime (Temporal::AudioTime);
	double    y   = 0.0;
	bool      ret = false;

	// Increment past current event
	switch (_type) {
	case NOTE_ON:
		++_note_iter;
		break;
	case NOTE_OFF:
		_active_notes.pop();
		break;
	case CONTROL:
		// Increment current controller iterator
		if (_force_discrete || _control_iter->list->interpolation() == ControlList::Discrete) {
			ret = _control_iter->list->rt_safe_earliest_event_discrete_unlocked (_control_iter->x, xtime, y, false);
		} else {
			ret = _control_iter->list->rt_safe_earliest_event_linear_unlocked (
				_control_iter->x, xtime, y, false, Temporal::timecnt_t::from_ticks (time_between_interpolated_controller_outputs));
		}
		assert(!ret || x > _control_iter->x);
		if (ret) {
			_control_iter->x = xtime;
			_control_iter->y = y;
		} else {
			_control_iter->list.reset();
			_control_iter->x = std::numeric_limits<Time>::max();
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
	case PATCH_CHANGE:
		++_active_patch_change_message;
		if (_active_patch_change_message == (*_patch_change_iter)->messages()) {
			++_patch_change_iter;
			_active_patch_change_message = 0;
		}
		break;
	default:
		assert(false);
	}

	// Choose the earliest event overall to point to
	choose_next(std::numeric_limits<Time>::max());

	// Set event from chosen sub-iterator
	set_event();

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
	_seq           = other._seq;
	_event         = other._event;
	_active_notes  = other._active_notes;
	_type          = other._type;
	_is_end        = other._is_end;
	_note_iter     = other._note_iter;
	_sysex_iter    = other._sysex_iter;
	_patch_change_iter = other._patch_change_iter;
	_control_iters = other._control_iters;
	_force_discrete = other._force_discrete;
	_active_patch_change_message = other._active_patch_change_message;

	if (other._lock) {
		_lock = _seq->read_lock();
	} else {
		_lock.reset();
	}

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
	, _overlapping_pitches_accepted (true)
	, _overlap_pitch_resolution (FirstOnFirstOff)
	, _writing(false)
	, _type_map(type_map)
	, _end_iter(*this, std::numeric_limits<Time>::max(), false, std::set<Evoral::Parameter> ())
	, _percussive(false)
	, _lowest_note(127)
	, _highest_note(0)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("Sequence constructed: %1\n", this));
	assert(_end_iter._is_end);
	assert( ! _end_iter._lock);

	for (int i = 0; i < 16; ++i) {
		_bank[i] = 0;
	}
}

template<typename Time>
Sequence<Time>::Sequence(const Sequence<Time>& other)
	: ControlSet (other)
	, _edited(false)
	, _overlapping_pitches_accepted (other._overlapping_pitches_accepted)
	, _overlap_pitch_resolution (other._overlap_pitch_resolution)
	, _writing(false)
	, _type_map(other._type_map)
	, _end_iter(*this, std::numeric_limits<Time>::max(), false, std::set<Evoral::Parameter> ())
	, _percussive(other._percussive)
	, _lowest_note(other._lowest_note)
	, _highest_note(other._highest_note)
{
	for (typename Notes::const_iterator i = other._notes.begin(); i != other._notes.end(); ++i) {
		NotePtr n (new Note<Time> (**i));
		_notes.insert (n);
	}

	for (typename SysExes::const_iterator i = other._sysexes.begin(); i != other._sysexes.end(); ++i) {
		boost::shared_ptr<Event<Time> > n (new Event<Time> (**i, true));
		_sysexes.insert (n);
	}

	for (typename PatchChanges::const_iterator i = other._patch_changes.begin(); i != other._patch_changes.end(); ++i) {
		PatchChangePtr n (new PatchChange<Time> (**i));
		_patch_changes.insert (n);
	}

	for (int i = 0; i < 16; ++i) {
		_bank[i] = other._bank[i];
	}

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("Sequence copied: %1\n", this));
	assert(_end_iter._is_end);
	assert(! _end_iter._lock);
}

/** Write the controller event pointed to by `iter` to `ev`.
 * The buffer of `ev` will be allocated or resized as necessary.
 * \return true on success
 */
template<typename Time>
bool
Sequence<Time>::control_to_midi_event(
	boost::shared_ptr< Event<Time> >& ev,
	const ControlIterator&            iter) const
{
	assert(iter.list.get());

	// initialize the event pointer with a new event, if necessary
	if (!ev) {
		ev = boost::shared_ptr< Event<Time> >(new Event<Time>(NO_EVENT, Time(), 3, NULL, true));
	}

	const uint8_t midi_type = _type_map.parameter_midi_type(iter.list->parameter());
	ev->set_event_type(MIDI_EVENT);
	ev->set_id(-1);
	switch (midi_type) {
	case MIDI_CMD_CONTROL:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.list->parameter().id() <= INT8_MAX);
		assert(iter.y <= INT8_MAX);

		ev->set_time(iter.x.beats());
		ev->realloc(3);
		ev->buffer()[0] = MIDI_CMD_CONTROL + iter.list->parameter().channel();
		ev->buffer()[1] = (uint8_t)iter.list->parameter().id();
		ev->buffer()[2] = (uint8_t)iter.y;
		break;

	case MIDI_CMD_PGM_CHANGE:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.y <= INT8_MAX);

		ev->set_time(iter.x.beats());
		ev->realloc(2);
		ev->buffer()[0] = MIDI_CMD_PGM_CHANGE + iter.list->parameter().channel();
		ev->buffer()[1] = (uint8_t)iter.y;
		break;

	case MIDI_CMD_BENDER:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.y < (1<<14));

		ev->set_time(iter.x.beats());
		ev->realloc(3);
		ev->buffer()[0] = MIDI_CMD_BENDER + iter.list->parameter().channel();
		ev->buffer()[1] = uint16_t(iter.y) & 0x7F; // LSB
		ev->buffer()[2] = (uint16_t(iter.y) >> 7) & 0x7F; // MSB
		break;

	case MIDI_CMD_NOTE_PRESSURE:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.list->parameter().id() <= INT8_MAX);
		assert(iter.y <= INT8_MAX);

		ev->set_time(iter.x.beats());
		ev->realloc(3);
		ev->buffer()[0] = MIDI_CMD_NOTE_PRESSURE + iter.list->parameter().channel();
		ev->buffer()[1] = (uint8_t)iter.list->parameter().id();
		ev->buffer()[2] = (uint8_t)iter.y;
		break;

	case MIDI_CMD_CHANNEL_PRESSURE:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.y <= INT8_MAX);

		ev->set_time(iter.x.beats());
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
	WriteLock lock(write_lock());
	_notes.clear();
	for (Controls::iterator li = _controls.begin(); li != _controls.end(); ++li)
		li->second->list()->clear();
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
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 : start_write (percussive = %2)\n", this, _percussive));
	WriteLock lock(write_lock());
	_writing = true;
	for (int i = 0; i < 16; ++i) {
		_write_notes[i].clear();
	}
}

/** Finish a write of events to the model.
 *
 * If \a delete_stuck is true and the current mode is Sustained, note on events
 * that were never resolved with a corresonding note off will be deleted.
 * Otherwise they will remain as notes with length 0.
 */
template<typename Time>
void
Sequence<Time>::end_write (StuckNoteOption option, Time when)
{
	WriteLock lock(write_lock());

	if (!_writing) {
		return;
	}

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 : end_write (%2 notes) delete stuck option %3 @ %4\n", this, _notes.size(), option, when));

	for (typename Notes::iterator n = _notes.begin(); n != _notes.end() ;) {
		typename Notes::iterator next = n;
		++next;

		if ((*n)->end_time() == std::numeric_limits<Temporal::Beats>::max()) {
			switch (option) {
			case Relax:
				break;
			case DeleteStuckNotes:
				cerr << "WARNING: Stuck note lost (end was " << when << "): " << (**n) << endl;
				_notes.erase(n);
				break;
			case ResolveStuckNotes:
				if (when <= (*n)->time()) {
					cerr << "WARNING: Stuck note resolution - end time @ "
					     << when << " is before note on: " << (**n) << endl;
					_notes.erase (n);
				} else {
					(*n)->set_length (when - (*n)->time());
					cerr << "WARNING: resolved note-on with no note-off to generate " << (**n) << endl;
				}
				break;
			}
		}

		n = next;
	}

	for (int i = 0; i < 16; ++i) {
		_write_notes[i].clear();
	}

	_writing = false;
}


template<typename Time>
bool
Sequence<Time>::add_note_unlocked(const NotePtr note, void* arg)
{
	/* This is the core method to add notes to a Sequence
	 */

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 add note %2 @ %3 dur %4\n", this, (int)note->note(), note->time(), note->length()));

	if (resolve_overlaps_unlocked (note, arg)) {
		DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 DISALLOWED: note %2 @ %3\n", this, (int)note->note(), note->time()));
		return false;
	}

	if (note->id() < 0) {
		note->set_id (Evoral::next_event_id());
	}

	if (note->note() < _lowest_note)
		_lowest_note = note->note();
	if (note->note() > _highest_note)
		_highest_note = note->note();

	_notes.insert (note);
	_pitches[note->channel()].insert (note);

	_edited = true;

	return true;
}

template<typename Time>
void
Sequence<Time>::remove_note_unlocked(const constNotePtr note)
{
	bool erased = false;
	bool id_matched = false;

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 remove note #%2 %3 @ %4\n", this, note->id(), (int)note->note(), note->time()));

	/* first try searching for the note using the time index, which is
	 * faster since the container is "indexed" by time. (technically, this
	 * means that lower_bound() can do a binary search rather than linear)
	 *
	 * this may not work, for reasons explained below.
	 */

	typename Sequence<Time>::Notes::iterator i;

	for (i = note_lower_bound(note->time()); i != _notes.end() && (*i)->time() == note->time(); ++i) {

		if (*i == note) {

			DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1\terasing note #%2 %3 @ %4\n", this, (*i)->id(), (int)(*i)->note(), (*i)->time()));
			_notes.erase (i);

			if (note->note() == _lowest_note || note->note() == _highest_note) {

				_lowest_note = 127;
				_highest_note = 0;

				for (typename Sequence<Time>::Notes::iterator ii = _notes.begin(); ii != _notes.end(); ++ii) {
					if ((*ii)->note() < _lowest_note)
						_lowest_note = (*ii)->note();
					if ((*ii)->note() > _highest_note)
						_highest_note = (*ii)->note();
				}
			}

			erased = true;
			break;
		}
	}

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1\ttime-based lookup did not find note #%2 %3 @ %4\n", this, note->id(), (int)note->note(), note->time()));

	if (!erased) {

		/* if the note's time property was changed in tandem with some
		 * other property as the next operation after it was added to
		 * the sequence, then at the point where we call this to undo
		 * the add, the note we are targetting currently has a
		 * different time property than the one we we passed via
		 * the argument.
		 *
		 * in this scenario, we have no choice other than to linear
		 * search the list of notes and find the note by ID.
		 */

		for (i = _notes.begin(); i != _notes.end(); ++i) {

			if ((*i)->id() == note->id()) {

				DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1\tID-based pass, erasing note #%2 %3 @ %4\n", this, (*i)->id(), (int)(*i)->note(), (*i)->time()));
				_notes.erase (i);

				if (note->note() == _lowest_note || note->note() == _highest_note) {

					_lowest_note = 127;
					_highest_note = 0;

					for (typename Sequence<Time>::Notes::iterator ii = _notes.begin(); ii != _notes.end(); ++ii) {
						if ((*ii)->note() < _lowest_note)
							_lowest_note = (*ii)->note();
						if ((*ii)->note() > _highest_note)
							_highest_note = (*ii)->note();
					}
				}

				erased = true;
				id_matched = true;
				break;
			}
		}
	}

	if (erased) {

		Pitches& p (pitches (note->channel()));

		typename Pitches::iterator j;

		/* if we had to ID-match above, we can't expect to find it in
		 * pitches via note comparison either. so do another linear
		 * search to locate it. otherwise, we can use the note index
		 * to potentially speed things up.
		 */

		if (id_matched) {

			for (j = p.begin(); j != p.end(); ++j) {
				if ((*j)->id() == note->id()) {
					p.erase (j);
					break;
				}
			}

		} else {

			/* Now find the same note in the "pitches" list (which indexes
			 * notes by channel+time. We care only about its note number
			 * so the search_note has all other properties unset.
			 */

			NotePtr search_note (new Note<Time>(0, Time(), Time(), note->note(), 0));

			for (j = p.lower_bound (search_note); j != p.end() && (*j)->note() == note->note(); ++j) {

				if ((*j) == note) {
					DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1\terasing pitch %2 @ %3\n", this, (int)(*j)->note(), (*j)->time()));
					p.erase (j);
					break;
				}
			}
		}

		if (j == p.end()) {
			warning << string_compose ("erased note %1 not found in pitches for channel %2", *note, (int) note->channel()) << endmsg;
		}

		_edited = true;

	} else {
		cerr << "Unable to find note to erase matching " << *note.get() << endmsg;
	}
}

template<typename Time>
void
Sequence<Time>::remove_patch_change_unlocked (const constPatchChangePtr p)
{
	typename Sequence<Time>::PatchChanges::iterator i = patch_change_lower_bound (p->time ());

	while (i != _patch_changes.end() && ((*i)->time() == p->time())) {

		typename Sequence<Time>::PatchChanges::iterator tmp = i;
		++tmp;

		if (**i == *p) {
			_patch_changes.erase (i);
		}

		i = tmp;
	}
}

template<typename Time>
void
Sequence<Time>::remove_sysex_unlocked (const SysExPtr sysex)
{
	typename Sequence<Time>::SysExes::iterator i = sysex_lower_bound (sysex->time ());
	while (i != _sysexes.end() && (*i)->time() == sysex->time()) {

		typename Sequence<Time>::SysExes::iterator tmp = i;
		++tmp;

		if (*i == sysex) {
			_sysexes.erase (i);
		}

		i = tmp;
	}
}

/** Append \a ev to model.  NOT realtime safe.
 *
 * The timestamp of event is expected to be relative to
 * the start of this model (t=0) and MUST be monotonically increasing
 * and MUST be >= the latest event currently in the model.
 */
template<typename Time>
void
Sequence<Time>::append(const Event<Time>& ev, event_id_t evid)
{
	WriteLock lock(write_lock());

	assert(_notes.empty() || ev.time() >= (*_notes.rbegin())->time());
	assert(_writing);

	if (!midi_event_is_valid(ev.buffer(), ev.size())) {
		cerr << "WARNING: Sequence ignoring illegal MIDI event" << endl;
		return;
	}

	if (ev.is_note_on() && ev.velocity() > 0) {
		append_note_on_unlocked (ev, evid);
	} else if (ev.is_note_off() || (ev.is_note_on() && ev.velocity() == 0)) {
		/* XXX note: event ID is discarded because we merge the on+off events into
		   a single note object
		*/
		append_note_off_unlocked (ev);
	} else if (ev.is_sysex()) {
		append_sysex_unlocked(ev, evid);
	} else if (ev.is_cc() && (ev.cc_number() == MIDI_CTL_MSB_BANK || ev.cc_number() == MIDI_CTL_LSB_BANK)) {
		/* note bank numbers in our _bank[] array, so that we can write an event when the program change arrives */
		if (ev.cc_number() == MIDI_CTL_MSB_BANK) {
			_bank[ev.channel()] &= ~(0x7f << 7);
			_bank[ev.channel()] |= ev.cc_value() << 7;
		} else {
			_bank[ev.channel()] &= ~0x7f;
			_bank[ev.channel()] |= ev.cc_value();
		}
	} else if (ev.is_cc()) {
		const ParameterType ptype = _type_map.midi_parameter_type(ev.buffer(), ev.size());
		append_control_unlocked(
			Parameter(ptype, ev.channel(), ev.cc_number()),
			ev.time(), ev.cc_value(), evid);
	} else if (ev.is_pgm_change()) {
		/* write a patch change with this program change and any previously set-up bank number */
		append_patch_change_unlocked (
			PatchChange<Time> (ev.time(), ev.channel(),
			                   ev.pgm_number(), _bank[ev.channel()]), evid);
	} else if (ev.is_pitch_bender()) {
		const ParameterType ptype = _type_map.midi_parameter_type(ev.buffer(), ev.size());
		append_control_unlocked(
			Parameter(ptype, ev.channel()),
			ev.time(), double ((0x7F & ev.pitch_bender_msb()) << 7
			                   | (0x7F & ev.pitch_bender_lsb())),
			evid);
	} else if (ev.is_poly_pressure()) {
		const ParameterType ptype = _type_map.midi_parameter_type(ev.buffer(), ev.size());
		append_control_unlocked (Parameter (ptype, ev.channel(), ev.poly_note()), ev.time(), ev.poly_pressure(), evid);
	} else if (ev.is_channel_pressure()) {
		const ParameterType ptype = _type_map.midi_parameter_type(ev.buffer(), ev.size());
		append_control_unlocked(
			Parameter(ptype, ev.channel()),
			ev.time(), ev.channel_pressure(), evid);
	} else if (!_type_map.type_is_midi(ev.event_type())) {
		printf("WARNING: Sequence: Unknown event type %X: ", ev.event_type());
		for (size_t i=0; i < ev.size(); ++i) {
			printf("%X ", ev.buffer()[i]);
		}
		printf("\n");
	} else {
		printf("WARNING: Sequence: Unknown MIDI event type %X\n", ev.type());
	}

	_edited = true;
}

template<typename Time>
void
Sequence<Time>::append_note_on_unlocked (const Event<Time>& ev, event_id_t evid)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 c=%2 note %3 on @ %4 v=%5\n", this,
	                                              (int)ev.channel(), (int)ev.note(),
	                                              ev.time(), (int)ev.velocity()));
	assert(_writing);

	if (ev.note() > 127) {
		error << string_compose (_("invalid note on number (%1) ignored"), (int) ev.note()) << endmsg;
		return;
	} else if (ev.channel() >= 16) {
		error << string_compose (_("invalid note on channel (%1) ignored"), (int) ev.channel()) << endmsg;
		return;
	} else if (ev.velocity() == 0) {
		// Note on with velocity 0 handled as note off by caller
		error << string_compose (_("invalid note on velocity (%1) ignored"), (int) ev.velocity()) << endmsg;
		return;
	}

	/* nascent (incoming notes without a note-off ...yet) have a duration
	   that extends to Beats::max()
	*/
	NotePtr note(new Note<Time>(ev.channel(), ev.time(), std::numeric_limits<Temporal::Beats>::max() - ev.time(), ev.note(), ev.velocity()));
	assert (note->end_time() == std::numeric_limits<Temporal::Beats>::max());
	note->set_id (evid);

	add_note_unlocked (note);

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("Appending active note on %1 channel %2\n",
	                                              (unsigned)(uint8_t)note->note(), note->channel()));
	_write_notes[note->channel()].insert (note);

}

template<typename Time>
void
Sequence<Time>::append_note_off_unlocked (const Event<Time>& ev)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 c=%2 note %3 OFF @ %4 v=%5\n",
	                                              this, (int)ev.channel(),
	                                              (int)ev.note(), ev.time(), (int)ev.velocity()));
	assert(_writing);

	if (ev.note() > 127) {
		error << string_compose (_("invalid note off number (%1) ignored"), (int) ev.note()) << endmsg;
		return;
	} else if (ev.channel() >= 16) {
		error << string_compose (_("invalid note off channel (%1) ignored"), (int) ev.channel()) << endmsg;
		return;
	}

	_edited = true;

	bool resolved = false;

	/* _write_notes is sorted earliest-latest, so this will find the first matching note (FIFO) that
	   matches this note (by pitch & channel). the MIDI specification doesn't provide any guidance
	   whether to use FIFO or LIFO for this matching process, so SMF is fundamentally a lossy
	   format.
	*/

	/* XXX use _overlap_pitch_resolution to determine FIFO/LIFO ... */

	for (typename WriteNotes::iterator n = _write_notes[ev.channel()].begin(); n != _write_notes[ev.channel()].end(); ) {

		typename WriteNotes::iterator tmp = n;
		++tmp;

		NotePtr nn = *n;
		if (ev.note() == nn->note() && nn->channel() == ev.channel()) {
			assert(ev.time() >= nn->time());

			nn->set_length (ev.time() - nn->time());
			nn->set_off_velocity (ev.velocity());

			_write_notes[ev.channel()].erase(n);
			DEBUG_TRACE (DEBUG::Sequence, string_compose ("resolved note @ %2 length: %1\n", nn->length(), nn->time()));
			resolved = true;
			break;
		}

		n = tmp;
	}

	if (!resolved) {
		cerr << this << " spurious note off chan " << (int)ev.channel()
		     << ", note " << (int)ev.note() << " @ " << ev.time() << endl;
	}
}

template<typename Time>
void
Sequence<Time>::append_control_unlocked(const Parameter& param, Time time, double value, event_id_t /* evid */)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 %2 @ %3 = %4 # controls: %5\n",
	                                              this, _type_map.to_symbol(param), time, value, _controls.size()));
	boost::shared_ptr<Control> c = control(param, true);
	c->list()->add (Temporal::timepos_t (time), value, true, false);
	/* XXX control events should use IDs */
}

template<typename Time>
void
Sequence<Time>::append_sysex_unlocked(const Event<Time>& ev, event_id_t /* evid */)
{
#ifdef DEBUG_SEQUENCE
	cerr << this << " SysEx @ " << ev.time() << " \t= \t [ " << hex;
	for (size_t i=0; i < ev.size(); ++i) {
		cerr << int(ev.buffer()[i]) << " ";
	} cerr << "]" << endl;
#endif

	boost::shared_ptr< Event<Time> > event(new Event<Time>(ev, true));
	/* XXX sysex events should use IDs */
	_sysexes.insert(event);
}

template<typename Time>
void
Sequence<Time>::append_patch_change_unlocked (const PatchChange<Time>& ev, event_id_t id)
{
	PatchChangePtr p (new PatchChange<Time> (ev));

	if (p->id() < 0) {
		p->set_id (id);
	}

	_patch_changes.insert (p);
}

template<typename Time>
void
Sequence<Time>::add_patch_change_unlocked (PatchChangePtr p)
{
	if (p->id () < 0) {
		p->set_id (Evoral::next_event_id ());
	}

	_patch_changes.insert (p);
}

template<typename Time>
void
Sequence<Time>::add_sysex_unlocked (SysExPtr s)
{
	if (s->id () < 0) {
		s->set_id (Evoral::next_event_id ());
	}

	_sysexes.insert (s);
}

template<typename Time>
bool
Sequence<Time>::contains (const NotePtr& note) const
{
	ReadLock lock (read_lock());
	return contains_unlocked (note);
}

template<typename Time>
bool
Sequence<Time>::contains_unlocked (const NotePtr& note) const
{
	const Pitches& p (pitches (note->channel()));
	NotePtr search_note(new Note<Time>(0, Time(), Time(), note->note()));

	for (typename Pitches::const_iterator i = p.lower_bound (search_note);
	     i != p.end() && (*i)->note() == note->note(); ++i) {

		if (**i == *note) {
			return true;
		}
	}

	return false;
}

template<typename Time>
bool
Sequence<Time>::overlaps (const NotePtr& note, const NotePtr& without) const
{
	ReadLock lock (read_lock());
	return overlaps_unlocked (note, without);
}

template<typename Time>
bool
Sequence<Time>::overlaps_unlocked (const NotePtr& note, const NotePtr& without) const
{
	Time sa = note->time();
	Time ea  = note->end_time();

	const Pitches& p (pitches (note->channel()));
	NotePtr search_note(new Note<Time>(0, Time(), Time(), note->note()));

	for (typename Pitches::const_iterator i = p.lower_bound (search_note);
	     i != p.end() && (*i)->note() == note->note(); ++i) {

		if (without && (**i) == *without) {
			continue;
		}

		Time sb = (*i)->time();
		Time eb = (*i)->end_time();

		if (((sb > sa) && (eb <= ea)) ||
		    ((eb >= sa) && (eb <= ea)) ||
		    ((sb > sa) && (sb <= ea)) ||
		    ((sa >= sb) && (sa <= eb) && (ea <= eb))) {
			return true;
		}
	}

	return false;
}

template<typename Time>
void
Sequence<Time>::set_notes (const typename Sequence<Time>::Notes& n)
{
	_notes = n;
}

// CONST iterator implementations (x3)

/** Return the earliest note with time >= t */
template<typename Time>
typename Sequence<Time>::Notes::const_iterator
Sequence<Time>::note_lower_bound (Time t) const
{
	NotePtr search_note(new Note<Time>(0, t, Time(), 0, 0));
	typename Sequence<Time>::Notes::const_iterator i = _notes.lower_bound(search_note);
	assert(i == _notes.end() || (*i)->time() >= t);
	return i;
}

/** Return the earliest patch change with time >= t */
template<typename Time>
typename Sequence<Time>::PatchChanges::const_iterator
Sequence<Time>::patch_change_lower_bound (Time t) const
{
	PatchChangePtr search (new PatchChange<Time> (t, 0, 0, 0));
	typename Sequence<Time>::PatchChanges::const_iterator i = _patch_changes.lower_bound (search);
	assert (i == _patch_changes.end() || (*i)->time() >= t);
	return i;
}

/** Return the earliest sysex with time >= t */
template<typename Time>
typename Sequence<Time>::SysExes::const_iterator
Sequence<Time>::sysex_lower_bound (Time t) const
{
	SysExPtr search (new Event<Time> (NO_EVENT, t));
	typename Sequence<Time>::SysExes::const_iterator i = _sysexes.lower_bound (search);
	assert (i == _sysexes.end() || (*i)->time() >= t);
	return i;
}

// NON-CONST iterator implementations (x3)

/** Return the earliest note with time >= t */
template<typename Time>
typename Sequence<Time>::Notes::iterator
Sequence<Time>::note_lower_bound (Time t)
{
	NotePtr search_note(new Note<Time>(0, t, Time(), 0, 0));
	typename Sequence<Time>::Notes::iterator i = _notes.lower_bound(search_note);
	assert(i == _notes.end() || (*i)->time() >= t);
	return i;
}

/** Return the earliest patch change with time >= t */
template<typename Time>
typename Sequence<Time>::PatchChanges::iterator
Sequence<Time>::patch_change_lower_bound (Time t)
{
	PatchChangePtr search (new PatchChange<Time> (t, 0, 0, 0));
	typename Sequence<Time>::PatchChanges::iterator i = _patch_changes.lower_bound (search);
	assert (i == _patch_changes.end() || (*i)->time() >= t);
	return i;
}

/** Return the earliest sysex with time >= t */
template<typename Time>
typename Sequence<Time>::SysExes::iterator
Sequence<Time>::sysex_lower_bound (Time t)
{
	SysExPtr search (new Event<Time> (NO_EVENT, t));
	typename Sequence<Time>::SysExes::iterator i = _sysexes.lower_bound (search);
	assert (i == _sysexes.end() || (*i)->time() >= t);
	return i;
}

template<typename Time>
void
Sequence<Time>::get_notes (Notes& n, NoteOperator op, uint8_t val, int chan_mask) const
{
	switch (op) {
	case PitchEqual:
	case PitchLessThan:
	case PitchLessThanOrEqual:
	case PitchGreater:
	case PitchGreaterThanOrEqual:
		get_notes_by_pitch (n, op, val, chan_mask);
		break;

	case VelocityEqual:
	case VelocityLessThan:
	case VelocityLessThanOrEqual:
	case VelocityGreater:
	case VelocityGreaterThanOrEqual:
		get_notes_by_velocity (n, op, val, chan_mask);
		break;
	}
}

template<typename Time>
void
Sequence<Time>::get_notes_by_pitch (Notes& n, NoteOperator op, uint8_t val, int chan_mask) const
{
	for (uint8_t c = 0; c < 16; ++c) {

		if (chan_mask != 0 && !((1<<c) & chan_mask)) {
			continue;
		}

		const Pitches& p (pitches (c));
		NotePtr search_note(new Note<Time>(0, Time(), Time(), val, 0));
		typename Pitches::const_iterator i;
		switch (op) {
		case PitchEqual:
			i = p.lower_bound (search_note);
			while (i != p.end() && (*i)->note() == val) {
				n.insert (*i);
			}
			break;
		case PitchLessThan:
			i = p.upper_bound (search_note);
			while (i != p.end() && (*i)->note() < val) {
				n.insert (*i);
			}
			break;
		case PitchLessThanOrEqual:
			i = p.upper_bound (search_note);
			while (i != p.end() && (*i)->note() <= val) {
				n.insert (*i);
			}
			break;
		case PitchGreater:
			i = p.lower_bound (search_note);
			while (i != p.end() && (*i)->note() > val) {
				n.insert (*i);
			}
			break;
		case PitchGreaterThanOrEqual:
			i = p.lower_bound (search_note);
			while (i != p.end() && (*i)->note() >= val) {
				n.insert (*i);
			}
			break;

		default:
			//fatal << string_compose (_("programming error: %1 %2", X_("get_notes_by_pitch() called with illegal operator"), op)) << endmsg;
			abort(); /* NOTREACHED*/
		}
	}
}

template<typename Time>
void
Sequence<Time>::get_notes_by_velocity (Notes& n, NoteOperator op, uint8_t val, int chan_mask) const
{
	ReadLock lock (read_lock());

	for (typename Notes::const_iterator i = _notes.begin(); i != _notes.end(); ++i) {

		if (chan_mask != 0 && !((1<<((*i)->channel())) & chan_mask)) {
			continue;
		}

		switch (op) {
		case VelocityEqual:
			if ((*i)->velocity() == val) {
				n.insert (*i);
			}
			break;
		case VelocityLessThan:
			if ((*i)->velocity() < val) {
				n.insert (*i);
			}
			break;
		case VelocityLessThanOrEqual:
			if ((*i)->velocity() <= val) {
				n.insert (*i);
			}
			break;
		case VelocityGreater:
			if ((*i)->velocity() > val) {
				n.insert (*i);
			}
			break;
		case VelocityGreaterThanOrEqual:
			if ((*i)->velocity() >= val) {
				n.insert (*i);
			}
			break;
		default:
			// fatal << string_compose (_("programming error: %1 %2", X_("get_notes_by_velocity() called with illegal operator"), op)) << endmsg;
			abort(); /* NOTREACHED*/

		}
	}
}

template<typename Time>
void
Sequence<Time>::set_overlap_pitch_resolution (OverlapPitchResolution opr)
{
	_overlap_pitch_resolution = opr;

	/* XXX todo: clean up existing overlaps in source data? */
}

template<typename Time>
void
Sequence<Time>::control_list_marked_dirty ()
{
	set_edited (true);
}

template<typename Time>
void
Sequence<Time>::dump (ostream& str, typename Sequence<Time>::const_iterator x, uint32_t limit) const
{
	typename Sequence<Time>::const_iterator i = begin();

	if (x != end()) {
		i = x;
	}

	str << "+++ dump";
	if (i != end()) {
		str << " from " << i->time();
	}
	str << endl;
	for (; i != end() && (limit >= 0); ++i) {
		str << *i << endl;
		if (limit) {
			if (--limit == 0) {
				break;
			}
		}
	}
	str << "--- dump\n";
}

template class Sequence<Temporal::Beats>;

} // namespace Evoral
