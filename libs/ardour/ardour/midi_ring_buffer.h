/*
    Copyright (C) 2006 Paul Davis 

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

#ifndef __ardour_midi_ring_buffer_h__
#define __ardour_midi_ring_buffer_h__

#include <iostream>
#include <algorithm>
#include <ardour/types.h>
#include <ardour/buffer.h>
#include <ardour/event_type_map.h>
#include <evoral/EventSink.hpp>
#include <evoral/EventRingBuffer.hpp>

namespace ARDOUR {


/** A RingBuffer for (MIDI) events.
 *
 * This is simply a wrapper around a raw ringbuffer which writes/reads events
 * as flat placked blobs.
 * The buffer looks like this:
 *
 * [timestamp][type][size][size bytes of raw MIDI][timestamp][type][size](etc...)
 */
class MidiRingBuffer : public Evoral::EventRingBuffer {
public:
	/** @param size Size in bytes.
	 */
	MidiRingBuffer(size_t size)
		: Evoral::EventRingBuffer(size)
		, _channel_mask(0x0000FFFF)
	{}

	inline bool read_prefix(Evoral::EventTime* time, Evoral::EventType* type, uint32_t* size);
	inline bool read_contents(uint32_t size, uint8_t* buf);

	size_t read(MidiBuffer& dst, nframes_t start, nframes_t end, nframes_t offset=0);
	
	/** Set the channel filtering mode.
	 * @param mask If mode is FilterChannels, each bit represents a midi channel:
	 *     bit 0 = channel 0, bit 1 = channel 1 etc. the read and write methods will only
	 *     process events whose channel bit is 1.
	 *     If mode is ForceChannel, mask is simply a channel number which all events will
	 *     be forced to while reading.
	 */
	void set_channel_mode(ChannelMode mode, uint16_t mask) {
		g_atomic_int_set(&_channel_mask, ((uint16_t)mode << 16) | mask);
	}

	ChannelMode get_channel_mode() const {
		return static_cast<ChannelMode>((g_atomic_int_get(&_channel_mask) & 0xFFFF0000) >> 16);
	}
	
	uint16_t get_channel_mask() const {
		return static_cast<ChannelMode>((g_atomic_int_get(&_channel_mask) & 0x0000FFFF));
	}
	
protected:
	inline bool is_channel_event(uint8_t event_type_byte) {
		// mask out channel information
		event_type_byte &= 0xF0;
		// midi channel events range from 0x80 to 0xE0
		return (0x80 <= event_type_byte) && (event_type_byte <= 0xE0);
	}
	
private:
	volatile uint32_t _channel_mask; // 16 bits mode, 16 bits mask
};


/** Read the time and size of an event.  This call MUST be immediately proceeded
 * by a call to read_contents (or the read pointer will be garabage).
 */
inline bool
MidiRingBuffer::read_prefix(Evoral::EventTime* time, Evoral::EventType* type, uint32_t* size)
{
	bool success = Evoral::EventRingBuffer::full_read(sizeof(Evoral::EventTime), (uint8_t*)time);
	if (success)
		success = Evoral::EventRingBuffer::full_read(sizeof(Evoral::EventType), (uint8_t*)type);
	if (success)
		success = Evoral::EventRingBuffer::full_read(sizeof(uint32_t), (uint8_t*)size);

	return success;
}


/** Read the contenst of an event.  This call MUST be immediately preceeded
 * by a call to read_prefix (or the returned even will be garabage).
 */
inline bool
MidiRingBuffer::read_contents(uint32_t size, uint8_t* buf)
{
	return Evoral::EventRingBuffer::full_read(size, buf);
}


/** Read a block of MIDI events from buffer.
 *
 * Timestamps of events returned are relative to start (ie event with stamp 0
 * occurred at start), with offset added.
 */
inline size_t
MidiRingBuffer::read(MidiBuffer& dst, nframes_t start, nframes_t end, nframes_t offset)
{
	if (read_space() == 0) {
		//std::cerr << "MRB: NO READ SPACE" << std::endl;
		return 0;
	}

	Evoral::EventTime ev_time;
	Evoral::EventType ev_type;
	uint32_t          ev_size;

	size_t count = 0;

	//std::cerr << "MRB read " << start << " .. " << end << " + " << offset << std::endl;

	while (read_space() >= sizeof(Evoral::EventTime) + sizeof(Evoral::EventType) + sizeof(uint32_t)) {

		full_peek(sizeof(Evoral::EventTime), (uint8_t*)&ev_time);

		if (ev_time > end) {
			//std::cerr << "MRB: PAST END (" << ev_time << " : " << end << ")" << std::endl;
			break;
		} else if (ev_time < start) {
			//std::cerr << "MRB (start " << start << ") - Skipping event at (too early) time " << ev_time << std::endl;
			break;
		}

		bool success = read_prefix(&ev_time, &ev_type, &ev_size);
		if (!success) {
			std::cerr << "WARNING: error reading event prefix from MIDI ring" << std::endl;
			continue;
		}

		// This event marks a loop happening. this means that
		// the next events timestamp will be non-monotonic.
		if (ev_type == LoopEventType) {
			ev_time -= start;
			ev_time += offset;
			Evoral::MIDIEvent loopevent(LoopEventType, ev_time); 
			dst.push_back(loopevent);

			
			// We can safely return, without reading the data, because
			// a LoopEvent does not have data.
			return count + 1;
		}

		uint8_t status;
		success = full_peek(sizeof(uint8_t), &status);
		assert(success); // If this failed, buffer is corrupt, all hope is lost

		// Ignore event if it doesn't match channel filter
		if (is_channel_event(status) && get_channel_mode() == FilterChannels) {
			const uint8_t channel = status & 0x0F;
			if ( !(get_channel_mask() & (1L << channel)) ) {
				//std::cerr << "MRB skipping event due to channel mask" << std::endl;
				skip(ev_size); // Advance read pointer to next event
				continue;
			}
		}

		//std::cerr << "MRB " << this << " - Reading event, time = "
		//	<< ev_time << " - " << start << " => " << ev_time - start
		//	<< ", size = " << ev_size << std::endl;

		assert(ev_time >= start);
		ev_time -= start;
		ev_time += offset;

		uint8_t* write_loc = dst.reserve(ev_time, ev_size);
		if (write_loc == NULL) {
			//std::cerr << "MRB: Unable to reserve space in buffer, event skipped";
			continue;
		}

		success = Evoral::EventRingBuffer::full_read(ev_size, write_loc);

		if (success) {
			if (is_channel_event(status) && get_channel_mode() == ForceChannel) {
				write_loc[0] = (write_loc[0] & 0xF0) | (get_channel_mask() & 0x0F);
			}
			++count;
			//std::cerr << "MRB - read event at time " << ev_time << std::endl;
		} else {
			std::cerr << "WARNING: error reading event contents from MIDI ring" << std::endl;
		}
	}
	
	//std::cerr << "MTB read space: " << read_space() << std::endl;

	return count;
}


} // namespace ARDOUR

#endif // __ardour_midi_ring_buffer_h__

