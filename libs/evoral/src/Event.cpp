/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
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

#include "evoral/Event.hpp"

namespace Evoral {

#ifdef EVORAL_EVENT_ALLOC

Event::Event(uint32_t tid, EventTime t, uint32_t s, uint8_t* b, bool owns_buffer)
	: _type(tid)
	, _time(t)
	, _size(s)
	, _buffer(b)
	, _owns_buffer(owns_buffer)
{
	if (owns_buffer) {
		_buffer = (uint8_t*)malloc(_size);
		if (b) {
			memcpy(_buffer, b, _size);
		} else {
			memset(_buffer, 0, _size);
		}
	}
}

Event::Event(const Event& copy, bool owns_buffer)
	: _type(copy._type)
	, _time(copy._time)
	, _size(copy._size)
	, _buffer(copy._buffer)
	, _owns_buffer(owns_buffer)
{
	if (owns_buffer) {
		_buffer = (uint8_t*)malloc(_size);
		if (copy._buffer) {
			memcpy(_buffer, copy._buffer, _size);
		} else {
			memset(_buffer, 0, _size);
		}
	}
}

Event::~Event() {
	if (_owns_buffer) {
		free(_buffer);
	}
}

#endif // EVORAL_EVENT_ALLOC

} // namespace MIDI

