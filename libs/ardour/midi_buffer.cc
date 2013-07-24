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

#include "ardour/debug.h"
#include "ardour/midi_buffer.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

// FIXME: mirroring for MIDI buffers?
MidiBuffer::MidiBuffer(size_t capacity)
	: Buffer(DataType::MIDI, capacity)
	, _data(0)
{
	if (capacity) {
		resize(_capacity);
		silence(_capacity);
	}
}

MidiBuffer::~MidiBuffer()
{
	free(_data);
}

void
MidiBuffer::resize(size_t size)
{
	assert(size > 0);

	if (size < _capacity) {
		return;
	}

	free(_data);

	_size = 0;
	_capacity = size;
	cache_aligned_malloc ((void**) &_data, _capacity);

	assert(_data);
}

void
MidiBuffer::copy(const MidiBuffer& copy)
{
	assert(_capacity >= copy._size);
	_size = copy._size;
	memcpy(_data, copy._data, copy._size);
}


/** Read events from @a src starting at time @a offset into the START of this buffer, for
 * time duration @a nframes.  Relative time, where 0 = start of buffer.
 *
 * Note that offset and nframes refer to sample time, NOT buffer offsets or event counts.
 */
void
MidiBuffer::read_from (const Buffer& src, framecnt_t nframes, framecnt_t dst_offset, framecnt_t src_offset)
{
	assert (src.type() == DataType::MIDI);
	assert (&src != this);

	const MidiBuffer& msrc = (const MidiBuffer&) src;

	assert (_capacity >= msrc.size());

	if (dst_offset == 0) {
		clear ();
		assert (_size == 0);
	}

	/* XXX use dst_offset somehow */

	for (MidiBuffer::const_iterator i = msrc.begin(); i != msrc.end(); ++i) {
		const Evoral::MIDIEvent<TimeType> ev(*i, false);
		if (ev.time() >= src_offset && ev.time() < (nframes+src_offset)) {
			push_back (ev);
		} else {
			cerr << "MIDI event @ " <<  ev.time() << " skipped, not within range "
			     << src_offset << " .. " << (nframes + src_offset) << endl;
		}
	}

	_silent = src.silent();
}

void
MidiBuffer::merge_from (const Buffer& src, framecnt_t /*nframes*/, framecnt_t /*dst_offset*/, framecnt_t /*src_offset*/)
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
MidiBuffer::push_back(const Evoral::MIDIEvent<TimeType>& ev)
{
	const size_t stamp_size = sizeof(TimeType);

	if (_size + stamp_size + ev.size() >= _capacity) {
		cerr << "MidiBuffer::push_back failed (buffer is full)" << endl;
		return false;
	}

	if (!Evoral::midi_event_is_valid(ev.buffer(), ev.size())) {
		cerr << "WARNING: MidiBuffer ignoring illegal MIDI event" << endl;
		return false;
	}

	push_back(ev.time(), ev.size(), ev.buffer());

	return true;
}


/** Push an event into the buffer.
 * @return false if operation failed (not enough room)
 */
bool
MidiBuffer::push_back(TimeType time, size_t size, const uint8_t* data)
{
	const size_t stamp_size = sizeof(TimeType);

#ifndef NDEBUG
	if (DEBUG::MidiIO & PBD::debug_bits) {
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

	if (_size + stamp_size + size >= _capacity) {
		cerr << "MidiBuffer::push_back failed (buffer is full)" << endl;
		return false;
	}

	if (!Evoral::midi_event_is_valid(data, size)) {
		cerr << "WARNING: MidiBuffer ignoring illegal MIDI event" << endl;
		return false;
	}

	uint8_t* const write_loc = _data + _size;
	*((TimeType*)write_loc) = time;
	memcpy(write_loc + stamp_size, data, size);

	_size += stamp_size + size;
	_silent = false;

	return true;
}

/** Reserve space for a new event in the buffer.
 *
 * This call is for copying MIDI directly into the buffer, the data location
 * (of sufficient size to write \a size bytes) is returned, or 0 on failure.
 * This call MUST be immediately followed by a write to the returned data
 * location, or the buffer will be corrupted and very nasty things will happen.
 */
uint8_t*
MidiBuffer::reserve(TimeType time, size_t size)
{
	const size_t stamp_size = sizeof(TimeType);
	if (_size + stamp_size + size >= _capacity) {
		return 0;
	}

	// write timestamp
	uint8_t* write_loc = _data + _size;
	*((TimeType*)write_loc) = time;

	// move write_loc to begin of MIDI buffer data to write to
	write_loc += stamp_size;

	_size += stamp_size + size;
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

		while (them != other.end() && (*them).time() < (*us).time()) {
			if (merge_offset == -1) {
				merge_offset = them.offset;
			}
			bytes_to_merge += sizeof (TimeType) + (*them).size();
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
						     (int) *(_data + us.offset + sizeof (TimeType)),
						     (int) *(other._data + them.offset + sizeof (TimeType))));
			
			uint8_t our_midi_status_byte = *(_data + us.offset + sizeof (TimeType));
			uint8_t their_midi_status_byte = *(other._data + them.offset + sizeof (TimeType));
			bool them_first = second_simultaneous_midi_byte_is_first (our_midi_status_byte, their_midi_status_byte);
			
			DEBUG_TRACE (DEBUG::MidiIO, string_compose ("other message came first ? %1\n", them_first));
			
			if (!them_first) {
				/* skip past our own event */
				++us;
			}
				
			bytes_to_merge = sizeof (TimeType) + (*them).size();
			
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

