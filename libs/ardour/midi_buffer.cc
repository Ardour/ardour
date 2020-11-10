/*
 * Copyright (C) 2007-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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
#include "pbd/debug.h"
#include "pbd/stacktrace.h"

#include "ardour/debug.h"
#include "ardour/midi_buffer.h"
#include "ardour/port.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

// FIXME: mirroring for MIDI buffers?
MidiBuffer::MidiBuffer(size_t capacity)
	: Buffer (DataType::MIDI)
	, _data (0)
	, _size (0)
{
	if (capacity) {
		resize (capacity);
		silence (capacity);
	}
}

MidiBuffer::~MidiBuffer()
{
	cache_aligned_free(_data);
}

void
MidiBuffer::resize (size_t size)
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
		assert (old_data);
		memcpy (_data, old_data, _size);
	}

	cache_aligned_free (old_data);
	_capacity = size;

	assert(_data);
}

void
MidiBuffer::copy(const MidiBuffer& copy)
{
	assert(_capacity >= copy._size);
	_size = copy._size;
	memcpy(_data, copy._data, copy._size);
}

void
MidiBuffer::copy(MidiBuffer const * const copy)
{
	assert(_capacity >= copy->size ());
	_size = copy->size ();
	memcpy(_data, copy->_data, _size);
}


void
MidiBuffer::read_from (const Buffer& src, samplecnt_t nframes, sampleoffset_t dst_offset, sampleoffset_t src_offset)
{
	assert (src.type() == DataType::MIDI);
	assert (&src != this);

	const MidiBuffer& msrc = (const MidiBuffer&) src;

	assert (_capacity >= msrc.size());

	clear ();
	assert (_size == 0);

	for (MidiBuffer::const_iterator i = msrc.begin(); i != msrc.end(); ++i) {
		const Evoral::Event<TimeType> ev(*i, false);

		if (ev.time() >= src_offset && ev.time() < nframes + src_offset) {
			push_back (ev.time() + dst_offset - src_offset, ev.event_type (), ev.size(), ev.buffer());
		} else {
			cerr << "\t!!!! MIDI event @ " <<  ev.time()
			     << " skipped, not within range. nframes: " << nframes
			     << " src_offset: " << src_offset
			     << " dst_offset: " << dst_offset
			     << "\n";
			PBD::stacktrace (cerr, 30);
		}
	}

	_silent = src.silent();
}

void
MidiBuffer::merge_from (const Buffer& src, samplecnt_t /*nframes*/, sampleoffset_t /*dst_offset*/, sampleoffset_t /*src_offset*/)
{
	const MidiBuffer* mbuf = dynamic_cast<const MidiBuffer*>(&src);
	assert (mbuf);
	assert (mbuf != this);

	/* XXX use nframes, and possible offsets */
	if (!merge_in_place (*mbuf)) {
		cerr << string_compose ("MidiBuffer::merge_in_place failed (buffer is full: size: %1 capacity %2 new bytes %3)", _size, _capacity, mbuf->size()) << endl;
		PBD::stacktrace (cerr, 20);
	}
}

/** Push an event into the buffer.
 *
 * Note that the raw MIDI pointed to by ev will be COPIED and unmodified.
 * That is, the caller still owns it, if it needs freeing it's Not My Problem(TM).
 * Realtime safe.
 * @return false if operation failed (not enough room)
 */
bool
MidiBuffer::push_back(const Evoral::Event<TimeType>& ev)
{
	return push_back (ev.time(), ev.event_type (), ev.size(), ev.buffer());
}


/** Push MIDI data into the buffer.
 *
 * Note that the raw MIDI pointed to by @param data will be COPIED and unmodified.
 * That is, the caller still owns it, if it needs freeing it's Not My Problem(TM).
 * Realtime safe.
 * @return false if operation failed (not enough room)
 */
bool
MidiBuffer::push_back(TimeType time, Evoral::EventType event_type, size_t size, const uint8_t* data)
{
	const size_t stamp_size = sizeof(TimeType);
	const size_t etype_size = sizeof(Evoral::EventType);

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::MidiIO)) {
		DEBUG_STR_DECL(a);
		DEBUG_STR_APPEND(a, string_compose ("midibuffer %1 push event @ %2 sz %3 ", this, time, size));
		for (size_t i=0; i < size; ++i) {
			DEBUG_STR_APPEND(a,hex);
			DEBUG_STR_APPEND(a,"0x");
			DEBUG_STR_APPEND(a,(int)data[i]);
			DEBUG_STR_APPEND(a,' ');
		}
		DEBUG_STR_APPEND(a,'\n');
		DEBUG_TRACE (DEBUG::MidiIO, DEBUG_STR(a).str());
	}
