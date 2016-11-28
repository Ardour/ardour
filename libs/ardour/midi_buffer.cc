/*
    Copyright (C) 2006-2007 Paul Davis
    Author: David Robillard

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
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
MidiBuffer::resize(size_t size)
{
	if (_data && size < _capacity) {

		if (_size < size) {
			/* truncate */
			_size = size;
		}

		return;
	}

	cache_aligned_free (_data);

	cache_aligned_malloc ((void**) &_data, size);

	_size = 0;
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


/** Read events from @a src starting at time @a offset into the START of this buffer, for
 * time duration @a nframes.  Relative time, where 0 = start of buffer.
 *
 * Note that offset and nframes refer to sample time, NOT buffer offsets or event counts.
 */
void
MidiBuffer::read_from (const Buffer& src, framecnt_t nframes, frameoffset_t dst_offset, frameoffset_t /* src_offset*/)
{
	assert (src.type() == DataType::MIDI);
	assert (&src != this);

	const MidiBuffer& msrc = (const MidiBuffer&) src;

	assert (_capacity >= msrc.size());

	if (dst_offset == 0) {
		clear ();
		assert (_size == 0);
	}

	for (MidiBuffer::const_iterator i = msrc.begin(); i != msrc.end(); ++i) {
		const Evoral::Event<TimeType>* ev = *i;

		if (dst_offset >= 0) {
			/* Positive offset: shifting events from internal
			   buffer view of time (always relative to to start of
			   current possibly split cycle) to from global/port
			   view of time (always relative to start of process
			   cycle).

			   Check it is within range of this (split) cycle, then shift.
			*/
			if (ev->time() >= 0 && ev->time() < nframes) {
				Evoral::Event<TimeType> new_time (Evoral::MIDI_EVENT, ev->time() + dst_offset, ev->size(), ev->buffer());
				push_back (new_time);
			} else {
				cerr << "\t!!!! MIDI event @ " <<  ev->time() << " skipped, not within range 0 .. " << nframes << ": ";
			}
		} else {
			/* Negative offset: shifting events from global/port
			   view of time (always relative to start of process
			   cycle) back to internal buffer view of time (always
			   relative to to start of current possibly split
			   cycle.

			   Shift first, then check it is within range of this
			   (split) cycle.
			*/
			const framepos_t evtime = ev->time() + dst_offset;

			if (evtime >= 0 && evtime < nframes) {
				Evoral::Event<TimeType> new_time (Evoral::MIDI_EVENT, evtime, ev->size(), ev->buffer());
				push_back (new_time);
			} else {
				cerr << "\t!!!! MIDI event @ " <<  evtime << " (based on " << ev->time() << " + " << dst_offset << ") skipped, not within range 0 .. " << nframes << ": ";
			}
		}
	}

	_silent = src.silent();
}

void
MidiBuffer::merge_from (const Buffer& src, framecnt_t /*nframes*/, frameoffset_t /*dst_offset*/, frameoffset_t /*src_offset*/)
{
	const MidiBuffer* mbuf = dynamic_cast<const MidiBuffer*>(&src);
	assert (mbuf);
	assert (mbuf != this);

	/* XXX use nframes, and possible offsets */
	merge_in_place (*mbuf);
}

/** Push an event into the buffer.
 *
 * Note that the raw MIDI pointed to by ev will be COPIED and unmodified.
 * That is, the caller still owns it, if it needs freeing it's Not My Problem(TM).
 * Realtime safe.
 * @return false if operation failed (not enough room)
 */
bool
MidiBuffer::push_back (const Evoral::Event<TimeType>& ev)
{
#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::MidiIO)) {
		DEBUG_STR_DECL(a);
		DEBUG_STR_APPEND(a, string_compose ("midibuffer %1 push event @ %2 sz %3 ", this, ev.time(), ev.size()));
		for (size_t i=0; i < ev.size(); ++i) {
			DEBUG_STR_APPEND(a,hex);
			DEBUG_STR_APPEND(a,"0x");
			DEBUG_STR_APPEND(a,(int)ev.buffer()[i]);
			DEBUG_STR_APPEND(a,' ');
		}
		DEBUG_STR_APPEND(a,'\n');
		DEBUG_TRACE (DEBUG::MidiIO, DEBUG_STR(a).str());
	}
#endif

	/* Event<T> uses the C zero-sized buffer hack to place data
	   contiguously "after" itself.
	*/

	if (_size + ev.object_size() >= _capacity) {
		return false;
	}

	if (!Evoral::midi_event_is_valid (ev.buffer(), ev.size())) {
		return false;
	}

	memcpy (_data + _size, &ev, ev.object_size());
	_size += ev.object_size();
	_silent = false;

	return true;
}

