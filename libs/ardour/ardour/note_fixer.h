/*
    Copyright (C) 2015 Paul Davis
    Author: David Robillard

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

#ifndef __ardour_note_fixer_h__
#define __ardour_note_fixer_h__

#include <list>

#include <boost/utility.hpp>

#include "ardour/midi_model.h"
#include "ardour/types.h"
#include "evoral/Beats.hpp"
#include "evoral/Note.hpp"

namespace Evoral { template<typename Time> class EventSink; }

namespace ARDOUR {

class BeatsFramesConverter;
class MidiStateTracker;
class TempoMap;

/** A tracker and compensator for note edit operations.
 *
 * This monitors edit operations sent to a model that affect active notes
 * during a read, and maintains a queue of synthetic events that should be sent
 * at the start of the next read to maintain coherent MIDI state.
 */
class NoteFixer : public boost::noncopyable
{
public:
	typedef Evoral::Note<Evoral::Beats> Note;

	~NoteFixer();

	/** Clear all internal state. */
	void clear();

	/** Handle a region edit during read.
	 *
	 * This must be called before the command is applied to the model.  Events
	 * are enqueued to compensate for edits which should be later sent with
	 * emit() at the start of the next read.
	 *
	 * @param cmd Command to compensate for.
	 * @param origin Timeline position of edited source.
	 * @param pos Current read position (last read end).
	 */
	void prepare(TempoMap&                          tempo_map,
	             const MidiModel::NoteDiffCommand*  cmd,
	             framepos_t                         origin,
	             framepos_t                         pos,
	             std::set< boost::weak_ptr<Note> >& active_notes);

	/** Emit any pending edit compensation events.
	 *
	 * @param dst Destination for events.
	 * @param pos Timestamp to be used for every event, should be the start of
	 * the read block immediately following any calls to prepare().
	 * @param tracker Tracker to update with emitted events.
	 */
	void emit(Evoral::EventSink<framepos_t>& dst,
	          framepos_t                     pos,
	          MidiStateTracker&              tracker);

private:
	typedef Evoral::Event<framepos_t> Event;
	typedef std::list<Event*>         Events;

	/** Copy a beats event to a frames event with the given time stamp. */
	Event* copy_event(framepos_t time, const Evoral::Event<Evoral::Beats>& ev);

	/** Return true iff `note` is active at `pos`. */
	bool note_is_active(const BeatsFramesConverter& converter,
	                    boost::shared_ptr<Note>     note,
	                    framepos_t                  pos);

	Events _events;
};

} /* namespace ARDOUR */

#endif	/* __ardour_note_fixer_h__ */


