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

#include <cassert>
#include <iostream>
#include <limits>
#include <glib.h>
#ifndef COMPILER_MSVC
#include "evoral/Note.hpp"
#endif

#include "pbd/stacktrace.h"

#include "evoral/Beats.hpp"

namespace Evoral {

template<typename Time>
Note<Time>::Note (EventPointer<Time> const & on, EventPointer<Time> const & off)
{
	/* Note "owns" the two events pointed to by @param on and @param off,
	   but they are refcnt'ed anyway.
	*/
	set_event (0, on);
	set_event (1, off);
}

template<typename Time>
Note<Time>::Note (EventPool& pool, uint8_t chan, Time time, Time length, uint8_t note, uint8_t velocity)
{
	/* this uses the ManagedEvent::default_event_pool to create the Events.
	 * This is suboptimal if the events/notes are going to end up in a
	 * Sequence, but isn't actually a problem.
	 */

	uint8_t data[3];

	data[0] = (MIDI_CMD_NOTE_ON|chan);
	data[1] = note;
	data[2] = velocity;

	std::cerr << "NEW NOTE\n";

	set_event (0, EventPointer<Time>::create (pool, Evoral::MIDI_EVENT, time, 3, data));

	data[0] = (MIDI_CMD_NOTE_OFF|chan);
	data[1] = note;
	data[2] = velocity;

	set_event (1, EventPointer<Time>::create (pool, Evoral::MIDI_EVENT, time + length, 3, data));
	std::cerr << "NEW NOTE DONE\n";
}

template<typename Time>
Note<Time>::Note (Note<Time> const & other)
{
	EventPointer<Time> const & on (other.on_event());
	EventPointer<Time> const & off (other.off_event());

	std::cerr << "NOTE COPY\n";

	set_event (0, EventPointer<Time>::create (other.pool(), Evoral::MIDI_EVENT, on->time(), on->size(), on->buffer()));
	set_event (1, EventPointer<Time>::create (other.pool(), Evoral::MIDI_EVENT, off->time(), off->size(), off->buffer()));
	std::cerr << "NOTE COPY DONE\n";
}

template<typename Time>
Note<Time>::~Note()
{
}

template<typename Time>
const Note<Time>&
Note<Time>::operator=(const Note<Time>& other)
{
	_events[0] = other._events[0];
	_events[2] = other._events[1];

	assert(time() == other.time());
	assert(end_time() == other.end_time());
	assert(length() == other.length());
	assert(note() == other.note());
	assert(velocity() == other.velocity());
	assert(_events[0]->channel() == _events[1]->channel());
	assert(channel() == other.channel());

	return *this;
}

template<typename Time>
NotePointer<Time>::NotePointer (NotePointer<Time> const & other)
	: MultiEventPointer<Note<Time> > (other.get())
{
}

template class Note<Evoral::Beats>;
template class NotePointer<Evoral::Beats>;

} // namespace Evoral
