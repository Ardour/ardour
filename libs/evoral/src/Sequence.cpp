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

#if __clang__
#include "evoral/Note.hpp"
#endif

#include "pbd/compose.h"
#include "pbd/error.h"

#include "evoral/Beats.hpp"
#include "evoral/Control.hpp"
#include "evoral/ControlList.hpp"
#include "evoral/ControlSet.hpp"
#include "evoral/EventSink.hpp"
#include "evoral/EventPool.h"
#include "evoral/ParameterDescriptor.hpp"
#include "evoral/Sequence.hpp"
#include "evoral/TypeMap.hpp"
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
static double const time_between_interpolated_controller_outputs = 1.0 / 256;

namespace Evoral {

/* Helpers for intrusive containers of intrusive pointers
 */


template<typename T>
struct delete_disposer {
	inline void operator()(T *delete_this) {  delete delete_this;  }
};

template<typename IntrusiveContainer, typename IntrusivePointer, typename Comparator>
void
ordered_insert (IntrusiveContainer & things, IntrusivePointer & new_thing, Comparator cmp)
{
	if (things.empty() || cmp (things.back(), new_thing)) { // new thing orders later than current end
		things.push_back (new_thing);
		return;
	}

	typename IntrusiveContainer::const_iterator i = std::lower_bound (things.begin(), things.end(), new_thing, cmp);

	while (i != things.end()) {
		/* iterate over all equivalently ordered things
		 *
		 * XXX this is inefficient. We should define a
		 * different operator for the Comparator to
		 * check for equality.
		 *
		 * if (*i is greater than or equal to new_thing AND
		 *     new_thing is less than *i) {
		 *    THEN
		 *     *i must be greater than new_thing
		 * }
		 */
		if (!cmp (*i, new_thing) && cmp (new_thing, *i)) {
			break;
		}
		++i;
	}

	if (i != things.end()) {
		/* insert before earliest-later event */
		things.insert (i, new_thing);
	} else {
		/* no earlier things, just push back */
		things.push_back (new_thing);
	}
}

template<typename IntrusiveContainer, typename IntrusivePointer, typename Comparator>
void
ordered_erase (IntrusiveContainer & things, IntrusivePointer & existing_thing, Comparator cmp)
{
	if (things.empty()) {
		return;
	}

	typename IntrusiveContainer::iterator i = std::lower_bound (things.begin(), things.end(), existing_thing, cmp);

	while (i != things.end()) {

		/* iterate over all identically timed things */

		if (!cmp (*i, existing_thing) && cmp (existing_thing, *i)) {
			/* *i compares greater than existing_thing */
			break;
		}

		/* compare ptr-to-ptr .. does not compare referenced objects */
		if (*i == existing_thing) {
			std::cerr << "Found object, erase and dispose\n";
			things.erase_and_dispose (i, delete_disposer<IntrusivePointer>());
			return;
		}

		++i;
	}

	if (i == things.end()) {
		/* not found by time; linear-search ... */
		for (i = things.begin(); i != things.begin(); ++i) {
			if (*i == existing_thing) {
				things.erase_and_dispose (i, delete_disposer<IntrusivePointer>());
				break;
			}
		}
	}
}

// Sequence

template<typename Time>
Sequence<Time>::Sequence(const TypeMap& type_map, EventPool* p)
	: _edited(false)
	, _overlapping_pitches_accepted (true)
	, _overlap_pitch_resolution (FirstOnFirstOff)
	, _writing(false)
	, _type_map(type_map)
	, _percussive(false)
	, _lowest_note(127)
	, _highest_note(0)
{
	if (p) {
		_event_pool = p;
	} else {
		EventPool::SizePairs sp;

		/* 2k 4 byte or smaller messages */

		vector<size_t> sizes;

#define aligned_managed_event_size(s) PBD::aligned_size (sizeof(ManagedEvent<Time>)+(s))

		sp.push_back (std::pair<size_t,size_t> (aligned_managed_event_size(3), 2048));
		sp.push_back (std::pair<size_t,size_t> (aligned_managed_event_size (8), 128));
		sp.push_back (std::pair<size_t,size_t> (aligned_managed_event_size (16), 128));
		sp.push_back (std::pair<size_t,size_t> (aligned_managed_event_size (32), 128));

		_event_pool = new EventPool (string_compose ("sequence@%1", this), sp);
	}

	for (int i = 0; i < 16; ++i) {
		_bank[i] = 0;
	}

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("Sequence constructed: %1\n", this));
}

