/*
    Copyright (C) 2006 Paul Davis 

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

#ifndef __ardour_midi_ring_buffer_h__
#define __ardour_midi_ring_buffer_h__

#include <algorithm>
#include <ardour/types.h>
#include <pbd/ringbufferNPT.h>
#include <ardour/buffer.h>

namespace ARDOUR {

/** A MIDI RingBuffer
 * (necessary because MIDI events are variable sized so a generic RB won't do).
 *
 * ALL publically accessible sizes refer to event COUNTS.  What actually goes
 * on in here is none of the callers business :)
 */
class MidiRingBuffer {
public:
	MidiRingBuffer (size_t size)
		: _size(size)
		, _max_event_size(MidiBuffer::max_event_size())
		, _ev_buf(new MidiEvent[size])
		, _raw_buf(new RawMidi[size * _max_event_size])
	{
		reset ();
		assert(read_space() == 0);
		assert(write_space() == size - 1);
	}
	
	virtual ~MidiRingBuffer() {
		delete[] _ev_buf;
		delete[] _raw_buf;
	}

	void reset () {
		/* !!! NOT THREAD SAFE !!! */
		g_atomic_int_set (&_write_ptr, 0);
		g_atomic_int_set (&_read_ptr, 0);
	}

	size_t write_space () {
		size_t w, r;
		
		w = g_atomic_int_get (&_write_ptr);
		r = g_atomic_int_get (&_read_ptr);
		
		if (w > r) {
			return ((r - w + _size) % _size) - 1;
		} else if (w < r) {
			return (r - w) - 1;
		} else {
			return _size - 1;
		}
	}
	
	size_t read_space () {
		size_t w, r;
		
		w = g_atomic_int_get (&_write_ptr);
		r = g_atomic_int_get (&_read_ptr);
		
		if (w > r) {
			return w - r;
		} else {
			return (w - r + _size) % _size;
		}
	}

	size_t capacity() const { return _size; }

	/** Read one event and appends it to @a out. */
	//size_t read(MidiBuffer& out);

	/** Write one event (@a in) */
	size_t write(const MidiEvent& in); // deep copies in

	/** Read events all events up to time @a end into @a out, leaving stamps intact.
	 * Any events before @a start will be dropped. */
	size_t read(MidiBuffer& out, jack_nframes_t start, jack_nframes_t end);

	/** Write all events from @a in, applying @a offset to all time stamps */
	size_t write(const MidiBuffer& in, jack_nframes_t offset = 0);

	inline void clear_event(size_t index);

private:

	// _event_ indices
	mutable gint _write_ptr;
	mutable gint _read_ptr;
	
	size_t     _size;           // size (capacity) in events
	size_t     _max_event_size; // ratio of raw_buf size to ev_buf size
	MidiEvent* _ev_buf;         // these point into...
	RawMidi*   _raw_buf;        // this

};

/** Just for sanity checking */
inline void
MidiRingBuffer::clear_event(size_t index)
{
	memset(&_ev_buf[index].buffer, 0, _max_event_size);
	_ev_buf[index].time = 0;
	_ev_buf[index].size = 0;
	_ev_buf[index].buffer = 0;

}
#if 0
inline size_t
MidiRingBuffer::read (MidiBuffer& buf)
{
	const size_t priv_read_ptr = g_atomic_int_get(&_read_ptr);

	if (read_space() == 0) {
		return 0;
	} else {
		MidiEvent* const read_ev = &_ev_buf[priv_read_ptr];
		assert(read_ev->size > 0);
		buf.push_back(*read_ev);
		printf("MRB - read %xd %d %d with time %u at index %zu\n",
			read_ev->buffer[0], read_ev->buffer[1], read_ev->buffer[2], read_ev->time,
			priv_read_ptr);
		clear_event(priv_read_ptr);
		g_atomic_int_set(&_read_ptr, (priv_read_ptr + 1) % _size);
		return 1;
	}
}
#endif
inline size_t
MidiRingBuffer::write (const MidiEvent& ev)
{
	//static jack_nframes_t last_write_time = 0;
	
	assert(ev.size > 0);

	size_t priv_write_ptr = g_atomic_int_get(&_write_ptr);

	if (write_space () == 0) {
		return 0;
	} else {
		//assert(ev.time >= last_write_time);

		const size_t raw_index = priv_write_ptr * _max_event_size;

		MidiEvent* const write_ev = &_ev_buf[priv_write_ptr];
		*write_ev = ev;

		memcpy(&_raw_buf[raw_index], ev.buffer, ev.size);
		write_ev->buffer = &_raw_buf[raw_index];
        g_atomic_int_set(&_write_ptr, (priv_write_ptr + 1) % _size);
		
		printf("MRB - wrote %xd %d %d with time %u at index %zu (raw index %zu)\n",
			write_ev->buffer[0], write_ev->buffer[1], write_ev->buffer[2], write_ev->time,
			priv_write_ptr, raw_index);
		
		assert(write_ev->size = ev.size);

		//last_write_time = ev.time;
		printf("(W) read space: %zu\n", read_space());

		return 1;
	}
}

inline size_t
MidiRingBuffer::read(MidiBuffer& dst, jack_nframes_t start, jack_nframes_t end)
{
	if (read_space() == 0)
		return 0;

	size_t         priv_read_ptr = g_atomic_int_get(&_read_ptr);
	jack_nframes_t time          = _ev_buf[priv_read_ptr].time;
	size_t         count         = 0;
	size_t         limit         = read_space();

	while (time <= end && limit > 0) {
		MidiEvent* const read_ev = &_ev_buf[priv_read_ptr];
		if (time >= start) {
			dst.push_back(*read_ev);
			printf("MRB - read %xd %d %d with time %u at index %zu\n",
				read_ev->buffer[0], read_ev->buffer[1], read_ev->buffer[2], read_ev->time,
				priv_read_ptr);
		} else {
			cerr << "MRB: LOST EVENT!" << endl;
		}

		clear_event(priv_read_ptr);

		++count;
		--limit;
		
		priv_read_ptr = (priv_read_ptr + 1) % _size;
		
		assert(read_ev->time <= end);
		time = _ev_buf[priv_read_ptr].time;
	}
	
	g_atomic_int_set(&_read_ptr, priv_read_ptr);
	printf("(R) read space: %zu\n", read_space());

	return count;
}

inline size_t
MidiRingBuffer::write(const MidiBuffer& in, jack_nframes_t offset)
{
	size_t num_events = in.size();
	size_t to_write = std::min(write_space(), num_events);

	// FIXME: double copy :/
	for (size_t i=0; i < to_write; ++i) {
		MidiEvent ev = in[i];
		ev.time += offset;
		write(ev);
	}

	return to_write;
}

} // namespace ARDOUR

#endif // __ardour_midi_ring_buffer_h__

