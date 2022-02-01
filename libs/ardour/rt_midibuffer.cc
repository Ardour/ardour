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
#include <algorithm>    // std::reverse

#include "pbd/malign.h"
#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/debug.h"

#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/midi_state_tracker.h"
#include "ardour/rt_midibuffer.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

RTMidiBuffer::RTMidiBuffer ()
	: _size (0)
	, _capacity (0)
	, _data (0)
	, _reversed (false)
	, _pool_size (0)
	, _pool_capacity (0)
	, _pool (0)
{
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
		assert (old_data);
		memcpy (_data, old_data, _size * sizeof (Item));
		cache_aligned_free (old_data);
	}

	_capacity = size;
}

bool
RTMidiBuffer::reversed () const
{
	return _reversed;
}

void
RTMidiBuffer::reverse ()
{
	if (_size == 0) {
		return;
	}

	Item* previous_note_on[16][128];
	uint8_t note_num;
	uint8_t channel;
	int32_t i;

	memset (previous_note_on, 0, sizeof (Item*) * 16 * 128);

	if (_reversed) {
		i = _size - 1;
	} else {
		i = 0;
	}

	/* iterate from start to end, or end-to-start, depending on current
	 * _reversed status. Find each note on, and swap it with the relevant
	 * note off.
	 */

	while ((_reversed && (i >= 0)) || (!_reversed && (i < (int32_t) _size))) {

		Item* item = &_data[i];

		if (!item->bytes[0]) {
			/* event is 3 bytes or less, so regular MIDI data */
			switch (item->bytes[1] & 0xf0) { /* status byte */
			case MIDI_CMD_NOTE_ON:
				note_num = item->bytes[2];
				channel = item->bytes[1] & 0xf;
				if (!previous_note_on[channel][note_num]) {
					previous_note_on[channel][note_num] = item;
				} else {
					std::cerr << "error: note is already on! ... ignored\n";
				}
				break;
			case MIDI_CMD_NOTE_OFF: /* note off */
				note_num = item->bytes[2];
				channel = item->bytes[1] & 0xf;
				if (previous_note_on[channel][note_num]) {
					swap (item->bytes[1], previous_note_on[channel][note_num]->bytes[1]);
					previous_note_on[channel][note_num] = 0;
				} else {
					std::cerr << "discovered note off without preceding note on... ignored\n";
				}
				break;
			default:
				break;
			}
		}

		if (_reversed) {
			--i;
		} else {
			++i;
		}
	}

	_reversed = !_reversed;
}

void
RTMidiBuffer::dump (uint32_t cnt)
{
	cerr << this << " total items: " << _size << " within " << _capacity << " blob pool: " << _pool_capacity << " used " << _pool_size << endl;

	for (uint32_t i = 0; i < _size && i < cnt; ++i) {

		Item* item = &_data[i];
		uint8_t* addr;
		uint32_t size;

		if (item->bytes[0]) {

			/* more than 3 bytes ... indirect */

			uint32_t offset = item->offset & ~(1<<(CHAR_BIT-1));
			Blob* blob = reinterpret_cast<Blob*> (&_pool[offset]);

			size = blob->size;
			addr = blob->data;

		} else {

			/* MIDI data is in bytes[1..3] (variable depending on message type */
			size = Evoral::midi_event_size (item->bytes[1]);
			addr = &item->bytes[1];

		}

		cerr << i << " @ " << item->timestamp << " sz=" << size << '\t';

		cerr << hex;
		for (size_t j =0 ; j < size; ++j) {
			cerr << "0x" << hex << (int)addr[j] << dec << '/' << (int)addr[j] << ' ';
		}
		cerr << dec << endl;
	}
}

