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

#include <errno.h>

#include "ardour/audio_buffer.h"
#include "pbd/error.h"
#include "pbd/malign.h"

#include "i18n.h"

using namespace PBD;
using namespace ARDOUR;

AudioBuffer::AudioBuffer(size_t capacity)
	: Buffer (DataType::AUDIO)
	, _owns_data (false)
	, _data (0)
{
	if (capacity) {
		_owns_data = true; // prevent resize() from gagging
		resize (capacity);
		_silent = false; // force silence on the intial buffer state
		clear ();
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
	if (!_owns_data) {
		/* XXX how the hell is this enforced? */
		_capacity = size;
		return;
	}

	if (_data && size < _capacity) {
		/* buffer is already large enough */
		
		if (size < _size) {
			/* truncate */
			_size = size;
		}

		return;
	}

	free (_data);

	cache_aligned_malloc ((void**) &_data, sizeof (Sample) * size);

	_capacity = size;
	_size = 0;
	_silent = false;
}

bool
AudioBuffer::check_silence (pframes_t nframes, pframes_t& n) const
{
	for (n = 0; n < _size && n < nframes; ++n) {
		if (_data[n] != Sample (0)) {
			return false;
		}
	}
	return true;
}
