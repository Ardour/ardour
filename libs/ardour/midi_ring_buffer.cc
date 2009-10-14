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

/** Read a block of MIDI events from buffer into a MidiBuffer.
 *
 * Timestamps of events returned are relative to start (i.e. event with stamp 0
 * occurred at start), with offset added.
 */
template<typename T>
size_t
MidiRingBuffer<T>::read(MidiBuffer& dst, nframes_t start, nframes_t end, nframes_t offset)
{
	if (this->read_space() == 0) {
		return 0;
	}

	T                 ev_time;
	Evoral::EventType ev_type;
	uint32_t          ev_size;

	size_t count = 0;

	while (this->read_space() >= sizeof(T) + sizeof(Evoral::EventType) + sizeof(uint32_t)) {

		this->full_peek(sizeof(T), (uint8_t*)&ev_time);

		if (ev_time > end) {
			// cerr << "MRB event @ " << ev_time << " past end @ " << end << endl;
			break;
		} else if (ev_time < start) {
			// cerr << "MRB event @ " << ev_time << " before start @ " << start << endl;
			break;
		}

		bool success = read_prefix(&ev_time, &ev_type, &ev_size);
		if (!success) {
			cerr << "WARNING: error reading event prefix from MIDI ring" << endl;
			continue;
		}

		// This event marks a loop end (i.e. the next event's timestamp will be non-monotonic)
		if (ev_type == LoopEventType) {
			/*ev_time -= start;
			  ev_time += offset;*/
			// cerr << "MRB loop boundary @ " << ev_time << endl;

			// Return without reading data or writing to buffer (loop events have no data)
			// FIXME: This is not correct, loses events after the loop this cycle
			return count + 1;
		}

		uint8_t status;
		success = this->full_peek(sizeof(uint8_t), &status);
		assert(success); // If this failed, buffer is corrupt, all hope is lost

		// Ignore event if it doesn't match channel filter
		if (is_channel_event(status) && get_channel_mode() == FilterChannels) {
			const uint8_t channel = status & 0x0F;
			if (!(get_channel_mask() & (1L << channel))) {
				// cerr << "MRB skipping event due to channel mask" << endl;
				this->skip(ev_size); // Advance read pointer to next event
				continue;
			}
		}

		assert(ev_time >= start);
		ev_time -= start;
		ev_time += offset;

		// write the timestamp to address (write_loc - 1)
		uint8_t* write_loc = dst.reserve(ev_time, ev_size);
		if (write_loc == NULL) {
			// cerr << "MRB: Unable to reserve space in buffer, event skipped";
			continue;
		}

		// write MIDI buffer contents
		success = Evoral::EventRingBuffer<T>::full_read(ev_size, write_loc);

#if 0
		cerr << "wrote MidiEvent to Buffer: " << hex;
		for (size_t i=0; i < ev_size; ++i) {
			cerr << (int) write_loc[i] << ' ';
		}
		cerr << dec << endl;
#endif

		if (success) {
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
	size_t read_ptr = g_atomic_int_get (&this->_read_ptr);

	str << "Dump @ " << read_ptr << endl;

	while (1) {
		uint8_t* wp;
		uint8_t* data;
		size_t write_ptr;

#define space(r,w) ((w > r) ? (w - r) : ((w - r + this->_size) % this->_size))

		write_ptr  = g_atomic_int_get (&this->_write_ptr);
		if (space (read_ptr, write_ptr) < sizeof (T)) {
			break;
		}

		wp = &this->_buf[read_ptr];
		memcpy (&ev_time, wp, sizeof (T));
		read_ptr = (read_ptr + sizeof (T)) % this->_size;
		str << "time " << ev_time;

		write_ptr  = g_atomic_int_get (&this->_write_ptr);
		if (space (read_ptr, write_ptr) < sizeof (ev_type)) {
			break;
		}

		wp = &this->_buf[read_ptr];
		memcpy (&ev_type, wp, sizeof (ev_type));
		read_ptr = (read_ptr + sizeof (ev_type)) % this->_size;
		str << " type " << ev_type;

		write_ptr  = g_atomic_int_get (&this->_write_ptr);
		if (space (read_ptr, write_ptr) < sizeof (ev_size)) {
			str << "!OUT!\n";
			break;
		}

		wp = &this->_buf[read_ptr];
		memcpy (&ev_size, wp, sizeof (ev_size));
		read_ptr = (read_ptr + sizeof (ev_size)) % this->_size;
		str << " size " << ev_size;

		write_ptr  = g_atomic_int_get (&this->_write_ptr);
		if (space (read_ptr, write_ptr) < ev_size) {
			str << "!OUT!\n";
			break;
		}

		data = new uint8_t[ev_size];

		wp = &this->_buf[read_ptr];
		memcpy (data, wp, ev_size);
		read_ptr = (read_ptr + ev_size) % this->_size;

		for (uint32_t i = 0; i != ev_size; ++i) {
			str << ' ' << hex << (int) data[i] << dec;
		}

		str << endl;

		delete [] data;
	}
}


template class MidiRingBuffer<nframes_t>;

} // namespace ARDOUR

