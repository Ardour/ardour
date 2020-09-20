/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2016 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_audiofilesource_h__
#define __ardour_audiofilesource_h__

#include <exception>
#include <time.h>
#include "ardour/audiosource.h"
#include "ardour/file_source.h"

namespace ARDOUR {

struct LIBARDOUR_API SoundFileInfo {
	float       samplerate;
	uint16_t    channels;
	int64_t     length;
	std::string format_name;
	int64_t     timecode;
	bool        seekable; // non-seekable files must be converted/imported
};

class LIBARDOUR_API AudioFileSource : public AudioSource, public FileSource {
public:
	virtual ~AudioFileSource ();

	std::string construct_peak_filepath (const std::string& audio_path, const bool in_session = false, const bool old_peak_name = false) const;

	static bool get_soundfile_info (const std::string& path, SoundFileInfo& _info, std::string& error);

	bool safe_file_extension (const std::string& path) const {
		return safe_audio_file_extension(path);
	}

	virtual samplepos_t last_capture_start_sample() const { return 0; }
	virtual void      mark_capture_start (samplepos_t) {}
	virtual void      mark_capture_end () {}
	virtual void      clear_capture_marks() {}
	virtual bool      one_of_several_channels () const { return false; }

	virtual void flush () = 0;
	virtual int update_header (samplepos_t when, struct tm&, time_t) = 0;
	virtual int flush_header () = 0;

	void mark_streaming_write_completed (const Lock& lock);

	int setup_peakfile ();
	void set_gain (float g, bool temporarily = false);

	XMLNode& get_state ();
	int set_state (const XMLNode&, int version);

	bool can_truncate_peaks() const { return true; }
	bool can_be_analysed() const    { return _length.positive(); }

	static bool safe_audio_file_extension (const std::string& path);

	static bool is_empty (Session&, std::string path);

	static void set_bwf_serial_number (int);
	static void set_header_position_offset (samplecnt_t offset);

	static PBD::Signal0<void> HeaderPositionOffsetChanged;

protected:
	/** Constructor to be called for existing external-to-session files */
	AudioFileSource (Session&, const std::string& path, Source::Flag flags);

	/** Constructor to be called for new in-session files */
	AudioFileSource (Session&, const std::string& path, const std::string& origin, Source::Flag flags,
			SampleFormat samp_format, HeaderFormat hdr_format);

	/** Constructor to be called for existing in-session files */
	AudioFileSource (Session&, const XMLNode&, bool must_exist = true);

	/** Constructor to be called for crash recovery. Final argument is not
	 * used but exists to differentiate from the external-to-session
	 * constructor above.
	 */
	AudioFileSource (Session&, const std::string& path, Source::Flag flags, bool);

	int init (const std::string& idstr, bool must_exist);

	virtual void set_header_natural_position () = 0;
	virtual void handle_header_position_change () {}

	int move_dependents_to_trash();

	static Sample* get_interleave_buffer (samplecnt_t size);

	static char bwf_country_code[3];
	static char bwf_organization_code[4];
	static char bwf_serial_number[13];

	/** Kept up to date with the position of the session location start */
	static samplecnt_t header_position_offset;
};

} // namespace ARDOUR

#endif /* __ardour_audiofilesource_h__ */

