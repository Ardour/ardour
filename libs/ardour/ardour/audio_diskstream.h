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

#ifndef __ardour_diskstream_h__
#define __ardour_diskstream_h__

#include <sigc++/signal.h>

#include <cmath>
#include <string>
#include <queue>
#include <map>
#include <vector>

#include <time.h>

#include <pbd/fastlog.h>
#include <pbd/ringbufferNPT.h>
#include <pbd/stateful.h> 

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/route_group.h>
#include <ardour/route.h>
#include <ardour/port.h>
#include <ardour/utils.h>

struct tm;

namespace ARDOUR {

class AudioEngine;
class Send;
class Session;
class AudioPlaylist;
class AudioFileSource;
class IO;

class AudioDiskstream : public Stateful, public sigc::trackable
{	
  public:
	enum Flag {
		Recordable = 0x1,
		Hidden = 0x2,
		Destructive = 0x4
	};

	AudioDiskstream (Session &, const string& name, Flag f = Recordable);
	AudioDiskstream (Session &, const XMLNode&);

	string name() const { return _name; }

	ARDOUR::IO* io() const { return _io; }
	void set_io (ARDOUR::IO& io);

	AudioDiskstream& ref() { _refcnt++; return *this; }
	void unref() { if (_refcnt) _refcnt--; if (_refcnt == 0) delete this; }
	uint32_t refcnt() const { return _refcnt; }

	float playback_buffer_load() const;
	float capture_buffer_load() const;

	void set_flag (Flag f) {
		_flags |= f;
	}

	void unset_flag (Flag f) {
		_flags &= ~f;
	}

	AlignStyle alignment_style() const { return _alignment_style; }
	void set_align_style (AlignStyle);
	void set_persistent_align_style (AlignStyle);

	bool hidden() const { return _flags & Hidden; }
	bool recordable() const { return _flags & Recordable; }
	bool destructive() const { return _flags & Destructive; }

	void set_destructive (bool yn);

	jack_nframes_t roll_delay() const { return _roll_delay; }
	void set_roll_delay (jack_nframes_t);

	int set_name (string str, void* src);

	string input_source (uint32_t n=0) const {
		if (n < channels.size()) {
			return channels[n].source ? channels[n].source->name() : "";
		} else {
			return ""; 
		}
	}

	Port *input_source_port (uint32_t n=0) const { 
		if (n < channels.size()) return channels[n].source; return 0; 
	}

	void set_record_enabled (bool yn, void *src);
	bool record_enabled() const { return g_atomic_int_get (&_record_enabled); }
	void punch_in ();
	void punch_out ();

	bool  reversed() const { return _actual_speed < 0.0f; }
	double speed() const { return _visible_speed; }
	void set_speed (double);

	float peak_power(uint32_t n=0) { 
		float x = channels[n].peak_power;
		channels[n].peak_power = 0.0f;
		if (x > 0.0f) {
			return 20.0f * fast_log10(x);
		} else {
			return minus_infinity();
		}
	}

	int  use_playlist (AudioPlaylist *);
	int use_new_playlist ();
	int use_copy_playlist ();

	void start_scrub (jack_nframes_t where);
	void end_scrub ();

	Sample *playback_buffer (uint32_t n=0) {
		if (n < channels.size())
			return channels[n].current_playback_buffer;
		return 0;
	}
	
	Sample *capture_buffer (uint32_t n=0) {
		if (n < channels.size())
			return channels[n].current_capture_buffer;
		return 0;
	}

	AudioPlaylist *playlist () { return _playlist; }

	AudioFileSource *write_source (uint32_t n=0) {
		if (n < channels.size())
			return channels[n].write_source;
		return 0;
	}

	jack_nframes_t current_capture_start() const { return capture_start_frame; }
	jack_nframes_t current_capture_end() const { return capture_start_frame + capture_captured; }
	jack_nframes_t get_capture_start_frame (uint32_t n=0);
	jack_nframes_t get_captured_frames (uint32_t n=0);
	
