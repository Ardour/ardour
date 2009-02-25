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

#include "ardour/audio_buffer.h"
#include "pbd/error.h"
#include <errno.h>

#include "i18n.h"

#ifdef __x86_64__
static const int CPU_CACHE_ALIGN = 64;
#else
static const int CPU_CACHE_ALIGN = 16; /* arguably 32 on most arches, but it matters less */
#endif

using namespace PBD;
using namespace ARDOUR;

AudioBuffer::AudioBuffer(size_t capacity)
	: Buffer(DataType::AUDIO, capacity)
	, _owns_data (false)
	, _data (0)
{
	if (_capacity > 0) {
		_owns_data = true; // prevent resize() from gagging
		resize (_capacity);
		_silent = false; // force silence on the intial buffer state
		silence (_capacity);
	}
}

AudioBuffer::~AudioBuffer()
{
	if (_owns_data)
		free(_data);
}

/* called to replace a pointer to an external buffer (e.g. JACK) with 
   buffer-owned memory.
*/

void
AudioBuffer::replace_data (size_t capacity)
{
	_owns_data = true;
	_data = 0;
	_capacity = 0; // force reallocation
	resize (capacity);
}

void
AudioBuffer::resize (size_t size)
{
	if (!_owns_data) {
		return;
	}

	if (size < _capacity) {
		_size = size;
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
	if (posix_memalign((void**)&_data, CPU_CACHE_ALIGN, sizeof(Sample) * _capacity)) {
		fatal << string_compose (_("Memory allocation error: posix_memalign (%1 * %2) failed (%3)"),
				CPU_CACHE_ALIGN, sizeof (Sample) * _capacity, strerror (errno)) << endmsg;
	}
#endif	

}

void
AudioBuffer::copy_to_internal (Sample* p, nframes_t cnt, nframes_t offset)
{
	memcpy (_data + offset, p, sizeof(Sample*) * cnt);
}

