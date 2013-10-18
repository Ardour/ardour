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

#ifndef __ardour_diskstream_h__
#define __ardour_diskstream_h__

#include <string>
#include <queue>
#include <map>
#include <vector>
#include <cmath>
#include <time.h>

#include <boost/utility.hpp>

#include "evoral/types.hpp"

#include "ardour/ardour.h"
#include "ardour/chan_count.h"
#include "ardour/session_object.h"
#include "ardour/libardour_visibility.h"
#include "ardour/types.h"
#include "ardour/utils.h"
#include "ardour/public_diskstream.h"

struct tm;

namespace ARDOUR {

class IO;
class Playlist;
class Processor;
class Source;
class Session;
class Track;
class Location;
class BufferSet;

/** Parent class for classes which can stream data to and from disk.
 *  These are used by Tracks to get playback and put recorded data.
 */
class LIBARDOUR_API Diskstream : public SessionObject, public PublicDiskstream
{
  public:
	enum Flag {
		Recordable  = 0x1,
		Hidden      = 0x2,
		Destructive = 0x4,
		NonLayered   = 0x8
	};

	Diskstream (Session &, const std::string& name, Flag f = Recordable);
	Diskstream (Session &, const XMLNode&);
	virtual ~Diskstream();

	virtual bool set_name (const std::string& str);

	boost::shared_ptr<ARDOUR::IO> io() const { return _io; }
	void set_track (ARDOUR::Track *);

	/** @return A number between 0 and 1, where 0 indicates that the playback buffer
	 *  is dry (ie the disk subsystem could not keep up) and 1 indicates that the
	 *  buffer is full.
	 */
	virtual float playback_buffer_load() const = 0;
	virtual float capture_buffer_load() const = 0;

	void set_flag (Flag f)   { _flags = Flag (_flags | f); }
	void unset_flag (Flag f) { _flags = Flag (_flags & ~f); }

	AlignStyle  alignment_style() const { return _alignment_style; }
	AlignChoice alignment_choice() const { return _alignment_choice; }
	void       set_align_style (AlignStyle, bool force=false);
	void       set_align_choice (AlignChoice a, bool force=false);

	framecnt_t roll_delay() const { return _roll_delay; }
	void       set_roll_delay (framecnt_t);

	bool         record_enabled() const { return g_atomic_int_get (&_record_enabled); }
	virtual void set_record_enabled (bool yn) = 0;

	bool destructive() const { return _flags & Destructive; }
	virtual int set_destructive (bool /*yn*/) { return -1; }
	virtual int set_non_layered (bool /*yn*/) { return -1; }
	virtual	bool can_become_destructive (bool& /*requires_bounce*/) const { return false; }

	bool           hidden()      const { return _flags & Hidden; }
	bool           recordable()  const { return _flags & Recordable; }
	bool           non_layered()  const { return _flags & NonLayered; }
	bool           reversed()    const { return _actual_speed < 0.0f; }
	double         speed()       const { return _visible_speed; }

	virtual void punch_in()  {}
	virtual void punch_out() {}

	void non_realtime_set_speed ();
	virtual void non_realtime_locate (framepos_t /*location*/) {};
	virtual void playlist_modified ();

	boost::shared_ptr<Playlist> playlist () { return _playlist; }

	virtual int use_playlist (boost::shared_ptr<Playlist>);
	virtual int use_new_playlist () = 0;
	virtual int use_copy_playlist () = 0;

	/** @return Start position of currently-running capture (in session frames) */
	framepos_t current_capture_start() const { return capture_start_frame; }
	framepos_t current_capture_end()   const { return capture_start_frame + capture_captured; }
	framepos_t get_capture_start_frame (uint32_t n = 0) const;
	framecnt_t get_captured_frames (uint32_t n = 0) const;

	ChanCount n_channels() { return _n_channels; }

	static framecnt_t disk_io_frames() { return disk_io_chunk_frames; }
	static void set_disk_io_chunk_frames (framecnt_t n) { disk_io_chunk_frames = n; }

	/* Stateful */
	virtual XMLNode& get_state(void);
	virtual int      set_state(const XMLNode&, int version);

	virtual void request_input_monitoring (bool) {}
	virtual void ensure_input_monitoring (bool) {}

	framecnt_t   capture_offset() const { return _capture_offset; }
	virtual void set_capture_offset ();

	bool slaved() const      { return _slaved; }
	void set_slaved(bool yn) { _slaved = yn; }

	int set_loop (Location *loc);

	std::list<boost::shared_ptr<Source> >& last_capture_sources () { return _last_capture_sources; }

	void handle_input_change (IOChange, void *src);

	void move_processor_automation (boost::weak_ptr<Processor>,
			std::list<Evoral::RangeMove<framepos_t> > const &);

	/** For non-butler contexts (allocates temporary working buffers) */
	virtual int do_refill_with_alloc() = 0;
	virtual void set_block_size (pframes_t) = 0;

	bool pending_overwrite () const {
		return _pending_overwrite;
	}

	PBD::Signal0<void>            RecordEnableChanged;
	PBD::Signal0<void>            SpeedChanged;
	PBD::Signal0<void>            ReverseChanged;
	/* Emitted when this diskstream is set to use a different playlist */
	PBD::Signal0<void>            PlaylistChanged;
	PBD::Signal0<void>            AlignmentStyleChanged;
	PBD::Signal1<void,Location *> LoopSet;