	uint32_t n_channels() { return _n_channels; }

	int add_channel ();
	int remove_channel ();
	
	static void set_disk_io_chunk_frames (uint32_t n) {
		disk_io_chunk_frames = n;
	}

	static jack_nframes_t disk_io_frames() { return disk_io_chunk_frames; }
	
	sigc::signal<void,void*> record_enable_changed;
	sigc::signal<void>       speed_changed;
	sigc::signal<void,void*> reverse_changed;
	sigc::signal<void>       PlaylistChanged;
	sigc::signal<void>       AlignmentStyleChanged;

	static sigc::signal<void> DiskOverrun;
	static sigc::signal<void> DiskUnderrun;
	static sigc::signal<void,AudioDiskstream*> AudioDiskstreamCreated;   // XXX use a ref with sigc2
	static sigc::signal<void,list<AudioFileSource*>*> DeleteSources;

	/* stateful */

	XMLNode& get_state(void);
	int set_state(const XMLNode& node);

	void monitor_input (bool);

	jack_nframes_t capture_offset() const { return _capture_offset; }
	void           set_capture_offset ();

	static void swap_by_ptr (Sample *first, Sample *last) {
		while (first < last) {
			Sample tmp = *first;
			*first++ = *last;
			*last-- = tmp;
		}
	}

	static void swap_by_ptr (Sample *first, Sample *last, jack_nframes_t n) {
		while (n--) {
			Sample tmp = *first;
			*first++ = *last;
			*last-- = tmp;
		}
	}

	bool slaved() const { return _slaved; }
	void set_slaved(bool yn) { _slaved = yn; }

	int set_loop (Location *loc);
	sigc::signal<void,Location *> LoopSet;

	std::list<Region*>& last_capture_regions () {
		return _last_capture_regions;
	}

	void handle_input_change (IOChange, void *src);

	const PBD::ID& id() const { return _id; }

	XMLNode* deprecated_io_node;

  protected:
	friend class Session;

	/* the Session is the only point of access for these
	   because they require that the Session is "inactive"
	   while they are called.
	*/

	void set_pending_overwrite (bool);
	int  overwrite_existing_buffers ();
	void reverse_scrub_buffer (bool to_forward);
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
	friend class AudioTrack;

	void prepare ();
	int  process (jack_nframes_t transport_frame, jack_nframes_t nframes, jack_nframes_t offset, bool can_record, bool rec_monitors_input);
	bool commit  (jack_nframes_t nframes);
	void recover (); /* called if commit will not be called, but process was */

  private:

	/* use unref() to destroy a diskstream */

	~AudioDiskstream();

	enum TransitionType {
		CaptureStart = 0,
		CaptureEnd
	};
	
	struct CaptureTransition {

		TransitionType   type;
		// the start or end file frame pos
		jack_nframes_t   capture_val;
	};
	
	struct ChannelInfo {

		Sample     *playback_wrap_buffer;
		Sample     *capture_wrap_buffer;
		Sample     *speed_buffer;

	        float       peak_power;

	        AudioFileSource   *fades_source;
		AudioFileSource   *write_source;

		Port         *source;
		Sample       *current_capture_buffer;
		Sample       *current_playback_buffer;

		RingBufferNPT<Sample> *playback_buf;
		RingBufferNPT<Sample> *capture_buf;

		Sample* scrub_buffer;
		Sample* scrub_forward_buffer;
		Sample* scrub_reverse_buffer;

		RingBufferNPT<Sample>::rw_vector playback_vector;
		RingBufferNPT<Sample>::rw_vector capture_vector;

		RingBufferNPT<CaptureTransition> * capture_transition_buf;
		// the following are used in the butler thread only
		jack_nframes_t                     curr_capture_cnt;
	};

	typedef vector<ChannelInfo> ChannelList;


	string            _name;
	ARDOUR::Session&  _session;
	ARDOUR::IO*       _io;
	ChannelList        channels;
	uint32_t      _n_channels;
	PBD::ID           _id;