#endif

	if (_size + stamp_size + etype_size + size >= _capacity) {
		return false;
	}

	if (!Evoral::midi_event_is_valid(data, size)) {
		return false;
	}

	uint8_t* const write_loc = _data + _size;
	*(reinterpret_cast<TimeType*>((uintptr_t)write_loc)) = time;
	*(reinterpret_cast<Evoral::EventType*>((uintptr_t)(write_loc + stamp_size))) = event_type;
	memcpy(write_loc + stamp_size + etype_size, data, size);

	_size += align32 (stamp_size + etype_size + size);
	_silent = false;

	return true;
}

extern PBD::Timing minsert;

bool
MidiBuffer::insert_event(const Evoral::Event<TimeType>& ev)
{
	if (size() == 0) {
		return push_back(ev);
	}

	const size_t stamp_size = sizeof(TimeType);
	const size_t etype_size = sizeof(Evoral::EventType);

	const size_t bytes_to_merge = align32 (stamp_size + etype_size + ev.size());

	if (_size + bytes_to_merge >= _capacity) {
		cerr << string_compose ("MidiBuffer::push_back failed (buffer is full: size: %1 capacity %2 new bytes %3)", _size, _capacity, bytes_to_merge) << endl;
		PBD::stacktrace (cerr, 20);
		return false;
	}

	TimeType t = ev.time();

	ssize_t insert_offset = -1;
	for (MidiBuffer::iterator m = begin(); m != end(); ++m) {
		if ((*m).time() < t) {
			continue;
		}
		if ((*m).time() == t) {
			const uint8_t our_midi_status_byte = *(_data + m.offset + stamp_size + etype_size);
			if (second_simultaneous_midi_byte_is_first (ev.type(), our_midi_status_byte)) {
				continue;
			}
		}
		insert_offset = m.offset;
		break;
	}

	if (insert_offset == -1) {
		bool r = push_back(ev);
		return r;
	}

	// don't use memmove - it may use malloc(!)
	// memmove (_data + insert_offset + bytes_to_merge, _data + insert_offset, _size - insert_offset);
	for (ssize_t a = _size + bytes_to_merge - 1, b = _size - 1; b >= insert_offset; --b, --a) {
		_data[a] = _data[b];
	}

	uint8_t* const write_loc = _data + insert_offset;
	assert((insert_offset + stamp_size + etype_size + ev.size()) <= _capacity);
	*(reinterpret_cast<TimeType*>((uintptr_t)write_loc)) = t;
	*(reinterpret_cast<Evoral::EventType*>((uintptr_t)(write_loc + stamp_size))) = ev.event_type ();
	memcpy(write_loc + stamp_size + etype_size, ev.buffer(), ev.size());

	_size += bytes_to_merge;

	return true;
}

uint32_t
MidiBuffer::write(TimeType time, Evoral::EventType type, uint32_t size, const uint8_t* buf)
{
	insert_event(Evoral::Event<TimeType>(type, time, size, const_cast<uint8_t*>(buf)));
	return size;
}

/** Reserve space for a new event in the buffer.
 *
 * This call is for copying MIDI directly into the buffer, the data location
 * (of sufficient size to write \a size bytes) is returned, or 0 on failure.
 * This call MUST be immediately followed by a write to the returned data
 * location, or the buffer will be corrupted and very nasty things will happen.
 */
uint8_t*
MidiBuffer::reserve(TimeType time, Evoral::EventType event_type, size_t size)
{
	const size_t stamp_size = sizeof(TimeType);
	const size_t etype_size = sizeof(Evoral::EventType);
	if (align32 (_size + stamp_size + etype_size + size) >= _capacity) {
		return 0;
	}

	// write timestamp and event-type
	uint8_t* write_loc = _data + _size;
	*(reinterpret_cast<TimeType*>((uintptr_t)write_loc)) = time;
	*(reinterpret_cast<Evoral::EventType*>((uintptr_t)(write_loc + stamp_size))) = event_type;

	// move write_loc to begin of MIDI buffer data to write to
	write_loc += stamp_size + etype_size;

	_size += align32 (stamp_size + etype_size + size);
	_silent = false;

	return write_loc;
}


