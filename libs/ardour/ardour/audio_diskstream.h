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

#ifndef __ardour_audio_diskstream_h__
#define __ardour_audio_diskstream_h__

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
#include <ardour/diskstream.h>
#include <ardour/audioplaylist.h>

struct tm;

namespace ARDOUR {

class AudioEngine;
class Send;
class Session;
class AudioPlaylist;
class AudioFileSource;
class IO;

class AudioDiskstream : public Diskstream
{	
  public:
	AudioDiskstream (Session &, const string& name, Diskstream::Flag f = Recordable);
	AudioDiskstream (Session &, const XMLNode&);

	void set_io (ARDOUR::IO& io);

	AudioDiskstream& ref() { _refcnt++; return *this; }
	//void unref() { if (_refcnt) _refcnt--; if (_refcnt == 0) delete this; }
	//uint32_t refcnt() const { return _refcnt; }

	float playback_buffer_load() const;
	float capture_buffer_load() const;

	//void set_align_style (AlignStyle);
	//void set_persistent_align_style (AlignStyle);

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
	//void set_speed (double);

	float peak_power(uint32_t n=0) { 
		float x = channels[n].peak_power;
		channels[n].peak_power = 0.0f;
		if (x > 0.0f) {
			return 20.0f * fast_log10(x);
		} else {
			return minus_infinity();
		}
	}

	int use_playlist (Playlist *);
	int use_new_playlist ();
	int use_copy_playlist ();

	void start_scrub (jack_nframes_t where) {} // FIXME?
	void end_scrub () {} // FIXME?

	Playlist *playlist () { return _playlist; }

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

	AudioFileSource *write_source (uint32_t n=0) {
		if (n < channels.size())
			return channels[n].write_source;
		return 0;
	}

	int add_channel ();
	int remove_channel ();
	
	
	/* stateful */

	XMLNode& get_state(void);
	int set_state(const XMLNode& node);

	void monitor_input (bool);

	// FIXME: these don't belong here
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

	//void handle_input_change (IOChange, void *src);
	
	//static sigc::signal<void> DiskOverrun;
	//static sigc::signal<void> DiskUnderrun;
	//static sigc::signal<void,AudioDiskstream*> AudioDiskstreamCreated;   // XXX use a ref with sigc2
	static sigc::signal<void,list<AudioFileSource*>*> DeleteSources;

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

	void set_pending_overwrite(bool);
	int  overwrite_existing_buffers ();
	void reverse_scrub_buffer (bool to_forward) {} // FIXME?
	void set_block_size (jack_nframes_t);
	int  internal_playback_seek (jack_nframes_t distance);
	int  can_internal_playback_seek (jack_nframes_t distance);
	int  rename_write_sources ();
	void reset_write_sources (bool, bool force = false);
	void non_realtime_input_change ();

  protected:
	friend class Auditioner;
	int  seek (jack_nframes_t which_sample, bool complete_refill = false);

  protected:
	friend class AudioTrack;

	int  process (jack_nframes_t transport_frame, jack_nframes_t nframes, jack_nframes_t offset, bool can_record, bool rec_monitors_input);
	bool commit  (jack_nframes_t nframes);

  private:

	/* use unref() to destroy a diskstream */
	~AudioDiskstream();

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

	/* the two central butler operations */

	int do_flush (char * workbuf, bool force = false);
	int do_refill (Sample *mixdown_buffer, float *gain_buffer, char *workbuf);
	
	virtual int non_realtime_do_refill() { return do_refill(0, 0, 0); }

	int read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer, char * workbuf, jack_nframes_t& start, jack_nframes_t cnt, 
		  ChannelInfo& channel_info, int channel, bool reversed);

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

	vector<CaptureInfo*> capture_info;
	Glib::Mutex  capture_info_lock;
	
	void init (Diskstream::Flag);

	void init_channel (ChannelInfo &chan);
	void destroy_channel (ChannelInfo &chan);
	
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

	std::list<Region*> _last_capture_regions;
	std::vector<AudioFileSource*> capturing_sources;
	int use_pending_capture_data (XMLNode& node);

	void get_input_sources ();
	void check_record_status (jack_nframes_t transport_frame, jack_nframes_t nframes, bool can_record);
	void set_align_style_from_io();
	void setup_destructive_playlist ();
	void use_destructive_playlist ();

	ChannelList    channels;
	AudioPlaylist* _playlist;
	void engage_record_enable (void* src);
	void disengage_record_enable (void* src);
};

}; /* namespace ARDOUR */

#endif /* __ardour_audio_diskstream_h__ */
