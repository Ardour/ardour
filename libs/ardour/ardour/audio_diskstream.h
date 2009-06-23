/*
    Copyright (C) 2000-2006 Paul Davis 

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

#ifndef __ardour_audio_diskstream_h__
#define __ardour_audio_diskstream_h__

#include <sigc++/signal.h>

#include <cmath>
#include <string>
#include <queue>
#include <map>
#include <vector>

#include <time.h>

#include "pbd/fastlog.h"
#include "pbd/ringbufferNPT.h"
#include "pbd/stateful.h" 
#include "pbd/rcu.h" 

#include "ardour/ardour.h"
#include "ardour/utils.h"
#include "ardour/diskstream.h"
#include "ardour/audioplaylist.h"
#include "ardour/port.h"
#include "ardour/interpolation.h"

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
	AudioDiskstream (Session &, const std::string& name, Diskstream::Flag f = Recordable);
	AudioDiskstream (Session &, const XMLNode&);
	~AudioDiskstream();

	float playback_buffer_load() const;
	float capture_buffer_load() const;

	std::string input_source (uint32_t n=0) const {
		boost::shared_ptr<ChannelList> c = channels.reader();
		if (n < c->size()) {
			return (*c)[n]->source ? (*c)[n]->source->name() : "";
		} else {
			return ""; 
		}
	}

	Port *input_source_port (uint32_t n=0) const { 
		boost::shared_ptr<ChannelList> c = channels.reader();
		if (n < c->size()) return (*c)[n]->source; return 0; 
	}

	void set_record_enabled (bool yn);
	int set_destructive (bool yn);
	int set_non_layered (bool yn);
	bool can_become_destructive (bool& requires_bounce) const;

	float peak_power(uint32_t n = 0) { 
		boost::shared_ptr<ChannelList> c = channels.reader();
		ChannelInfo* chaninfo = (*c)[n];
		float x = chaninfo->peak_power;
		chaninfo->peak_power = 0.0f;
		if (x > 0.0f) {
			return 20.0f * fast_log10(x);
		} else {
			return minus_infinity();
		}
	}
	
	boost::shared_ptr<AudioPlaylist> audio_playlist () { return boost::dynamic_pointer_cast<AudioPlaylist>(_playlist); }

	int use_playlist (boost::shared_ptr<Playlist>);
	int use_new_playlist ();
	int use_copy_playlist ();

	Sample *playback_buffer (uint32_t n = 0) {
		boost::shared_ptr<ChannelList> c = channels.reader();
		if (n < c->size())
			return (*c)[n]->current_playback_buffer;
		return 0;
	}
	
	Sample *capture_buffer (uint32_t n = 0) {
		boost::shared_ptr<ChannelList> c = channels.reader();
		if (n < c->size())
			return (*c)[n]->current_capture_buffer;
		return 0;
	}

	boost::shared_ptr<AudioFileSource> write_source (uint32_t n=0) {
		boost::shared_ptr<ChannelList> c = channels.reader();
		if (n < c->size())
			return (*c)[n]->write_source;
		return boost::shared_ptr<AudioFileSource>();
	}

	int add_channel (uint32_t how_many);
	int remove_channel (uint32_t how_many);
	
	/* stateful */

	XMLNode& get_state(void);
	int      set_state(const XMLNode& node);

	void monitor_input (bool);

	static void swap_by_ptr (Sample *first, Sample *last) {
		while (first < last) {
			Sample tmp = *first;
			*first++ = *last;
			*last-- = tmp;
		}
	}

	static void swap_by_ptr (Sample *first, Sample *last, nframes_t n) {
		while (n--) {
			Sample tmp = *first;
			*first++ = *last;
			*last-- = tmp;
		}
	}
	
	FixedPointLinearInterpolation interpolation;

	XMLNode* deprecated_io_node;
	
  protected:
	friend class Session;

	/* the Session is the only point of access for these
	   because they require that the Session is "inactive"
	   while they are called.
	*/

	void set_pending_overwrite(bool);
	int  overwrite_existing_buffers ();
	void set_block_size (nframes_t);
	int  internal_playback_seek (nframes_t distance);
	int  can_internal_playback_seek (nframes_t distance);
	int  rename_write_sources ();
	void reset_write_sources (bool, bool force = false);
	void non_realtime_input_change ();
	void non_realtime_locate (nframes_t location);

  protected:
	friend class Auditioner;
	int  seek (nframes_t which_sample, bool complete_refill = false);

  protected:
	friend class AudioTrack;

	int  process (nframes_t transport_frame, nframes_t nframes, bool can_record, bool rec_monitors_input);
	bool commit  (nframes_t nframes);

  private:

	struct ChannelInfo {
	    
	    ChannelInfo (nframes_t buffer_size, nframes_t speed_buffer_size, nframes_t wrap_buffer_size);
	    ~ChannelInfo ();

	    Sample     *playback_wrap_buffer;
	    Sample     *capture_wrap_buffer;
	    Sample     *speed_buffer;
	    
	    float       peak_power;
	    
	    boost::shared_ptr<AudioFileSource> fades_source;
	    boost::shared_ptr<AudioFileSource> write_source;

	    /// the Port that our audio data comes from
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
	    nframes_t                     curr_capture_cnt;
	};

	typedef std::vector<ChannelInfo*> ChannelList;

	void process_varispeed_playback(nframes_t nframes, boost::shared_ptr<ChannelList> c);
	
	/* The two central butler operations */
	int do_flush (RunContext context, bool force = false);
	int do_refill () { return _do_refill(_mixdown_buffer, _gain_buffer); }
	
	int do_refill_with_alloc ();

	int read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer,
		  nframes_t& start, nframes_t cnt, 
		  ChannelInfo* channel_info, int channel, bool reversed);


	void finish_capture (bool rec_monitors_input, boost::shared_ptr<ChannelList>);
	void transport_stopped (struct tm&, time_t, bool abort);
	void transport_looped (nframes_t transport_frame);

	void init (Diskstream::Flag);

	void init_channel (ChannelInfo &chan);
	void destroy_channel (ChannelInfo &chan);
	
	int use_new_write_source (uint32_t n=0);

	int find_and_use_playlist (const std::string &);

	void allocate_temporary_buffers ();

	int use_pending_capture_data (XMLNode& node);

	void get_input_sources ();
	void check_record_status (nframes_t transport_frame, nframes_t nframes, bool can_record);
	void set_align_style_from_io();
	void setup_destructive_playlist ();
	void use_destructive_playlist ();

	void engage_record_enable ();
	void disengage_record_enable ();

	// Working buffers for do_refill (butler thread)
	static void allocate_working_buffers();
	static void free_working_buffers();

	static size_t  _working_buffers_size;
	static Sample* _mixdown_buffer;
	static gain_t* _gain_buffer;

	std::vector<boost::shared_ptr<AudioFileSource> > capturing_sources;
	
	SerializedRCUManager<ChannelList> channels;
	
 /* really */
  private:
	int _do_refill (Sample *mixdown_buffer, float *gain_buffer);

	int add_channel_to (boost::shared_ptr<ChannelList>, uint32_t how_many);
	int remove_channel_from (boost::shared_ptr<ChannelList>, uint32_t how_many);

};

} // namespace ARDOUR

#endif /* __ardour_audio_diskstream_h__ */
