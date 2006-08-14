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

#include <sigc++/signal.h>

#include <cmath>
#include <cassert>
#include <string>
#include <queue>
#include <map>
#include <vector>

#include <time.h>

#include <pbd/fastlog.h>
#include <pbd/ringbufferNPT.h>
 

#include <ardour/ardour.h>
#include <ardour/configuration.h>
#include <ardour/session.h>
#include <ardour/route_group.h>
#include <ardour/route.h>
#include <ardour/port.h>
#include <ardour/utils.h>
#include <ardour/diskstream.h>
#include <ardour/midi_playlist.h>
struct tm;

namespace ARDOUR {

class MidiEngine;
class Send;
class Session;
class MidiPlaylist;
class SMFSource;
class IO;

class MidiDiskstream : public Diskstream
{	
  public:
	MidiDiskstream (Session &, const string& name, Diskstream::Flag f = Recordable);
	MidiDiskstream (Session &, const XMLNode&);

	float playback_buffer_load() const;
	float capture_buffer_load() const;
	
	RawMidi* playback_buffer () { return _current_playback_buffer; }
	RawMidi* capture_buffer ()  { return _current_capture_buffer; }

	void set_record_enabled (bool yn);

	MidiPlaylist* midi_playlist () { return dynamic_cast<MidiPlaylist*>(_playlist); }

	int use_playlist (Playlist *);
	int use_new_playlist ();
	int use_copy_playlist ();

	/* stateful */

	XMLNode& get_state(void);
	int set_state(const XMLNode& node);

	void monitor_input (bool);

	MidiSource* write_source() { return (MidiSource*)_write_source; }
	
	void set_destructive (bool yn); // doom!

  protected:
	friend class Session;

	/* the Session is the only point of access for these
	   because they require that the Session is "inactive"
	   while they are called.
	*/

	void set_pending_overwrite(bool);
	int  overwrite_existing_buffers ();
	void set_block_size (jack_nframes_t);
	int  internal_playback_seek (jack_nframes_t distance);
	int  can_internal_playback_seek (jack_nframes_t distance);
	int  rename_write_sources ();
	void reset_write_sources (bool, bool force = false);
	void non_realtime_input_change ();

  protected:
	int seek (jack_nframes_t which_sample, bool complete_refill = false);

  protected:
	friend class MidiTrack;

	int  process (jack_nframes_t transport_frame, jack_nframes_t nframes, jack_nframes_t offset, bool can_record, bool rec_monitors_input);
	bool commit  (jack_nframes_t nframes);

  private:

	/* use unref() to destroy a diskstream */
	~MidiDiskstream();

	/* The two central butler operations */
	int do_flush (Session::RunContext context, bool force = false);
	int do_refill ();
	
	int do_refill_with_alloc();

	int read (MidiBuffer& dst, jack_nframes_t& start, jack_nframes_t cnt, bool reversed);

	void finish_capture (bool rec_monitors_input);
	void transport_stopped (struct tm&, time_t, bool abort);

	void init (Diskstream::Flag);

	int use_new_write_source (uint32_t n=0);

	int find_and_use_playlist (const string&);

	void allocate_temporary_buffers ();

	int use_pending_capture_data (XMLNode& node);

	void get_input_sources ();
	void check_record_status (jack_nframes_t transport_frame, jack_nframes_t nframes, bool can_record);
	void set_align_style_from_io();
	
	void engage_record_enable ();
	void disengage_record_enable ();
	
	// FIXME: This is basically a single ChannelInfo.. abstractify that concept?
	RingBufferNPT<RawMidi>*           _playback_buf;
	RingBufferNPT<RawMidi>*           _capture_buf;
	RawMidi*                          _current_playback_buffer;
	RawMidi*                          _current_capture_buffer;
	RawMidi*                          _playback_wrap_buffer;
	RawMidi*                          _capture_wrap_buffer;
	MidiPort*                         _source_port;
	SMFSource*                        _write_source; ///< aka capturing source
	RingBufferNPT<CaptureTransition>* _capture_transition_buf;
	RingBufferNPT<RawMidi>::rw_vector _playback_vector;
	RingBufferNPT<RawMidi>::rw_vector _capture_vector;
};

}; /* namespace ARDOUR */

#endif /* __ardour_midi_diskstream_h__ */