	static PBD::Signal0<void>     DiskOverrun;
	static PBD::Signal0<void>     DiskUnderrun;

  protected:
	friend class Session;
	friend class Butler;

	/* the Session is the only point of access for these because they require
	 * that the Session is "inactive" while they are called.
	 */

	virtual void set_pending_overwrite (bool) = 0;
	virtual int  overwrite_existing_buffers () = 0;
	virtual int  internal_playback_seek (framecnt_t distance) = 0;
	virtual int  can_internal_playback_seek (framecnt_t distance) = 0;
	virtual void reset_write_sources (bool, bool force = false) = 0;
	virtual void non_realtime_input_change () = 0;

  protected:
	friend class Auditioner;
	virtual int  seek (framepos_t which_sample, bool complete_refill = false) = 0;

  protected:
	friend class Track;

    virtual int  process (BufferSet&, framepos_t transport_frame, pframes_t nframes, framecnt_t &, bool need_disk_signal) = 0;
    virtual frameoffset_t calculate_playback_distance (pframes_t nframes) = 0;
	virtual bool commit  (framecnt_t) = 0;

	//private:

	enum TransitionType {
		CaptureStart = 0,
		CaptureEnd
	};

	struct CaptureTransition {
		TransitionType   type;
		framepos_t       capture_val; ///< The start or end file frame position
	};

	/* The two central butler operations */
	virtual int do_flush (RunContext context, bool force = false) = 0;
	virtual int do_refill () = 0;

	/* XXX fix this redundancy ... */

	virtual void playlist_changed (const PBD::PropertyChange&);
	virtual void playlist_deleted (boost::weak_ptr<Playlist>);
	virtual void playlist_ranges_moved (std::list< Evoral::RangeMove<framepos_t> > const &, bool);

	virtual void transport_stopped_wallclock (struct tm&, time_t, bool abort) = 0;
	virtual void transport_looped (framepos_t transport_frame) = 0;

	struct CaptureInfo {
		framepos_t start;
		framecnt_t frames;
	};

	virtual int use_new_write_source (uint32_t n=0) = 0;

	virtual int find_and_use_playlist (const std::string&) = 0;

	virtual void allocate_temporary_buffers () = 0;

	virtual bool realtime_set_speed (double, bool global_change);

	std::list<boost::shared_ptr<Source> > _last_capture_sources;

	virtual int use_pending_capture_data (XMLNode& node) = 0;

	virtual void check_record_status (framepos_t transport_frame, bool can_record);
	virtual void prepare_record_status (framepos_t /*capture_start_frame*/) {}
	virtual void set_align_style_from_io() {}
	virtual void setup_destructive_playlist () {}
	virtual void use_destructive_playlist () {}
	virtual void prepare_to_stop (framepos_t pos);

	void engage_record_enable ();
	void disengage_record_enable ();

        virtual bool prep_record_enable () = 0;
        virtual bool prep_record_disable () = 0;

	void calculate_record_range (
		Evoral::OverlapType ot, framepos_t transport_frame, framecnt_t nframes,
		framecnt_t& rec_nframes, framecnt_t& rec_offset
		);

	static framecnt_t disk_io_chunk_frames;
	std::vector<CaptureInfo*> capture_info;
	mutable Glib::Threads::Mutex capture_info_lock;

	uint32_t i_am_the_modifier;

	boost::shared_ptr<ARDOUR::IO>  _io;
	Track*       _track;
	ChanCount    _n_channels;

	boost::shared_ptr<Playlist> _playlist;

	mutable gint _record_enabled;
	double       _visible_speed;
	double       _actual_speed;
	/* items needed for speed change logic */
	bool         _buffer_reallocation_required;
	bool         _seek_required;

	/** Start of currently running capture in session frames */
	framepos_t    capture_start_frame;
	framecnt_t    capture_captured;
	bool          was_recording;
	framecnt_t    adjust_capture_position;
	framecnt_t   _capture_offset;
	/** The number of frames by which this diskstream's output should be delayed
	    with respect to the transport frame.  This is used for latency compensation.
	*/
	framecnt_t   _roll_delay;
	framepos_t    first_recordable_frame;
	framepos_t    last_recordable_frame;
	int           last_possibly_recording;
	AlignStyle   _alignment_style;
	AlignChoice  _alignment_choice;
	bool         _slaved;
	Location*     loop_location;
	framepos_t    overwrite_frame;
	off_t         overwrite_offset;
	bool          _pending_overwrite;
	bool          overwrite_queued;
	IOChange      input_change_pending;
	framecnt_t    wrap_buffer_size;
	framecnt_t    speed_buffer_size;

	double        _speed;
	double        _target_speed;

	/** The next frame position that we should be reading from in our playlist */
	framepos_t     file_frame;
	framepos_t     playback_sample;

	bool          in_set_state;

	Glib::Threads::Mutex state_lock;

	PBD::ScopedConnectionList playlist_connections;

	PBD::ScopedConnection ic_connection;

	Flag _flags;
	XMLNode* deprecated_io_node;

	void route_going_away ();
};

}; /* namespace ARDOUR */

#endif /* __ardour_diskstream_h__ */
