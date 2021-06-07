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

#ifndef _ardour_ffmpegfile_source_h_
#define _ardour_ffmpegfile_source_h_

#include <string>

#include "ardour/audiofilesource.h"
#include "ardour/ffmpegfileimportable.h"

namespace ARDOUR {

class LIBARDOUR_API FFMPEGFileSource : public AudioFileSource {
public:
	FFMPEGFileSource(ARDOUR::Session &, const std::string &path, int chn, Flag);
	~FFMPEGFileSource();

	/* AudioSource API */
	float sample_rate() const { return _ffmpeg.samplerate (); }
	bool clamped_at_unity () const { return false; }

	/* AudioFileSource API */
	void flush () {}
	int update_header (samplepos_t when, struct tm&, time_t) { return 0; }
	int flush_header () { return 0; }
	void set_header_natural_position () {};

	static int get_soundfile_info (const std::string& path, SoundFileInfo& _info, std::string& error_msg);
	static bool safe_audio_file_extension (const std::string &file);

protected:
	/* FileSource API */
	void close ();
	/* AudioSource API */
	samplecnt_t read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const;
	samplecnt_t write_unlocked (Sample *, samplecnt_t) { return 0; }

private:
	mutable FFMPEGFileImportableSource _ffmpeg;
	int _channel;
};

}
#endif