/** Clear all events from the model.
 */
template<typename Time>
void
Sequence<Time>::clear()
{
	WriteLock lock (write_lock());

	_events.clear_and_dispose (delete_disposer<typename EventsByTime::value_type>());
	_sysexes.clear_and_dispose (delete_disposer<typename EventsByTime::value_type>());
	_patch_changes.clear_and_dispose (delete_disposer<typename PatchChanges::value_type>());

	_notes.clear_and_dispose (delete_disposer<typename Notes::value_type>());
	for (int n = 0; n < 16; ++n) {
		_pitches[n].clear_and_dispose (delete_disposer<typename Pitches::value_type>());
		_pitches[n].clear ();
	}

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

	for (int i = 0; i < 16; ++i) {

		for (typename EventsByTime::iterator n = _write_notes[i].begin(); n != _write_notes[i].end(); ) {

			typename EventsByTime::iterator tmp;
			tmp = n;
			++tmp;

			switch (option) {
			case Relax:
				// XXX option effectively doesn't exist
				// any more, because Note objects are
				// not added to Notes container until
				// their note off is seen or they are
				// resolved.
				break;
			case DeleteStuckNotes:
				cerr << "WARNING: Stuck note lost: " << *n << endl;
				break;
			case ResolveStuckNotes:
				if (when <= (*n)->time()) {
					cerr << "WARNING: Stuck note resolution - end time @ "
					     << when << " is before note on: " << *n << endl;
				} else {
					EventPtr off (EventPtr::create (*_event_pool,
					                                MIDI_EVENT,
					                                when,
					                                3,
					                                (*n)->buffer(),
					                                next_event_id()));
					off->set_type (MIDI_CMD_NOTE_OFF);
					cerr << "WARNING: resolved note-on with no note-off to generate " << *off << endl;

					NotePtr* np = new NotePtr (new Note<Time> (*n, off));
					add_note_unlocked (*np);
				}
				break;
			}

			EventPtr& ref (*n);
			_write_notes[i].erase (n);
			delete &ref;

			n = tmp;
		}
	}

	_writing = false;
}

template<typename Time>
bool
Sequence<Time>::add_note_unlocked (NotePtr & note, void* arg)
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

	if (note->note() < _lowest_note) {
		_lowest_note = note->note();
	}
	if (note->note() > _highest_note) {
		_highest_note = note->note();
	}

	ordered_insert (_events, note->on_event (), TimeComparator<EventPtr,Time>());
	ordered_insert (_events, note->off_event (), TimeComparator<EventPtr,Time>());
	ordered_insert (_notes, note, TimeComparator<NotePtr,Time>());

	/* already inserted @param note into one intrusive list (_notes); need to copy
	   so that we can do add it to a second list (_pitches). we are only
	   copying the NotePtr; the copy still references the same Note.
	*/

	ordered_insert (_pitches[note->channel()], *(new NotePtr (note)), LowerNoteValueComparator<NotePtr>());

	_edited = true;

	return true;
}

template<typename Time>
void
Sequence<Time>::remove_note_unlocked (NotePtr & note)
{
	ordered_erase (_events, note->on_event (), TimeComparator<EventPtr,Time>());
	ordered_erase (_events, note->off_event (), TimeComparator<EventPtr,Time>());
	ordered_erase (_notes, note, TimeComparator<NotePtr,Time>());

	Pitches& p (pitches (note->channel()));
	ordered_erase (p, note, LowerNoteValueComparator<NotePtr>());

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
}

