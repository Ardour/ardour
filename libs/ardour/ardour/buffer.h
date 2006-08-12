/*
    Copyright (C) 2006 Paul Davis 
    
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

#ifndef __ardour_buffer_h__
#define __ardour_buffer_h__

#include <cstdlib>
#include <cassert>
#include <iostream>
#include <ardour/types.h>
#include <ardour/data_type.h>

namespace ARDOUR {


/** A buffer of recordable/playable data.
 *
 * This is a datatype-agnostic base class for all buffers (there are no
 * methods to actually access the data).  This provides a way for code that
 * doesn't care about the data type to still deal with buffers (which is
 * why the base class can't be a template).
 * 
 * To actually read/write buffer contents, use the appropriate derived class.
 */
class Buffer
{
public:
	virtual ~Buffer() {}

	/** Factory function */
	static Buffer* create(DataType type, size_t capacity);

	/** Maximum capacity of buffer.
	 * Note in some cases the entire buffer may not contain valid data, use size. */
	size_t capacity() const { return _capacity; }

	/** Amount of valid data in buffer.  Use this over capacity almost always. */
	size_t size() const { return _size; }

	/** Type of this buffer.
	 * Based on this you can static cast a Buffer* to the desired type. */
	DataType type() const { return _type; }

	/** Clear (eg zero, or empty) buffer starting at TIME @a offset */
	virtual void silence(jack_nframes_t len, jack_nframes_t offset=0) = 0;
	
	/** Clear the entire buffer */
	virtual void clear() { silence(_capacity, 0); }

	virtual void read_from(const Buffer& src, jack_nframes_t offset, jack_nframes_t len) = 0;

protected:
	Buffer(DataType type, size_t capacity)
	: _type(type), _capacity(capacity), _size(0) 
	{}

	DataType _type;
	size_t   _capacity;
	size_t   _size;

private:
	// Prevent copies (undefined)
	Buffer(const Buffer& copy);
	void operator=(const Buffer& other);
};


/* Inside every class with a type in it's name is a template waiting to get out... */


/** Buffer containing 32-bit floating point (audio) data. */
class AudioBuffer : public Buffer
{
public:
	AudioBuffer(size_t capacity);
	
	~AudioBuffer();

	void silence(jack_nframes_t len, jack_nframes_t offset=0)
	{
		assert(_capacity > 0);
		assert(offset + len <= _capacity);
		memset(_data + offset, 0, sizeof (Sample) * len);
	}
	
	/** Read @a len frames FROM THE START OF @a src into self at @a offset */
	void read_from(const Buffer& src, jack_nframes_t len, jack_nframes_t offset)
	{
		assert(_capacity > 0);
		assert(src.type() == _type == DataType::AUDIO);
		assert(offset + len <= _capacity);
		memcpy(_data + offset, ((AudioBuffer&)src).data(len), sizeof(Sample) * len);
	}
	
	/** Accumulate (add)@a len frames FROM THE START OF @a src into self at @a offset */
	void accumulate_from(const AudioBuffer& src, jack_nframes_t len, jack_nframes_t offset)
	{
		assert(_capacity > 0);
		assert(offset + len <= _capacity);

		Sample*       const dst_raw = _data + offset;
		const Sample* const src_raw = src.data(len);

		for (jack_nframes_t n = 0; n < len; ++n) {
			dst_raw[n] += src_raw[n];
		}
	}
	
	/** Accumulate (add) @a len frames FROM THE START OF @a src into self at @a offset
	 * scaling by @a gain_coeff */
	void accumulate_with_gain_from(const AudioBuffer& src, jack_nframes_t len, jack_nframes_t offset, gain_t gain_coeff)
	{
		assert(_capacity > 0);
		assert(offset + len <= _capacity);

		Sample*       const dst_raw = _data + offset;
		const Sample* const src_raw = src.data(len);

		for (jack_nframes_t n = 0; n < len; ++n) {
			dst_raw[n] += src_raw[n] * gain_coeff;
		}
	}
	
	/** Set the data contained by this buffer manually (for setting directly to jack buffer).
	 * 
	 * Constructor MUST have been passed capacity=0 or this will die (to prevent mem leaks).
	 */
	void set_data(Sample* data, size_t size)
	{
		assert(!_owns_data); // prevent leaks
		_capacity = size;
		_size = size;
		_data = data;
	}

	const Sample* data(jack_nframes_t nframes, jack_nframes_t offset=0) const
		{ assert(offset + nframes <= _capacity); return _data + offset; }

	Sample* data(jack_nframes_t nframes, jack_nframes_t offset=0)
		{ assert(offset + nframes <= _capacity); return _data + offset; }

private:
	// These are undefined (prevent copies)
	AudioBuffer(const AudioBuffer& copy);            
	AudioBuffer& operator=(const AudioBuffer& copy);

	bool    _owns_data;
	Sample* _data; ///< Actual buffer contents
};



/** Buffer containing 8-bit unsigned char (MIDI) data. */
class MidiBuffer : public Buffer
{
public:
	MidiBuffer(size_t capacity);
	
	~MidiBuffer();

	// FIXME: clear events starting at offset in time
	void silence(jack_nframes_t len, jack_nframes_t offset=0) { assert(offset == 0); _size = 0; }
	
	void read_from(const Buffer& src, jack_nframes_t nframes, jack_nframes_t offset);

	void set_size(size_t size) { _size = size; }

	const RawMidi* data() const { return _data; }
	RawMidi*       data()       { return _data; }

private:
	// These are undefined (prevent copies)
	MidiBuffer(const MidiBuffer& copy);            
	MidiBuffer& operator=(const MidiBuffer& copy);

	bool     _owns_data;
	RawMidi* _data; ///< Actual buffer contents
};

} // namespace ARDOUR

#endif // __ardour_buffer_h__
