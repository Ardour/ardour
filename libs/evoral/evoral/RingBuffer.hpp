/* This file is part of Evoral.
 * Copyright (C) 2008 Dave Robillard <http://drobilla.net>
 * 
 * Evoral is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation; either version 2 of the License, or (at your option) any later
 * version.
 * 
 * Evoral is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE.  See the GNU General Public License for details.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef EVORAL_RING_BUFFER_HPP
#define EVORAL_RING_BUFFER_HPP

#include <cassert>
#include <iostream>
#include <glib.h>

namespace Evoral {


/** A lock-free RingBuffer.
 * Read/Write realtime safe.
 * Single-reader Single-writer thread safe.
 */
template <typename T>
class RingBuffer {
public:

	/** @param size Size in bytes.
	 */
	RingBuffer(size_t size)
		: _size(size)
		, _buf(new T[size])
	{
		reset();
		assert(read_space() == 0);
		assert(write_space() == size - 1);
	}
	
	virtual ~RingBuffer() {
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
RingBuffer<T>::peek(size_t size, T* dst)
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
RingBuffer<T>::full_peek(size_t size, T* dst)
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
RingBuffer<T>::read(size_t size, T* dst)
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
RingBuffer<T>::full_read(size_t size, T* dst)
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
RingBuffer<T>::skip(size_t size)
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
RingBuffer<T>::write(size_t size, const T* src)
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

	
} // namespace Evoral

#endif // EVORAL_RING_BUFFER_HPP