template<typename Time>
void
Sequence<Time>::remove_sysex_unlocked (EventPtr & sysex)
{
	ordered_erase (_events, sysex, TimeComparator<EventPtr,Time>());
	ordered_erase (_sysexes, sysex, TimeComparator<EventPtr,Time>());
}

template<typename Time>
void
Sequence<Time>::remove_patch_change_unlocked (PatchChangePtr & p)
{
	typename Sequence<Time>::PatchChanges::iterator i = std::lower_bound (_patch_changes.begin(), _patch_changes.end(), p, EarlierPatchChangeComparator());

	while (i != _patch_changes.end() && ((*i)->time() == p->time())) {

		typename Sequence<Time>::PatchChanges::iterator tmp = i;
		++tmp;

		if (**i == *p) {
			ordered_erase (_events, p->program_message(), TimeComparator<EventPtr,Time>());
			ordered_erase (_events, p->bank_msb_message(), TimeComparator<EventPtr,Time>());
			ordered_erase (_events, p->bank_lsb_message(), TimeComparator<EventPtr,Time>());
			ordered_erase (_patch_changes, p, TimeComparator<PatchChangePtr,Time>());
		}

		i = tmp;
	}
}

template<typename Time>
void
Sequence<Time>::add_event (EventType type, Time tm, uint8_t sz, uint8_t* data, event_id_t evid)
{
	/* This is where we make (most of) the events that end up in this sequence
	 */

	EventPtr new_event = EventPtr::create (*_event_pool, type, tm, sz, data, evid);
	add_event (new_event);
}

template<typename Time>
void
Sequence<Time>::add_event (EventPtr & ev)
{
	WriteLock lock(write_lock());

	/* All events inside a Sequence must have their ID set */

	if (ev->id() < 0) {
		ev->set_id (next_event_id());
	}

	if (!midi_event_is_valid(ev->buffer(), ev->size())) {
		cerr << "WARNING: Sequence ignoring illegal MIDI event" << endl;
		return;
	}

	if (ev->is_note_on() && ev->velocity() > 0) {

		start_note (ev);

	} else if (ev->is_note_off() || (ev->is_note_on() && ev->velocity() == 0)) {

		end_note (ev);

	} else if (ev->is_sysex()) {

		add_sysex_unlocked (ev);

	} else if (ev->is_cc() && (ev->cc_number() == MIDI_CTL_MSB_BANK || ev->cc_number() == MIDI_CTL_LSB_BANK)) {

		/* stpre bank numbers in our _bank[] array, so that we can
		 * write an event when the program change arrives
		 */

		if (ev->cc_number() == MIDI_CTL_MSB_BANK) {
			_bank[ev->channel()] &= ~(0x7f << 7);
			_bank[ev->channel()] |= ev->cc_value() << 7;
		} else {
			_bank[ev->channel()] &= ~0x7f;
			_bank[ev->channel()] |= ev->cc_value();
		}

	} else if (ev->is_cc()) {

		const ParameterType ptype = _type_map.midi_parameter_type(ev->buffer(), ev->size());
		append_control_unlocked (
			Parameter (ptype, ev->channel(), ev->cc_number()),
			ev->time(), ev->cc_value());

	} else if (ev->is_pgm_change()) {

		/* write a patch change with this program change and any previously set-up bank number */

		PatchChangePtr* pcp = new PatchChangePtr (
			new PatchChange<Time> (*_event_pool,
			                       ev->time(), ev->channel(),
			                       ev->pgm_number(), _bank[ev->channel()]));
		add_patch_change_unlocked (*pcp);

	} else if (ev->is_pitch_bender()) {

		const ParameterType ptype = _type_map.midi_parameter_type(ev->buffer(), ev->size());
		append_control_unlocked(
			Parameter(ptype, ev->channel()),
			ev->time(), double ((0x7F & ev->pitch_bender_msb()) << 7
			                    | (0x7F & ev->pitch_bender_lsb())));

	} else if (ev->is_poly_pressure()) {

		append_control_unlocked (Parameter (ev->event_type(), ev->channel(), ev->poly_note()), ev->time(), ev->poly_pressure());

	} else if (ev->is_channel_pressure()) {

		const ParameterType ptype = _type_map.midi_parameter_type(ev->buffer(), ev->size());
		append_control_unlocked (Parameter(ptype, ev->channel()), ev->time(), ev->channel_pressure());

	} else if (!_type_map.type_is_midi (ev->event_type())) {

		printf("WARNING: Sequence: Unknown event type %X: ", ev->event_type());
		for (size_t i=0; i < ev->size(); ++i) {
			printf("%X ", ev->buffer()[i]);
		}
		printf("\n");

	} else {
		printf("WARNING: Sequence: Unknown MIDI event type %X\n", ev->type());
	}

	_edited = true;
}

