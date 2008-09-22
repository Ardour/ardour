/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
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

#include <glib.h>
#include <evoral/RingBuffer.hpp>
#include <evoral/EventSink.hpp>
#include <evoral/types.hpp>

#include <iostream>
using namespace std;

namespace Evoral {


/** A RingBuffer of events (generic time-stamped binary "blobs").
 *
 * This packs a timestamp, size, and size bytes of data flat into the buffer.
 * Useful for MIDI events, OSC messages, etc.
 */
class EventRingBuffer : public Evoral::RingBuffer<uint8_t>, public Evoral::EventSink {
public:

	/** @param capacity Ringbuffer capacity in bytes.
	 */
	EventRingBuffer(size_t capacity) : RingBuffer<uint8_t>(capacity)
	{}

	size_t capacity() const { return _size; }
	
	bool peek_time(EventTime* time);

	uint32_t write(EventTime  time, EventType  type, uint32_t  size, const uint8_t* buf);
	bool     read (EventTime* time, EventType* type, uint32_t* size,       uint8_t* buf);
};


inline bool
EventRingBuffer::peek_time(EventTime* time)
{
	bool success = RingBuffer<uint8_t>::full_peek(sizeof(EventTime), (uint8_t*)time);
	return success;
}


inline bool
EventRingBuffer::read(EventTime* time, EventType* type, uint32_t* size, uint8_t* buf)
{
	bool success = RingBuffer<uint8_t>::full_read(sizeof(EventTime), (uint8_t*)time);
	if (success)
		success = RingBuffer<uint8_t>::full_read(sizeof(EventType), (uint8_t*)type);
	if (success)
		success = RingBuffer<uint8_t>::full_read(sizeof(uint32_t), (uint8_t*)size);
	if (success)
		success = RingBuffer<uint8_t>::full_read(*size, buf);
	
	return success;
}


inline uint32_t
EventRingBuffer::write(EventTime time, EventType type, uint32_t size, const uint8_t* buf)
{
	if (write_space() < (sizeof(EventTime) + sizeof(EventType) + sizeof(uint32_t) + size)) {
		return 0;
	} else {
		RingBuffer<uint8_t>::write(sizeof(EventTime), (uint8_t*)&time);
		RingBuffer<uint8_t>::write(sizeof(EventType), (uint8_t*)&type);
		RingBuffer<uint8_t>::write(sizeof(uint32_t), (uint8_t*)&size);
		RingBuffer<uint8_t>::write(size, buf);
		return size;
	}
}


} // namespace Evoral

#endif // EVORAL_EVENT_RING_BUFFER_HPP

