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

#ifndef __ardour_disk_writer_h__
#define __ardour_disk_writer_h__

#include <list>
#include <vector>

#include "ardour/disk_io.h"

namespace ARDOUR
{

class AudioFileSource;
class SMFSource;

class LIBARDOUR_API DiskWriter : public DiskIOProcessor
{
  public:
	DiskWriter (Session&, std::string const & name, DiskIOProcessor::Flag f = DiskIOProcessor::Flag (0));

	virtual bool set_write_source_name (const std::string& str);

	bool           recordable()  const { return _flags & Recordable; }

	static framecnt_t chunk_frames() { return _chunk_frames; }
	static framecnt_t default_chunk_frames ();
	static void set_chunk_frames (framecnt_t n) { _chunk_frames = n; }

	void run (BufferSet& /*bufs*/, framepos_t /*start_frame*/, framepos_t /*end_frame*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);
	void non_realtime_locate (framepos_t);

	virtual XMLNode& state (bool full);
	int set_state (const XMLNode&, int version);

	virtual int use_new_write_source (uint32_t n=0) = 0;

	std::string write_source_name () const {
		if (_write_source_name.empty()) {
			return name();
		} else {
			return _write_source_name;
		}
	}

	boost::shared_ptr<AudioFileSource> audio_write_source (uint32_t n=0) {
		boost::shared_ptr<ChannelList> c = channels.reader();
		if (n < c->size()) {
			return (*c)[n]->write_source;
		}

		return boost::shared_ptr<AudioFileSource>();
	}

	boost::shared_ptr<SMFSource> midi_write_source () { return _midi_write_source; }

	virtual std::string steal_write_source_name () { return std::string(); }

	AlignStyle  alignment_style() const { return _alignment_style; }
	AlignChoice alignment_choice() const { return _alignment_choice; }
	void       set_align_style (AlignStyle, bool force=false);
	void       set_align_choice (AlignChoice a, bool force=false);

	PBD::Signal0<void> AlignmentStyleChanged;

	void set_input_latency (framecnt_t);
	framecnt_t input_latency () const { return _input_latency; }

	std::list<boost::shared_ptr<Source> >& last_capture_sources () { return _last_capture_sources; }

	bool         record_enabled() const { return g_atomic_int_get (const_cast<gint*>(&_record_enabled)); }
	bool         record_safe () const { return g_atomic_int_get (const_cast<gint*>(&_record_safe)); }
	virtual void set_record_enabled (bool yn) = 0;
	virtual void set_record_safe (bool yn) = 0;

	bool destructive() const { return _flags & Destructive; }
	int set_destructive (bool yn);
	int set_non_layered (bool yn);
	bool can_become_destructive (bool& requires_bounce) const;

	/** @return Start position of currently-running capture (in session frames) */
	framepos_t current_capture_start() const { return capture_start_frame; }
	framepos_t current_capture_end()   const { return capture_start_frame + capture_captured; }
	framepos_t get_capture_start_frame (uint32_t n = 0) const;
	framecnt_t get_captured_frames (uint32_t n = 0) const;

	float buffer_load() const;

	virtual void request_input_monitoring (bool) {}
	virtual void ensure_input_monitoring (bool) {}

	framecnt_t   capture_offset() const { return _capture_offset; }
	virtual void set_capture_offset ();

	static PBD::Signal0<void> Overrun;

	PBD::Signal0<void> RecordEnableChanged;
	PBD::Signal0<void> RecordSafeChanged;

  protected:
	virtual int do_flush (RunContext context, bool force = false) = 0;

	void get_input_sources ();
	void check_record_status (framepos_t transport_frame, bool can_record);
	void prepare_record_status (framepos_t /*capture_start_frame*/);
	void set_align_style_from_io();
	void setup_destructive_playlist ();
	void use_destructive_playlist ();
	void prepare_to_stop (framepos_t transport_pos, framepos_t audible_frame);

	void engage_record_enable ();
	void disengage_record_enable ();
	void engage_record_safe ();
	void disengage_record_safe ();

	bool prep_record_enable ();
	bool prep_record_disable ();

	void calculate_record_range (
		Evoral::OverlapType ot, framepos_t transport_frame, framecnt_t nframes,
		framecnt_t& rec_nframes, framecnt_t& rec_offset
		);

	static framecnt_t disk_read_chunk_frames;
	static framecnt_t disk_write_chunk_frames;

	struct CaptureInfo {
		framepos_t start;
		framecnt_t frames;
	};

	std::vector<CaptureInfo*> capture_info;
	mutable Glib::Threads::Mutex capture_info_lock;

  private:
	framecnt_t   _input_latency;
	gint         _record_enabled;
	gint         _record_safe;
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
	std::string   _write_source_name;
	boost::shared_ptr<SMFSource> _midi_write_source;

	std::list<boost::shared_ptr<Source> > _last_capture_sources;
	std::vector<boost::shared_ptr<AudioFileSource> > capturing_sources;
	
	static framecnt_t _chunk_frames;

	NoteMode                     _note_mode;
	volatile gint                _frames_pending_write;
	volatile gint                _num_captured_loops;
	framepos_t                   _accumulated_capture_offset;

	void finish_capture (boost::shared_ptr<ChannelList> c);
};

} // namespace

#endif /* __ardour_disk_writer_h__ */
