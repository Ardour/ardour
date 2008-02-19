/*
    Copyright (C) 2007 Paul Davis
    Author: Dave Robillard

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

#include <ardour/note.h>

namespace ARDOUR {

Note::Note(uint8_t chan, double t, double d, uint8_t n, uint8_t v)
	: _on_event(t, 3, NULL, true)
	, _off_event(t + d, 3, NULL, true)
{
	assert(chan < 16);

	_on_event.buffer()[0] = MIDI_CMD_NOTE_ON + chan;
	_on_event.buffer()[1] = n;
	_on_event.buffer()[2] = v;
	
	_off_event.buffer()[0] = MIDI_CMD_NOTE_OFF + chan;
	_off_event.buffer()[1] = n;
	_off_event.buffer()[2] = 0x40;
	
	assert(time() == t);
	assert(duration() == d);
	assert(note() == n);
	assert(velocity() == v);
}


Note::Note(const Note& copy)
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
	assert(note() == copy.note());
	assert(velocity() == copy.velocity());
	assert(duration() == copy.duration());
}


const Note&
Note::operator=(const Note& copy)
{
	_on_event = copy._on_event;
	_off_event = copy._off_event;
	/*_on_event.time = copy._on_event.time;
	assert(copy._on_event.size == 3);
	memcpy(_on_event_buffer, copy._on_event_buffer, 3);
	
	_off_event.time = copy._off_event.time;
	assert(copy._off_event.size == 3);
	memcpy(_off_event_buffer, copy._off_event_buffer, 3);
	*/
	
	assert(time() == copy.time());
	assert(end_time() == copy.end_time());
	assert(note() == copy.note());
	assert(velocity() == copy.velocity());
	assert(duration() == copy.duration());

	return *this;
}

} // namespace ARDOUR
