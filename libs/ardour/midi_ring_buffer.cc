/*
 * Copyright (C) 2008-2016 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#include "pbd/compose.h"
#include "pbd/enumwriter.h"
#include "pbd/error.h"

#include "ardour/debug.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_buffer.h"
#include "ardour/event_type_map.h"

using namespace std;
using namespace PBD;

namespace ARDOUR {

/** Read a block of MIDI events from this buffer into a MidiBuffer.
 *
 * Timestamps of events returned are relative to start (i.e. event with stamp 0
 * occurred at start), with offset added.
 */
template<typename T>
size_t
MidiRingBuffer<T>::read (MidiBuffer& dst, samplepos_t start, samplepos_t end, samplecnt_t offset, bool stop_on_overflow_in_dst)
{
	if (this->read_space() == 0) {
		return 0;
	}

	T                 ev_time;
	Evoral::EventType ev_type;
	uint32_t          ev_size;
	size_t            count = 0;
	const size_t      prefix_size = sizeof(T) + sizeof(Evoral::EventType) + sizeof(uint32_t);

	while (this->read_space() >= prefix_size) {

		uint8_t peekbuf[prefix_size];

		/* this cannot fail, because we've already verified that there
		 * is prefix_space to read
		 */
		this->peek (peekbuf, prefix_size);

		ev_time = *(reinterpret_cast<T*>((uintptr_t)peekbuf));
		ev_type = *(reinterpret_cast<Evoral::EventType*>((uintptr_t)(peekbuf + sizeof(T))));
		ev_size = *(reinterpret_cast<uint32_t*>((uintptr_t)(peekbuf + sizeof(T) + sizeof (Evoral::EventType))));

		/* check that we have both the prefix and the full event
		 *  present in the buffer before continuing. If not, we can't do
		 *  anything (and since we have only peek'ed at the buffer, it
		 * remains in the same state for the next read() call.
		 */
		if (this->read_space() < prefix_size + ev_size) {
			break;
		}

		if (ev_time >= end) {
			DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("MRB event @ %1 past end @ %2\n", ev_time, end));
			break;
		} else if (ev_time < start) {
			DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("MRB event @ %1 before start @ %2\n", ev_time, start));
			break;
		} else {
			DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("MRB event @ %1 in range %2 .. %3\n", ev_time, start, end));
		}

		ev_time -= start;
		ev_time += offset;

		/* we're good to go ahead and read the data now but since we
		 * have the prefix data already, just skip over that
		 */
		this->increment_read_ptr (prefix_size);

		uint8_t status;
		bool r = this->peek (&status, sizeof(uint8_t));
		assert (r); // If this failed, buffer is corrupt, all hope is lost

		/* lets see if we are going to be able to write this event into dst.
		 */
		uint8_t* write_loc = dst.reserve (ev_time, ev_type, ev_size);
		if (write_loc == 0) {
			/* nowhere to write to; we've already consumed the
			 * prefix for this event (and cannot backup). Consume the
			 * rest of the event (advances read pointer to next event)
			 */
			this->increment_read_ptr (ev_size);
			if (stop_on_overflow_in_dst) {
				DEBUG_TRACE (DEBUG::MidiRingBuffer, string_compose ("MidiRingBuffer: overflow in destination MIDI buffer, stopped after %1 events\n", count));
				break;
			}
			error << "MRB: Unable to reserve space in buffer, event skipped" << endmsg;
			continue;
		}

		// write MIDI buffer contents

		bool success = read_contents (ev_size, write_loc);
#ifndef NDEBUG
		if (DEBUG_ENABLED (DEBUG::MidiRingBuffer)) {
			DEBUG_STR_DECL(a);
			DEBUG_STR_APPEND(a, string_compose ("wrote MidiEvent to Buffer (time=%1, start=%2 offset=%3) ", ev_time, start, offset));
			for (size_t i=0; i < ev_size; ++i) {
				DEBUG_STR_APPEND(a,hex);
				DEBUG_STR_APPEND(a,"0x");
				DEBUG_STR_APPEND(a,(int)write_loc[i]);
				DEBUG_STR_APPEND(a,' ');
			}
			DEBUG_STR_APPEND(a,'\n');
			DEBUG_TRACE (DEBUG::MidiRingBuffer, DEBUG_STR(a).str());
		}
#endif
		if (success) {
			_tracker.track(write_loc);
			++count;
		} else {
			cerr << "WARNING: error reading event contents from MIDI ring" << endl;
		}
	}

	return count;
}