template<typename Time>
void
Sequence<Time>::remove_event (EventPtr const & ev)
{
	/* step 1: remove the EventPointer from the list */

	/* 1a: find the event by time */

	const_iterator i = first_event_at_or_after (ev->time());

	while (i != _events.end() && (*i)->time() <= ev->time()) {
		if (*i == ev) {
			_events.erase (i);
			break;
		}
		++i;
	}

	if (i == _events.end()) {
		/* not found by time; linear-search ... */
		for (i = _events.begin(); i != _events.begin(); ++i) {
			if (*i == ev) {
				_events.erase (i);
				break;
			}
		}
	}
}

template<typename Time>
void
Sequence<Time>::start_note (EventPtr & ev)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 c=%2 note %3 on @ %4 v=%5\n", this,
	                                              (int)ev->channel(), (int)ev->note(),
	                                              ev->time(), (int)ev->velocity()));
	assert(_writing);

	if (ev->note() > 127) {
		error << string_compose (_("invalid note on number (%1) ignored"), (int) ev->note()) << endmsg;
		return;
	} else if (ev->channel() >= 16) {
		error << string_compose (_("invalid note on channel (%1) ignored"), (int) ev->channel()) << endmsg;
		return;
	} else if (ev->velocity() == 0) {
		// Note on with velocity 0 handled as note off by caller
		error << string_compose (_("invalid note on velocity (%1) ignored"), (int) ev->velocity()) << endmsg;
		return;
	}

	/* we do not add an actual Note object yet. that happens when a
	 * matching note off arrives. For now, we just put a EventPtr object
	 * into the _write_notes container.
	 */


	DEBUG_TRACE (DEBUG::Sequence, string_compose ("Appending active note on %1 channel %2\n",
	                                              (unsigned)(uint8_t)ev->note(), ev->channel()));

	ordered_insert (_write_notes[ev->channel()], ev, TimeComparator<EventPtr,Time>());
}

