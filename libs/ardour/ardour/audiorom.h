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

#ifndef __ardour_audio_rom_h__
#define __ardour_audio_rom_h__

#include "ardour/libardour_visibility.h"
#include "ardour/readable.h"
#include "ardour/types.h"

namespace ARDOUR {

class Session;

class LIBARDOUR_API AudioRom : public Readable
{
public:
	static boost::shared_ptr<AudioRom> new_rom (Sample*, size_t);
	~AudioRom();

	samplecnt_t read (Sample*, samplepos_t pos, samplecnt_t cnt, int channel) const;
	samplecnt_t readable_length_samples() const { return _size; }
	uint32_t  n_channels () const { return 1; }

private:
	AudioRom (Sample*, size_t);
	AudioRom (AudioRom const&);

	Sample*     _rom;
	samplecnt_t _size;
};

}

#endif /* __ardour_readable_h__ */
