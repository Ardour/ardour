/*
    Copyright (C) 2006 Paul Davis 
    Written by Dave Robillard, 2006
    
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
#include <ardour/types.h>
#include <jack/jack.h>

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
	/** Unfortunately using RTTI and dynamic_cast to find the type of the
	 * buffer is just too slow, this is done in very performance critical
	 * bits of the code. */
	enum Type { NIL = 0, AUDIO, MIDI };

	Buffer(Type type, size_t capacity)
	: _type(type), _capacity(capacity), _size(0) 
	{}

	virtual ~Buffer() {}

	/** Maximum capacity of buffer.
	 * Note in some cases the entire buffer may not contain valid data, use size. */
	size_t capacity() const { return _capacity; }

	/** Amount of valid data in buffer.  Use this over capacity almost always. */
	size_t size() const { return _size; }

	/** Type of this buffer.
	 * Based on this you can cast a Buffer* to the desired type. */
	virtual Type type() const { return _type; }

	/** Jack type (eg JACK_DEFAULT_AUDIO_TYPE) */
	const char* jack_type() const { return type_to_jack_type(type()); }
	
	/** Separate for creating ports (before a buffer exists to call jack_type on) */
	static const char* type_to_jack_type(Type t) {
		switch (t) {
			case AUDIO: return JACK_DEFAULT_AUDIO_TYPE;
			case MIDI:  return JACK_DEFAULT_MIDI_TYPE;
			default:    return "";
		}
	}

protected:
	Type   _type;
	size_t _capacity;
	size_t _size;
};


/* Since we only have two types, templates aren't worth it, yet.. */


/** Buffer containing 32-bit floating point (audio) data. */
class AudioBuffer : public Buffer
{
public:
	AudioBuffer(size_t capacity)
		: Buffer(AUDIO, capacity)
		, _data(NULL)
	{
		_size = capacity; // For audio buffers, size = capacity always
		posix_memalign((void**)_data, 16, sizeof(Sample) * capacity);
		assert(_data);
		memset(_data, 0, sizeof(Sample) * capacity);
	}

	const Sample* data() const { return _data; }
	Sample*       data()       { return _data; }

private:
	// These are undefined (prevent copies)
	AudioBuffer(const AudioBuffer& copy);            
	AudioBuffer& operator=(const AudioBuffer& copy);

	Sample* const _data; ///< Actual buffer contents
};



/** Buffer containing 8-bit unsigned char (MIDI) data. */
class MidiBuffer : public Buffer
{
public:
	MidiBuffer(size_t capacity)
		: Buffer(MIDI, capacity)
		, _data(NULL)
	{
		posix_memalign((void**)_data, 16, sizeof(RawMidi) * capacity);
		assert(_data);
		assert(_size == 0);
		memset(_data, 0, sizeof(Sample) * capacity);
	}

	const RawMidi* data() const { return _data; }
	RawMidi*       data()       { return _data; }

private:
	// These are undefined (prevent copies)
	MidiBuffer(const MidiBuffer& copy);            
	MidiBuffer& operator=(const MidiBuffer& copy);

	RawMidi* const _data; ///< Actual buffer contents
};


} // namespace ARDOUR

#endif // __ardour_buffer_h__
