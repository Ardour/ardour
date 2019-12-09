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

#define MINIMP3_IMPLEMENTATION

#include <fcntl.h>

#ifdef PLATFORM_WINDOWS
#include <windows.h>
#else
#include <sys/mman.h>
#endif

#include <glib.h>
#include "pbd/gstdio_compat.h"

#include "pbd/error.h"
#include "ardour/mp3fileimportable.h"

using namespace std;

namespace ARDOUR {

Mp3FileImportableSource::Mp3FileImportableSource (const string& path)
	: _fd (-1)
	, _map_addr (0)
	, _map_length (0)
	, _buffer (0)
	, _remain (0)
	, _read_position (0)
	, _pcm_off (0)
	, _n_frames (0)
{
	mp3dec_init (&_mp3d);
	memset (&_info, 0, sizeof (_info));

	GStatBuf statbuf;
	if (g_stat (path.c_str(), &statbuf) != 0) {
		throw failed_constructor ();
	}

	_fd = g_open (path.c_str (), O_RDONLY, 0444);
	if (_fd == -1) {
		throw failed_constructor ();
	}
	_map_length = statbuf.st_size;

#ifdef PLATFORM_WINDOWS
	HANDLE file_handle = (HANDLE) _get_osfhandle (int(_fd));

	HANDLE map_handle = CreateFileMapping (file_handle, NULL, PAGE_READONLY, 0, 0, NULL);
	if (map_handle == NULL) {
		close (_fd);
		throw failed_constructor ();
	}

	LPVOID view_handle = MapViewOfFile (map_handle, FILE_MAP_READ, 0, 0, _map_length);
	if (view_handle == NULL) {
		CloseHandle (map_handle);
		close (_fd);
		throw failed_constructor ();
	}
	_map_addr = (const uint8_t*)view_handle;
	CloseHandle (map_handle);
#else
	_map_addr = (const uint8_t *)mmap (NULL, _map_length, PROT_READ, MAP_PRIVATE, _fd, 0);
	if (_map_addr == MAP_FAILED) {
		close (_fd);
		throw failed_constructor ();
	}
#endif

	_buffer = _map_addr;
	_remain = _map_length;

	if (!decode_mp3 ()) {
		unmap_mem ();
		throw failed_constructor ();
	}

	/* estimate length, fixed bitrate */
	_length = _n_frames * _map_length / _info.frame_bytes;

#if 1 /* detect accurate length by parsing frame headers */
	_length = _n_frames;
	while (decode_mp3 (true)) {
		_length += _n_frames;
	}
	_read_position = _length;
	seek (0);
#endif
}

Mp3FileImportableSource::~Mp3FileImportableSource ()
{
	unmap_mem ();
}

void
Mp3FileImportableSource::unmap_mem ()
{
#ifdef PLATFORM_WINDOWS
	if (_map_addr) {
		UnmapViewOfFile (_map_addr);
	}
#else
	munmap (const_cast<unsigned char*>(_map_addr), _map_length);
#endif
	close (_fd);
	_map_addr = 0;
}

int
Mp3FileImportableSource::decode_mp3 (bool parse_only)
{
	_pcm_off = 0;
	do {
		_n_frames = mp3dec_decode_frame (&_mp3d, _buffer, _remain, parse_only ? NULL : _pcm, &_info);
		_buffer += _info.frame_bytes;
		_remain -= _info.frame_bytes;
		if (_n_frames) {
			break;
		}
	} while (_info.frame_bytes);
	return _n_frames;
}

void
Mp3FileImportableSource::seek (samplepos_t pos)
{
	if (_read_position == pos) {
		return;
	}

	/* rewind, then decode to pos */
	if (pos < _read_position) {
		_buffer        = _map_addr;
		_remain        = _map_length;
		_read_position = 0;
		_pcm_off       = 0;
		mp3dec_init (&_mp3d);
		decode_mp3 ();
	}

	while (_read_position + _n_frames <= pos) {
		/* skip ahead, until the frame before the target,
		 * then start decoding. This provides sufficient
		 * context to prevent audible hiccups, while still
		 * providing fast and accurate seeking.
		 */
		if (!decode_mp3 (_read_position + 3 * _n_frames <= pos)) {
			break;
		}
		_read_position += _n_frames;
	}

	if (_n_frames > 0) {
		_pcm_off = _info.channels * (pos - _read_position);
		_n_frames -= pos - _read_position;
		_read_position = pos;
	}
	assert (_pcm_off < MINIMP3_MAX_SAMPLES_PER_FRAME);
}

samplecnt_t
Mp3FileImportableSource::read (Sample* dst, samplecnt_t nframes)
{
	size_t dst_off = 0;
	int remain = nframes; // == samples * channels
	while (remain > 0) {
		samplecnt_t samples_to_copy = std::min (remain, _n_frames * _info.channels);
		if (samples_to_copy > 0) {
			memcpy (&dst[dst_off], &_pcm[_pcm_off], samples_to_copy * sizeof (Sample));
			remain         -= samples_to_copy;
			dst_off        += samples_to_copy;
			_n_frames      -= samples_to_copy / _info.channels;
			_pcm_off       += samples_to_copy;
			_read_position += samples_to_copy / _info.channels;
		}
		assert (_n_frames >= 0);
		if (_n_frames <= 0 && !decode_mp3 ()) {
			/* EOF, or decode error */
			break;
		}
	}
	return dst_off;
}

samplecnt_t
Mp3FileImportableSource::read_unlocked (Sample* dst, samplepos_t start, samplecnt_t cnt, uint32_t chn)
{
	const uint32_t n_chn = channels ();
	if (chn > n_chn || cnt == 0) {
		return 0;
	}
	if (start != _read_position) {
		seek (start);
	}

	size_t      dst_off = 0;
	samplecnt_t remain = cnt;

	while (remain > 0) {
		samplecnt_t samples_to_copy = std::min (remain, (samplecnt_t)_n_frames);

		for (samplecnt_t n = 0; n < samples_to_copy; ++n) {
			dst[dst_off] = _pcm[_pcm_off + chn];
			dst_off        += 1;
			remain         -= 1;
			_n_frames      -= 1;
			_pcm_off       += n_chn;
			_read_position += 1;
		}

		assert (_n_frames >= 0);
		if (_n_frames <= 0 && !decode_mp3 ()) {
			/* EOF, or decode error */
			break;
		}
	}
	return dst_off;
}

}