bool
MidiBuffer::push_back(TimeType time, size_t size, const uint8_t* data)
{
	/* Event<T> uses the C zero-sized buffer hack to place data
	   contiguously "after" itself.
	*/

	if (_size + sizeof (Evoral::Event<TimeType>) + size >= _capacity) {
		return false;
	}

	if (!Evoral::midi_event_is_valid (data, size)) {
		return false;
	}

	/* stack-allocated event of zero size */
	Evoral::Event<TimeType> ev (Evoral::MIDI_EVENT, time, 0, 0);
	/* fake adjust size so that what we copy with memcpy is correct */
	ev.set_size (size);
	/* copy C++ object */
	memcpy (_data + size, &ev, sizeof (ev));
	/* copy data */
	memcpy (_data + size + sizeof (ev), data, size);
	_size += sizeof (ev) + size;
	_silent = false;

	return true;
}

bool
MidiBuffer::insert_event(const Evoral::Event<TimeType>& ev)
{
	if (size() == 0) {
		return push_back(ev);
	}

	if (_size + ev.object_size() >= _capacity) {
		cerr << "MidiBuffer::push_back failed (buffer is full)" << endl;
		PBD::stacktrace (cerr, 20);
		return false;
	}

	MidiBuffer::iterator m = begin();

	while (m != end()) {
		if (!(*m)->time_order_before (ev)) {
			break;
		}
		++m;
	}

	if (m == end()) {
		return push_back (ev);
	}

	/* move all data from m.offset later in the buffer to make room for the
	   new event.
	*/
	memmove (_data + m.offset + (*m)->object_size(), _data + m.offset, _size - m.offset);

	/* merge the new event */
	memcpy (_data + m.offset, &ev, ev.object_size());
	_size += ev.object_size();

	return true;
}

uint32_t
MidiBuffer::write (TimeType time, Evoral::EventType type, uint32_t size, const uint8_t* buf)
{
	if (insert_event (Evoral::Event<TimeType>(type, time, size, const_cast<uint8_t*>(buf)))) {
		return size;
	}
	return 0;
}

/** Reserve space for a new event in the buffer.
 *
 * This call is for copying MIDI directly into the buffer, the data location
 * (of sufficient size to write \a size bytes) is returned, or 0 on failure.
 * This call MUST be immediately followed by a write to the returned data
 * location, or the buffer will be corrupted and very nasty things will happen.
 */
uint8_t*
MidiBuffer::reserve (size_t object_size)
{
	uint8_t * const write_loc = _data + _size;
	_size += object_size;
	_silent = false;
	return write_loc;
}


void
MidiBuffer::silence (framecnt_t /*nframes*/, framecnt_t /*offset*/)
{
	/* XXX iterate over existing events, find all in range given by offset & nframes,
	   and delete them.
	*/

	_size = 0;
	_silent = true;
}

/** Merge \a other into this buffer.  Realtime safe. */
bool
MidiBuffer::merge_in_place (const MidiBuffer &other)
{
	if (other.size() && size()) {
		DEBUG_TRACE (DEBUG::MidiIO, string_compose ("merge in place, sizes %1/%2\n", size(), other.size()));
	}

	if (other.size() == 0) {
		return true;
	}

	if (size() == 0) {
		copy (other);
		return true;
	}

	if (size() + other.size() > _capacity) {
		return false;
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

		while (them != other.end() && (*them)->time_order_before (**us)) {
			if (merge_offset == -1) {
				merge_offset = them.offset;
			}
			bytes_to_merge += (*them)->object_size();
			++them;
		}

		/* "them" now points to either:
		 *
		 * 1) an event that has the same or later timestamp or semantic
		 *        ordering than the *        event pointed to by "us"
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

		if ((*us)->time() == (*them)->time()) {

			DEBUG_TRACE (DEBUG::MidiIO,
				     string_compose ("simultaneous MIDI events discovered during merge, times %1/%2 status %3/%4\n",
						     (*us)->time(), (*them)->time(),
						     (int) *(_data + us.offset + sizeof (TimeType)),
						     (int) *(other._data + them.offset + sizeof (TimeType))));

			bool them_first = (*them)->time_order_before (**us);

			DEBUG_TRACE (DEBUG::MidiIO, string_compose ("other message came first ? %1\n", them_first));

			if (!them_first) {
				/* skip past our own event */
				++us;
			}

			bytes_to_merge = (*them)->object_size();

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

			while (us != end() && (*us)->time_order_before (**them)) {
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
