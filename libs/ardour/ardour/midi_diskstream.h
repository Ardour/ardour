/*
    Copyright (C) 2000 Paul Davis 

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

    $Id: diskstream.h 579 2006-06-12 19:56:37Z essej $
*/

#ifndef __ardour_midi_diskstream_h__
#define __ardour_midi_diskstream_h__

#include <sigc++/signal.h>

#include <cmath>
#include <string>
#include <queue>
#include <map>
#include <vector>

#include <time.h>

#include <pbd/fastlog.h>
#include <pbd/ringbufferNPT.h>
 

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/route_group.h>
#include <ardour/route.h>
#include <ardour/port.h>
#include <ardour/utils.h>
#include <ardour/diskstream.h>
#include <ardour/midi_playlist.h>
struct tm;

namespace ARDOUR {

class MidiEngine;
class Send;
class Session;
class MidiPlaylist;
class SMFSource;
class IO;

class MidiDiskstream : public Diskstream
{	
  public:
	MidiDiskstream (Session &, const string& name, Diskstream::Flag f = Recordable);
	MidiDiskstream (Session &, const XMLNode&);

	void set_io (ARDOUR::IO& io);

	MidiDiskstream& ref() { _refcnt++; return *this; }
	//void unref() { if (_refcnt) _refcnt--; if (_refcnt == 0) delete this; }
	//uint32_t refcnt() const { return _refcnt; }

	float playback_buffer_load() const;
	float capture_buffer_load() const;

	//void set_align_style (AlignStyle);
	//void set_persistent_align_style (AlignStyle);

	void set_record_enabled (bool yn, void *src);
	//void set_speed (double);

	int use_playlist (Playlist *);
	int use_new_playlist ();
	int use_copy_playlist ();

	void start_scrub (jack_nframes_t where) {} // FIXME?
	void end_scrub () {} // FIXME?

	Playlist *playlist () { return _playlist; }

	static sigc::signal<void,list<SMFSource*>*> DeleteSources;

	/* stateful */

	XMLNode& get_state(void);
	int set_state(const XMLNode& node);

	void monitor_input (bool);

	//void handle_input_change (IOChange, void *src);

  protected:
	friend class Session;

	/* the Session is the only point of access for these
	   because they require that the Session is "inactive"
	   while they are called.
	*/

	void set_pending_overwrite(bool);
	int  overwrite_existing_buffers ();
	void reverse_scrub_buffer (bool to_forward) {} // FIXME?
	void set_block_size (jack_nframes_t);
	int  internal_playback_seek (jack_nframes_t distance);
	int  can_internal_playback_seek (jack_nframes_t distance);
	int  rename_write_sources ();
	void reset_write_sources (bool, bool force = false);
	void non_realtime_input_change ();

	uint32_t read_data_count() const { return _read_data_count; }
	uint32_t write_data_count() const { return _write_data_count; }

  protected:
	friend class Auditioner;
	int  seek (jack_nframes_t which_sample, bool complete_refill = false);

  protected:
	friend class MidiTrack;

	int  process (jack_nframes_t transport_frame, jack_nframes_t nframes, jack_nframes_t offset, bool can_record, bool rec_monitors_input);
	bool commit  (jack_nframes_t nframes);

  private:

	/* use unref() to destroy a diskstream */
	~MidiDiskstream();

	MidiPlaylist* _playlist;

	/* the two central butler operations */

	int do_flush (char * workbuf, bool force = false);
	int do_refill (RawMidi *mixdown_buffer, float *gain_buffer, char *workbuf);
	
	virtual int non_realtime_do_refill() { return do_refill(0, 0, 0); }

	int read (RawMidi* buf, RawMidi* mixdown_buffer, char * workbuf, jack_nframes_t& start, jack_nframes_t cnt, bool reversed);

	/* XXX fix this redundancy ... */

	//void playlist_changed (Change);
	//void playlist_modified ();
	void playlist_deleted (Playlist*);
	void session_controls_changed (Session::ControlType) {} // FIXME?

	void finish_capture (bool rec_monitors_input);
	void clean_up_capture (struct tm&, time_t, bool abort) {} // FIXME?
	void transport_stopped (struct tm&, time_t, bool abort);

	struct CaptureInfo {
	    uint32_t start;
	    uint32_t frames;
	};

	void init (Diskstream::Flag);

	int use_new_write_source (uint32_t n=0);
	int use_new_fade_source (uint32_t n=0) { return 0; } // FIXME?

	int find_and_use_playlist (const string&);

	void allocate_temporary_buffers ();

	int  create_input_port () { return 0; } // FIXME?
	int  connect_input_port () { return 0; } // FIXME?
	int  seek_unlocked (jack_nframes_t which_sample) { return 0; } // FIXME?

	int ports_created () { return 0; } // FIXME?

	//bool realtime_set_speed (double, bool global_change);
	void non_realtime_set_speed ();

	int use_pending_capture_data (XMLNode& node);

	void get_input_sources ();
	void check_record_status (jack_nframes_t transport_frame, jack_nframes_t nframes, bool can_record);
	void set_align_style_from_io();
	void setup_destructive_playlist ();
	void use_destructive_playlist ();
	
	std::list<Region*>      _last_capture_regions;
	std::vector<SMFSource*> _capturing_sources;
};

}; /* namespace ARDOUR */

#endif /* __ardour_midi_diskstream_h__ */
