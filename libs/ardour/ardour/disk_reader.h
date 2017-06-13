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

#include "pbd/i18n.h"

#include "ardour/disk_io.h"
#include "ardour/midi_buffer.h"

namespace ARDOUR
{

class Playlist;
class AudioPlaylist;
class MidiPlaylist;
template<typename T> class MidiRingBuffer;

class LIBARDOUR_API DiskReader : public DiskIOProcessor
{
  public:
	DiskReader (Session&, std::string const & name, DiskIOProcessor::Flag f = DiskIOProcessor::Flag (0));
	~DiskReader ();

	bool set_name (std::string const & str);
	std::string display_name() const { return std::string (_("reader")); }

	static framecnt_t chunk_frames() { return _chunk_frames; }
	static framecnt_t default_chunk_frames ();
	static void set_chunk_frames (framecnt_t n) { _chunk_frames = n; }

	void run (BufferSet& /*bufs*/, framepos_t /*start_frame*/, framepos_t /*end_frame*/, double speed, pframes_t /*nframes*/, bool /*result_required*/);
	void realtime_handle_transport_stopped ();
	void realtime_locate ();
	int overwrite_existing_buffers ();
	void set_pending_overwrite (bool yn);

	framecnt_t roll_delay() const { return _roll_delay; }
	void set_roll_delay (framecnt_t);

	virtual XMLNode& state (bool full);
	int set_state (const XMLNode&, int version);

	PBD::Signal0<void>            AlignmentStyleChanged;

	float buffer_load() const;

	void move_processor_automation (boost::weak_ptr<Processor>, std::list<Evoral::RangeMove<framepos_t> > const &);

	/* called by the Butler in a non-realtime context */

	int do_refill () {
		return refill (_mixdown_buffer, _gain_buffer, 0);
	}

	/** For non-butler contexts (allocates temporary working buffers)
	 *
	 * This accessible method has a default argument; derived classes
	 * must inherit the virtual method that we call which does NOT
	 * have a default argument, to avoid complications with inheritance
	 */
	int do_refill_with_alloc (bool partial_fill = true) {
		return _do_refill_with_alloc (partial_fill);
	}

	bool pending_overwrite () const { return _pending_overwrite; }

	// Working buffers for do_refill (butler thread)
	static void allocate_working_buffers();
	static void free_working_buffers();

	void adjust_buffering ();

	int can_internal_playback_seek (framecnt_t distance);
	int internal_playback_seek (framecnt_t distance);
	int seek (framepos_t frame, bool complete_refill = false);

	static PBD::Signal0<void> Underrun;

	void playlist_modified ();
	void reset_tracker ();

	static void set_midi_readahead_frames (framecnt_t frames_ahead) { midi_readahead = frames_ahead; }

  protected:
	friend class Track;
	friend class MidiTrack;

	void resolve_tracker (Evoral::EventSink<framepos_t>& buffer, framepos_t time);
	boost::shared_ptr<MidiBuffer> get_gui_feed_buffer () const;

	void playlist_changed (const PBD::PropertyChange&);
	int use_playlist (DataType, boost::shared_ptr<Playlist>);
	void playlist_ranges_moved (std::list< Evoral::RangeMove<framepos_t> > const &, bool);

  private:
	/** The number of frames by which this diskstream's output should be delayed
	    with respect to the transport frame.  This is used for latency compensation.
	*/
	framecnt_t   _roll_delay;
	framepos_t    overwrite_frame;
	off_t         overwrite_offset;
	bool          _pending_overwrite;
	bool          overwrite_queued;
	IOChange      input_change_pending;
	framecnt_t    wrap_buffer_size;

	int _do_refill_with_alloc (bool partial_fill);

	static framecnt_t _chunk_frames;
	static framecnt_t midi_readahead;

	/* The MIDI stuff */

	/** A buffer that we use to put newly-arrived MIDI data in for
	    the GUI to read (so that it can update itself).
	*/
	MidiBuffer                   _gui_feed_buffer;
	mutable Glib::Threads::Mutex _gui_feed_buffer_mutex;

	int audio_read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer,
	                framepos_t& start, framecnt_t cnt,
	                int channel, bool reversed);
	int midi_read (framepos_t& start, framecnt_t cnt, bool reversed);

	static Sample* _mixdown_buffer;
	static gain_t* _gain_buffer;

	int refill (Sample* mixdown_buffer, float* gain_buffer, framecnt_t fill_level);
	int refill_audio (Sample *mixdown_buffer, float *gain_buffer, framecnt_t fill_level);
	int refill_midi ();

	frameoffset_t calculate_playback_distance (pframes_t);

	void get_midi_playback (MidiBuffer& dst, framecnt_t nframes, MonitorState, BufferSet&, double speed, framecnt_t distance);
};

} // namespace

#endif /* __ardour_disk_reader_h__ */
