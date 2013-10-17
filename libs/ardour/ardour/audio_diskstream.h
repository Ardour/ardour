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


#include <cmath>
#include <string>
#include <queue>
#include <map>
#include <vector>

#include <time.h>

#include <boost/utility.hpp>

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

struct LIBARDOUR_API tm;

namespace ARDOUR {

class AudioEngine;
class Send;
class Session;
class AudioPlaylist;
class AudioFileSource;
class IO;

class LIBARDOUR_API AudioDiskstream : public Diskstream
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
			return (*c)[n]->source.name;
		} else {
			return "";
		}
	}

	void set_record_enabled (bool yn);
	int set_destructive (bool yn);
	int set_non_layered (bool yn);
	bool can_become_destructive (bool& requires_bounce) const;

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

	bool set_name (std::string const &);

	/* stateful */

	XMLNode& get_state(void);
	int      set_state(const XMLNode& node, int version);

	void request_input_monitoring (bool);

	static void swap_by_ptr (Sample *first, Sample *last) {
		while (first < last) {
			Sample tmp = *first;
			*first++ = *last;
			*last-- = tmp;
		}
	}

	CubicInterpolation interpolation;

  protected:
	friend class Session;

	/* the Session is the only point of access for these
	   because they require that the Session is "inactive"
	   while they are called.
	*/

	void set_pending_overwrite(bool);
	int  overwrite_existing_buffers ();
	void set_block_size (pframes_t);
	int  internal_playback_seek (framecnt_t distance);
	int  can_internal_playback_seek (framecnt_t distance);
	std::list<boost::shared_ptr<Source> > steal_write_sources();
	void reset_write_sources (bool, bool force = false);
	void non_realtime_input_change ();
	void non_realtime_locate (framepos_t location);

  protected:
	friend class Auditioner;
	int  seek (framepos_t which_sample, bool complete_refill = false);

  protected:
	friend class AudioTrack;

        int  process (BufferSet&, framepos_t transport_frame, pframes_t nframes, framecnt_t &, bool need_disk_signal);
        frameoffset_t calculate_playback_distance (pframes_t nframes);
	bool commit  (framecnt_t);

  private:
	struct ChannelSource {
		std::string name;

		bool is_physical () const;
		void request_input_monitoring (bool) const;
	};

	/** Information about one of our channels */
	struct ChannelInfo : public boost::noncopyable {

		ChannelInfo (framecnt_t playback_buffer_size,
		             framecnt_t capture_buffer_size,
		             framecnt_t speed_buffer_size,
		             framecnt_t wrap_buffer_size);
		~ChannelInfo ();

		Sample     *playback_wrap_buffer;
		Sample     *capture_wrap_buffer;
		Sample     *speed_buffer;

		boost::shared_ptr<AudioFileSource> write_source;

		/** Information about the Port that our audio data comes from */
		ChannelSource source;

		Sample       *current_capture_buffer;
		Sample       *current_playback_buffer;

		/** A ringbuffer for data to be played back, written to in the
		    butler thread, read from in the process thread.
		*/
		PBD::RingBufferNPT<Sample> *playback_buf;
		PBD::RingBufferNPT<Sample> *capture_buf;

		Sample* scrub_buffer;
		Sample* scrub_forward_buffer;
		Sample* scrub_reverse_buffer;

		PBD::RingBufferNPT<Sample>::rw_vector playback_vector;
		PBD::RingBufferNPT<Sample>::rw_vector capture_vector;

		PBD::RingBufferNPT<CaptureTransition> * capture_transition_buf;
		// the following are used in the butler thread only
		framecnt_t                     curr_capture_cnt;

		void resize_playback (framecnt_t);
		void resize_capture (framecnt_t);
	};

	typedef std::vector<ChannelInfo*> ChannelList;

	/* The two central butler operations */
	int do_flush (RunContext context, bool force = false);
	int do_refill () { return _do_refill(_mixdown_buffer, _gain_buffer); }

	int do_refill_with_alloc ();

	int read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer,
	          framepos_t& start, framecnt_t cnt,
	          int channel, bool reversed);

	void finish_capture (boost::shared_ptr<ChannelList>);
	void transport_stopped_wallclock (struct tm&, time_t, bool abort);
	void transport_looped (framepos_t transport_frame);

	void init ();

	void init_channel (ChannelInfo &chan);
	void destroy_channel (ChannelInfo &chan);

	int use_new_write_source (uint32_t n=0);

	int find_and_use_playlist (const std::string &);

	void allocate_temporary_buffers ();

	int use_pending_capture_data (XMLNode& node);

	void get_input_sources ();
	void prepare_record_status(framepos_t capture_start_frame);
	void set_align_style_from_io();
	void setup_destructive_playlist ();
	void use_destructive_playlist ();

	void adjust_playback_buffering ();
	void adjust_capture_buffering ();

        bool prep_record_enable ();
	bool prep_record_disable ();
    
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
