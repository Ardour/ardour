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

#include <iostream>
#include <algorithm>
#include <ardour/types.h>
#include <ardour/buffer.h>
#include <evoral/EventSink.hpp>

namespace ARDOUR {


/* FIXME: this is probably too much inlined code */


/** A RingBuffer.
 * Read/Write realtime safe.
 * Single-reader Single-writer thread safe.
 *
 * This is Raul::RingBuffer, lifted for MIDIRingBuffer to inherit from as it works
 * a bit differently than PBD::Ringbuffer.  This could/should be replaced with
 * the PBD ringbuffer to decrease code size, but this code is tested and known to
 * work, so here it sits for now...
 *
 * Ignore this class, use MidiRingBuffer.
 */
template <typename T>
class MidiRingBufferBase {
public:

	/** @param size Size in bytes.
	 */
	MidiRingBufferBase(size_t size)
		: _size(size)
		, _buf(new T[size])
	{
		reset();
		assert(read_space() == 0);
		assert(write_space() == size - 1);
	}
	
	virtual ~MidiRingBufferBase() {
		delete[] _buf;
	}

	/** Reset(empty) the ringbuffer.
	 * NOT thread safe.
	 */
	void reset() {
		g_atomic_int_set(&_write_ptr, 0);
		g_atomic_int_set(&_read_ptr, 0);
	}

	size_t write_space() const {
		
		const size_t w = g_atomic_int_get(&_write_ptr);
		const size_t r = g_atomic_int_get(&_read_ptr);
		
		if (w > r) {
			return ((r - w + _size) % _size) - 1;
		} else if (w < r) {
			return (r - w) - 1;
		} else {
			return _size - 1;
		}
	}
	
	size_t read_space() const {
		
		const size_t w = g_atomic_int_get(&_write_ptr);
		const size_t r = g_atomic_int_get(&_read_ptr);
		
		if (w > r) {
			return w - r;
		} else {
			return (w - r + _size) % _size;
		}
	}

	size_t capacity() const { return _size; }

	size_t peek(size_t size, T* dst);
	bool   full_peek(size_t size, T* dst);

	size_t read(size_t size, T* dst);
	bool   full_read(size_t size, T* dst);

	bool   skip(size_t size);
	
	void   write(size_t size, const T* src);

protected:
	mutable int _write_ptr;
	mutable int _read_ptr;
	
	size_t _size; ///< Size (capacity) in bytes
	T*     _buf;  ///< size, event, size, event...
};


/** Peek at the ringbuffer (read w/o advancing read pointer).
 *
 * Note that a full read may not be done if the data wraps around.
 * Caller must check return value and call again if necessary, or use the 
 * full_peek method which does this automatically.
 */
template<typename T>
size_t
MidiRingBufferBase<T>::peek(size_t size, T* dst)
{
	const size_t priv_read_ptr = g_atomic_int_get(&_read_ptr);

	const size_t read_size = (priv_read_ptr + size < _size)
			? size
			: _size - priv_read_ptr;
	
	memcpy(dst, &_buf[priv_read_ptr], read_size);

	return read_size;
}


template<typename T>
bool
MidiRingBufferBase<T>::full_peek(size_t size, T* dst)
{
	if (read_space() < size) {
		return false;
	}

	const size_t read_size = peek(size, dst);
	
	if (read_size < size) {
		peek(size - read_size, dst + read_size);
	}

	return true;
}


/** Read from the ringbuffer.
 *
 * Note that a full read may not be done if the data wraps around.
 * Caller must check return value and call again if necessary, or use the 
 * full_read method which does this automatically.
 */
template<typename T>
size_t
MidiRingBufferBase<T>::read(size_t size, T* dst)
{
	const size_t priv_read_ptr = g_atomic_int_get(&_read_ptr);

	const size_t read_size = (priv_read_ptr + size < _size)
			? size
			: _size - priv_read_ptr;
	
	memcpy(dst, &_buf[priv_read_ptr], read_size);
        
	g_atomic_int_set(&_read_ptr, (priv_read_ptr + read_size) % _size);

	return read_size;
}


template<typename T>
bool
MidiRingBufferBase<T>::full_read(size_t size, T* dst)
{
	if (read_space() < size) {
		return false;
	}

	const size_t read_size = read(size, dst);
	
	if (read_size < size) {
		read(size - read_size, dst + read_size);
	}

	return true;
}


template<typename T>
bool
MidiRingBufferBase<T>::skip(size_t size)
{
	if (read_space() < size) {
		std::cerr << "WARNING: Attempt to skip past end of MIDI ring buffer" << std::endl;
		return false;
	}
	
	const size_t priv_read_ptr = g_atomic_int_get(&_read_ptr);
	g_atomic_int_set(&_read_ptr, (priv_read_ptr + size) % _size);

	return true;
}


template<typename T>
inline void
MidiRingBufferBase<T>::write(size_t size, const T* src)
{
	const size_t priv_write_ptr = g_atomic_int_get(&_write_ptr);
	
	if (priv_write_ptr + size <= _size) {
		memcpy(&_buf[priv_write_ptr], src, size);
		g_atomic_int_set(&_write_ptr, (priv_write_ptr + size) % _size);
	} else {
		const size_t this_size = _size - priv_write_ptr;
		assert(this_size < size);
		assert(priv_write_ptr + this_size <= _size);
		memcpy(&_buf[priv_write_ptr], src, this_size);
		memcpy(&_buf[0], src+this_size, size - this_size);
		g_atomic_int_set(&_write_ptr, size - this_size);
	}
}


/* ******************************************************************** */
	

/** A MIDI RingBuffer.
 *
 * This is timestamps and MIDI packed sequentially into a single buffer, similarly
 * to LV2 MIDI.  The buffer looks like this:
 *
 * [timestamp][size][size bytes of raw MIDI][timestamp][size][etc..]
 */
class MidiRingBuffer : public MidiRingBufferBase<uint8_t>, public Evoral::EventSink {
public:
	/** @param size Size in bytes.
	 */
	MidiRingBuffer(size_t size)
		: MidiRingBufferBase<uint8_t>(size), _channel_mask(0x0000FFFF)
	{}

