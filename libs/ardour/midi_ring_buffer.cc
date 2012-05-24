/*
    Copyright (C) 2006-2008 Paul Davis

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

#include "pbd/compose.h"
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
MidiRingBuffer<T>::read(MidiBuffer& dst, framepos_t start, framepos_t end, framecnt_t offset, bool stop_on_overflow_in_dst)
{
	if (this->read_space() == 0) {
		return 0;
	}

	T                 ev_time;
	Evoral::EventType ev_type;
	uint32_t          ev_size;

	/* If we see the end of a loop during this read, we must write the events after it
	   to the MidiBuffer with adjusted times.  The situation is as follows:

	   session frames----------------------------->

	             |                            |                    |
	        start_of_loop                   start              end_of_loop

	   The MidiDiskstream::read method which will have happened before this checks for
	   loops ending, and helpfully inserts a magic LoopEvent into the ringbuffer.  After this,
	   the MidiDiskstream continues to write events with their proper session frame times,
	   so after the LoopEvent event times will go backwards (ie non-monotonically).

	   Once we hit end_of_loop, we need to fake it to make it look as though the loop has been
	   immediately repeated.  Say that an event E after the end_of_loop in the ringbuffer
	   has time E_t, which is a time in session frames.  Its offset from the start
	   of the loop will be E_t - start_of_loop.  Its `faked' time will therefore be
	   end_of_loop + E_t - start_of_loop.  And so its port-buffer-relative time (for
	   writing to the MidiBuffer) will be end_of_loop + E_t - start_of_loop - start.

	   The subtraction of start is already taken care of, so if we see a LoopEvent, we'll
	   set up loop_offset to equal end_of_loop - start_of_loop, so that given an event
	   time E_t in the ringbuffer we can get the port-buffer-relative time as
	   E_t + offset - start.
	*/

	frameoffset_t loop_offset = 0;

	size_t count = 0;

	const size_t prefix_size = sizeof(T) + sizeof(Evoral::EventType) + sizeof(uint32_t);

	while (this->read_space() >= prefix_size) {

		uint8_t peekbuf[prefix_size];
		bool success;

		success = this->peek (peekbuf, prefix_size);
		/* this cannot fail, because we've already verified that there
		   is prefix_space to read
		*/
		assert (success);

		ev_time = *((T*) peekbuf);
		ev_type = *((Evoral::EventType*)(peekbuf + sizeof (T)));
		ev_size = *((uint32_t*)(peekbuf + sizeof(T) + sizeof (Evoral::EventType)));

		if (ev_time + loop_offset >= end) {
			DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("MRB event @ %1 past end @ %2\n", ev_time, end));
			break;
		} else if (ev_time + loop_offset < start) {
			DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("MRB event @ %1 before start @ %2\n", ev_time, start));
			break;
		} else {
			DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("MRB event @ %1 in range %2 .. %3\n", ev_time, start, end));
		}

		assert(ev_time >= start);

		ev_time -= start;
		ev_time += offset;

		// This event marks a loop end (i.e. the next event's timestamp
		// will be non-monotonic). Don't write it into the buffer - the
		// significance of this event ends here.
		
		if (ev_type == LoopEventType) {
			assert (ev_size == sizeof (framepos_t));
			framepos_t loop_start;
			read_contents (ev_size, (uint8_t *) &loop_start);
			loop_offset = ev_time - loop_start;
			_tracker.resolve_notes (dst, ev_time);
			continue;
		}

		/* we're good to go ahead and read the data now but since we
		 * have the prefix data already, just skip over that
		 */
		this->increment_read_ptr (prefix_size);
		ev_time += loop_offset;

		uint8_t status;
		success = this->peek (&status, sizeof(uint8_t));
		assert(success); // If this failed, buffer is corrupt, all hope is lost

		// Ignore event if it doesn't match channel filter
		if (is_channel_event(status) && get_channel_mode() == FilterChannels) {
			const uint8_t channel = status & 0x0F;
			if (!(get_channel_mask() & (1L << channel))) {
				DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("MRB skipping event (%3 bytes) due to channel mask (mask = %1 chn = %2)\n",
										      get_channel_mask(), (int) channel, ev_size));
				this->increment_read_ptr (ev_size); // Advance read pointer to next event
				continue;
			}
		}

		/* lets see if we are going to be able to write this event into dst.
		 */
		uint8_t* write_loc = dst.reserve (ev_time, ev_size);
		if (write_loc == 0) {
			if (stop_on_overflow_in_dst) {
				DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("MidiRingBuffer: overflow in destination MIDI buffer, stopped after %1 events\n", count));
				break;
			}
			error << "MRB: Unable to reserve space in buffer, event skipped" << endmsg;
			this->increment_read_ptr (ev_size); // Advance read pointer to next event
			continue;
		}

		// write MIDI buffer contents
		success = read_contents (ev_size, write_loc);

#ifndef NDEBUG
		if (DEBUG::MidiDiskstreamIO && PBD::debug_bits) {
			DEBUG_STR_DECL(a);
			DEBUG_STR_APPEND(a, string_compose ("wrote MidiEvent to Buffer (time=%1, start=%2 offset=%3)", ev_time, start, offset));
			for (size_t i=0; i < ev_size; ++i) {
				DEBUG_STR_APPEND(a,hex);
				DEBUG_STR_APPEND(a,"0x");
				DEBUG_STR_APPEND(a,(int)write_loc[i]);
				DEBUG_STR_APPEND(a,' ');
			}
			DEBUG_STR_APPEND(a,'\n');
			DEBUG_TRACE (DEBUG::MidiDiskstreamIO, DEBUG_STR(a).str());
		}
#endif

		if (success) {

			if (is_note_on(write_loc[0]) ) {
				_tracker.add (write_loc[1], write_loc[0] & 0xf);
			} else if (is_note_off(write_loc[0])) {
				_tracker.remove (write_loc[1], write_loc[0] & 0xf);
			}
			
			if (is_channel_event(status) && get_channel_mode() == ForceChannel) {
				write_loc[0] = (write_loc[0] & 0xF0) | (get_channel_mask() & 0x0F);
			}
			++count;
		} else {
			cerr << "WARNING: error reading event contents from MIDI ring" << endl;
		}
	}

	return count;
}

template<typename T>
void
MidiRingBuffer<T>::dump(ostream& str)
{
	size_t rspace;

	if ((rspace = this->read_space()) == 0) {
		str << "MRB::dump: empty\n";
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

template class MidiRingBuffer<framepos_t>;

}  // namespace ARDOUR
