/*
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_disk_io_h__
#define __ardour_disk_io_h__

#include <vector>
#include <string>
#include <exception>

#include "pbd/ringbufferNPT.h"
#include "pbd/rcu.h"

#include "ardour/interpolation.h"
#include "ardour/midi_buffer.h"
#include "ardour/processor.h"
#include "ardour/rt_midibuffer.h"

namespace PBD {
	template<class T> class PlaybackBuffer;
}

namespace ARDOUR {

class AudioFileSource;
class AudioPlaylist;
class Location;
class MidiPlaylist;
class Playlist;
class Track;
class Session;

template<typename T> class MidiRingBuffer;

class LIBARDOUR_API DiskIOProcessor : public Processor
{
public:
	enum Flag {
		Recordable  = 0x1,
		Hidden      = 0x2,
		NonLayered  = 0x8 // deprecated (kept only for enum compat)
	};

	static const std::string state_node_name;

	DiskIOProcessor (Session&, Track&, const std::string& name, Flag f, Temporal::TimeDomain td);

	virtual ~DiskIOProcessor ();

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

	virtual void non_realtime_locate (samplepos_t);

	virtual void punch_in()  {}
	virtual void punch_out() {}

	bool slaved() const      { return _slaved; }
	void set_slaved(bool yn) { _slaved = yn; }

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

	virtual void adjust_buffering() = 0;

protected:
	friend class Auditioner;
	virtual int  seek (samplepos_t which_sample, bool complete_refill = false) = 0;
	virtual void configuration_changed () = 0;

protected:
	Flag         _flags;
	bool         _slaved;
	bool          in_set_state;
	samplepos_t   playback_sample;
	bool         _need_butler;
	Track&       _track;

	void init ();

	Glib::Threads::Mutex state_lock;

	static bool get_buffering_presets (BufferingPreset bp,
	                                   samplecnt_t& read_chunk_size,
	                                   samplecnt_t& read_buffer_size,
	                                   samplecnt_t& write_chunk_size,
	                                   samplecnt_t& write_buffer_size);

	enum TransitionType {
		CaptureStart = 0,
		CaptureEnd
	};

	struct CaptureTransition {
		TransitionType   type;
		samplepos_t       capture_val; ///< The start or end file sample position
	};

	/** Information about one audio channel, playback or capture
	 * (depending on the derived class)
	 */
	struct ChannelInfo : public boost::noncopyable {

		ChannelInfo (samplecnt_t buffer_size);
		virtual ~ChannelInfo ();

		/** A semi-random-access ringbuffers for data to be played back.
		 * written to in the butler thread, read from in the process
		 * thread.
		 */
		PBD::PlaybackBuffer<Sample>* rbuf;

		/** A ringbuffer for data to be recorded back, written to in the
		 * process thread, read from in the butler thread.
		 */
		PBD::RingBufferNPT<Sample>* wbuf;
		PBD::RingBufferNPT<Sample>::rw_vector rw_vector;

		/* used only by capture */
		boost::shared_ptr<AudioFileSource> write_source;
		PBD::RingBufferNPT<CaptureTransition>* capture_transition_buf;

		/* used in the butler thread only */
		samplecnt_t curr_capture_cnt;

		virtual void resize (samplecnt_t) = 0;
	};

	typedef std::vector<ChannelInfo*> ChannelList;
	SerializedRCUManager<ChannelList> channels;

	virtual int add_channel_to (boost::shared_ptr<ChannelList>, uint32_t how_many) = 0;
	int remove_channel_from (boost::shared_ptr<ChannelList>, uint32_t how_many);

	boost::shared_ptr<Playlist> _playlists[DataType::num_types];
	PBD::ScopedConnectionList playlist_connections;

	virtual void playlist_changed (const PBD::PropertyChange&) {}
	virtual void playlist_deleted (boost::weak_ptr<Playlist>);
	virtual void playlist_ranges_moved (std::list<Temporal::RangeMove> const &, bool) {}

	/* The MIDI stuff */

	MidiRingBuffer<samplepos_t>*  _midi_buf;

	static void get_location_times (const Location* location, timepos_t* start, timepos_t* end, timecnt_t* length);
};

} // namespace ARDOUR

#endif /* __ardour_disk_io_h__ */
