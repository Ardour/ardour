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

    $Id$
*/

#ifndef __ardour_auditioner_h__
#define __ardour_auditioner_h__

#include <string>

#include <glibmm/thread.h>

#include <ardour/ardour.h>
#include <ardour/audio_track.h>

namespace ARDOUR {

class Session;
class AudioRegion;
class AudioPlaylist;

class Auditioner : public AudioTrack
{
  public:
	Auditioner (Session&);
	~Auditioner ();

	void audition_region (AudioRegion&);

	ARDOUR::AudioPlaylist& prepare_playlist ();
	void audition_current_playlist ();

	int  play_audition (jack_nframes_t nframes);

	void cancel_audition () { 
		g_atomic_int_set (&_active, 0);
	}

	bool active() const { return g_atomic_int_get (&_active); }

  private:
	AudioRegion *the_region;
	jack_nframes_t current_frame;
	mutable gint _active;
	Glib::Mutex lock;
	jack_nframes_t length;

	void drop_ports ();
	static void *_drop_ports (void *);
	void actually_drop_ports ();
	void output_changed (IOChange, void*);
};	

}; /* namespace ARDOUR */

#endif /* __ardour_auditioner_h__ */