template<typename T>
size_t
MidiRingBuffer<T>::skip_to(samplepos_t start)
{
	if (this->read_space() == 0) {
		return 0;
	}

	T                 ev_time;
	uint32_t          ev_size;
	size_t            count = 0;
	const size_t      prefix_size = sizeof(T) + sizeof(Evoral::EventType) + sizeof(uint32_t);

	while (this->read_space() >= prefix_size) {

		uint8_t peekbuf[prefix_size];
		bool r = this->peek (peekbuf, prefix_size);
		assert (r);

		ev_time = *(reinterpret_cast<T*>((uintptr_t)peekbuf));
		ev_size = *(reinterpret_cast<uint32_t*>((uintptr_t)(peekbuf + sizeof(T) + sizeof (Evoral::EventType))));

		if (ev_time >= start) {
			return count;
		}

		if (this->read_space() < ev_size) {
			continue;
		}

		this->increment_read_ptr (prefix_size);

		uint8_t status;
		r = this->peek (&status, sizeof(uint8_t));
		assert (r); // If this failed, buffer is corrupt, all hope is lost

		++count;

		/* TODO investigate and think:
		 *
		 * Does it makes sense to keep track of notes
		 * that are skipped (because they're either too late
		 * (underrun) or never used (read-ahead, loop) ?
		 *
		 * skip_to() is called on the rinbuffer between
		 * disk and process. it seems wrong to track them
		 * (a potential synth never sees skipped notes, either)
		 * but there may be more to this.
		 */

		if (ev_size >= 8) {
			this->increment_read_ptr (ev_size);
		} else {
			// we only track note on/off, 8 bytes are plenty.
			uint8_t write_loc[8];
			bool success = read_contents (ev_size, write_loc);
			if (success) {
				_tracker.track(write_loc);
			}
		}
	}
	return count;
}



template<typename T>
void
MidiRingBuffer<T>::flush (samplepos_t /*start*/, samplepos_t end)
{
	const size_t prefix_size = sizeof(T) + sizeof(Evoral::EventType) + sizeof(uint32_t);

	while (this->read_space() >= prefix_size) {
		uint8_t  peekbuf[prefix_size];
		bool     success;
		uint32_t ev_size;
		T        ev_time;

		success = this->peek (peekbuf, prefix_size);
		/* this cannot fail, because we've already verified that there
		   is prefix_space to read
		*/
		assert (success);

		ev_time = *(reinterpret_cast<T*>((uintptr_t)peekbuf));

		if (ev_time >= end) {
			break;
		}

		ev_size = *(reinterpret_cast<uint32_t*>((uintptr_t)(peekbuf + sizeof(T) + sizeof (Evoral::EventType))));
		this->increment_read_ptr (prefix_size);
		this->increment_read_ptr (ev_size);
	}
}

template<typename T>
void
MidiRingBuffer<T>::dump(ostream& str)
{
	size_t rspace;

	if ((rspace = this->read_space()) == 0) {
		str << this << " MRB::dump: empty\n";
		return;
	}

	T                 ev_time;
	Evoral::EventType ev_type;
	uint32_t          ev_size;

	RingBufferNPT<uint8_t>::rw_vector vec;
	RingBufferNPT<uint8_t>::get_read_vector (&vec);

	if (vec.len[0] == 0) {
		return;
	}

	str << this << ": Dump size = " << vec.len[0] + vec.len[1]
	    << " r@ " << RingBufferNPT<uint8_t>::get_read_ptr()
	    << " w@" << RingBufferNPT<uint8_t>::get_write_ptr() << endl;


	uint8_t *buf = new uint8_t[vec.len[0] + vec.len[1]];
	memcpy (buf, vec.buf[0], vec.len[0]);

	if (vec.len[1]) {
		memcpy (buf+vec.len[1], vec.buf[1], vec.len[1]);
	}

	uint8_t* data = buf;
	const uint8_t* end = buf + vec.len[0] + vec.len[1];

	while (data < end) {

		memcpy (&ev_time, data, sizeof (T));
		data += sizeof (T);
		str << "\ttime " << ev_time;

		if (data >= end) {
			str << "(incomplete)\n ";
			break;
		}

		memcpy (&ev_type, data, sizeof (ev_type));
		data += sizeof (ev_type);
		str << " type " << ev_type;

		if (data >= end) {
			str << "(incomplete)\n";
			break;
		}

		memcpy (&ev_size, data, sizeof (ev_size));
		data += sizeof (ev_size);
		str << " size " << ev_size;

		if (data >= end) {
			str << "(incomplete)\n";
			break;
		}

		for (uint32_t i = 0; i != ev_size && data < end; ++i) {
			str << ' ' << hex << (int) data[i] << dec;
		}

		data += ev_size;

		str << endl;
	}

	delete [] buf;
}

template<typename T>
void
MidiRingBuffer<T>::reset_tracker ()
{
	_tracker.reset ();
}

template<typename T>
void
MidiRingBuffer<T>::resolve_tracker (MidiBuffer& dst, samplepos_t t)
{
	_tracker.resolve_notes (dst, t);
}

template<typename T>
void
MidiRingBuffer<T>::resolve_tracker (Evoral::EventSink<samplepos_t>& dst, samplepos_t t)
{
	_tracker.resolve_notes(dst, t);
}

template class MidiRingBuffer<samplepos_t>;

}  // namespace ARDOUR
