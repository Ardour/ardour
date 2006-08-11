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

#include <ardour/buffer.h>
#include <iostream>

namespace ARDOUR {


Buffer*
Buffer::create(DataType type, size_t capacity)
{
	if (type == DataType::AUDIO)
		return new AudioBuffer(capacity);
	else if (type == DataType::MIDI)
		return new MidiBuffer(capacity);
	else
		return NULL;
}


AudioBuffer::AudioBuffer(size_t capacity)
	: Buffer(DataType::AUDIO, capacity)
	, _data(NULL)
{
	_size = capacity; // For audio buffers, size = capacity (always)
	if (capacity > 0) {
#ifdef NO_POSIX_MEMALIGN
		_data =  (Sample *) malloc(sizeof(Sample) * capacity);
#else
		posix_memalign((void**)&_data, 16, sizeof(Sample) * capacity);
#endif	
		assert(_data);
		clear();
		_owns_data = true;
	} else {
		_owns_data = false;
	}
}

AudioBuffer::~AudioBuffer()
{
	if (_owns_data)
		free(_data);
}

// FIXME: mirroring for MIDI buffers?
MidiBuffer::MidiBuffer(size_t capacity)
	: Buffer(DataType::MIDI, capacity)
	, _owns_data(true)
	, _data(NULL)
{
	assert(capacity > 0);

	_size = capacity; // For audio buffers, size = capacity (always)
#ifdef NO_POSIX_MEMALIGN
	_data =  (RawMidi *) malloc(sizeof(RawMidi) * capacity);
#else
	posix_memalign((void**)&_data, 16, sizeof(RawMidi) * capacity);
#endif	
	assert(_data);
	memset(_data, 0, sizeof(RawMidi) * capacity);
}

MidiBuffer::~MidiBuffer()
{
	if (_owns_data)
		free(_data);
}

/** Note that offset and nframes refer to sample time, not actual buffer locations */
void
MidiBuffer::write(const Buffer& src, jack_nframes_t offset, jack_nframes_t nframes)
{
	assert(src.type() == DataType::MIDI);
	assert(offset == 0);
	MidiBuffer& msrc = (MidiBuffer&)src;
	_size = 0;
	for (size_t i=0; i < msrc.size() && msrc.data()[i].time < nframes; ++i) {
		assert(i < _capacity);
		_data[i] = msrc.data()[i];
		++_size;
	}
	assert(_size == msrc.size());

	if (_size > 0)
		std::cerr << "MidiBuffer wrote " << _size << " events.\n";
}


} // namespace ARDOUR

