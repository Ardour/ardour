/*
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#ifndef _ardour_mp3file_importable_source_h_
#define _ardour_mp3file_importable_source_h_

#include <stdint.h>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/importable_source.h"

namespace ARDOUR {

/* 64bit CPUs always have SSE2, armhf/aarch64 has NEON,
 * except apple/OSX does not provide immintrin.h
 */
#if defined ( __i386__) || defined  (__PPC__) || defined (__APPLE__)
#define MINIMP3_NO_SIMD // disable for portability
#endif

#define MINIMP3_NONSTANDARD_BUT_LOGICAL
#define MINIMP3_FLOAT_OUTPUT

/* use Ardour namespace for minimp3 symbols */
#include "ardour/minimp3.h"

class LIBARDOUR_API Mp3FileImportableSource : public ImportableSource {
public:
	Mp3FileImportableSource (const std::string& path);
	virtual ~Mp3FileImportableSource();

	/* ImportableSource API */
	uint32_t    channels () const { return _info.channels; }
	samplecnt_t length () const { return _length; }
	samplecnt_t samplerate () const { return _info.hz; }
	samplepos_t natural_position () const { return 0 ; }
	void        seek (samplepos_t pos);
	samplecnt_t read (Sample*, samplecnt_t nframes);

	bool clamped_at_unity () const { return false; }

	samplecnt_t layer () const { return _info.layer; }
	samplecnt_t bitrate () const { return _info.bitrate_kbps; }
	samplecnt_t read_unlocked (Sample*, samplepos_t start, samplecnt_t cnt, uint32_t chn);

private:
	void unmap_mem ();
	int  decode_mp3 (bool parse_only = false);

	mp3dec_t            _mp3d;
	mp3dec_frame_info_t _info;
	samplecnt_t         _length;

	int            _fd;
	const uint8_t* _map_addr;
	size_t         _map_length;

	const uint8_t* _buffer;
	size_t         _remain;

	samplepos_t   _read_position;
	mp3d_sample_t _pcm[MINIMP3_MAX_SAMPLES_PER_FRAME];
	size_t        _pcm_off;
	int           _n_frames;
};

}

#endif /* __ardour_mp3file_importable_source_h__ */
