/*
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2012 Hans Baier <hansfbaier@googlemail.com>
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

#ifndef __ardour_midi_ring_buffer_h__
#define __ardour_midi_ring_buffer_h__

#include <iostream>
#include <algorithm>

#include "ardour/event_ring_buffer.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
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
class /*LIBARDOUR_API*/ MidiRingBuffer : public EventRingBuffer<T> {
public:
	/** @param size Size in bytes. */
	MidiRingBuffer(size_t size) : EventRingBuffer<T>(size) {}

	inline bool read_prefix(T* time, Evoral::EventType* type, uint32_t* size);
	inline bool read_contents(uint32_t size, uint8_t* buf);

	size_t read(MidiBuffer& dst, samplepos_t start, samplepos_t end, samplecnt_t offset=0, bool stop_on_overflow_in_destination=false);
	size_t skip_to(samplepos_t start);

	void dump(std::ostream& dst);
	void flush (samplepos_t start, samplepos_t end);

	void reset_tracker ();
	void resolve_tracker (MidiBuffer& dst, samplepos_t);
	void resolve_tracker (Evoral::EventSink<samplepos_t>& dst, samplepos_t);

private:
	MidiNoteTracker _tracker;
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

