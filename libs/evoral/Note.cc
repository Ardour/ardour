/*
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cassert>
#include <iostream>
#include <limits>
#include <glib.h>
#ifndef COMPILER_MSVC
#include "evoral/Note.h"
#endif

#include "temporal/beats.h"

namespace Evoral {

template<typename Time>
Note<Time>::Note(uint8_t chan, Time t, Time l, uint8_t n, uint8_t v)
	: _on_event (MIDI_EVENT, t, 3, NULL, true)
	, _off_event (MIDI_EVENT, t + l, 3, NULL, true)
{
	assert(chan < 16);

	_on_event.buffer()[0] = MIDI_CMD_NOTE_ON + chan;
	_on_event.buffer()[1] = n;
	_on_event.buffer()[2] = v;

	_off_event.buffer()[0] = MIDI_CMD_NOTE_OFF + chan;
	_off_event.buffer()[1] = n;
	_off_event.buffer()[2] = 0x40;

	assert(time() == t);
	assert(length() == l);
	assert(note() == n);
	assert(velocity() == v);
	assert(_on_event.channel() == _off_event.channel());
	assert(channel() == chan);
}


template<typename Time>
Note<Time>::Note(const Note<Time>& copy)
	: _on_event(copy._on_event, true)
	, _off_event(copy._off_event, true)
{
	assert(_on_event.buffer());
	assert(_off_event.buffer());
	/*
	  assert(copy._on_event.size == 3);
	  _on_event.buffer = _on_event_buffer;
	  memcpy(_on_event_buffer, copy._on_event_buffer, 3);

	  assert(copy._off_event.size == 3);
	  _off_event.buffer = _off_event_buffer;
	  memcpy(_off_event_buffer, copy._off_event_buffer, 3);
	*/

	assert(time() == copy.time());
	assert(end_time() == copy.end_time());
	assert(length() == copy.length());
	assert(note() == copy.note());
	assert(velocity() == copy.velocity());
	assert(_on_event.channel() == _off_event.channel());
	assert(channel() == copy.channel());
}

template<typename Time>
Note<Time>::~Note()
{
}

template<typename Time> void
Note<Time>::set_id (event_id_t id)
{
	_on_event.set_id (id);
	_off_event.set_id (id);
}

template class Note<Temporal::Beats>;

} // namespace Evoral

