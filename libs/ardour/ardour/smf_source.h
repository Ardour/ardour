/*
    Copyright (C) 2006 Paul Davis
	Written by Dave Robillard, 2006

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

#ifndef __ardour_smf_filesource_h__ 
#define __ardour_smf_filesource_h__

#include <cstdio>
#include <time.h>

#include <ardour/midi_source.h>

namespace ARDOUR {

class MidiRingBuffer;

/** Standard Midi File (Type 0) Source */
class SMFSource : public MidiSource {
  public:
	enum Flag {
		Writable = 0x1,
		CanRename = 0x2,
		Broadcast = 0x4,
		Removable = 0x8,
		RemovableIfEmpty = 0x10,
		RemoveAtDestroy = 0x20,
		BuildPeaks = 0x40
	};
	
	/** Constructor for existing external-to-session files */
	SMFSource (Session& session, std::string path, Flag flags = Flag(0));

	/* Constructor for existing in-session files */
	SMFSource (Session& session, const XMLNode&);

	virtual ~SMFSource ();

	/* this block of methods do nothing for regular file sources, but are significant
	   for files used in destructive recording.
	*/
	// FIXME and thus are useless for MIDI.. but make MidiDiskstream compile easier! :)

	virtual nframes_t last_capture_start_frame() const { return 0; }
	virtual void           mark_capture_start (nframes_t) {}
	virtual void           mark_capture_end () {}
	virtual void           clear_capture_marks() {}

	int set_name (string newname, bool destructive);

	string path() const { return _path; }

	void set_allow_remove_if_empty (bool yn);
	void mark_for_remove();

	int update_header (nframes_t when, struct tm&, time_t);
	int flush_header ();
	int flush_footer ();

	int move_to_trash (const string trash_dir_name);

	static bool is_empty (string path);
	void mark_streaming_write_completed ();

	void   mark_take (string);
	string take_id() const { return _take_id; }

	static void set_search_path (string);
	static void set_header_position_offset (nframes_t offset, bool negative);

	XMLNode& get_state ();
	int set_state (const XMLNode&);

	void seek_to(nframes_t time);

  private:

	int init (string idstr, bool must_exist);

	nframes_t read_unlocked (MidiRingBuffer& dst, nframes_t start, nframes_t cn, nframes_t stamp_offset) const;
	nframes_t write_unlocked (MidiRingBuffer& dst, nframes_t cnt);

	bool find (std::string path, bool must_exist, bool& is_new);
	bool removable() const;
	bool writable() const { return _flags & Writable; }

	int open();

	void     write_chunk_header(char id[4], uint32_t length);
	void     write_chunk(char id[4], uint32_t length, void* data);
	size_t   write_var_len(uint32_t val);
	uint32_t read_var_len() const;
	int      read_event(MidiEvent& ev) const;

	uint16_t       _channel;
	string         _path;
	Flag           _flags;
	string         _take_id;
	bool           _allow_remove_if_empty;
	uint64_t       _timeline_position;
	FILE*          _fd;
	nframes_t _last_ev_time; // last frame time written, relative to source start
	uint32_t       _track_size;
	uint32_t       _header_size; // size of SMF header, including MTrk chunk header

	static string _search_path;
};

}; /* namespace ARDOUR */

#endif /* __ardour_smf_filesource_h__ */

