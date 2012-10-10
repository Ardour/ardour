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

#include "evoral/EventRingBuffer.hpp"

#include "ardour/types.h"
#include "ardour/buffer.h"
#include "ardour/midi_state_tracker.h"

namespace ARDOUR {

class MidiBuffer;

/** A RingBuffer for (MIDI) events.
 *
 * This is simply a wrapper around a raw ringbuffer which writes/reads events
 * as flat placked blobs.
 * The buffer looks like this:
 *
 * [timestamp][type][size][size bytes of raw MIDI][timestamp][type][size](etc...)
 */
template<typename T>
class MidiRingBuffer : public Evoral::EventRingBuffer<T> {
public:
	/** @param size Size in bytes.
	 */
	MidiRingBuffer(size_t size)
		: Evoral::EventRingBuffer<T>(size)
		, _channel_mask(0x0000FFFF)
	{}

	inline bool read_prefix(T* time, Evoral::EventType* type, uint32_t* size);
	inline bool read_contents(uint32_t size, uint8_t* buf);

	size_t read(MidiBuffer& dst, framepos_t start, framepos_t end, framecnt_t offset=0, bool stop_on_overflow_in_destination=false);
	void dump(std::ostream& dst);
	void flush (framepos_t start, framepos_t end);

	/** Set the channel filtering mode.
	 * @param mask If mode is FilterChannels, each bit represents a midi channel:
	 *     bit 0 = channel 0, bit 1 = channel 1 etc. the read and write methods will only
	 *     process events whose channel bit is 1.
	 *     If mode is ForceChannel, mask is simply a channel number which all events will
	 *     be forced to while reading.
	 */
	void set_channel_mode(ChannelMode mode, uint16_t mask) {
		g_atomic_int_set(&_channel_mask, (uint32_t(mode) << 16) | uint32_t(mask));
	}

	ChannelMode get_channel_mode() const {
		return static_cast<ChannelMode>((g_atomic_int_get(&_channel_mask) & 0xFFFF0000) >> 16);
	}

	uint16_t get_channel_mask() const {
		return g_atomic_int_get(&_channel_mask) & 0x0000FFFF;
	}

	void reset_tracker ();
	
protected:
	inline bool is_channel_event(uint8_t event_type_byte) {
		// mask out channel information
		event_type_byte &= 0xF0;
		// midi channel events range from 0x80 to 0xE0
		return (0x80 <= event_type_byte) && (event_type_byte <= 0xE0);
	}

	inline bool is_note_on(uint8_t event_type_byte) {
		// mask out channel information
		return (event_type_byte & 0xF0) == MIDI_CMD_NOTE_ON;
	}

	inline bool is_note_off(uint8_t event_type_byte) {
		// mask out channel information
		return (event_type_byte & 0xF0) == MIDI_CMD_NOTE_OFF;
	}

private:
	volatile uint32_t _channel_mask; // 16 bits mode, 16 bits mask
	MidiStateTracker _tracker;
};


/** Read the time and size of an event.  This call MUST be immediately proceeded
 * by a call to read_contents (or the read pointer will be garbage).
 */
template<typename T>
inline bool
MidiRingBuffer<T>::read_prefix(T* time, Evoral::EventType* type, uint32_t* size)
{
	if (PBD::RingBufferNPT<uint8_t>::read((uint8_t*)time, sizeof(T)) != sizeof (T)) {
		return false;
	}

	if (PBD::RingBufferNPT<uint8_t>::read((uint8_t*)type, sizeof(Evoral::EventType)) != sizeof (Evoral::EventType)) {
		return false;
	}

	if (PBD::RingBufferNPT<uint8_t>::read((uint8_t*)size, sizeof(uint32_t)) != sizeof (uint32_t)) {
		return false;
	}

	return true;
}


/** Read the content of an event.  This call MUST be immediately preceded
 * by a call to read_prefix (or the returned even will be garbage).
 */
template<typename T>
inline bool
MidiRingBuffer<T>::read_contents(uint32_t size, uint8_t* buf)
{
	return PBD::RingBufferNPT<uint8_t>::read(buf, size) == size;
}


} // namespace ARDOUR

#endif // __ardour_midi_ring_buffer_h__

