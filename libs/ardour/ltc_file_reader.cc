/*
    Copyright (C) 2015 Robin Gareus <robin@gareus.org>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <fcntl.h>
#include <sys/stat.h>

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include <assert.h>
#include <string.h>
#include <glibmm.h>

#include "pbd/error.h"
#include "pbd/failed_constructor.h"

#include "timecode/time.h"
#include "ardour/ltc_file_reader.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using std::string;

#define BUFFER_SIZE 1024 // audio chunk size

LTCFileReader::LTCFileReader (std::string path, double expected_fps, LTC_TV_STANDARD tv_standard)
	: _path (path)
	, _expected_fps (expected_fps)
	, _ltc_tv_standard (tv_standard)
	, _sndfile (0)
	, _interleaved_audio_buffer (0)
	, _frames_decoded (0)
	, _samples_read (0)
{
	memset (&_info, 0, sizeof (_info));
        assert (Glib::file_test (_path, Glib::FILE_TEST_EXISTS));

	if (open ()) {
		throw failed_constructor ();
	}

	const int apv = rintf (_info.samplerate / _expected_fps);
	decoder = ltc_decoder_create (apv, 8); // must be able to hold frmes for BUFFER_SIZE

#if 0 // TODO allow to auto-detect
	if (expected_fps == 25.0) {
		_ltc_tv_standard = LTC_TV_625_50;
	}
	else if (expected_fps == 30.0) {
		_ltc_tv_standard = LTC_TV_525_60;
	}
	else {
		_ltc_tv_standard = LTC_TV_FILM_24;
	}
#endif
}

LTCFileReader::~LTCFileReader ()
{
	close ();
	ltc_decoder_free (decoder);
	free (_interleaved_audio_buffer);
}

int
LTCFileReader::open ()
{
	if (_sndfile) {
		return 0;
	}

#ifdef PLATFORM_WINDOWS
	int fd = g_open (_path.c_str (), O_RDONLY, 0444);
#else
	int fd = ::open (_path.c_str (), O_RDONLY, 0444);
#endif
	if (fd == -1) {
		error << string_compose (_("LTCFileReader: cannot open file \"%1\""), _path) << endmsg;
		return -1;
	}

	_sndfile = sf_open_fd (fd, SFM_READ, &_info, true);

	if (_sndfile == 0) {
		char errbuf[1024];
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		error << string_compose (_("LTCFileReader: cannot open file \"%1\" (%3)"), _path, errbuf) << endmsg;
		return -1;
	}
	if (_info.frames == 0 || _info.channels < 1) {
		error << string_compose (_("LTCFileReader: \"%1\" is an empty audio file"), _path) << endmsg;
		return -1;
	}
	_interleaved_audio_buffer = (float*) calloc (_info.channels * BUFFER_SIZE, sizeof (float));
	return 0;
}

void
LTCFileReader::close ()
{
	if (_sndfile) {
		sf_close (_sndfile);
		_sndfile = 0;
	}
}

std::vector<LTCFileReader::LTCMap>
LTCFileReader::read_ltc (uint32_t channel, uint32_t max_frames)
{
	std::vector<LTCFileReader::LTCMap> rv;
	ltcsnd_sample_t sound[BUFFER_SIZE];
	LTCFrameExt frame;

	const uint32_t channels = _info.channels;
	if (channel >= channels) {
		warning << _("LTCFileReader:: invalid audio channel selected") << endmsg;
		return rv;
	}

	while (1) {
		int64_t n = sf_readf_float (_sndfile, _interleaved_audio_buffer, BUFFER_SIZE);
		if (n <= 0) {
			break;
		}

		// convert audio to 8bit unsigned
		for (int64_t i = 0; i < n; ++i) {
			sound [i]= 128 + _interleaved_audio_buffer[channels * i + channel] * 127;
		}

		ltc_decoder_write (decoder, sound, n, _samples_read);

		while (ltc_decoder_read (decoder, &frame)) {
			SMPTETimecode stime;
			++_frames_decoded;

			ltc_frame_to_time (&stime, &frame.ltc, /*use_date*/ 0);

			// convert Timecode to samples @ audio-file rate
			Timecode::Time timecode (_expected_fps);
			timecode.hours   = stime.hours;
			timecode.minutes = stime.mins;
			timecode.seconds = stime.secs;
			timecode.frames  = stime.frame;

			int64_t sample = 0;
			Timecode::timecode_to_sample (
					timecode, sample, false, false,
					_info.samplerate,
					0, 0, 0);

			// align LTC frame relative to video-frame
			sample -= ltc_frame_alignment (
					_info.samplerate / _expected_fps,
					_ltc_tv_standard);

			// convert to seconds (session can use session-rate)
			double fp_sec = frame.off_start / (double) _info.samplerate;
			double tc_sec = sample / (double) _info.samplerate;
			rv.push_back (LTCMap (fp_sec, tc_sec));

#if 0 // DEBUG
			printf("LTC %02d:%02d:%02d:%02d @%9lld -> %9lld -> %fsec\n",
					stime.hours,
					stime.mins,
					stime.secs,
					stime.frame,
					frame.off_start,
					sample,
					tc_sec);
#endif
		}

		if (n > 0) {
			_samples_read += n;
		}

		if (max_frames > 0 && rv.size () >= max_frames) {
			break;
		}

	}

	return rv;
}
