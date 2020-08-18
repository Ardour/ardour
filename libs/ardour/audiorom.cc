/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <string.h>

#include "pbd/failed_constructor.h"
#include "ardour/audiorom.h"
#include "ardour/runtime_functions.h"

using namespace ARDOUR;

AudioRom::AudioRom (Sample* rom, size_t size)
	: _size (size)
{
	_rom = (Sample*) malloc (sizeof (Sample) * _size);
	if (!_rom) {
		throw failed_constructor ();
	}
	memcpy (_rom, rom, sizeof (Sample) * _size);
}

boost::shared_ptr<AudioRom>
AudioRom::new_rom (Sample* rom, size_t size)
{
	return boost::shared_ptr<AudioRom> (new AudioRom (rom, size));
}

AudioRom::~AudioRom ()
{
	free (_rom);
}

samplecnt_t
AudioRom::read (Sample* dst, samplepos_t pos, samplecnt_t cnt, int channel) const
{
	if (channel != 0 || pos >= _size) {
		return 0;
	}
	samplecnt_t to_copy = std::min (cnt, _size - pos);
	copy_vector (dst, &_rom[pos], to_copy);
	return to_copy;
}