uint32_t
RTMidiBuffer::write (TimeType time, Evoral::EventType /*type*/, uint32_t size, const uint8_t* buf)
{
	/* This buffer stores only MIDI, we don't care about the value of "type" */

	if (_size + size >= _capacity) {
		if (size > 1024) {
			resize (_capacity + size + 1024); // XXX 1024 is completely arbitrary
		} else {
			resize (_capacity + 1024); // XXX 1024 is completely arbitrary
		}
	}

	_data[_size].timestamp = time;

	if (size > 3) {

		uint32_t off = store_blob (size, buf);

		/* non-zero MSbit indicates that the data (more than 3 bytes) is not inline */
		_data[_size].offset = (off | (1<<(CHAR_BIT-1)));

	} else {

		assert ((int) size == Evoral::midi_event_size (buf[0]));

		/* zero MSbit indicates that the data (up to 3 bytes) is inline */
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

/* requires C++20 to be usable */
/*
static
bool
item_timestamp_earlier (ARDOUR::RTMidiBuffer::Item const & item, samplepos_t const & time)
{
	return item.timestamp < time;
}
*/

static
bool
item_item_earlier (ARDOUR::RTMidiBuffer::Item const & item, ARDOUR::RTMidiBuffer::Item const & other)
{
	return item.timestamp < other.timestamp;
}

uint32_t
RTMidiBuffer::read (MidiBuffer& dst, samplepos_t start, samplepos_t end, MidiNoteTracker& tracker, samplecnt_t offset)
{
	Glib::Threads::RWLock::ReaderLock lm (_lock, Glib::Threads::TRY_LOCK);

	if (!lm.locked()) {
		return 0;
	}

	bool reverse;
	Item foo;
	Item* iend;
	Item* item;
	foo.timestamp = start;
	uint32_t count = 0;

	if (start < end) {
		iend = _data+_size;
		item = lower_bound (_data, iend, foo, item_item_earlier);
		reverse = false;
	} else {
		iend = _data;
		--iend; /* yes, this is technically "illegal" but we will never indirect */
		Item* uend = _data + _size;
		item = upper_bound (_data, uend, foo, item_item_earlier);

		if (item == uend) {
			--item;
		}

		reverse = true;
	}

#ifndef NDEBUG
	TimeType unadjusted_time;
	Item* last = &_data[_size-1];
#endif

	DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("read from %1 .. %2 .. initial index = %3 (time = %4) (range in list of  %7 %5..%6)\n", start, end, item - _data, item->timestamp, _data->timestamp, last->timestamp, _size));
	// dump (999);

	while ((item != iend) && ((reverse && (item->timestamp > end)) || (!reverse && (item->timestamp < end)))) {

		TimeType evtime = item->timestamp;

#ifndef NDEBUG
		unadjusted_time = evtime;
#endif
		/* Adjust event times to be relative to 'start', taking
		 * 'offset' into account.
		 */

		if (reverse) {
			if (evtime > start) {
				--item;
				continue;
			}
			evtime = start - evtime;
		} else {
			if (evtime < start) {
				++item;
				continue;
			}
			evtime -= start;
		}

		evtime += offset;

		uint32_t size;
		uint8_t* addr;

		if (item->bytes[0]) {

			/* more than 3 bytes ... indirect */

			uint32_t offset = item->offset & ~(1<<(CHAR_BIT-1));
			Blob* blob = reinterpret_cast<Blob*> (&_pool[offset]);

			size = blob->size;
			addr = blob->data;

		} else {

			size = Evoral::midi_event_size (item->bytes[1]);
			addr = &item->bytes[1];

		}

		if (!dst.push_back (evtime, Evoral::MIDI_EVENT, size, addr)) {
			DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("MidiRingBuffer: overflow in destination MIDI buffer, stopped after %1 events, dst size = %2\n", count, dst.size()));
			break;
		}

		DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("read event sz %1 @ %2 (=> %3 via -%4 +%5\n", size, unadjusted_time, evtime, start, offset));

#if 0
		cerr << "\tevent @ " << unadjusted_time << " evtime " << evtime << " off " << offset << " sz=" << size << '\t';
		cerr << "\t0x" << hex << (int)addr[0] << dec << ' ';
		for (size_t j = 1 ; j < size; ++j) {
			cerr << "0x" << hex << (int)addr[j] << dec << '/' << (int)addr[j] << ' ';
		}
		cerr << '\n';
#endif

		tracker.track (addr);

		if (reverse) {
			--item;
		} else {
			++item;
		}
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

		cache_aligned_malloc ((void **) &_pool, (_pool_capacity * sizeof (Blob)));
		memcpy (_pool, old_pool, _pool_size * sizeof (Blob));
		cache_aligned_free (old_pool);
	}

	uint32_t offset = _pool_size;
#if defined(__arm__) || defined(__aarch64_)
		_pool_size += ((size - 1) | 3) + 1;
#else
		_pool_size += size;
#endif

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
	/* rendering new data .. it will not be reversed */
	_reversed = false;
}

samplecnt_t
RTMidiBuffer::span() const
{
	if (_size == 0 || _size == 1) {
		return 0;
	}

	const Item* last = &_data[_size-1];
	const Item* first = &_data[0];

	return last->timestamp - first->timestamp;
}

