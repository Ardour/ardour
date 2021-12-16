/*
 * Copyright (C) 2021 Marijn Kruisselbrink <mek@google.com>
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

#include "ardour/ffmpegfileimportable.h"
#include "ardour/ffmpegfilesource.h"
#include "ardour/filesystem_paths.h"

namespace ARDOUR {

/** Constructor to be called for existing external-to-session files
 * Sources created with this method are never writable or removable.
 */

FFMPEGFileSource::FFMPEGFileSource (Session& s, const std::string& path, int chn, Flag flags)
	: Source (s, DataType::AUDIO, path,
			Source::Flag (flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy)))
	, AudioFileSource (s, path,
			Source::Flag (flags & ~(Writable|Removable|RemovableIfEmpty|RemoveAtDestroy)))
	, _ffmpeg (path, chn)
{
	_length = timecnt_t (_ffmpeg.length ());
}

FFMPEGFileSource::~FFMPEGFileSource ()
{
}

void
FFMPEGFileSource::close ()
{
}

samplecnt_t
FFMPEGFileSource::read_unlocked (Sample* dst, samplepos_t start, samplecnt_t cnt) const
{
	_ffmpeg.seek (start);
	return _ffmpeg.read (dst, cnt);
}

int
FFMPEGFileSource::get_soundfile_info (const std::string& path, SoundFileInfo &_info, std::string &error_msg)
{
	if (!safe_audio_file_extension (path)) {
		return -1;
	}

	try {
		FFMPEGFileImportableSource ffmpeg (path);
		_info.samplerate  = ffmpeg.samplerate ();
		_info.channels    = ffmpeg.channels ();
		_info.length      = ffmpeg.length ();
		_info.format_name = ffmpeg.format_name ();
		_info.timecode    = ffmpeg.natural_position ();
		_info.seekable    = false;
		return 0;
	} catch (...) {}
	return -1;
}

bool
FFMPEGFileSource::safe_audio_file_extension (const std::string &file)
{
	std::string unused;
	if (!ArdourVideoToolPaths::transcoder_exe (unused, unused)) {
	    return false;
    }

	const char *suffixes[] = {
		 ".m4a", ".M4A",
	};

	for (size_t n = 0; n < sizeof(suffixes) / sizeof(suffixes[0]); ++n) {
		size_t pos = file.rfind (suffixes[n]);
		if (pos > 0 && pos == file.length() - strlen(suffixes[n])) {
			return true;
		}
	}

	return false;
}

}