template<typename Time>
bool
Sequence<Time>::end_note (EventPtr & ev)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 c=%2 note %3 OFF @ %4 v=%5\n",
	                                              this, (int)ev->channel(),
	                                              (int)ev->note(), ev->time(), (int)ev->velocity()));
	assert(_writing);

	if (ev->note() > 127) {
		error << string_compose (_("invalid note off number (%1) ignored"), (int) ev->note()) << endmsg;
		return false;
	} else if (ev->channel() >= 16) {
		error << string_compose (_("invalid note off channel (%1) ignored"), (int) ev->channel()) << endmsg;
		return false;
	}

	_edited = true;

	bool resolved = false;

	/* _write_notes is sorted earliest-latest, so this will find the first matching note (FIFO) that
	   matches this note (by pitch & channel). the MIDI specification doesn't provide any guidance
	   whether to use FIFO or LIFO for this matching process, so SMF is fundamentally a lossy
	   format.
	*/

	/* XXX use _overlap_pitch_resolution to determine FIFO/LIFO ... */

	for (typename EventsByTime::iterator n = _write_notes[ev->channel()].begin(); n != _write_notes[ev->channel()].end(); ) {

		typename EventsByTime::iterator tmp = n;
		++tmp;

		if (ev->note() == (*n)->note() && (*n)->channel() == ev->channel()) {

			assert (ev->time() >= (*n)->time());

			/* add the new note object */

			// XXX RT allocation violation
			NotePtr& np (*(new NotePtr (new Note<Time> (*n, ev))));
			add_note_unlocked (np);

			/* remove the now-resolved nascent note */

			// XXX probably need to delete this
			_write_notes[ev->channel()].erase (n);
			DEBUG_TRACE (DEBUG::Sequence, string_compose ("resolved note @ %2 length: %1\n",
			                                              np->length(), np->time()));
			resolved = true;
			break;
		}

		n = tmp;
	}

	if (!resolved) {
		cerr << this << " spurious note off chan " << (int)ev->channel()
		     << ", note " << (int)ev->note() << " @ " << ev->time() << endl;
	}

	return resolved;
}

template<typename Time>
void
Sequence<Time>::append_control_unlocked(const Parameter& param, Time time, double value)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("%1 %2 @ %3 = %4 # controls: %5\n",
	                                              this, _type_map.to_symbol(param), time, value, _controls.size()));
	boost::shared_ptr<Control> c = control(param, true);
	c->list()->add (time.to_double(), value, true, false);
	/* XXX control events should use IDs */
}

template<typename Time>
void
Sequence<Time>::add_sysex_unlocked(EventPtr & ev)
{
#ifdef DEBUG_SEQUENCE
	cerr << this << " SysEx @ " << ev.time() << " \t= \t [ " << hex;
	for (size_t i=0; i < ev.size(); ++i) {
		cerr << int(ev.buffer()[i]) << " ";
	} cerr << "]" << endl;
#endif

	ordered_insert (_events, ev, TimeComparator<EventPtr,Time>());
	ordered_insert (_sysexes, ev, TimeComparator<EventPtr,Time>());
}

