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
#include <evoral/SMF.hpp>

namespace Evoral { template<typename T> class Event; }

namespace ARDOUR {

template<typename T> class MidiRingBuffer;

/** Standard Midi File (Type 0) Source */
class SMFSource : public MidiSource, public Evoral::SMF {
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

	bool set_name (const std::string& newname) { return (set_source_name(newname, false) == 0); }
	int set_source_name (string newname, bool destructive);
	
	static bool safe_file_extension (const Glib::ustring& path);
	
	Glib::ustring path() const { return _path; }

	void set_allow_remove_if_empty (bool yn);
	void mark_for_remove();

	void append_event_unlocked(EventTimeUnit unit, const Evoral::Event<double>& ev);

	int move_to_trash (const string trash_dir_name);

	void mark_streaming_midi_write_started (NoteMode mode, nframes_t start_time);
	void mark_streaming_write_completed ();

	void   mark_take (string);
	string take_id() const { return _take_id; }

	static void set_search_path (string);
	static void set_header_position_offset (nframes_t offset, bool negative);

	XMLNode& get_state ();
	int set_state (const XMLNode&);

	void load_model(bool lock=true, bool force_reload=false);
	void destroy_model();

	double last_event_time() const { return _last_ev_time; }

	void flush_midi();

  private:
	int init (string idstr, bool must_exist);

	nframes_t read_unlocked (
			MidiRingBuffer<nframes_t>& dst,
			nframes_t start,
			nframes_t cn,
			nframes_t stamp_offset,
			nframes_t negative_stamp_offset) const;

	nframes_t write_unlocked (
			MidiRingBuffer<nframes_t>& src,
			nframes_t cnt);

	bool find (std::string path, bool must_exist, bool& is_new);
	bool removable() const;
	bool writable() const { return _flags & Writable; }
	
	void set_default_controls_interpolation();

	Glib::ustring  _path;
	Flag           _flags;
	string         _take_id;
	bool           _allow_remove_if_empty;
	double         _last_ev_time;

	static string _search_path;
};

}; /* namespace ARDOUR */

#endif /* __ardour_smf_filesource_h__ */

