/*
    Copyright (C) 2000 Paul Davis

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

    $Id: diskstream.h 579 2006-06-12 19:56:37Z essej $
*/

#ifndef __ardour_midi_diskstream_h__
#define __ardour_midi_diskstream_h__


#include <cmath>
#include <cassert>
#include <string>
#include <queue>
#include <map>
#include <vector>

#include <time.h>

#include "pbd/fastlog.h"
#include "pbd/ringbufferNPT.h"

#include "ardour/ardour.h"
#include "ardour/diskstream.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/utils.h"

struct tm;

namespace ARDOUR {

class IO;
class MidiEngine;
class MidiPort;
class MidiRingbuffer;
class SMFSource;
class Send;
class Session;

class MidiDiskstream : public Diskstream
{
  public:
	MidiDiskstream (Session &, const string& name, Diskstream::Flag f = Recordable);
	MidiDiskstream (Session &, const XMLNode&);
	~MidiDiskstream();

	float playback_buffer_load() const;
	float capture_buffer_load() const;

	void get_playback (MidiBuffer& dst, framecnt_t);
        void flush_playback (framepos_t, framepos_t);

	void set_record_enabled (bool yn);
	
	void reset_tracker ();

	boost::shared_ptr<MidiPlaylist> midi_playlist () { return boost::dynamic_pointer_cast<MidiPlaylist>(_playlist); }

	int use_playlist (boost::shared_ptr<Playlist>);
	int use_new_playlist ();
	int use_copy_playlist ();

	bool set_name (std::string const &);

	/* stateful */
	XMLNode& get_state(void);
	int set_state(const XMLNode&, int version);

	void ensure_jack_monitors_input (bool);

	boost::shared_ptr<SMFSource> write_source ()    { return _write_source; }

	int set_destructive (bool yn); // doom!

	void set_note_mode (NoteMode m);

	uint16_t get_channel_mask() {
		uint16_t playback_mask = _playback_buf->get_channel_mask();
#ifndef NDEBUG
		uint16_t capture_mask  = _capture_buf->get_channel_mask();
		assert(playback_mask == capture_mask);
#endif
		return playback_mask;
	}

	void set_channel_mode(ChannelMode mode, uint16_t mask) {
		_playback_buf->set_channel_mode(mode, mask);
		_capture_buf->set_channel_mode(mode, mask);
	}

	ChannelMode get_channel_mode() {
		ChannelMode playback_mode = _playback_buf->get_channel_mode();
#ifndef NDEBUG
		ChannelMode capture_mode  = _capture_buf->get_channel_mode();
		assert(playback_mode == capture_mode);
#endif
		return playback_mode;
	}

	/** Emitted when some MIDI data has been received for recording.
	 *  Parameter is the source that it is destined for.
	 *  A caller can get a copy of the data with get_gui_feed_buffer ()
	 */
	PBD::Signal1<void, boost::weak_ptr<MidiSource> > DataRecorded;

	boost::shared_ptr<MidiBuffer> get_gui_feed_buffer () const;

  protected:
	friend class Session;
	friend class Butler;

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

	static void set_readahead_frames (framecnt_t frames_ahead) { midi_readahead = frames_ahead; }

  protected:
	int seek (framepos_t which_sample, bool complete_refill = false);

  protected:
	friend class MidiTrack;

	int  process (framepos_t transport_frame, pframes_t nframes, framecnt_t &);
	bool commit  (framecnt_t nframes);
	static framecnt_t midi_readahead;

  private:

	/* The two central butler operations */
	int do_flush (RunContext context, bool force = false);
	int do_refill ();

	int do_refill_with_alloc();

	int read (framepos_t& start, framecnt_t cnt, bool reversed);

	void finish_capture ();
	void transport_stopped_wallclock (struct tm&, time_t, bool abort);
	void transport_looped (framepos_t transport_frame);

	void init ();

	int use_new_write_source (uint32_t n=0);

	int find_and_use_playlist (const string&);

	void allocate_temporary_buffers ();

	int use_pending_capture_data (XMLNode& node);

	void get_input_sources ();
	void set_align_style_from_io();

	/* fixed size buffers per instance of ardour for now (non-dynamic)
	 */

	void adjust_playback_buffering () {}
	void adjust_capture_buffering () {}

	bool prep_record_enable ();
	bool prep_record_disable ();
    
	MidiRingBuffer<framepos_t>*  _playback_buf;
	MidiRingBuffer<framepos_t>*  _capture_buf;
	boost::weak_ptr<MidiPort>    _source_port;
	boost::shared_ptr<SMFSource> _write_source;
	NoteMode                     _note_mode;
	volatile gint                _frames_written_to_ringbuffer;
	volatile gint                _frames_read_from_ringbuffer;
	volatile gint                _frames_pending_write;
	volatile gint                _num_captured_loops;

	/** A buffer that we use to put newly-arrived MIDI data in for
	    the GUI to read (so that it can update itself).
	*/
	MidiBuffer                   _gui_feed_buffer;
	mutable Glib::Threads::Mutex _gui_feed_buffer_mutex;
};

}; /* namespace ARDOUR */

#endif /* __ardour_midi_diskstream_h__ */
