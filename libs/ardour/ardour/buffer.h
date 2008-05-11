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

#define _XOPEN_SOURCE 600
#include <cstdlib> // for posix_memalign
#include <cassert>
#include <cstring>
#include <ardour/types.h>
#include <ardour/data_type.h>

namespace ARDOUR {


/* Yes, this is a bit of a mess right now.  I'll clean it up when everything
 * using it works out.. */


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
	Buffer(DataType type, size_t capacity)
	: _type(type), _capacity(capacity), _size(0) 
	{}

	virtual ~Buffer() {}

	/** Maximum capacity of buffer.
	 * Note in some cases the entire buffer may not contain valid data, use size. */
	size_t capacity() const { return _capacity; }

	/** Amount of valid data in buffer.  Use this over capacity almost always. */
	size_t size() const { return _size; }

	/** Type of this buffer.
	 * Based on this you can static cast a Buffer* to the desired type. */
	DataType type() const { return _type; }

protected:
	DataType _type;
	size_t   _capacity;
	size_t   _size;
};


/* Inside every class with a type in it's name is a template waiting to get out... */


/** Buffer containing 32-bit floating point (audio) data. */
class AudioBuffer : public Buffer
{
public:
	AudioBuffer(size_t capacity)
		: Buffer(DataType::AUDIO, capacity)
		, _data(NULL)
	{
		_size = capacity; // For audio buffers, size = capacity (always)
#ifdef NO_POSIX_MEMALIGN
		_data =  (Sample *) malloc(sizeof(Sample) * capacity);
#else
		posix_memalign((void**)_data, 16, sizeof(Sample) * capacity);
#endif	
		assert(_data);
		memset(_data, 0, sizeof(Sample) * capacity);
	}

	const Sample* data() const { return _data; }
	Sample*       data()       { return _data; }

private:
	// These are undefined (prevent copies)
	AudioBuffer(const AudioBuffer& copy);            
	AudioBuffer& operator=(const AudioBuffer& copy);

	Sample* _data; ///< Actual buffer contents
};


} // namespace ARDOUR

#endif // __ardour_buffer_h__