void
MidiBuffer::silence (samplecnt_t /*nframes*/, samplecnt_t /*offset*/)
{
	/* XXX iterate over existing events, find all in range given by offset & nframes,
	   and delete them.
	*/

	_size = 0;
	_silent = true;
}

bool
MidiBuffer::second_simultaneous_midi_byte_is_first (uint8_t a, uint8_t b)
{
	bool b_first = false;

	/* two events at identical times. we need to determine
	   the order in which they should occur.

	   the rule is:

	   Controller messages
	   Program Change
	   Note Off
	   Note On
	   Note Pressure
	   Channel Pressure
	   Pitch Bend
	*/

	if ((a) >= 0xf0 || (b) >= 0xf0 || ((a & 0xf) != (b & 0xf))) {

		/* if either message is not a channel message, or if the channels are
		 * different, we don't care about the type.
		 */

		b_first = true;

	} else {

		switch (b & 0xf0) {
		case MIDI_CMD_CONTROL:
			b_first = true;
			break;

		case MIDI_CMD_PGM_CHANGE:
			switch (a & 0xf0) {
			case MIDI_CMD_CONTROL:
				break;
			case MIDI_CMD_PGM_CHANGE:
			case MIDI_CMD_NOTE_OFF:
			case MIDI_CMD_NOTE_ON:
			case MIDI_CMD_NOTE_PRESSURE:
			case MIDI_CMD_CHANNEL_PRESSURE:
			case MIDI_CMD_BENDER:
				b_first = true;
			}
			break;

		case MIDI_CMD_NOTE_OFF:
			switch (a & 0xf0) {
			case MIDI_CMD_CONTROL:
			case MIDI_CMD_PGM_CHANGE:
				break;
			case MIDI_CMD_NOTE_OFF:
			case MIDI_CMD_NOTE_ON:
			case MIDI_CMD_NOTE_PRESSURE:
			case MIDI_CMD_CHANNEL_PRESSURE:
			case MIDI_CMD_BENDER:
				b_first = true;
			}
			break;

		case MIDI_CMD_NOTE_ON:
			switch (a & 0xf0) {
			case MIDI_CMD_CONTROL:
			case MIDI_CMD_PGM_CHANGE:
			case MIDI_CMD_NOTE_OFF:
				break;
			case MIDI_CMD_NOTE_ON:
			case MIDI_CMD_NOTE_PRESSURE:
			case MIDI_CMD_CHANNEL_PRESSURE:
			case MIDI_CMD_BENDER:
				b_first = true;
			}
			break;
		case MIDI_CMD_NOTE_PRESSURE:
			switch (a & 0xf0) {
			case MIDI_CMD_CONTROL:
			case MIDI_CMD_PGM_CHANGE:
			case MIDI_CMD_NOTE_OFF:
			case MIDI_CMD_NOTE_ON:
				break;
			case MIDI_CMD_NOTE_PRESSURE:
			case MIDI_CMD_CHANNEL_PRESSURE:
			case MIDI_CMD_BENDER:
				b_first = true;
			}
			break;

		case MIDI_CMD_CHANNEL_PRESSURE:
			switch (a & 0xf0) {
			case MIDI_CMD_CONTROL:
			case MIDI_CMD_PGM_CHANGE:
			case MIDI_CMD_NOTE_OFF:
			case MIDI_CMD_NOTE_ON:
			case MIDI_CMD_NOTE_PRESSURE:
				break;
			case MIDI_CMD_CHANNEL_PRESSURE:
			case MIDI_CMD_BENDER:
				b_first = true;
			}
			break;
		case MIDI_CMD_BENDER:
			switch (a & 0xf0) {
			case MIDI_CMD_CONTROL:
			case MIDI_CMD_PGM_CHANGE:
			case MIDI_CMD_NOTE_OFF:
			case MIDI_CMD_NOTE_ON:
			case MIDI_CMD_NOTE_PRESSURE:
			case MIDI_CMD_CHANNEL_PRESSURE:
				break;
			case MIDI_CMD_BENDER:
				b_first = true;
			}
			break;
		}
	}

	return b_first;
}

