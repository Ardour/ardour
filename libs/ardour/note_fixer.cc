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

#include "evoral/EventList.hpp"

#include "ardour/beats_frames_converter.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/note_fixer.h"
#include "ardour/tempo.h"

namespace ARDOUR {

NoteFixer::~NoteFixer()
{
	clear();
}

void
NoteFixer::clear()
{
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		delete *i;
	}
}

void
NoteFixer::prepare(TempoMap&                          tempo_map,
                   const MidiModel::NoteDiffCommand*  cmd,
                   const framepos_t                   origin,
                   const framepos_t                   pos,
                   std::set< boost::weak_ptr<Note> >& active_notes)
{
	typedef MidiModel::NoteDiffCommand Command;

	BeatsFramesConverter converter(tempo_map, origin);

	for (Command::NoteList::const_iterator i = cmd->removed_notes().begin();
	     i != cmd->removed_notes().end(); ++i) {
		if (note_is_active(converter, *i, pos)) {
			/* Deleted note spans the end of the latest read, so we will never
			   read its off event.  Emit a note off to prevent a stuck note. */
			_events.push_back(copy_event(pos, (*i)->off_event()));
			active_notes.erase(*i);
		}
	}

	for (Command::NoteList::const_iterator i = cmd->added_notes().begin();
	     i != cmd->added_notes().end(); ++i) {
		if (note_is_active(converter, *i, pos)) {
			/* Added note spans the end of the latest read, so we missed its on
			   event.  Emit note on immediately to make the state consistent. */
			_events.push_back(copy_event(pos, (*i)->on_event()));
			active_notes.insert(*i);
		}
	}

	for (Command::ChangeList::const_iterator i = cmd->changes().begin();
	     i != cmd->changes().end(); ++i) {
		if (!note_is_active(converter, i->note, pos)) {
			/* Note is not currently active, no compensation needed. */
			continue;
		}

		/* Changed note spans the end of the latest read. */
		if (i->property == Command::NoteNumber) {
			/* Note number has changed, end the old note. */
			_events.push_back(copy_event(pos, i->note->off_event()));

			/* Start a new note on the new note number.  The same note object
			   is active, so we leave active_notes alone. */
			Event* on = copy_event(pos, i->note->on_event());
			on->buffer()[1] = (uint8_t)i->new_value.get_int();
			_events.push_back(on);
		} else if (i->property == Command::StartTime &&
		           converter.to(i->new_value.get_beats()) >= pos) {
			/* Start time has moved from before to after the end of the
			   latest read, end the old note. */
			_events.push_back(copy_event(pos, i->note->off_event()));
			active_notes.erase(i->note);
		} else if (i->property == Command::Length &&
		           converter.to(i->note->time() + i->new_value.get_beats()) < pos) {
			/* Length has shortened to before the end of the latest read,
			   end the note. */
			_events.push_back(copy_event(pos, i->note->off_event()));
			active_notes.erase(i->note);
		} else if (i->property == Command::Channel) {
			/* Channel has changed, end the old note. */
			_events.push_back(copy_event(pos, i->note->off_event()));

			/* Start a new note on the new channel.  See number change above. */
			Event* on = copy_event(pos, i->note->on_event());
			on->buffer()[0] &= 0xF0;
			on->buffer()[0] |= (uint8_t)i->new_value.get_int();
			_events.push_back(on);
		}
	}
}

void
NoteFixer::emit(Evoral::EventSink<framepos_t>& dst,
                framepos_t                     pos,
                MidiStateTracker&              tracker)
{
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		dst.write(pos, (*i)->event_type(), (*i)->size(), (*i)->buffer());
		tracker.track(**i);
		delete *i;
	}
	_events.clear();
}

NoteFixer::Event*
NoteFixer::copy_event(framepos_t time, const Evoral::Event<Evoral::Beats>& ev)
{
	return new Event(ev.event_type(), time, ev.size(), ev.buffer());
}

bool
NoteFixer::note_is_active(const BeatsFramesConverter& converter,
                          boost::shared_ptr<Note>     note,
                          framepos_t                  pos)
{
	const framepos_t start_time = converter.to(note->time());
	const framepos_t end_time   = converter.to(note->end_time());

	return (start_time < pos && end_time >= pos);
}

} // namespace ARDOUR
