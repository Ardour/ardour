/*
    Copyright (C) 2006-2007 Paul Davis 
    
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

#include <ardour/audio_buffer.h>

#ifdef __x86_64__
static const int CPU_CACHE_ALIGN = 64;
#else
static const int CPU_CACHE_ALIGN = 16; /* arguably 32 on most arches, but it matters less */
#endif

namespace ARDOUR {


AudioBuffer::AudioBuffer(size_t capacity)
	: Buffer(DataType::AUDIO, capacity)
	, _owns_data (false)
	, _data (0)
{
	if (_capacity) {
		_owns_data = true; // prevent resize() from gagging
		resize (_capacity);
		silence (_capacity);
	}
}

AudioBuffer::~AudioBuffer()
{
	if (_owns_data)
		free(_data);
}

void
AudioBuffer::resize (size_t size)
{
	if (!_owns_data || (size < _capacity)) {
		return;
	}

	if (_data) {
		free (_data);
	}

	_capacity = size;
	_size = size;
	_silent = false;

#ifdef NO_POSIX_MEMALIGN
	_data =  (Sample *) malloc(sizeof(Sample) * _capacity);
#else
	posix_memalign((void**)&_data, CPU_CACHE_ALIGN, sizeof(Sample) * _capacity);
#endif	
	
	_owns_data = true;
}

} // namespace ARDOUR

