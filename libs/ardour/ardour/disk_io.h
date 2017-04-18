/*
    Copyright (C) 2009-2016 Paul Davis

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

#ifndef __ardour_disk_io_h__
#define __ardour_disk_io_h__

#include <vector>
#include <string>
#include <exception>

#include "pbd/ringbufferNPT.h"
#include "pbd/rcu.h"

#include "ardour/interpolation.h"
#include "ardour/processor.h"

namespace ARDOUR {

class AudioFileSource;
class AudioPlaylist;
class Location;
class MidiPlaylist;
class Playlist;
class Route;
class Route;
class Session;

template<typename T> class MidiRingBuffer;

class LIBARDOUR_API DiskIOProcessor : public Processor
{
  public:
	enum Flag {
		Recordable  = 0x1,
		Hidden      = 0x2,
		Destructive = 0x4,
		NonLayered   = 0x8
	};

	static const std::string state_node_name;

	DiskIOProcessor (Session&, const std::string& name, Flag f);

	void set_route (boost::shared_ptr<Route>);

	static void set_buffering_parameters (BufferingPreset bp);

	int set_block_size (pframes_t);
	bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out);

	/** @return A number between 0 and 1, where 0 indicates that the playback/capture buffer
	 *  is dry (ie the disk subsystem could not keep up) and 1 indicates that the
	 *  buffer is full.
	 */
	virtual float buffer_load() const = 0;

	void set_flag (Flag f)   { _flags = Flag (_flags | f); }
	void unset_flag (Flag f) { _flags = Flag (_flags & ~f); }

	bool           hidden()      const { return _flags & Hidden; }
	bool           recordable()  const { return _flags & Recordable; }
	bool           non_layered()  const { return _flags & NonLayered; }

	virtual void non_realtime_locate (framepos_t);

	void non_realtime_speed_change ();
	bool realtime_speed_change ();

	virtual void punch_in()  {}
	virtual void punch_out() {}

	bool slaved() const      { return _slaved; }
	void set_slaved(bool yn) { _slaved = yn; }

	int set_loop (Location *loc);

	PBD::Signal1<void,Location *> LoopSet;
	PBD::Signal0<void>            SpeedChanged;
	PBD::Signal0<void>            ReverseChanged;

	int set_state (const XMLNode&, int version);

	int add_channel (uint32_t how_many);
	int remove_channel (uint32_t how_many);

	bool need_butler() const { return _need_butler; }

	boost::shared_ptr<Playlist>      get_playlist (DataType dt) const { return _playlists[dt]; }
	boost::shared_ptr<MidiPlaylist>  midi_playlist() const;
	boost::shared_ptr<AudioPlaylist> audio_playlist() const;

	virtual void playlist_modified () {}
	virtual int use_playlist (DataType, boost::shared_ptr<Playlist>);

	PBD::Signal1<void,DataType>   PlaylistChanged;

	virtual void adjust_buffering() = 0;

  protected:
	friend class Auditioner;
	virtual int  seek (framepos_t which_sample, bool complete_refill = false) = 0;

  protected:
	Flag         _flags;
	uint32_t      i_am_the_modifier;
	double       _actual_speed;
	double       _target_speed;
	/* items needed for speed change logic */
	bool         _buffer_reallocation_required;
	bool         _seek_required;
	bool         _slaved;
	Location*     loop_location;
	bool          in_set_state;
	framepos_t     file_frame;
	framepos_t     playback_sample;
	framecnt_t    wrap_buffer_size;
	framecnt_t    speed_buffer_size;
	bool         _need_butler;
	boost::shared_ptr<Route> _route;

	void init ();

	Glib::Threads::Mutex state_lock;

	static bool get_buffering_presets (BufferingPreset bp,
	                                   framecnt_t& read_chunk_size,
	                                   framecnt_t& read_buffer_size,
	                                   framecnt_t& write_chunk_size,
	                                   framecnt_t& write_buffer_size);

	enum TransitionType {
		CaptureStart = 0,
		CaptureEnd
	};

	struct CaptureTransition {
		TransitionType   type;
		framepos_t       capture_val; ///< The start or end file frame position
	};

	/** Information about one audio channel, playback or capture
	 * (depending on the derived class)
	 */
	struct ChannelInfo : public boost::noncopyable {

		ChannelInfo (framecnt_t buffer_size);
		~ChannelInfo ();

		/** A ringbuffer for data to be played back, written to in the
		    butler thread, read from in the process thread.
		*/
		PBD::RingBufferNPT<Sample>* buf;

		Sample* scrub_buffer;
		Sample* scrub_forward_buffer;
		Sample* scrub_reverse_buffer;

		PBD::RingBufferNPT<Sample>::rw_vector rw_vector;

		/* used only by capture */
		boost::shared_ptr<AudioFileSource> write_source;
		PBD::RingBufferNPT<CaptureTransition> * capture_transition_buf;
		// the following are used in the butler thread only
		framecnt_t                     curr_capture_cnt;

		void resize (framecnt_t);
	};

	typedef std::vector<ChannelInfo*> ChannelList;
	SerializedRCUManager<ChannelList> channels;

	int add_channel_to (boost::shared_ptr<ChannelList>, uint32_t how_many);
	int remove_channel_from (boost::shared_ptr<ChannelList>, uint32_t how_many);

	CubicInterpolation interpolation;

	boost::shared_ptr<Playlist> _playlists[DataType::num_types];
	PBD::ScopedConnectionList playlist_connections;

	virtual void playlist_changed (const PBD::PropertyChange&) {}
	virtual void playlist_deleted (boost::weak_ptr<Playlist>);
	virtual void playlist_ranges_moved (std::list< Evoral::RangeMove<framepos_t> > const &, bool) {}

	/* The MIDI stuff */

	MidiRingBuffer<framepos_t>*  _midi_buf;
	gint                         _frames_written_to_ringbuffer;
	gint                         _frames_read_from_ringbuffer;
	CubicMidiInterpolation        midi_interpolation;

	static void get_location_times (const Location* location, framepos_t* start, framepos_t* end, framepos_t* length);
};

} // namespace ARDOUR

#endif /* __ardour_disk_io_h__ */
