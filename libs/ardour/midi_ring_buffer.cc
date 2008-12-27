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

#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_buffer.h"
#include "ardour/event_type_map.h"

using namespace std;

namespace ARDOUR {

/** Read a block of MIDI events from buffer.
 *
 * Timestamps of events returned are relative to start (i.e. event with stamp 0
 * occurred at start), with offset added.
 */
size_t
MidiRingBuffer::read(MidiBuffer& dst, nframes_t start, nframes_t end, nframes_t offset)
{
	if (read_space() == 0) {
		//cerr << "MRB: NO READ SPACE" << endl;
		return 0;
	}

	Evoral::EventTime ev_time;
	Evoral::EventType ev_type;
	uint32_t          ev_size;

	size_t count = 0;

	//cerr << "MRB read " << start << " .. " << end << " + " << offset << endl;

	while (read_space() >= sizeof(Evoral::EventTime) + sizeof(Evoral::EventType) + sizeof(uint32_t)) {

		full_peek(sizeof(Evoral::EventTime), (uint8_t*)&ev_time);

		if (ev_time > end) {
			//cerr << "MRB: PAST END (" << ev_time << " : " << end << ")" << endl;
			break;
		} else if (ev_time < start) {
			//cerr << "MRB (start " << start << ") - Skipping event at (too early) time " << ev_time << endl;
			break;
		}

		bool success = read_prefix(&ev_time, &ev_type, &ev_size);
		if (!success) {
			cerr << "WARNING: error reading event prefix from MIDI ring" << endl;
			continue;
		}

		// This event marks a loop happening. this means that
		// the next events timestamp will be non-monotonic.
		if (ev_type == LoopEventType) {
			ev_time -= start;
			ev_time += offset;
			Evoral::MIDIEvent loopevent(LoopEventType, ev_time); 
			dst.push_back(loopevent);
			
			// We can safely return, without reading the data, because
			// a LoopEvent does not have data.
			return count + 1;
		}

		uint8_t status;
		success = full_peek(sizeof(uint8_t), &status);
		assert(success); // If this failed, buffer is corrupt, all hope is lost

		// Ignore event if it doesn't match channel filter
		if (is_channel_event(status) && get_channel_mode() == FilterChannels) {
			const uint8_t channel = status & 0x0F;
			if ( !(get_channel_mask() & (1L << channel)) ) {
				//cerr << "MRB skipping event due to channel mask" << endl;
				skip(ev_size); // Advance read pointer to next event
				continue;
			}
		}

		//cerr << "MRB " << this << " - Reading event, time = "
		//	<< ev_time << " - " << start << " => " << ev_time - start
		//	<< ", size = " << ev_size << endl;

		assert(ev_time >= start);
		ev_time -= start;
		ev_time += offset;

		uint8_t* write_loc = dst.reserve(ev_time, ev_size);
		if (write_loc == NULL) {
			//cerr << "MRB: Unable to reserve space in buffer, event skipped";
			continue;
		}

		success = Evoral::EventRingBuffer::full_read(ev_size, write_loc);

		if (success) {
			if (is_channel_event(status) && get_channel_mode() == ForceChannel) {
				write_loc[0] = (write_loc[0] & 0xF0) | (get_channel_mask() & 0x0F);
			}
			++count;
			//cerr << "MRB - read event at time " << ev_time << endl;
		} else {
			//cerr << "WARNING: error reading event contents from MIDI ring" << endl;
		}
	}
	
	//cerr << "MTB read space: " << read_space() << endl;

	return count;
}


} // namespace ARDOUR

