/*
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
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

#ifndef __ardour_event_ring_buffer_h__
#define __ardour_event_ring_buffer_h__

#include <algorithm>
#include <iostream>

#include "pbd/ringbufferNPT.h"

#include "evoral/EventSink.h"
#include "evoral/types.h"

namespace ARDOUR {

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
class EventRingBuffer : public PBD::RingBufferNPT<uint8_t>
                      , public Evoral::EventSink<Time> {
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

	inline uint32_t write(Time  time, Evoral::EventType  type, uint32_t  size, const uint8_t* buf);
	inline bool     read (Time* time, Evoral::EventType* type, uint32_t* size,       uint8_t* buf);
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

	assert (vec.len[0] > 0 || vec.len[1] > 0);

	if (vec.len[0] > 0) {
		memcpy (buf, vec.buf[0], std::min (vec.len[0], size));
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
EventRingBuffer<Time>::read(Time* time, Evoral::EventType* type, uint32_t* size, uint8_t* buf)
{
	if (PBD::RingBufferNPT<uint8_t>::read ((uint8_t*)time, sizeof (Time)) != sizeof (Time)) {
		return false;
	}

	if (PBD::RingBufferNPT<uint8_t>::read ((uint8_t*)type, sizeof(Evoral::EventType)) != sizeof (Evoral::EventType)) {
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
EventRingBuffer<Time>::write(Time time, Evoral::EventType type, uint32_t size, const uint8_t* buf)
{
	assert (size > 0);
	if (!buf || size == 0 || write_space() < (sizeof(Time) + sizeof(Evoral::EventType) + sizeof(uint32_t) + size)) {
		return 0;
	} else {
		PBD::RingBufferNPT<uint8_t>::write ((uint8_t*)&time, sizeof(Time));
		PBD::RingBufferNPT<uint8_t>::write ((uint8_t*)&type, sizeof(Evoral::EventType));
		PBD::RingBufferNPT<uint8_t>::write ((uint8_t*)&size, sizeof(uint32_t));
		PBD::RingBufferNPT<uint8_t>::write (buf, size);
		return size;
	}
}

} // namespace ARDOUR

#endif // __ardour_event_ring_buffer_h__
