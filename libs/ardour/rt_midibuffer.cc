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

#include "evoral/midi_util.h"

#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/rt_midibuffer.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

RTMidiBuffer::RTMidiBuffer (size_t capacity)
	: _size (0)
	, _capacity (0)
	, _data (0)
	, _pool_size (0)
	, _pool_capacity (0)
	, _pool (0)
{
	if (capacity) {
		resize (capacity);
	}
}

RTMidiBuffer::~RTMidiBuffer()
{
	cache_aligned_free (_data);
	cache_aligned_free (_pool);
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

	Item* old_data = _data;

	cache_aligned_malloc ((void**) &_data, size * sizeof (Item));

	if (_size) {
		memcpy (_data, old_data, _size);
		cache_aligned_free (old_data);
	}

	_capacity = size;
}

void
RTMidiBuffer::dump (uint32_t cnt)
{
	for (uint32_t i = 0; i < _size && i < cnt; ++i) {

		Item* item = &_data[i];
		uint8_t* addr;
		uint32_t size;

		if (item->bytes[0]) {

			/* more than 3 bytes ... indirect */

			uint32_t offset = item->offset & ~(1<<(sizeof(uint8_t)-1));
			Blob* blob = reinterpret_cast<Blob*> (&_pool[offset]);

			size = blob->size;
			addr = blob->data;

		} else {

			size = Evoral::midi_event_size (item->bytes[1]);
			addr = &item->bytes[1];

		}

		cerr << "@ " << item->timestamp << " sz=" << size << '\t';

		cerr << hex;
		for (size_t j =0 ; j < size; ++j) {
			cerr << "0x" << hex << (int)addr[j] << dec << '/' << (int)addr[i] << ' ';
		}
		cerr << dec << endl;
	}
}

uint32_t
RTMidiBuffer::write (TimeType time, Evoral::EventType /*type*/, uint32_t size, const uint8_t* buf)
{
	/* This buffer stores only MIDI, we don't care about the value of "type" */

	if (_size == _capacity) {
		resize (_capacity + 1024); // XXX 1024 is completely arbitrary
	}

	_data[_size].timestamp = time;

	if (size > 3) {

		uint32_t off = store_blob (size, buf);

		/* this indicates that the data (more than 3 bytes) is not inline */
		_data[_size].offset = (off | (1<<(sizeof(uint8_t)-1)));

	} else {

		assert ((int) size == Evoral::midi_event_size (buf[0]));

		/* this indicates that the data (up to 3 bytes) is inline */
		_data[_size].bytes[0] = 0;

		switch (size) {
		case 3:
			_data[_size].bytes[3] = buf[2];
			/* fallthru */
		case 2:
			_data[_size].bytes[2] = buf[1];
			/* fallthru */
		case 1:
			_data[_size].bytes[1] = buf[0];
			break;
		}
	}

	++_size;

	return size;
}

static
bool
item_timestamp_earlier (ARDOUR::RTMidiBuffer::Item const & item, samplepos_t time)
{
	return item.timestamp < time;
}



uint32_t
RTMidiBuffer::read (MidiBuffer& dst, samplepos_t start, samplepos_t end, MidiStateTracker& tracker, samplecnt_t offset)
{
	Item* iend = _data+_size;
	Item* item = lower_bound (_data, iend, start, item_timestamp_earlier);
	uint32_t count = 0;

#ifndef NDEBUG
	TimeType unadjusted_time;
#endif

	DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("read from %1 .. %2 .. initial index = %3 (time = %4)\n", start, end, item, item->timestamp));

	while ((item < iend) && (item->timestamp < end)) {

		TimeType evtime = item->timestamp;

#ifndef NDEBUG
		unadjusted_time = evtime;
#endif
		/* Adjust event times to be relative to 'start', taking
		 * 'offset' into account.
		 */

		evtime -= start;
		evtime += offset;

		uint32_t size;
		uint8_t* addr;

		if (item->bytes[0]) {

			/* more than 3 bytes ... indirect */

			uint32_t offset = item->offset & ~(1<<(sizeof(uint8_t)-1));
			Blob* blob = reinterpret_cast<Blob*> (&_pool[offset]);

			size = blob->size;
			addr = blob->data;

		} else {

			size = Evoral::midi_event_size (item->bytes[1]);
			addr = &item->bytes[1];

		}

		uint8_t* write_loc = dst.reserve (evtime, size);

		if (write_loc == 0) {
			DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("MidiRingBuffer: overflow in destination MIDI buffer, stopped after %1 events, dst size = %2\n", count, dst.size()));
			break;
		}

		memcpy (write_loc, addr, size);

		DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("read event sz %1 @ %2\n", size, unadjusted_time));
		tracker.track (addr);

		++item;
		++count;
	}

	DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("total events found for %1 .. %2 = %3\n", start, end, count));
	return count;
}

uint32_t
RTMidiBuffer::alloc_blob (uint32_t size)
{
	if (_pool_size + size > _pool_capacity) {
		uint8_t* old_pool = _pool;

		_pool_capacity += size * 4;

		cache_aligned_malloc ((void **) &_pool, _pool_capacity * 2);
		memcpy (_pool, old_pool, _pool_size);
		cache_aligned_free (old_pool);
	}

	uint32_t offset = _pool_size;
	_pool_size += size;

	return offset;
}

uint32_t
RTMidiBuffer::store_blob (uint32_t size, uint8_t const * data)
{
	uint32_t offset = alloc_blob (size);
	uint8_t* addr = &_pool[offset];

	*(reinterpret_cast<uint32_t*> (addr)) = size;
	addr += sizeof (size);
	memcpy (addr, data, size);

	return offset;
}

void
RTMidiBuffer::clear ()
{
	/* mark main array as empty */
	_size = 0;
	/* free the entire current pool size, if any */
	_pool_size = 0;
}
