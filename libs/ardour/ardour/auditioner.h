/*
    Copyright (C) 2001 Paul Davis

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

#ifndef __ardour_auditioner_h__
#define __ardour_auditioner_h__

#include <string>

#include <glibmm/threads.h>

#include "ardour/ardour.h"
#include "ardour/track.h"

namespace ARDOUR {

class Session;
class AudioDiskstream;
class AudioRegion;
class AudioPlaylist;
class MidiDiskstream;
class MidiRegion;

class LIBARDOUR_API Auditioner : public Track
{
  public:
	Auditioner (Session&);
	~Auditioner ();

	int init ();
	int connect ();

	void audition_region (boost::shared_ptr<Region>);

	void seek_to_frame (frameoffset_t pos) { if (_seek_frame < 0 && !_seeking) { _seek_frame = pos; }}
	void seek_to_percent (float const pos) { if (_seek_frame < 0 && !_seeking) { _seek_frame = floorf(length * pos / 100.0); }}

	ARDOUR::AudioPlaylist& prepare_playlist ();

	int play_audition (framecnt_t nframes);

	MonitorState monitoring_state () const;

	void cancel_audition () {
		g_atomic_int_set (&_auditioning, 0);
	}

	bool auditioning() const { return g_atomic_int_get (&_auditioning); }
	bool needs_monitor() const { return via_monitor; }

	virtual ChanCount input_streams () const;

	frameoffset_t seek_frame() const { return _seeking ? _seek_frame : -1;}
	void seek_response(frameoffset_t pos) {
		_seek_complete = true;
		if (_seeking) { current_frame = pos; _seek_complete = true;}
	}

	PBD::Signal2<void, ARDOUR::framecnt_t, ARDOUR::framecnt_t> AuditionProgress;

	/* Track */
	int roll (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler);
	DataType data_type () const;

	int roll_audio (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler);
	int roll_midi (pframes_t nframes, framepos_t start_frame, framepos_t end_frame, int declick, bool& need_butler);

	boost::shared_ptr<Diskstream> create_diskstream ();
	void set_diskstream (boost::shared_ptr<Diskstream> ds);

	/* fake track */
	void set_state_part_two () {}
	int set_state (const XMLNode&, int) { return 0; }
	bool bounceable (boost::shared_ptr<Processor>, bool) const { return false; }
	void freeze_me (InterThreadInfo&) {}
	void unfreeze () {}

	boost::shared_ptr<Region> bounce (InterThreadInfo&)
		{ return boost::shared_ptr<Region> (); }

	boost::shared_ptr<Region> bounce_range (framepos_t, framepos_t, InterThreadInfo&, boost::shared_ptr<Processor>, bool)
		{ return boost::shared_ptr<Region> (); }

	int export_stuff (BufferSet&, framepos_t, framecnt_t, boost::shared_ptr<Processor>, bool, bool, bool)
		{ return -1; }

	boost::shared_ptr<Diskstream> diskstream_factory (XMLNode const &)
		{ return boost::shared_ptr<Diskstream> (); }

	boost::shared_ptr<AudioDiskstream> audio_diskstream() const;
	boost::shared_ptr<MidiDiskstream> midi_diskstream() const;

  private:
	boost::shared_ptr<AudioRegion> the_region;
	boost::shared_ptr<MidiRegion> midi_region;
	framepos_t current_frame;
	mutable gint _auditioning;
	Glib::Threads::Mutex lock;
	framecnt_t length;
	frameoffset_t _seek_frame;
	bool _seeking;
	bool _seek_complete;
	bool via_monitor;
	bool _midi_audition;
	bool _synth_added;
	bool _synth_changed;
	bool _queue_panic;

	boost::shared_ptr<Diskstream> _diskstream_audio;
	boost::shared_ptr<Diskstream> _diskstream_midi;
	boost::shared_ptr<Processor> asynth;

	void drop_ports ();
	void lookup_synth ();
	void config_changed (std::string);
	static void *_drop_ports (void *);
	void actually_drop_ports ();
	void output_changed (IOChange, void*);
	frameoffset_t _import_position;
};

}; /* namespace ARDOUR */

#endif /* __ardour_auditioner_h__ */
