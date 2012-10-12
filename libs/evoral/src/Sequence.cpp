/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
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

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <stdint.h>
#include <cstdio>

#include "pbd/compose.h"
#include "pbd/error.h"

#include "evoral/Control.hpp"
#include "evoral/ControlList.hpp"
#include "evoral/ControlSet.hpp"
#include "evoral/EventSink.hpp"
#include "evoral/MIDIParameters.hpp"
#include "evoral/Sequence.hpp"
#include "evoral/TypeMap.hpp"
#include "evoral/midi_util.h"

#include "i18n.h"

using namespace std;
using namespace PBD;

/** Minimum time between MIDI outputs from a single interpolated controller,
    expressed in beats.  This is to limit the rate at which MIDI messages
    are generated.  It is only applied to interpolated controllers.

    XXX: This is a hack.  The time should probably be expressed in
    seconds rather than beats, and should be configurable etc. etc.
*/
static double const time_between_interpolated_controller_outputs = 1.0 / 256;

namespace Evoral {

// Read iterator (const_iterator)

template<typename Time>
Sequence<Time>::const_iterator::const_iterator()
	: _seq(NULL)
	, _is_end(true)
	, _control_iter(_control_iters.end())
{
	_event = boost::shared_ptr< Event<Time> >(new Event<Time>());
}

/** @param force_discrete true to force ControlLists to use discrete evaluation, otherwise false to get them to use their configured mode */
template<typename Time>
Sequence<Time>::const_iterator::const_iterator(const Sequence<Time>& seq, Time t, bool force_discrete, std::set<Evoral::Parameter> const & filtered)
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
	ControlIterator earliest_control(boost::shared_ptr<ControlList>(), DBL_MAX, 0.0);
	_control_iters.reserve(seq._controls.size());
	bool   found                  = false;
	size_t earliest_control_index = 0;
	for (Controls::const_iterator i = seq._controls.begin(); i != seq._controls.end(); ++i) {

		if (filtered.find (i->first) != filtered.end()) {
			/* this parameter is filtered, so don't bother setting up an iterator for it */
			continue;
		}

		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Iterator: control: %1\n", seq._type_map.to_symbol(i->first)));
		double x, y;
		bool ret;
		if (_force_discrete) {
			ret = i->second->list()->rt_safe_earliest_event_discrete_unlocked (t, x, y, true);
		} else {
			ret = i->second->list()->rt_safe_earliest_event_unlocked(t, x, y, true);
		}
		if (!ret) {
			DEBUG_TRACE (DEBUG::Sequence, string_compose ("Iterator: CC %1 (size %2) has no events past %3\n",
			                                              i->first.id(), i->second->list()->size(), t));
			continue;
		}

		assert(x >= 0);

		if (y < i->first.min() || y > i->first.max()) {
			cerr << "ERROR: Controller value " << y
			     << " out of range [" << i->first.min() << "," << i->first.max()
			     << "], event ignored" << endl;
			continue;
		}

		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Iterator: CC %1 added (%2, %3)\n", i->first.id(), x, y));

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
		assert(_control_iter != _control_iters.end());
	} else {
		_control_iter = _control_iters.end();
	}

	// Now find the earliest event overall and point to it
	Time earliest_t = t;

	if (_note_iter != seq.notes().end()) {
		_type = NOTE_ON;
		earliest_t = (*_note_iter)->time();
	}

	if (_sysex_iter != seq.sysexes().end()
	    && ((*_sysex_iter)->time() < earliest_t || _type == NIL)) {
		_type = SYSEX;
		earliest_t = (*_sysex_iter)->time();
	}

	if (_patch_change_iter != seq.patch_changes().end() && ((*_patch_change_iter)->time() < earliest_t || _type == NIL)) {
		_type = PATCH_CHANGE;
		earliest_t = (*_patch_change_iter)->time ();
	}

	if (_control_iter != _control_iters.end()
	    && earliest_control.list && earliest_control.x >= t
	    && (earliest_control.x < earliest_t || _type == NIL)) {
		_type = CONTROL;
		earliest_t = earliest_control.x;
	}

