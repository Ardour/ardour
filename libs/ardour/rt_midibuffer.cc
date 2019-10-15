/*
 * Copyright (C) 2019 Paul Davis <paul@linuxaudiosystems.com>
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

#include <iostream>

#include "pbd/malign.h"
#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/debug.h"
#include "pbd/stacktrace.h"

#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/rt_midibuffer.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

RTMidiBuffer::RTMidiBuffer (size_t capacity)
	: _size (0)
	, _capacity (0)
	, _data (0)
{
	if (capacity) {
		resize (capacity);
		clear ();
	}
}

RTMidiBuffer::~RTMidiBuffer()
{
	cache_aligned_free (_data);
}

void
RTMidiBuffer::resize (size_t size)
{
	if (_data && size < _capacity) {

		if (_size < size) {
			/* truncate */
			_size = size;
		}

		return;
	}

	uint8_t* old_data = _data;

	cache_aligned_malloc ((void**) &_data, size);

	if (_size) {
		memcpy (_data, old_data, _size);
	}

	cache_aligned_free (old_data);
	_capacity = size;

	assert(_data);
}

uint32_t
RTMidiBuffer::write (TimeType time, Evoral::EventType /*type*/, uint32_t size, const uint8_t* buf)
{
	/* This buffer stores only MIDI, we don't care about the value of "type" */

	const size_t bytes_to_merge = sizeof (size) + size;

	if (_size + bytes_to_merge > _capacity) {
		resize (_capacity + 8192); // XXX 8192 is completely arbitrary
	}

	_map.insert (make_pair (time, _size));

	uint8_t* addr = &_data[_size];

	*(reinterpret_cast<uint32_t*>(addr)) = size;
	addr += sizeof (size);
	memcpy (addr, buf, size);

	_size += bytes_to_merge;

	return size;
}

uint32_t
RTMidiBuffer::read (MidiBuffer& dst, samplepos_t start, samplepos_t end, samplecnt_t offset)
{
	Map::iterator iter = _map.lower_bound (start);
	uint32_t count = 0;
#ifndef NDEBUG
	TimeType unadjusted_time;
#endif

	DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("read from %1 .. %2\n", start, end));

	while ((iter != _map.end()) && (iter->first < end)) {

		/* the event consists of a size followed by bytes of MIDI
		 * data. It begins at _data[iter->second], which was stored in
		 * our map when we wrote the event into the data structure.
		 */

		uint8_t* addr = &_data[iter->second];
		TimeType evtime = iter->first;

#ifndef NDEBUG
		unadjusted_time = evtime;
#endif
		uint32_t size = *(reinterpret_cast<Evoral::EventType*>(addr));
		addr += sizeof (size);

		/* Adjust event times to be relative to 'start', taking
		 * 'offset' into account.
		 */

		evtime -= start;
		evtime += offset;

		uint8_t* write_loc = dst.reserve (evtime, size);

		if (write_loc == 0) {
			DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("MidiRingBuffer: overflow in destination MIDI buffer, stopped after %1 events, dst size = %2\n", count, dst.size()));
			cerr << string_compose ("MidiRingBuffer: overflow in destination MIDI buffer, stopped after %1 events, dst size = %1\n", count, dst.size()) << endl;
			break;
		}

		DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("read event sz %1 @ %2\n", size, unadjusted_time));

		memcpy (write_loc, addr, size);

		++iter;
		++count;
	}

	DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("total events found for %1 .. %2 = %3\n", start, end, count));
	return count;
}
