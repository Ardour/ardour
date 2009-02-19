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

#include <ardour/audiofilesource.h>
#include <ardour/broadcast_info.h>

namespace ARDOUR {

class SndFileSource : public AudioFileSource {
  public:
	/** Constructor to be called for existing external-to-session files */
	SndFileSource (Session&, const Glib::ustring& path, bool embedded, int chn, Flag flags);

	/* Constructor to be called for new in-session files */
	SndFileSource (Session&, const Glib::ustring& path, bool embedded,
			SampleFormat samp_format, HeaderFormat hdr_format, nframes_t rate, 
			Flag flags = SndFileSource::default_writable_flags);
		       
	/** Constructor to be called for existing in-session files */
	SndFileSource (Session&, const XMLNode&);

	~SndFileSource ();

	float sample_rate () const;
	int update_header (sframes_t when, struct tm&, time_t);
	int flush_header ();

	sframes_t natural_position () const;

	sframes_t last_capture_start_frame() const;
	void mark_capture_start (sframes_t);
	void mark_capture_end ();
	void clear_capture_marks();

	bool set_destructive (bool yn);

	bool one_of_several_channels () const;

	static void setup_standard_crossfades (nframes_t sample_rate);
	static const Source::Flag default_writable_flags;

	static int get_soundfile_info (const Glib::ustring& path, SoundFileInfo& _info, string& error_msg);

  protected:
	void set_header_timeline_position ();

	nframes_t read_unlocked (Sample *dst, sframes_t start, nframes_t cnt) const;
	nframes_t write_unlocked (Sample *dst, nframes_t cnt);

	nframes_t write_float (Sample* data, sframes_t pos, nframes_t cnt);

  private:
	SNDFILE *sf;
	SF_INFO _info;
	BroadcastInfo *_broadcast_info;

	void init_sndfile ();
	int open();
	int setup_broadcast_info (sframes_t when, struct tm&, time_t);

	/* destructive */

	static nframes_t xfade_frames;
	static gain_t* out_coefficient;
	static gain_t* in_coefficient;

	bool          _capture_start;
	bool          _capture_end;
	sframes_t      capture_start_frame;
	sframes_t      file_pos; // unit is frames
	nframes_t      xfade_out_count;
	nframes_t      xfade_in_count;
	Sample*        xfade_buf;

	nframes_t crossfade (Sample* data, nframes_t cnt, int dir);
	void set_timeline_position (int64_t);
	nframes_t destructive_write_unlocked (Sample *dst, nframes_t cnt);
	nframes_t nondestructive_write_unlocked (Sample *dst, nframes_t cnt);
	void handle_header_position_change ();
};

} // namespace ARDOUR

#endif /* __sndfile_source_h__ */

