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

#ifndef __ardour_audiofilesource_h__ 
#define __ardour_audiofilesource_h__

#include <exception>
#include <time.h>
#include <ardour/audiosource.h>
#include <ardour/file_source.h>

namespace ARDOUR {

struct SoundFileInfo {
    float       samplerate;
    uint16_t    channels;
    int64_t     length;
    std::string format_name;
    int64_t     timecode;
};

class AudioFileSource : public AudioSource, public FileSource {
public:
	virtual ~AudioFileSource ();

	bool set_name (const std::string& newname) {
		return (set_source_name(newname, destructive()) == 0);
	}
	
	Glib::ustring peak_path (Glib::ustring audio_path);
	Glib::ustring find_broken_peakfile (Glib::ustring missing_peak_path,
					     Glib::ustring audio_path);

	static void set_peak_dir (Glib::ustring dir) { peak_dir = dir; }

	static bool get_soundfile_info (Glib::ustring path, SoundFileInfo& _info, std::string& error);

	bool safe_file_extension (const Glib::ustring& path) const {
		return safe_audio_file_extension(path);
	}

	/* this block of methods do nothing for regular file sources, but are significant
	   for files used in destructive recording.
	*/
	virtual nframes_t last_capture_start_frame() const { return 0; }
	virtual void      mark_capture_start (nframes_t) {}
	virtual void      mark_capture_end () {}
	virtual void      clear_capture_marks() {}
	virtual bool      one_of_several_channels () const { return false; }

	virtual int update_header (nframes_t when, struct tm&, time_t) = 0;
	virtual int flush_header () = 0;

	void mark_streaming_write_completed ();

	int setup_peakfile ();

	XMLNode& get_state ();
	int set_state (const XMLNode&);

	bool can_truncate_peaks() const { return !destructive(); }
	bool can_be_analysed() const    { return _length > 0; } 
	
	static bool safe_audio_file_extension (const Glib::ustring& path);
	
	static bool is_empty (Session&, Glib::ustring path);
	
	static void set_bwf_serial_number (int);
	static void set_header_position_offset (nframes_t offset );

	static sigc::signal<void> HeaderPositionOffsetChanged;

protected:
	/** Constructor to be called for existing external-to-session files */
	AudioFileSource (Session&, const Glib::ustring& path, bool embedded, Source::Flag flags);

	/** Constructor to be called for new in-session files */
	AudioFileSource (Session&, const Glib::ustring& path, bool embedded, Source::Flag flags,
			 SampleFormat samp_format, HeaderFormat hdr_format);

	/** Constructor to be called for existing in-session files */
	AudioFileSource (Session&, const XMLNode&, bool must_exist = true);

	int init (const Glib::ustring& idstr, bool must_exist);
	
	virtual void set_header_timeline_position () = 0;
	virtual void handle_header_position_change () {}
	
	int move_dependents_to_trash();

	static Sample* get_interleave_buffer (nframes_t size);

	static Glib::ustring peak_dir;

	static char bwf_country_code[3];
	static char bwf_organization_code[4];
	static char bwf_serial_number[13];

	static uint64_t header_position_offset;

  private:
	Glib::ustring old_peak_path (Glib::ustring audio_path);
	Glib::ustring broken_peak_path (Glib::ustring audio_path);
};

} // namespace ARDOUR

#endif /* __ardour_audiofilesource_h__ */