	switch (_type) {
	case NOTE_ON:
		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Starting at note on event @ %1\n", earliest_t));
		_event = boost::shared_ptr<Event<Time> > (new Event<Time> ((*_note_iter)->on_event(), true));
		_active_notes.push(*_note_iter);
		break;
	case SYSEX:
		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Starting at sysex event @ %1\n", earliest_t));
		_event = boost::shared_ptr< Event<Time> >(
			new Event<Time>(*(*_sysex_iter), true));
		break;
	case CONTROL:
		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Starting at control event @ %1\n", earliest_t));
		seq.control_to_midi_event(_event, earliest_control);
		break;
	case PATCH_CHANGE:
		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Starting at patch change event @ %1\n", earliest_t));
		_event = boost::shared_ptr<Event<Time> > (new Event<Time> ((*_patch_change_iter)->message (_active_patch_change_message), true));
		break;
	default:
		break;
	}

	if (_type == NIL || !_event || _event->size() == 0) {
		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Starting at end @ %1\n", t));
		_type   = NIL;
		_is_end = true;
	} else {
		DEBUG_TRACE (DEBUG::Sequence, string_compose ("New iterator = 0x%1 : 0x%2 @ %3\n",
		                                              (int)_event->event_type(),
		                                              (int)((MIDIEvent<Time>*)_event.get())->type(),
		                                              _event->time()));

		assert(midi_event_is_valid(_event->buffer(), _event->size()));
	}

}

template<typename Time>
Sequence<Time>::const_iterator::~const_iterator()
{
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
		_patch_change_iter = _seq->patch_changes().end();
		_active_patch_change_message = 0;
	}
	_control_iter = _control_iters.end();
	_lock.reset();
}

