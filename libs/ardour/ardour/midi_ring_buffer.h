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
#include <ardour/buffer.h>

namespace ARDOUR {


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
		} else if(w < r) {
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
	
	void   write(size_t size, const T* src);

protected:
	mutable gint _write_ptr;
	mutable gint _read_ptr;
	
	size_t _size; ///< Size (capacity) in bytes
	T*  _buf;  ///< size, event, size, event...
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
	if (read_space() < size)
		return false;

	const size_t read_size = peek(size, dst);
	
	if (read_size < size)
		peek(size - read_size, dst + read_size);

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
	if (read_space() < size)
		return false;

	const size_t read_size = read(size, dst);
	
	if (read_size < size)
		read(size - read_size, dst + read_size);

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
class MidiRingBuffer : public MidiRingBufferBase<Byte> {
public:

	/** @param size Size in bytes.
	 */
	MidiRingBuffer(size_t size)
		: MidiRingBufferBase<Byte>(size)
	{}

	size_t write(nframes_t time, size_t size, const Byte* buf);
	bool   read(nframes_t time, size_t* size, Byte* buf);

	size_t read(MidiBuffer& dst, nframes_t start, nframes_t end);
};


inline bool
MidiRingBuffer::read(nframes_t time, size_t* size, Byte* buf)
{
	bool success = MidiRingBufferBase<Byte>::full_read(sizeof(nframes_t), (Byte*)time);
	if (success)
		success = MidiRingBufferBase<Byte>::full_read(sizeof(size_t), (Byte*)size);
	if (success)
		success = MidiRingBufferBase<Byte>::full_read(*size, buf);

	return success;
}


inline size_t
MidiRingBuffer::write(nframes_t time, size_t size, const Byte* buf)
{
	assert(size > 0);

	if (write_space() < (sizeof(nframes_t) + sizeof(size_t) + size)) {
		return 0;
	} else {
		MidiRingBufferBase<Byte>::write(sizeof(nframes_t), (Byte*)&time);
		MidiRingBufferBase<Byte>::write(sizeof(size_t), (Byte*)&size);
		MidiRingBufferBase<Byte>::write(size, buf);
		return size;
	}
}


inline size_t
MidiRingBuffer::read(MidiBuffer& dst, nframes_t start, nframes_t end)
{
	if (read_space() == 0)
		return 0;

	MidiEvent ev;

	size_t count = 0;

	while (read_space() > sizeof(nframes_t) + sizeof(size_t)) {
	
		full_peek(sizeof(nframes_t), (Byte*)&ev.time);
	
		if (ev.time > end)
			break;

		bool success = MidiRingBufferBase<Byte>::full_read(sizeof(nframes_t), (Byte*)&ev.time);
		if (success)
			success = MidiRingBufferBase<Byte>::full_read(sizeof(size_t), (Byte*)&ev.size);
		
		if (!success) {
			cerr << "MRB: READ ERROR (time/size)" << endl;
			continue;
		}

		if (ev.time >= start) {
			Byte* write_loc = dst.reserve(ev.time, ev.size);
			success = MidiRingBufferBase<Byte>::full_read(ev.size, write_loc);
		
			if (!success)
				cerr << "MRB: READ ERROR (data)" << endl;
			
			//printf("MRB - read %#X %d %d with time %u at index %zu\n",
			//	ev.buffer[0], ev.buffer[1], ev.buffer[2], ev.time,
			//	priv_read_ptr);
			//
		} else {
			printf("MRB - SKIPPING EVENT (with time %u)\n", ev.time);
			break;
		}

		++count;

		assert(ev.time <= end);
	}
	
	//printf("(R) read space: %zu\n", read_space());

	return count;
}


} // namespace ARDOUR

#endif // __ardour_midi_ring_buffer_h__

