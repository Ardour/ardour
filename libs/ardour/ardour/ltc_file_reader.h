/*
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __libardour_ltc_file_reader_h__
#define __libardour_ltc_file_reader_h__

#include <vector>
#include <sndfile.h>

#include <ltc.h>

#include "ardour/libardour_visibility.h"
#include "ardour/types.h"

namespace ARDOUR {

class LIBARDOUR_API LTCReader
{
public:
	LTCReader (int expected_apv, LTC_TV_STANDARD tv_standard = LTC_TV_FILM_24);
	~LTCReader ();

	void write (float const*, samplecnt_t n_samples, samplepos_t pos = -1);
	void raw_write (ltcsnd_sample_t*, size_t, ltc_off_t);

	samplepos_t read (uint32_t& hh, uint32_t& mm, uint32_t& ss, uint32_t& ff);

private:
	LTCDecoder* _decoder;
	samplecnt_t _position;
};

class LIBARDOUR_API LTCFileReader
{
public:
	struct LTCMap {
		double framepos_sec; // relative to start of file
		double timecode_sec; // timecode

		LTCMap (double p, double t) {
			framepos_sec = p;
			timecode_sec = t;
		}
	};

	LTCFileReader (std::string path, double expected_fps, LTC_TV_STANDARD tv_standard = LTC_TV_FILM_24);
	~LTCFileReader ();

	uint32_t channels () const { return _info.channels; }
	std::vector<LTCMap> read_ltc (uint32_t channel, uint32_t max_frames = 1);

private:
	int open();
	void close ();

	std::string _path;

	double          _expected_fps;
	LTC_TV_STANDARD _ltc_tv_standard;

	SNDFILE* _sndfile;
	SF_INFO  _info;

	LTCReader*  _reader;
	float*      _interleaved_audio_buffer;
	samplecnt_t _samples_read;

};

} // namespace ARDOUR

#endif /* __libardour_ltc_file_reader_h__ */
