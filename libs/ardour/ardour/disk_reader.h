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

#ifndef __ardour_disk_reader_h__
#define __ardour_disk_reader_h__

#include "ardour/disk_io.h"

namespace ARDOUR
{

class Playlist;
class AudioPlaylist;
class MidiPlaylist;

class LIBARDOUR_API DiskReader : public DiskIOProcessor
{
  public:
	DiskReader (Session&, std::string const & name, DiskIOProcessor::Flag f = DiskIOProcessor::Flag (0));
	~DiskReader ();

	bool set_name (std::string const & str);

	static framecnt_t chunk_frames() { return _chunk_frames; }
	static framecnt_t default_chunk_frames ();
	static void set_chunk_frames (framecnt_t n) { _chunk_frames = n; }

	void run (BufferSet& /*bufs*/, framepos_t /*start_frame*/, framepos_t /*end_frame*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);
	void silence (framecnt_t /*nframes*/, framepos_t /*start_frame*/);
	bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) = 0;
	ChanCount input_streams () const;
	ChanCount output_streams() const;
	void realtime_handle_transport_stopped ();
	void realtime_locate ();

	framecnt_t roll_delay() const { return _roll_delay; }
	void set_roll_delay (framecnt_t);

	virtual XMLNode& state (bool full);
	int set_state (const XMLNode&, int version);

	boost::shared_ptr<Playlist>      playlist();
	boost::shared_ptr<Playlist>      get_playlist (DataType);
	boost::shared_ptr<MidiPlaylist>  midi_playlist();
	boost::shared_ptr<AudioPlaylist> audio_playlist();

	virtual void playlist_modified ();
	virtual int use_playlist (boost::shared_ptr<Playlist>);
	virtual int use_new_playlist () = 0;
	virtual int use_copy_playlist () = 0;

	PBD::Signal0<void>            PlaylistChanged;
	PBD::Signal0<void>            AlignmentStyleChanged;

	float buffer_load() const;

	void move_processor_automation (boost::weak_ptr<Processor>, std::list<Evoral::RangeMove<framepos_t> > const &);

	/** For non-butler contexts (allocates temporary working buffers)
	 *
	 * This accessible method has a default argument; derived classes
	 * must inherit the virtual method that we call which does NOT
	 * have a default argument, to avoid complications with inheritance
	 */
	int do_refill_with_alloc(bool partial_fill = true) {
		return _do_refill_with_alloc (partial_fill);
	}

	bool pending_overwrite () const { return _pending_overwrite; }

	virtual int find_and_use_playlist (std::string const &) = 0;

  protected:
	virtual int do_refill () = 0;

	boost::shared_ptr<Playlist> _playlist;

	virtual void playlist_changed (const PBD::PropertyChange&);
	virtual void playlist_deleted (boost::weak_ptr<Playlist>);
	virtual void playlist_ranges_moved (std::list< Evoral::RangeMove<framepos_t> > const &, bool);

  private:
	typedef std::map<DataType,boost::shared_ptr<Playlist> > Playlists;

	/** The number of frames by which this diskstream's output should be delayed
	    with respect to the transport frame.  This is used for latency compensation.
	*/
	framecnt_t   _roll_delay;
	Playlists     _playlists;
	framepos_t    overwrite_frame;
	off_t         overwrite_offset;
	bool          _pending_overwrite;
	bool          overwrite_queued;
	IOChange      input_change_pending;
	framecnt_t    wrap_buffer_size;
	framecnt_t    speed_buffer_size;
	framepos_t     file_frame;
	framepos_t     playback_sample;

	PBD::ScopedConnectionList playlist_connections;

	virtual int _do_refill_with_alloc (bool partial_fill);

	static framecnt_t _chunk_frames;
};

} // namespace

#endif /* __ardour_disk_reader_h__ */
