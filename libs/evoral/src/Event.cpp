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
	return g_atomic_int_add (&_event_id_counter, 1);
}

#ifdef EVORAL_EVENT_ALLOC

template<typename Timestamp>
Event<Timestamp>::Event(EventType type, Timestamp time, uint32_t size, uint8_t* buf, bool alloc)
	: _type(type)
	, _original_time(time)
	, _nominal_time(time)
	, _size(size)
	, _buf(buf)
	, _id(-1)
	, _owns_buf(alloc)
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
	, _id(copy.id())
	, _owns_buf(owns_buf)
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

template<typename Timestamp>
const Event<Timestamp>&
Event<Timestamp>::operator=(const Event& copy)
{
	_id = copy.id(); // XXX is this right? do we want ID copy semantics?
	_type = copy._type;
	_original_time = copy._original_time;
	_nominal_time = copy._nominal_time;
	if (_owns_buf) {
		if (copy._buf) {
			if (copy._size > _size) {
				_buf = (uint8_t*)::realloc(_buf, copy._size);
			}
			memcpy(_buf, copy._buf, copy._size);
		} else {
			free(_buf);
			_buf = NULL;
		}
	} else {
		_buf = copy._buf;
	}

	_size = copy._size;
	return *this;
}

template<typename Timestamp>
void
Event<Timestamp>::set (const uint8_t* buf, uint32_t size, Timestamp t)
{
	if (_owns_buf) {
		if (_size < size) {
			_buf = (uint8_t*) ::realloc(_buf, size);
		}
		memcpy (_buf, buf, size);
	} else {
		/* XXX this is really dangerous given the
		   const-ness of buf. The API should really
		   intervene here.
		*/
		_buf = const_cast<uint8_t*> (buf);
	}

	_original_time = t;
	_nominal_time = t;
	_size = size;
}

template<typename Timestamp>
void
Event<Timestamp>::set_time (Timestamp t)
{
	_nominal_time = t;
}

template<typename Timestamp>
void
Event<Timestamp>::set_original_time (Timestamp t)
{
	_original_time = t;
}
	
#endif // EVORAL_EVENT_ALLOC

template class Event<Evoral::MusicalTime>;
template class Event<int64_t>;

} // namespace Evoral