	mutable gint             _record_enabled;
	AudioPlaylist*           _playlist;
	double                   _visible_speed;
	double                   _actual_speed;
	/* items needed for speed change logic */
	bool                     _buffer_reallocation_required;
	bool                     _seek_required;
	
	bool                      force_refill;
	jack_nframes_t            capture_start_frame;
	jack_nframes_t            capture_captured;
	bool                      was_recording;
	jack_nframes_t            adjust_capture_position;
	jack_nframes_t           _capture_offset;
	jack_nframes_t           _roll_delay;
	jack_nframes_t            first_recordable_frame;
	jack_nframes_t            last_recordable_frame;
	int                       last_possibly_recording;
	AlignStyle               _alignment_style;
	bool                     _scrubbing;
	bool                     _slaved;
	bool                     _processed;
	Location*                 loop_location;
	jack_nframes_t            overwrite_frame;
	off_t                     overwrite_offset;
	bool                      pending_overwrite;
	bool                      overwrite_queued;
	IOChange                  input_change_pending;
	jack_nframes_t            wrap_buffer_size;
	jack_nframes_t            speed_buffer_size;

	uint64_t                  last_phase;
	uint64_t                  phi;
	
	jack_nframes_t            file_frame;		
	jack_nframes_t            playback_sample;
	jack_nframes_t            playback_distance;

	uint32_t                 _read_data_count;
	uint32_t                 _write_data_count;

	bool                      in_set_state;
	AlignStyle               _persistent_alignment_style;
	bool                      first_input_change;

	Glib::Mutex  state_lock;

	jack_nframes_t scrub_start;
	jack_nframes_t scrub_buffer_size;
	jack_nframes_t scrub_offset;
	uint32_t _refcnt;

	sigc::connection ports_created_c;
	sigc::connection plmod_connection;
	sigc::connection plstate_connection;
	sigc::connection plgone_connection;

	/* the two central butler operations */

	int do_flush (char * workbuf, bool force = false);
	int do_refill (Sample *mixdown_buffer, float *gain_buffer, char *workbuf);

	int read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer, char * workbuf, jack_nframes_t& start, jack_nframes_t cnt, 
		  ChannelInfo& channel_info, int channel, bool reversed);

	uint32_t i_am_the_modifier;
	
	/* XXX fix this redundancy ... */

	void playlist_changed (Change);
	void playlist_modified ();
	void playlist_deleted (Playlist*);
	void session_controls_changed (Session::ControlType);

	void finish_capture (bool rec_monitors_input);
	void clean_up_capture (struct tm&, time_t, bool abort);
	void transport_stopped (struct tm&, time_t, bool abort);

	struct CaptureInfo {
	    uint32_t start;
	    uint32_t frames;
	};

	vector<CaptureInfo*> capture_info;
	Glib::Mutex  capture_info_lock;
	
	void init (Flag);

	void init_channel (ChannelInfo &chan);
	void destroy_channel (ChannelInfo &chan);
	
	static jack_nframes_t disk_io_chunk_frames;

	int use_new_write_source (uint32_t n=0);
	int use_new_fade_source (uint32_t n=0);

	int find_and_use_playlist (const string&);

	void allocate_temporary_buffers ();

	unsigned char _flags;

	int  create_input_port ();
	int  connect_input_port ();
	int  seek_unlocked (jack_nframes_t which_sample);

	int ports_created ();

	bool realtime_set_speed (double, bool global_change);
	void non_realtime_set_speed ();

	std::list<Region*> _last_capture_regions;
	std::vector<AudioFileSource*> capturing_sources;
	int use_pending_capture_data (XMLNode& node);

	void get_input_sources ();
	void check_record_status (jack_nframes_t transport_frame, jack_nframes_t nframes, bool can_record);
	void set_align_style_from_io();
	void setup_destructive_playlist ();
	void use_destructive_playlist ();
};

}; /* namespace ARDOUR */

#endif /* __ardour_diskstream_h__ */