template<typename Time>
const typename Sequence<Time>::const_iterator&
Sequence<Time>::const_iterator::operator++()
{
	if (_is_end) {
		throw std::logic_error("Attempt to iterate past end of Sequence");
	}

	DEBUG_TRACE(DEBUG::Sequence, "Sequence::const_iterator++\n");
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
		if (_force_discrete || _control_iter->list->interpolation() == ControlList::Discrete) {
			ret = _control_iter->list->rt_safe_earliest_event_discrete_unlocked (
				_control_iter->x, x, y, false
			                                                                     );
		} else {
			ret = _control_iter->list->rt_safe_earliest_event_linear_unlocked (
				_control_iter->x + time_between_interpolated_controller_outputs, x, y, false
			                                                                   );
		}
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

	// Now find the earliest event overall and point to it
	_type = NIL;
	Time earliest_t = std::numeric_limits<Time>::max();

	// Next earliest note on
	if (_note_iter != _seq->notes().end()) {
		_type = NOTE_ON;
		earliest_t = (*_note_iter)->time();
	}

	// Use the next note off iff it's earlier or the same time as the note on
#ifdef PERCUSSIVE_IGNORE_NOTE_OFFS
	if (!_seq->percussive() && (!_active_notes.empty())) {
#else
	if ((!_active_notes.empty())) {
#endif
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

	// Use the next earliest patch change iff it's earlier than the SysEx
	if (_patch_change_iter != _seq->patch_changes().end()) {
		if (_type == NIL || (*_patch_change_iter)->time() < earliest_t) {
			_type = PATCH_CHANGE;
			earliest_t = (*_patch_change_iter)->time();
		}
	}

	// Set event to reflect new position
	switch (_type) {
	case NOTE_ON:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = note on\n");
		*_event = (*_note_iter)->on_event();
		_active_notes.push(*_note_iter);
		break;
	case NOTE_OFF:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = note off\n");
		assert(!_active_notes.empty());
		*_event = _active_notes.top()->off_event();
		_active_notes.pop();
		break;
	case CONTROL:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = control\n");
		_seq->control_to_midi_event(_event, *_control_iter);
		break;
	case SYSEX:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = sysex\n");
		*_event = *(*_sysex_iter);
		break;
	case PATCH_CHANGE:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = patch change\n");
		*_event = (*_patch_change_iter)->message (_active_patch_change_message);
		break;
	default:
		DEBUG_TRACE(DEBUG::Sequence, "iterator = end\n");
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
	, _end_iter(*this, DBL_MAX, false, std::set<Evoral::Parameter> ())
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
	, _end_iter(*this, DBL_MAX, false, std::set<Evoral::Parameter> ())
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

		ev->set_time(iter.x);
		ev->realloc(3);
		ev->buffer()[0] = MIDI_CMD_CONTROL + iter.list->parameter().channel();
		ev->buffer()[1] = (uint8_t)iter.list->parameter().id();
		ev->buffer()[2] = (uint8_t)iter.y;
		break;

	case MIDI_CMD_PGM_CHANGE:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.y <= INT8_MAX);

		ev->set_time(iter.x);
		ev->realloc(2);
		ev->buffer()[0] = MIDI_CMD_PGM_CHANGE + iter.list->parameter().channel();
		ev->buffer()[1] = (uint8_t)iter.y;
		break;

	case MIDI_CMD_BENDER:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.y < (1<<14));

		ev->set_time(iter.x);
		ev->realloc(3);
		ev->buffer()[0] = MIDI_CMD_BENDER + iter.list->parameter().channel();
		ev->buffer()[1] = uint16_t(iter.y) & 0x7F; // LSB
		ev->buffer()[2] = (uint16_t(iter.y) >> 7) & 0x7F; // MSB
		break;

	case MIDI_CMD_CHANNEL_PRESSURE:
		assert(iter.list.get());
		assert(iter.list->parameter().channel() < 16);
		assert(iter.y <= INT8_MAX);

		ev->set_time(iter.x);
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

	#ifdef PERCUSSIVE_IGNORE_NOTE_OFFS
	if (!_percussive) {
	#endif
		for (typename Notes::iterator n = _notes.begin(); n != _notes.end() ;) {
			typename Notes::iterator next = n;
			++next;

			if ((*n)->length() == 0) {
				switch (option) {
				case Relax:
					break;
				case DeleteStuckNotes:
					cerr << "WARNING: Stuck note lost: " << (*n)->note() << endl;
					_notes.erase(n);
					break;
				case ResolveStuckNotes:
					if (when <= (*n)->time()) {
						cerr << "WARNING: Stuck note resolution - end time @ "
						     << when << " is before note on: " << (**n) << endl;
						_notes.erase (*n);
					} else {
						(*n)->set_length (when - (*n)->time());
						cerr << "WARNING: resolved note-on with no note-off to generate " << (**n) << endl;
					}
					break;
				}
			}

			n = next;
		}
	#ifdef PERCUSSIVE_IGNORE_NOTE_OFFS
	}
	#endif

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

	_edited = true;

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 remove note %2 @ %3\n", this, (int)note->note(), note->time()));

	typename Sequence<Time>::Notes::iterator i = note_lower_bound(note->time());
	while (i != _notes.end() && (*i)->time() == note->time()) {

		typename Sequence<Time>::Notes::iterator tmp = i;
		++tmp;

		if (*i == note) {

			NotePtr n = *i;

			DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1\terasing note %2 @ %3\n", this, (int)(*i)->note(), (*i)->time()));
			_notes.erase (i);

			if (n->note() == _lowest_note || n->note() == _highest_note) {

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
		}

		i = tmp;
	}

	Pitches& p (pitches (note->channel()));

	NotePtr search_note(new Note<Time>(0, 0, 0, note->note(), 0));

	typename Pitches::iterator j = p.lower_bound (search_note);
	while (j != p.end() && (*j)->note() == note->note()) {
		typename Pitches::iterator tmp = j;
		++tmp;

		if (*j == note) {
			DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1\terasing pitch %2 @ %3\n", this, (int)(*j)->note(), (*j)->time()));
			p.erase (j);
		}

		j = tmp;
	}

	if (!erased) {
		cerr << "Unable to find note to erase matching " << *note.get() << endl;
	}
}

template<typename Time>
void
Sequence<Time>::remove_patch_change_unlocked (const constPatchChangePtr p)
{
	typename Sequence<Time>::PatchChanges::iterator i = patch_change_lower_bound (p->time ());
	while (i != _patch_changes.end() && (*i)->time() == p->time()) {

		typename Sequence<Time>::PatchChanges::iterator tmp = i;
		++tmp;

		if (*i == p) {
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
Sequence<Time>::append(const Event<Time>& event, event_id_t evid)
{
	WriteLock lock(write_lock());

	const MIDIEvent<Time>& ev = (const MIDIEvent<Time>&)event;

	assert(_notes.empty() || ev.time() >= (*_notes.rbegin())->time());
	assert(_writing);

	if (!midi_event_is_valid(ev.buffer(), ev.size())) {
		cerr << "WARNING: Sequence ignoring illegal MIDI event" << endl;
		return;
	}

	if (ev.is_note_on()) {
		NotePtr note(new Note<Time>(ev.channel(), ev.time(), 0, ev.note(), ev.velocity()));
		append_note_on_unlocked (note, evid);
	} else if (ev.is_note_off()) {
		NotePtr note(new Note<Time>(ev.channel(), ev.time(), 0, ev.note(), ev.velocity()));
		/* XXX note: event ID is discarded because we merge the on+off events into
		   a single note object
		*/
		append_note_off_unlocked (note);
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
		append_control_unlocked(
			Evoral::MIDI::ContinuousController(ev.event_type(), ev.channel(), ev.cc_number()),
			ev.time(), ev.cc_value(), evid);
	} else if (ev.is_pgm_change()) {
		/* write a patch change with this program change and any previously set-up bank number */
		append_patch_change_unlocked (PatchChange<Time> (ev.time(), ev.channel(), ev.pgm_number(), _bank[ev.channel()]), evid);
	} else if (ev.is_pitch_bender()) {
		append_control_unlocked(
			Evoral::MIDI::PitchBender(ev.event_type(), ev.channel()),
			ev.time(), double ((0x7F & ev.pitch_bender_msb()) << 7
			                   | (0x7F & ev.pitch_bender_lsb())),
			evid);
	} else if (ev.is_channel_pressure()) {
		append_control_unlocked(
			Evoral::MIDI::ChannelPressure(ev.event_type(), ev.channel()),
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
Sequence<Time>::append_note_on_unlocked (NotePtr note, event_id_t evid)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 c=%2 note %3 on @ %4 v=%5\n", this,
	                                              (int) note->channel(), (int) note->note(),
	                                              note->time(), (int) note->velocity()));
	assert(_writing);

	if (note->note() > 127) {
		error << string_compose (_("illegal note number (%1) used in Note on event - event will be ignored"), (int)  note->note()) << endmsg;
		return;
	}
	if (note->channel() >= 16) {
		error << string_compose (_("illegal channel number (%1) used in Note on event - event will be ignored"), (int) note->channel()) << endmsg;
		return;
	}

	if (note->id() < 0) {
		note->set_id (evid);
	}

	if (note->velocity() == 0) {
		append_note_off_unlocked (note);
		return;
	}

	add_note_unlocked (note);

	#ifdef PERCUSSIVE_IGNORE_NOTE_OFFS
	if (!_percussive) {
	#endif

		DEBUG_TRACE (DEBUG::Sequence, string_compose ("Sustained: Appending active note on %1 channel %2\n",
		                                              (unsigned)(uint8_t)note->note(), note->channel()));
		_write_notes[note->channel()].insert (note);

	#ifdef PERCUSSIVE_IGNORE_NOTE_OFFS
	} else {
		DEBUG_TRACE(DEBUG::Sequence, "Percussive: NOT appending active note on\n");
	}
	#endif

}

template<typename Time>
void
Sequence<Time>::append_note_off_unlocked (NotePtr note)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 c=%2 note %3 OFF @ %4 v=%5\n",
	                                              this, (int)note->channel(),
	                                              (int)note->note(), note->time(), (int)note->velocity()));
	assert(_writing);

	if (note->note() > 127) {
		error << string_compose (_("illegal note number (%1) used in Note off event - event will be ignored"), (int) note->note()) << endmsg;
		return;
	}
	if (note->channel() >= 16) {
		error << string_compose (_("illegal channel number (%1) used in Note off event - event will be ignored"), (int) note->channel()) << endmsg;
		return;
	}

	_edited = true;

	#ifdef PERCUSSIVE_IGNORE_NOTE_OFFS
	if (_percussive) {
		DEBUG_TRACE(DEBUG::Sequence, "Sequence Ignoring note off (percussive mode)\n");
		return;
	}
	#endif

	bool resolved = false;

	/* _write_notes is sorted earliest-latest, so this will find the first matching note (FIFO) that
	   matches this note (by pitch & channel). the MIDI specification doesn't provide any guidance
	   whether to use FIFO or LIFO for this matching process, so SMF is fundamentally a lossy
	   format.
	*/

	/* XXX use _overlap_pitch_resolution to determine FIFO/LIFO ... */

	for (typename WriteNotes::iterator n = _write_notes[note->channel()].begin(); n != _write_notes[note->channel()].end(); ) {

		typename WriteNotes::iterator tmp = n;
		++tmp;

		NotePtr nn = *n;
		if (note->note() == nn->note() && nn->channel() == note->channel()) {
			assert(note->time() >= nn->time());

			nn->set_length (note->time() - nn->time());
			nn->set_off_velocity (note->velocity());

			_write_notes[note->channel()].erase(n);
			DEBUG_TRACE (DEBUG::Sequence, string_compose ("resolved note @ %2 length: %1\n", nn->length(), nn->time()));
			resolved = true;
			break;
		}

		n = tmp;
	}

	if (!resolved) {
		cerr << this << " spurious note off chan " << (int)note->channel()
		     << ", note " << (int)note->note() << " @ " << note->time() << endl;
	}
}

template<typename Time>
void
Sequence<Time>::append_control_unlocked(const Parameter& param, Time time, double value, event_id_t /* evid */)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 %2 @ %3\t=\t%4 # controls: %5\n",
	                                              this, _type_map.to_symbol(param), time, value, _controls.size()));
	boost::shared_ptr<Control> c = control(param, true);
	c->list()->add (time, value);
	/* XXX control events should use IDs */
}

