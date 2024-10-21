/*
 * Copyright (C) 2006-2009 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
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

#pragma once

#include "CAExtAudioFile.h"

#include "ardour/audiofilesource.h"
#include <string>

namespace ARDOUR {

class LIBARDOUR_API CoreAudioSource : public AudioFileSource {
  public:
	CoreAudioSource (ARDOUR::Session&, const XMLNode&);
	CoreAudioSource (ARDOUR::Session&, const std::string& path, int chn, Flag);
	~CoreAudioSource ();

	void set_path (const std::string& p);

	float sample_rate() const;
	int update_header (samplepos_t when, struct tm&, time_t);

    uint32_t channel_count () const { return n_channels; }

	int flush_header () {return 0;};
	void set_header_natural_position () {};
	bool clamped_at_unity () const { return false; }

	void flush () {}

	static int get_soundfile_info (std::string path, SoundFileInfo& _info, std::string& error_msg);

  protected:
	void close ();
	samplecnt_t read_unlocked (Sample *dst, samplepos_t start, samplecnt_t cnt) const;
	samplecnt_t write_unlocked (Sample const * src, samplecnt_t) { return 0; }

  private:
	mutable CAExtAudioFile af;
	uint16_t n_channels;

	void init_cafile ();
	int safe_read (Sample*, samplepos_t start, samplecnt_t cnt, AudioBufferList&) const;
};

}; /* namespace ARDOUR */


