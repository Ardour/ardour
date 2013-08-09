/*
    Copyright (C) 2006 Paul Davis

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

#ifndef __sndfile_source_h__
#define __sndfile_source_h__

#include <sndfile.h>

#include "ardour/audiofilesource.h"
#include "ardour/broadcast_info.h"
#include "pbd/sndfile_manager.h"

namespace ARDOUR {

class SndFileSource : public AudioFileSource {
  public:
	/** Constructor to be called for existing external-to-session files */
	SndFileSource (Session&, const std::string& path, int chn, Flag flags);

	/* Constructor to be called for new in-session files */
	SndFileSource (Session&, const std::string& path, const std::string& origin,
	               SampleFormat samp_format, HeaderFormat hdr_format, framecnt_t rate,
	               Flag flags = SndFileSource::default_writable_flags);

	/** Constructor to be called for existing in-session files */
	SndFileSource (Session&, const XMLNode&);

	~SndFileSource ();

	float sample_rate () const;
	int update_header (framepos_t when, struct tm&, time_t);
	int flush_header ();
	void flush ();

	framepos_t natural_position () const;

	framepos_t last_capture_start_frame() const;
	void mark_capture_start (framepos_t);
	void mark_capture_end ();
	void clear_capture_marks();

	bool set_destructive (bool yn);

	bool one_of_several_channels () const;

	bool clamped_at_unity () const;

	static void setup_standard_crossfades (Session const &, framecnt_t sample_rate);
	static const Source::Flag default_writable_flags;

	static int get_soundfile_info (const std::string& path, SoundFileInfo& _info, std::string& error_msg);

  protected:
	void set_path (const std::string& p);
	void set_header_timeline_position ();

	framecnt_t read_unlocked (Sample *dst, framepos_t start, framecnt_t cnt) const;
	framecnt_t write_unlocked (Sample *dst, framecnt_t cnt);
	framecnt_t write_float (Sample* data, framepos_t pos, framecnt_t cnt);

  private:
	PBD::SndFileDescriptor* _descriptor;
	SF_INFO _info;
	BroadcastInfo *_broadcast_info;

	void init_sndfile ();
	int open();
	int setup_broadcast_info (framepos_t when, struct tm&, time_t);
	void file_closed ();

	/* destructive */

	static framecnt_t xfade_frames;
	static gain_t* out_coefficient;
	static gain_t* in_coefficient;

	bool          _capture_start;
	bool          _capture_end;
	framepos_t     capture_start_frame;
	framepos_t     file_pos; // unit is frames
	framecnt_t     xfade_out_count;
	framecnt_t     xfade_in_count;
	Sample*        xfade_buf;

	framecnt_t crossfade (Sample* data, framecnt_t cnt, int dir);
	void set_timeline_position (framepos_t);
	framecnt_t destructive_write_unlocked (Sample *dst, framecnt_t cnt);
	framecnt_t nondestructive_write_unlocked (Sample *dst, framecnt_t cnt);
	void handle_header_position_change ();
	PBD::ScopedConnection header_position_connection;
	PBD::ScopedConnection file_manager_connection;
};

} // namespace ARDOUR

#endif /* __sndfile_source_h__ */

