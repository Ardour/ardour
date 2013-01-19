/* This file is part of Evoral.
 * Copyright (C) 2008 David Robillard <http://drobilla.net>
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

#ifndef EVORAL_EVENT_RING_BUFFER_HPP
#define EVORAL_EVENT_RING_BUFFER_HPP

#include <iostream>

#include "pbd/ringbufferNPT.h"

#include "evoral/EventSink.hpp"
#include "evoral/types.hpp"

using namespace std;

namespace Evoral {

/** A RingBuffer of events (generic time-stamped binary "blobs").
 *
 * This packs a timestamp, size, and size bytes of data flat into the buffer.
 * Useful for MIDI events, OSC messages, etc.
 *
 * Note: the uint8_t template argument to RingBufferNPT<> indicates "byte
 * oriented data", not anything particular linked to MIDI or any other
 * possible interpretation of uint8_t.
 */
template<typename Time>
class EventRingBuffer : public PBD::RingBufferNPT<uint8_t>, public Evoral::EventSink<Time> {
public:

	/** @param capacity Ringbuffer capacity in bytes.
	 */
	EventRingBuffer(size_t capacity) : PBD::RingBufferNPT<uint8_t>(capacity)
	{}

	inline size_t capacity() const { return bufsize(); }

	/** Peek at the ringbuffer (read w/o advancing read pointer).
	 * @return how much has been peeked (wraps around if read exceeds
	 * the end of the buffer):
	 * <pre>
	 * |===========--------------R=============================|
	 *            read-pointer---^
	 * </pre>
	 */
	inline bool peek (uint8_t*, size_t size);

	inline uint32_t write(Time  time, EventType  type, uint32_t  size, const uint8_t* buf);
	inline bool     read (Time* time, EventType* type, uint32_t* size,       uint8_t* buf);
};

template<typename Time>
inline bool
EventRingBuffer<Time>::peek (uint8_t* buf, size_t size)
{
	PBD::RingBufferNPT<uint8_t>::rw_vector vec;

	get_read_vector (&vec);

	if (vec.len[0] + vec.len[1] < size) {
		return false;
	}

	if (vec.len[0] > 0) {
		memcpy (buf, vec.buf[0], min (vec.len[0], size));
	}

	if (vec.len[0] < size) {
		if (vec.len[1]) {
			memcpy (buf + vec.len[0], vec.buf[1], size - vec.len[0]);
		}
	}

	return true;
}

template<typename Time>
inline bool
EventRingBuffer<Time>::read(Time* time, EventType* type, uint32_t* size, uint8_t* buf)
{
	if (PBD::RingBufferNPT<uint8_t>::read ((uint8_t*)time, sizeof (Time)) != sizeof (Time)) {
		return false;
	}

	if (PBD::RingBufferNPT<uint8_t>::read ((uint8_t*)type, sizeof(EventType)) != sizeof (EventType)) {
		return false;
	}

	if (PBD::RingBufferNPT<uint8_t>::read ((uint8_t*)size, sizeof(uint32_t)) != sizeof (uint32_t)) {
		return false;
	}

	if (PBD::RingBufferNPT<uint8_t>::read (buf, *size) != *size) {
		return false;
	}

	return true;
}


template<typename Time>
inline uint32_t
EventRingBuffer<Time>::write(Time time, EventType type, uint32_t size, const uint8_t* buf)
{
	if (!buf || write_space() < (sizeof(Time) + sizeof(EventType) + sizeof(uint32_t) + size)) {
		return 0;
	} else {
		PBD::RingBufferNPT<uint8_t>::write ((uint8_t*)&time, sizeof(Time));
		PBD::RingBufferNPT<uint8_t>::write ((uint8_t*)&type, sizeof(EventType));
		PBD::RingBufferNPT<uint8_t>::write ((uint8_t*)&size, sizeof(uint32_t));
		PBD::RingBufferNPT<uint8_t>::write (buf, size);
		return size;
	}
}


} // namespace Evoral

#endif // EVORAL_EVENT_RING_BUFFER_HPP

