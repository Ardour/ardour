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

#include <glib.h>
#include "evoral/Event.hpp"

namespace Evoral {

static event_id_t _event_id_counter = 0;

event_id_t
event_id_counter()
{
        return g_atomic_int_get (&_event_id_counter); 
}

void 
init_event_id_counter(event_id_t n) 
{ 
        g_atomic_int_set (&_event_id_counter, n); 
}

event_id_t
next_event_id ()
{
        return g_atomic_int_exchange_and_add (&_event_id_counter, 1);
}

#ifdef EVORAL_EVENT_ALLOC

template<typename Timestamp>
Event<Timestamp>::Event(EventType type, Timestamp time, uint32_t size, uint8_t* buf, bool alloc)
        : _type(type)
	, _original_time(time)
	, _nominal_time(time)
	, _size(size)
	, _buf(buf)
	, _owns_buf(alloc)
        , _id (-1)
{
	if (alloc) {
		_buf = (uint8_t*)malloc(_size);
		if (buf) {
			memcpy(_buf, buf, _size);
		} else {
			memset(_buf, 0, _size);
		}
	}
}

template<typename Timestamp>
Event<Timestamp>::Event(const Event& copy, bool owns_buf)
	: _type(copy._type)
	, _original_time(copy._original_time)
	, _nominal_time(copy._nominal_time)
	, _size(copy._size)
	, _buf(copy._buf)
	, _owns_buf(owns_buf)
        , _id (copy.id())
{
	if (owns_buf) {
		_buf = (uint8_t*)malloc(_size);
		if (copy._buf) {
			memcpy(_buf, copy._buf, _size);
		} else {
			memset(_buf, 0, _size);
		}
	}
}

template<typename Timestamp>
Event<Timestamp>::~Event() {
	if (_owns_buf) {
		free(_buf);
	}
}

#endif // EVORAL_EVENT_ALLOC

template class Event<Evoral::MusicalTime>;
template class Event<uint32_t>;

} // namespace Evoral

