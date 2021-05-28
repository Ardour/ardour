/*
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#include <glib.h>

#include "pbd/g_atomic_compat.h"
#include "temporal/beats.h"
#include "evoral/Event.h"

namespace Evoral {

static GATOMIC_QUAL event_id_t _event_id_counter = 0;

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
	/* TODO: handle 31bit overflow , event_id_t is an int32_t,
	 * and libsmf only supports loading uint32_t vlq's, see smf_extract_vlq()
	 *
	 * event-IDs only have to be unique per .mid file.
	 * Previously (Ardour 4.2ish) Ardour re-generated those IDs when loading the
	 * file but that lead to .mid files being modified on every load/save.
	 *
	 * current user-record: is event-counter="276390506" (just abov 2^28)
	 */
	return g_atomic_int_add (&_event_id_counter, 1);
}

#ifdef EVORAL_EVENT_ALLOC

template<typename Timestamp>
Event<Timestamp>::Event(EventType type, Timestamp time, uint32_t size, uint8_t* buf, bool alloc)
	: _type(type)
	, _time(time)
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
Event<Timestamp>::Event(EventType      type,
                        Timestamp      time,
                        uint32_t       size,
                        const uint8_t* buf)
	: _type(type)
	, _time(time)
	, _size(size)
	, _buf((uint8_t*)malloc(size))
	, _id(-1)
	, _owns_buf(true)
{
	memcpy(_buf, buf, _size);
}

template<typename Timestamp>
Event<Timestamp>::Event(const Event& copy, bool owns_buf)
	: _type(copy._type)
	, _time(copy._time)
	, _size(copy._size)
	, _buf(copy._buf)
	, _id (next_event_id ())
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
void
Event<Timestamp>::assign(const Event& other)
{
	_id = other._id;
	_type = other._type;
	_time = other._time;
	_owns_buf = other._owns_buf;
	if (_owns_buf) {
		if (other._buf) {
			if (other._size > _size) {
				_buf = (uint8_t*)::realloc(_buf, other._size);
			}
			memcpy(_buf, other._buf, other._size);
		} else {
			free(_buf);
			_buf = NULL;
		}
	} else {
		_buf = other._buf;
	}

	_size = other._size;
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

	_time = t;
	_size = size;
}

#endif // EVORAL_EVENT_ALLOC

template class Event<Temporal::Beats>;
template class Event<double>;
template class Event<int64_t>;

} // namespace Evoral