template<typename Time>
void
Sequence<Time>::append_sysex_unlocked(const MIDIEvent<Time>& ev, event_id_t /* evid */)
{
#ifdef DEBUG_SEQUENCE
	cerr << this << " SysEx @ " << ev.time() << " \t= \t [ " << hex;
	for (size_t i=0; i < ev.size(); ++i) {
		cerr << int(ev.buffer()[i]) << " ";
	} cerr << "]" << endl;
#endif

	boost::shared_ptr<MIDIEvent<Time> > event(new MIDIEvent<Time>(ev, true));
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
	NotePtr search_note(new Note<Time>(0, 0, 0, note->note()));

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
	NotePtr search_note(new Note<Time>(0, 0, 0, note->note()));

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
Sequence<Time>::set_notes (const Sequence<Time>::Notes& n)
{
	_notes = n;
}

/** Return the earliest note with time >= t */
template<typename Time>
typename Sequence<Time>::Notes::const_iterator
Sequence<Time>::note_lower_bound (Time t) const
{
	NotePtr search_note(new Note<Time>(0, t, 0, 0, 0));
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
	SysExPtr search (new Event<Time> (0, t));
	typename Sequence<Time>::SysExes::const_iterator i = _sysexes.lower_bound (search);
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
		NotePtr search_note(new Note<Time>(0, 0, 0, val, 0));
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
			abort ();
			/* NOTREACHED*/
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
			abort ();
			/* NOTREACHED*/

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
Sequence<Time>::dump (ostream& str) const
{
	typename Sequence<Time>::const_iterator i;
	str << "+++ dump\n";
	for (i = begin(); i != end(); ++i) {
		str << *i << endl;
	}
	str << "--- dump\n";
}

template class Sequence<Evoral::MusicalTime>;

} // namespace Evoral

