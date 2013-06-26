/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __ardour_public_diskstream_h__
#define __ardour_public_diskstream_h__

namespace ARDOUR {

class Playlist;
class Source;
class Location;

/** Public interface to a Diskstream */
class PublicDiskstream
{
public:
	virtual ~PublicDiskstream() {}

	virtual boost::shared_ptr<Playlist> playlist () = 0;
	virtual void request_jack_monitors_input (bool) = 0;
	virtual void ensure_jack_monitors_input (bool) = 0;
	virtual bool destructive () const = 0;
	virtual std::list<boost::shared_ptr<Source> > & last_capture_sources () = 0;
	virtual void set_capture_offset () = 0;
	virtual std::list<boost::shared_ptr<Source> > steal_write_sources () = 0;
	virtual void reset_write_sources (bool, bool force = false) = 0;
	virtual float playback_buffer_load () const = 0;
	virtual float capture_buffer_load () const = 0;
	virtual int do_refill () = 0;
	virtual int do_flush (RunContext, bool force = false) = 0;
	virtual void set_pending_overwrite (bool) = 0;
	virtual int seek (framepos_t, bool complete_refill = false) = 0;
	virtual bool hidden () const = 0;
	virtual int can_internal_playback_seek (framecnt_t) = 0;
	virtual int internal_playback_seek (framecnt_t) = 0;
	virtual void non_realtime_input_change () = 0;
	virtual void non_realtime_locate (framepos_t) = 0;
	virtual void non_realtime_set_speed () = 0;
	virtual int overwrite_existing_buffers () = 0;
	virtual framecnt_t get_captured_frames (uint32_t n = 0) const = 0;
	virtual int set_loop (Location *) = 0;
	virtual void transport_looped (framepos_t) = 0;
	virtual bool realtime_set_speed (double, bool) = 0;
	virtual void transport_stopped_wallclock (struct tm &, time_t, bool) = 0;
	virtual bool pending_overwrite () const = 0;
	virtual double speed () const = 0;
	virtual void prepare_to_stop (framepos_t) = 0;
	virtual void set_slaved (bool) = 0;
	virtual ChanCount n_channels () = 0;
	virtual framepos_t get_capture_start_frame (uint32_t n = 0) const = 0;
	virtual AlignStyle alignment_style () const = 0;
	virtual framepos_t current_capture_start () const = 0;
	virtual framepos_t current_capture_end () const = 0;
	virtual void playlist_modified () = 0;
	virtual int use_playlist (boost::shared_ptr<Playlist>) = 0;
	virtual void set_align_style (AlignStyle, bool force=false) = 0;
	virtual void set_align_choice (AlignChoice, bool force=false) = 0;
	virtual int use_copy_playlist () = 0;
	virtual int use_new_playlist () = 0;
	virtual void adjust_playback_buffering () = 0;
	virtual void adjust_capture_buffering () = 0;
};

}

#endif