/** Merge \a other into this buffer.  Realtime safe. */
bool
MidiBuffer::merge_in_place (const MidiBuffer &other)
{
	const size_t header_size = sizeof(TimeType) + sizeof(Evoral::EventType);

	if (other.size() && size()) {
		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("merge in place, sizes %1/%2\n", size(), other.size()));
	}

	if (other.size() == 0) {
		return true;
	}

	if (size() + other.size() > _capacity) {
		return false;
	}

	if (size() == 0) {
		copy (other);
		return true;
	}

	const_iterator them = other.begin();
	iterator us = begin();

	while (them != other.end()) {

		size_t bytes_to_merge;
		ssize_t merge_offset;

		/* gather up total size of events that are earlier than
		   the event referenced by "us"
		*/

		merge_offset = -1;
		bytes_to_merge = 0;

		while (them != other.end() && (*them).time() < (*us).time()) {
			if (merge_offset == -1) {
				merge_offset = them.offset;
			}
			bytes_to_merge += align32 (header_size + (*them).size());
			++them;
		}

		/* "them" now points to either:
		 *
		 * 1) an event that has the same or later timestamp than the
		 *        event pointed to by "us"
		 *
		 * OR
		 *
		 * 2) the end of the "other" buffer
		 *
		 * if "sz" is non-zero, there is data to be merged from "other"
		 * into this buffer before we do anything else, corresponding
		 * to the events from "other" that we skipped while advancing
		 * "them".
		 */

		if (bytes_to_merge) {
			assert(merge_offset >= 0);
			/* move existing */
			memmove (_data + us.offset + bytes_to_merge, _data + us.offset, _size - us.offset);
			/* increase _size */
			_size += bytes_to_merge;
			assert (_size <= _capacity);
			/* insert new stuff */
			memcpy  (_data + us.offset, other._data + merge_offset, bytes_to_merge);
			/* update iterator to our own events. this is a miserable hack */
			us.offset += bytes_to_merge;
		}

		/* if we're at the end of the other buffer, we're done */

		if (them == other.end()) {
			break;
		}

		/* if we have two messages messages with the same timestamp. we
		 * must order them correctly.
		 */

		if ((*us).time() == (*them).time()) {

			DEBUG_TRACE (DEBUG::MidiIO,
				     string_compose ("simultaneous MIDI events discovered during merge, times %1/%2 status %3/%4\n",
						     (*us).time(), (*them).time(),
						     (int) *(_data + us.offset + header_size),
						     (int) *(other._data + them.offset + header_size)));

			uint8_t our_midi_status_byte = *(_data + us.offset + header_size);
			uint8_t their_midi_status_byte = *(other._data + them.offset + header_size);
			bool them_first = second_simultaneous_midi_byte_is_first (our_midi_status_byte, their_midi_status_byte);

			DEBUG_TRACE (DEBUG::MidiIO, string_compose ("other message came first ? %1\n", them_first));

			if (!them_first) {
				/* skip past our own event */
				++us;
			}

			bytes_to_merge = align32 (header_size + (*them).size());

			/* move our remaining events later in the buffer by
			 * enough to fit the one message we're going to merge
			 */

			memmove (_data + us.offset + bytes_to_merge, _data + us.offset, _size - us.offset);
			/* increase _size */
			_size += bytes_to_merge;
			assert(_size <= _capacity);
			/* insert new stuff */
			memcpy  (_data + us.offset, other._data + them.offset, bytes_to_merge);
			/* update iterator to our own events. this is a miserable hack */
			us.offset += bytes_to_merge;
			/* 'us' is now an iterator to the event right after the
			   new ones that we merged
			*/
			if (them_first) {
				/* need to skip the event pointed to by 'us'
				   since its at the same time as 'them'
				   (still), and we'll enter
				*/

				if (us != end()) {
					++us;
				}
			}

			/* we merged one event from the other buffer, so
			 * advance the iterator there.
			 */

			++them;

		} else {

			/* advance past our own events to get to the correct insertion
			   point for the next event(s) from "other"
			*/

			while (us != end() && (*us).time() <= (*them).time()) {
				++us;
			}
		}

		/* check to see if we reached the end of this buffer while
		 * looking for the insertion point.
		 */

		if (us == end()) {

			/* just append the rest of other and we're done*/

			memcpy (_data + us.offset, other._data + them.offset, other._size - them.offset);
			_size += other._size - them.offset;
			assert(_size <= _capacity);
			break;
		}
	}

	return true;
}
