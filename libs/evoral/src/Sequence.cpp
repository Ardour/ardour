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

// Sequence

template<typename Time>
Sequence<Time>::Sequence(const TypeMap& type_map)
	: _edited(false)
	, _overlapping_pitches_accepted (true)
	, _overlap_pitch_resolution (FirstOnFirstOff)
	, _writing(false)
	, _type_map(type_map)
	, _percussive(false)
	, _lowest_note(127)
	, _highest_note(0)
{
	DEBUG_TRACE (DEBUG::Sequence, string_compose ("Sequence constructed: %1\n", this));

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

		if (!(*n)->length()) {
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
 * the start of this model (t=0).
 */
template<typename Time>
void
Sequence<Time>::append(EventPtr const & event, event_id_t evid)
{
	WriteLock lock(write_lock());

	const MIDIEvent<Time>& ev = (const MIDIEvent<Time>&)event;

	assert (_writing);

	if (!midi_event_is_valid(ev.buffer(), ev.size())) {
		cerr << "WARNING: Sequence ignoring illegal MIDI event" << endl;
		return;
	}

	_events.insert (event);

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
		append_control_unlocked(
			Parameter(ev.event_type(), ev.channel(), ev.cc_number()),
			ev.time(), ev.cc_value(), evid);
	} else if (ev.is_pgm_change()) {
		/* write a patch change with this program change and any previously set-up bank number */
		append_patch_change_unlocked (PatchChange<Time> (ev.time(), ev.channel(), ev.pgm_number(), _bank[ev.channel()]), evid);
	} else if (ev.is_pitch_bender()) {
		append_control_unlocked(
			Parameter(ev.event_type(), ev.channel()),
			ev.time(), double ((0x7F & ev.pitch_bender_msb()) << 7
			                   | (0x7F & ev.pitch_bender_lsb())),
			evid);
	} else if (ev.is_poly_pressure()) {
		append_control_unlocked (Parameter (ev.event_type(), ev.channel(), ev.poly_note()), ev.time(), ev.poly_pressure(), evid);
	} else if (ev.is_channel_pressure()) {
		append_control_unlocked(
			Parameter(ev.event_type(), ev.channel()),
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
Sequence<Time>::append_note_on_unlocked (const MIDIEvent<Time>& ev, event_id_t evid)
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

	NotePtr note(new Note<Time>(ev.channel(), ev.time(), Time(), ev.note(), ev.velocity()));
	note->set_id (evid);

	add_note_unlocked (note);

	DEBUG_TRACE (DEBUG::Sequence, string_compose ("Appending active note on %1 channel %2\n",
	                                              (unsigned)(uint8_t)note->note(), note->channel()));
	_write_notes[note->channel()].insert (note);

}

template<typename Time>
void
Sequence<Time>::append_note_off_unlocked (const MIDIEvent<Time>& ev)
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
	c->list()->add (time.to_double(), value);
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
	SysExPtr search (new Event<Time> (0, t));
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
	SysExPtr search (new Event<Time> (0, t));
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
Sequence<Time>::dump (ostream& str) const
{
	typename Sequence<Time>::const_iterator i;
	str << "+++ dump\n";
	for (i = begin (); i != end(); ++i) {
		str << *i << endl;
	}
	str << "--- dump\n";
}

template class Sequence<Evoral::Beats>;

} // namespace Evoral
