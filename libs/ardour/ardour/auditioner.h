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
#include "ardour/audio_track.h"

namespace ARDOUR {

class Session;
class AudioRegion;
class AudioPlaylist;

class LIBARDOUR_API Auditioner : public AudioTrack
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
	void seek_response(frameoffset_t pos) { _seek_complete = true; if (_seeking) { current_frame = pos; _seek_complete = true;} }
	PBD::Signal2<void, ARDOUR::framecnt_t, ARDOUR::framecnt_t> AuditionProgress;

  private:
	boost::shared_ptr<AudioRegion> the_region;
	framepos_t current_frame;
	mutable gint _auditioning;
	Glib::Threads::Mutex lock;
	framecnt_t length;
	frameoffset_t _seek_frame;
	bool _seeking;
	bool _seek_complete;
	bool via_monitor;

	void drop_ports ();
	static void *_drop_ports (void *);
	void actually_drop_ports ();
	void output_changed (IOChange, void*);
};

}; /* namespace ARDOUR */

#endif /* __ardour_auditioner_h__ */