template<typename Time>
void
Sequence<Time>::add_patch_change_unlocked (PatchChangePtr & p)
{
	ordered_insert (_events, p->bank_msb_message(), TimeComparator<EventPtr,Time>());
	ordered_insert (_events, p->bank_lsb_message(), TimeComparator<EventPtr,Time>());
	ordered_insert (_events, p->program_message(), TimeComparator<EventPtr,Time>());
	ordered_insert (_patch_changes, p, TimeComparator<PatchChangePtr,Time>());
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

	for (typename Pitches::const_iterator i = std::lower_bound (p.begin(), p.end(), note->note(), NoteNumberComparator());
	     i != p.end() && (*i)->note() == note->note(); ++i) {

		/* compare note contents */
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

	Pitches const & p (pitches (note->channel()));
	Note<Time>* search_note = new Note<Time> (note->on_event(), note->off_event());
	NotePtr snp (search_note); /* will delete search_note when it is
	                            * destroyed, since it holds the only
	                            * reference
	                            */

	for (typename Pitches::const_iterator i = std::lower_bound (p.begin(), p.end(), note->note(), NoteNumberComparator());
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
Sequence<Time>::set_notes (typename Sequence<Time>::Notes const & n)
{
	WriteLock lock(write_lock());

	_notes.clear_and_dispose (delete_disposer<typename Notes::value_type>());

	for (int chn = 0; chn < 16; ++chn) {
		_pitches[chn].clear_and_dispose (delete_disposer<typename Pitches::value_type>());
		_pitches[chn].clear ();
	}

	/* remove all note events from _events */

	for (typename EventsByTime::iterator i = _events.begin(); i != _events.end(); ) {
		i = _events.erase_and_dispose (i, delete_disposer<typename EventsByTime::value_type> ());
	}

	for (typename Notes::const_iterator i = n.begin(); i != n.end(); ++i) {
		/* on and off events share event IDs. seems ... risky */
		event_id_t id = next_event_id ();

		EventPtr on (EventPtr::create (*_event_pool, MIDI_EVENT, (*i)->on_event()->time(), (*i)->on_event()->size(), (*i)->on_event()->buffer(), id));
		EventPtr off (EventPtr::create (*_event_pool, MIDI_EVENT, (*i)->off_event()->time(), (*i)->off_event()->size(), (*i)->off_event()->buffer(), id));

		NotePtr& np (*(new NotePtr (new Note<Time> (on, off))));

		add_note_unlocked (np);
	}
}

// CONST iterator implementations (x3)

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

/** This fills the first argument with a set of NotePtr's that point to the
 * Note objects matching the predicate defined by @param op, @param val and
 * @param chan_mask
 *
 * The NotePtr's are copied; the underlying objects (Note, and ultimately
 * Events) are not; the new NotePtr's point to the same objects as are present
 * in the Sequence's own container.
 *
 * The results will be in note-number (pitch) order, from lowest to highest.
 */
template<typename Time>
void
Sequence<Time>::get_notes_by_pitch (Notes& n, NoteOperator op, uint8_t val, int chan_mask) const
{
	for (uint8_t c = 0; c < 16; ++c) {

		if (chan_mask != 0 && !((1<<c) & chan_mask)) {
			continue;
		}

		Pitches const & p (pitches (c));
		typename Pitches::const_iterator i;

		switch (op) {
		case PitchEqual:
			i = std::lower_bound (p.begin(), p.end(), val, LowerNoteValueComparator<NotePtr>());
			while (i != p.end() && (*i)->note() == val) {
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, LowerNoteValueComparator<NotePtr>());
			}
			break;
		case PitchLessThan:
			//i = std::upper_bound (p.begin(), p.end(), val, LowerNoteValueComparator<NotePtr>());
			while (i != p.end() && (*i)->note() < val) {
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, LowerNoteValueComparator<NotePtr>());
			}
			break;
		case PitchLessThanOrEqual:
			// i = std::upper_bound (p.begin(), p.end(), val, LowerNoteValueComparator<NotePtr>());
			while (i != p.end() && (*i)->note() <= val) {
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, LowerNoteValueComparator<NotePtr>());
			}
			break;
		case PitchGreater:
			i = std::lower_bound (p.begin(), p.end(), val, LowerNoteValueComparator<NotePtr>());
			while (i != p.end() && (*i)->note() > val) {
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, LowerNoteValueComparator<NotePtr>());
			}
			break;
		case PitchGreaterThanOrEqual:
			i = std::lower_bound (p.begin(), p.end(), val, LowerNoteValueComparator<NotePtr>());
			while (i != p.end() && (*i)->note() >= val) {
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, LowerNoteValueComparator<NotePtr>());
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
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, TimeComparator<NotePtr,Time>());
			}
			break;
		case VelocityLessThan:
			if ((*i)->velocity() < val) {
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, TimeComparator<NotePtr,Time>());
			}
			break;
		case VelocityLessThanOrEqual:
			if ((*i)->velocity() <= val) {
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, TimeComparator<NotePtr,Time>());
			}
			break;
		case VelocityGreater:
			if ((*i)->velocity() > val) {
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, TimeComparator<NotePtr,Time>());
			}
			break;
		case VelocityGreaterThanOrEqual:
			if ((*i)->velocity() >= val) {
				NotePtr& copy (*(new NotePtr (*i)));
				ordered_insert (n, copy, TimeComparator<NotePtr,Time>());
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
Sequence<Time>::dump (ostream& str) const
{
	str << "+++ dump\n";
	for (typename Sequence<Time>::const_iterator i = begin(); i != end(); ++i) {
		str << *i << endl;
	}
	str << "--- dump\n";
}

template class Sequence<Evoral::Beats>;

} // namespace Evoral
