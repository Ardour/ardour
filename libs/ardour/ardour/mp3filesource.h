/*
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

#ifndef _ardour_mp3file_source_h_
#define _ardour_mp3file_source_h_

#include "ardour/audiofilesource.h"
#include "ardour/mp3fileimportable.h"
#include <string>

using namespace std;

namespace ARDOUR {

class LIBARDOUR_API Mp3FileSource : public AudioFileSource {
public:
	Mp3FileSource (ARDOUR::Session&, const string& path, int chn, Flag);
	~Mp3FileSource ();

	/* AudioSource API */
	float sample_rate() const { return _mp3.samplerate (); }
	bool clamped_at_unity () const { return false; }

	/* AudioFileSource API */
	void flush () {}
	int update_header (samplepos_t when, struct tm&, time_t) { return 0; }
	int flush_header () { return 0; }
	void set_header_natural_position () {};

	static int get_soundfile_info (string path, SoundFileInfo& _info, string& error_msg);

protected:
	/* FileSource API */
	void close ();
	/* AudioSource API */
	samplecnt_t read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const;
	samplecnt_t write_unlocked (Sample *, samplecnt_t) { return 0; }

private:
	mutable Mp3FileImportableSource _mp3;
	int _channel;
};

};
#endif
