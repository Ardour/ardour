/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
 * Copyright (C) 2000-2016 Paul Davis
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

#include "evoral/Note.hpp"
#include "evoral/Sequencer.hpp"

using namespace Evoral;

template<typename Time>
Sequencer<Time>::Sequencer (TypeMap const& map)
	: Sequence<Time> (map)
{
}

template<typename Time>
Sequencer<Time>::Sequencer (Sequencer<Time> const & other)
	: Sequence<Time> (other)
{
}

template<typename Time>
event_id_t
Sequencer<Time>::insert_note (uint8_t pitch, uint8_t velocity_on, uint8_t velocity_off, uint8_t channel, Time time, Time duration)
{
	typename Sequence<Time>::WriteLock lock (Sequence<Time>::write_lock());
	typename Sequence<Time>::NotePtr note(new Note<Time>(channel, time, Time(), pitch, velocity_on));

	note->set_id (Evoral::next_event_id());
	note->set_length (duration);
	note->set_off_velocity (velocity_off);

	add_note_unlocked (note);

	return note->event_id ();
}

template<typename Time>
void
Sequencer<Time>::remove_note (uint8_t pitch, Time time)
{
}
