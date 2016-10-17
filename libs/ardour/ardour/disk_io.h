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

#include "ardour/processor.h"

namespace ARDOUR {

class Session;
class Route;

class LIBARDOUR_API DiskIOProcessor : public Processor
{
  public:
	static const std::string state_node_name;

	DiskIOProcessor(Session&, const std::string& name);

	void run (BufferSet& /*bufs*/, framepos_t /*start_frame*/, framepos_t /*end_frame*/, double speed, pframes_t /*nframes*/, bool /*result_required*/) {}
	void silence (framecnt_t /*nframes*/, framepos_t /*start_frame*/) {}

	bool configure_io (ChanCount in, ChanCount out);
	bool can_support_io_configuration (const ChanCount& in, ChanCount& out) = 0;
	ChanCount input_streams () const { return _configured_input; }
	ChanCount output_streams() const { return _configured_output; }

	virtual void realtime_handle_transport_stopped () {}
	virtual void realtime_locate () {}

	/* note: derived classes should implement state(), NOT get_state(), to allow
	   us to merge C++ inheritance and XML lack-of-inheritance reasonably
	   smoothly.
	 */

	virtual XMLNode& state (bool full);
	XMLNode& get_state (void);
	int set_state (const XMLNode&, int version);

	static framecnt_t disk_read_frames() { return disk_read_chunk_frames; }
	static framecnt_t disk_write_frames() { return disk_write_chunk_frames; }
	static void set_disk_read_chunk_frames (framecnt_t n) { disk_read_chunk_frames = n; }
	static void set_disk_write_chunk_frames (framecnt_t n) { disk_write_chunk_frames = n; }
	static framecnt_t default_disk_read_chunk_frames ();
	static framecnt_t default_disk_write_chunk_frames ();

	static void set_buffering_parameters (BufferingPreset bp);

protected:
	static framecnt_t disk_read_chunk_frames;
	static framecnt_t disk_write_chunk_frames;

	uint32_t i_am_the_modifier;

	Track*       _track;
	ChanCount    _n_channels;

	double       _visible_speed;
	double       _actual_speed;
	/* items needed for speed change logic */
	bool         _buffer_reallocation_required;
	bool         _seek_required;
	bool         _slaved;
	Location*     loop_location;
	double        _speed;
	double        _target_speed;
	bool          in_set_state;

	Glib::Threads::Mutex state_lock;
	Flag _flags;
};

class LIBARDOUR_API DiskReader : public DiskIOProcessor
{
  public:
	DiskReader (Session&, std::string const & name);
	~DiskReader ();

  private:
	boost::shared_ptr<Playlist> _playlist;

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

	PBD::ScopedConnectionList playlist_connections;
	PBD::ScopedConnection ic_connection;
};

class LIBARDOUR_API DiskWriter : public DiskIOProcessor
{
  public:
	DiskWriter (Session&, std::string const & name);
	~DiskWriter ();

  private:
	std::vector<CaptureInfo*> capture_info;
	mutable Glib::Threads::Mutex capture_info_lock;

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
	framepos_t    first_recordable_frame;
	framepos_t    last_recordable_frame;
	int           last_possibly_recording;
	AlignStyle   _alignment_style;
	AlignChoice  _alignment_choice;
	framecnt_t    wrap_buffer_size;
	framecnt_t    speed_buffer_size;

	std::string   _write_source_name;

	PBD::ScopedConnection ic_connection;
};


} // namespace ARDOUR

#endif /* __ardour_processor_h__ */