	size_t write(double time, uint32_t size, const uint8_t* buf);
	bool   read(double* time, uint32_t* size, uint8_t* buf);

	bool   read_prefix(double* time, size_t* size);
	bool   read_contents(size_t size, uint8_t* buf);

	size_t read(MidiBuffer& dst, nframes_t start, nframes_t end, nframes_t offset=0);
	
	/** Set the channel filtering mode.
	 * @param mask If mode is FilterChannels, each bit represents a midi channel:
	 *     bit 0 = channel 0, bit 1 = channel 1 etc. the read and write methods will only
	 *     process events whose channel bit is 1.
	 *     If mode is ForceChannel, mask is simply a channel number which all events will
	 *     be forced to while reading.
	 */
	void set_channel_mode(ChannelMode mode, uint16_t mask) {
		g_atomic_int_set(&_channel_mask, ((uint16_t)mode << 16) | mask);
	}

	ChannelMode get_channel_mode() const {
		return static_cast<ChannelMode>((g_atomic_int_get(&_channel_mask) & 0xFFFF0000) >> 16);
	}
	
	uint16_t get_channel_mask() const {
		return static_cast<ChannelMode>((g_atomic_int_get(&_channel_mask) & 0x0000FFFF));
	}
	
protected:
	inline bool is_channel_event(uint8_t event_type_byte) {
		// mask out channel information
		event_type_byte &= 0xF0;
		// midi channel events range from 0x80 to 0xE0
		return (0x80 <= event_type_byte) && (event_type_byte <= 0xE0);
	}
	
private:
	volatile uint32_t _channel_mask; // 16 bits mode, 16 bits mask
};


inline bool
MidiRingBuffer::read(double* time, uint32_t* size, uint8_t* buf)
{
	bool success = MidiRingBufferBase<uint8_t>::full_read(sizeof(double), (uint8_t*)time);
	
	if (success) {
		success = MidiRingBufferBase<uint8_t>::full_read(sizeof(size_t), (uint8_t*)size);
	}
	if (success) {
		success = MidiRingBufferBase<uint8_t>::full_read(*size, buf);
	}
	
	return success;
}


/** Read the time and size of an event.  This call MUST be immediately proceeded
 * by a call to read_contents (or the read pointer will be garabage).
 */
inline bool
MidiRingBuffer::read_prefix(double* time, size_t* size)
{
	bool success = MidiRingBufferBase<uint8_t>::full_read(sizeof(double), (uint8_t*)time);
	if (success) {
		success = MidiRingBufferBase<uint8_t>::full_read(sizeof(size_t), (uint8_t*)size);
	}

	return success;
}


/** Read the contenst of an event.  This call MUST be immediately preceeded
 * by a call to read_prefix (or the returned even will be garabage).
 */
inline bool
MidiRingBuffer::read_contents(size_t size, uint8_t* buf)
{
	return MidiRingBufferBase<uint8_t>::full_read(size, buf);
}


inline size_t
MidiRingBuffer::write(double time, uint32_t size, const uint8_t* buf)
{
	/*fprintf(stderr, "MRB %p write (t = %f) ", this, time);
	for (size_t i = 0; i < size; ++i)
		fprintf(stderr, "%X", (char)buf[i]);
	fprintf(stderr, "\n");*/
	
	assert(size > 0);
	
	// Don't write event if it doesn't match channel filter
	if (is_channel_event(buf[0]) && get_channel_mode() == FilterChannels) {
		uint8_t channel = buf[0] & 0x0F;
		if ( !(get_channel_mask() & (1L << channel)) ) {
			return 0;
		}
	}

	if (write_space() < (sizeof(double) + sizeof(size_t) + size)) {
		return 0;
	} else {
		MidiRingBufferBase<uint8_t>::write(sizeof(double), (uint8_t*)&time);
		MidiRingBufferBase<uint8_t>::write(sizeof(size_t), (uint8_t*)&size);
		if (is_channel_event(buf[0]) && get_channel_mode() == ForceChannel) {
			assert(size == 2 || size == 3);
			uint8_t tmp_buf[3];
			// Force event to channel
			tmp_buf[0] = (buf[0] & 0xF0) | (get_channel_mask() & 0x0F);
			tmp_buf[1] = buf[1];
			if (size == 3) {
				tmp_buf[2] = buf[2];
			}
			MidiRingBufferBase<uint8_t>::write(size, tmp_buf);
		} else {
			MidiRingBufferBase<uint8_t>::write(size, buf);
		}
		return size;
	}

}


/** Read a block of MIDI events from buffer.
 *
 * Timestamps of events returned are relative to start (ie event with stamp 0
 * occurred at start), with offset added.
 */
inline size_t
MidiRingBuffer::read(MidiBuffer& dst, nframes_t start, nframes_t end, nframes_t offset)
{
	if (read_space() == 0)
		return 0;

	double   ev_time;
	uint32_t ev_size;

	size_t count = 0;

	//printf("---- MRB read %u .. %u + %u\n", start, end, offset);

	while (read_space() > sizeof(double) + sizeof(size_t)) {
	
		full_peek(sizeof(double), (uint8_t*)&ev_time);
	
		if (ev_time > end) {
			break;
		}
		
		bool success = MidiRingBufferBase<uint8_t>::full_read(sizeof(double), (uint8_t*)&ev_time);
		if (success) {
			success = MidiRingBufferBase<uint8_t>::full_read(sizeof(size_t), (uint8_t*)&ev_size);
		}

		if (!success) {
			std::cerr << "MRB: READ ERROR (time/size)" << std::endl;
			continue;
		}
		
		uint8_t status;
		success = full_peek(sizeof(uint8_t), &status);
		assert(success); // If this failed, buffer is corrupt, all hope is lost
		
		// Ignore event if it doesn't match channel filter
		if (is_channel_event(status) && get_channel_mode() == FilterChannels) {
			const uint8_t channel = status & 0x0F;
			if ( !(get_channel_mask() & (1L << channel)) ) {
				skip(ev_size); // Advance read pointer to next event
				continue;
			}
		}

		if (ev_time >= start) {

			/*std::cerr << "MRB " << this << " - Reading event, time = "
			 	<< ev_time << " - " << start << " => " << ev_time - start
				<< ", size = " << ev_size << std::endl;*/
			
			ev_time -= start;
			
			uint8_t* write_loc = dst.reserve(ev_time, ev_size);
			if (write_loc == NULL) {
				std::cerr << "MRB: Unable to reserve space in buffer, event skipped";
				continue;
			}
			
			success = MidiRingBufferBase<uint8_t>::full_read(ev_size, write_loc);
		
			if (success) {
				if (is_channel_event(status) && get_channel_mode() == ForceChannel) {
					write_loc[0] = (write_loc[0] & 0xF0) | (get_channel_mask() & 0x0F);
				}
				++count;
				//printf("MRB - read event at time %lf\n", ev_time);
			} else {
				std::cerr << "MRB: READ ERROR (data)" << std::endl;
			}
			
		} else {
			printf("MRB (start %u) - Skipping event at (too early) time %f\n", start, ev_time);
		}
	}
	
	//printf("(R) read space: %zu\n", read_space());

	return count;
}


} // namespace ARDOUR

#endif // __ardour_midi_ring_buffer_h__

