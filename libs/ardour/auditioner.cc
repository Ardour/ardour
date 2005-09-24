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

#include <pbd/lockmonitor.h>

#include <ardour/diskstream.h>
#include <ardour/audioregion.h>
#include <ardour/route.h>
#include <ardour/session.h>
#include <ardour/auditioner.h>
#include <ardour/audioplaylist.h>
#include <ardour/panner.h>

using namespace std;
using namespace ARDOUR;

Auditioner::Auditioner (Session& s)
	: AudioTrack (s, "auditioner", Route::Hidden)
{
	string left = Config->get_auditioner_output_left();
	string right = Config->get_auditioner_output_right();
	
	if ((left.length() == 0) && (right.length() == 0)) {
		return;
	}

	defer_pan_reset ();

	if (left.length()) {
		add_output_port (left, this);
	}

	if (right.length()) {
		disk_stream().add_channel();
		add_output_port (right, this);
	}

	allow_pan_reset ();
	
	IO::output_changed.connect (mem_fun (*this, &Auditioner::output_changed));

	the_region = 0;
	atomic_set (&_active, 0);
}

Auditioner::~Auditioner ()
{
}

AudioPlaylist&
Auditioner::prepare_playlist ()
{
	diskstream->playlist()->clear (false, false);
	return *diskstream->playlist();
}

void
Auditioner::audition_current_playlist ()
{
	if (atomic_read (&_active)) {
		/* don't go via session for this, because we are going
		   to remain active.
		*/
		cancel_audition ();
	}

	LockMonitor lm (lock, __LINE__, __FILE__);
	diskstream->seek (0);
	length = diskstream->playlist()->get_maximum_extent();
	current_frame = 0;

	/* force a panner reset now that we have all channels */

	_panner->reset (n_outputs(), diskstream->n_channels());

	atomic_set (&_active, 1);
}

void
Auditioner::audition_region (AudioRegion& region)
{
	if (atomic_read (&_active)) {
		/* don't go via session for this, because we are going
		   to remain active.
		*/
		cancel_audition ();
	}

	LockMonitor lm (lock, __LINE__, __FILE__);

	the_region = new AudioRegion (region);
	the_region->set_position (0, this);

	diskstream->playlist()->clear (true, false);
	diskstream->playlist()->add_region (*the_region, 0, 1, false);

	while (diskstream->n_channels() < the_region->n_channels()) {
		diskstream->add_channel ();
	}

	while (diskstream->n_channels() > the_region->n_channels()) {
		diskstream->remove_channel ();
	}

	/* force a panner reset now that we have all channels */

	_panner->reset (n_outputs(), diskstream->n_channels());

	length = the_region->length();
	diskstream->seek (0);
	current_frame = 0;
	atomic_set (&_active, 1);
}

int
Auditioner::play_audition (jack_nframes_t nframes)
{
	bool need_butler;
	jack_nframes_t this_nframes;
	int ret;

	if (atomic_read (&_active) == 0) {
		silence (nframes, 0);
		return 0;
	}

	this_nframes = min (nframes, length - current_frame);

	diskstream->prepare ();

	if ((ret = roll (this_nframes, current_frame, current_frame + nframes, 0, false, false, false)) != 0) {
		silence (nframes, 0);
		return ret;
	}

	need_butler = diskstream->commit (this_nframes);
	current_frame += this_nframes;

	if (current_frame >= length) {
		_session.cancel_audition ();
		return 0;
	} else {
		return need_butler ? 1 : 0;
	}
}

void
Auditioner::output_changed (IOChange change, void* src)
{
	if (change & ConnectionsChanged) {
		const char ** connections;
		connections =  output (0)->get_connections ();
		if (connections) {
			Config->set_auditioner_output_left (connections[0]);
			free (connections);
		}
		
		connections = output (1)->get_connections ();
		if (connections) {
			Config->set_auditioner_output_right (connections[0]);
			free (connections);
		}
	}
}
