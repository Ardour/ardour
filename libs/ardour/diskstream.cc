/*
    Copyright (C) 2000-2003 Paul Davis 

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

    $Id: diskstream.cc 567 2006-06-07 14:54:12Z trutkin $
*/

#include <fstream>
#include <cstdio>
#include <unistd.h>
#include <cmath>
#include <cerrno>
#include <string>
#include <climits>
#include <fcntl.h>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include <sys/mman.h>

#include <pbd/error.h>
#include <pbd/basename.h>
#include <glibmm/thread.h>
#include <pbd/xml++.h>

#include <ardour/ardour.h>
#include <ardour/audioengine.h>
#include <ardour/diskstream.h>
#include <ardour/utils.h>
#include <ardour/configuration.h>
#include <ardour/audiofilesource.h>
#include <ardour/destructive_filesource.h>
#include <ardour/send.h>
#include <ardour/playlist.h>
#include <ardour/cycle_timer.h>
#include <ardour/region.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

jack_nframes_t Diskstream::disk_io_chunk_frames;

sigc::signal<void,Diskstream*>    Diskstream::DiskstreamCreated;
//sigc::signal<void,list<AudioFileSource*>*> Diskstream::DeleteSources;
sigc::signal<void>                Diskstream::DiskOverrun;
sigc::signal<void>                Diskstream::DiskUnderrun;

Diskstream::Diskstream (Session &sess, const string &name, Flag flag)
	: _name (name)
	, _session (sess)
{
	init (flag);
}
	
Diskstream::Diskstream (Session& sess, const XMLNode& node)
	: _session (sess)
	
{
	init (Recordable);
}

void
Diskstream::init (Flag f)
{
	_refcnt = 0;
	_flags = f;
	_io = 0;
	_alignment_style = ExistingMaterial;
	_persistent_alignment_style = ExistingMaterial;
	first_input_change = true;
	i_am_the_modifier = 0;
	g_atomic_int_set (&_record_enabled, 0);
	was_recording = false;
	capture_start_frame = 0;
	capture_captured = 0;
	_visible_speed = 1.0f;
	_actual_speed = 1.0f;
	_buffer_reallocation_required = false;
	_seek_required = false;
	first_recordable_frame = max_frames;
	last_recordable_frame = max_frames;
	_roll_delay = 0;
	_capture_offset = 0;
	_processed = false;
	_slaved = false;
	adjust_capture_position = 0;
	last_possibly_recording = 0;
	loop_location = 0;
	wrap_buffer_size = 0;
	speed_buffer_size = 0;
	last_phase = 0;
	phi = (uint64_t) (0x1000000);
	file_frame = 0;
	playback_sample = 0;
	playback_distance = 0;
	_read_data_count = 0;
	_write_data_count = 0;

	/* there are no channels at this point, so these
	   two calls just get speed_buffer_size and wrap_buffer
	   size setup without duplicating their code.
	*/

	//set_block_size (_session.get_block_size());
	//allocate_temporary_buffers ();

	pending_overwrite = false;
	overwrite_frame = 0;
	overwrite_queued = false;
	input_change_pending = NoChange;

	//add_channel ();
	_n_channels = 0;//1;
}

Diskstream::~Diskstream ()
{
	// Taken by child.. assure lock?
	//Glib::Mutex::Lock lm (state_lock);

	//if (_playlist) {
	//	_playlist->unref ();
	//}

	//for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
	//	destroy_channel((*chan));
	//}
	
	//channels.clear();
}

void
Diskstream::handle_input_change (IOChange change, void *src)
{
	Glib::Mutex::Lock lm (state_lock);

	if (!(input_change_pending & change)) {
		input_change_pending = IOChange (input_change_pending|change);
		_session.request_input_change_handling ();
	}
}

bool
Diskstream::realtime_set_speed (double sp, bool global)
{
	bool changed = false;
	double new_speed = sp * _session.transport_speed();
	
	if (_visible_speed != sp) {
		_visible_speed = sp;
		changed = true;
	}
	
	if (new_speed != _actual_speed) {
		
		jack_nframes_t required_wrap_size = (jack_nframes_t) floor (_session.get_block_size() * 
									    fabs (new_speed)) + 1;
		
		if (required_wrap_size > wrap_buffer_size) {
			_buffer_reallocation_required = true;
		}
		
		_actual_speed = new_speed;
		phi = (uint64_t) (0x1000000 * fabs(_actual_speed));
	}

	if (changed) {
		if (!global) {
			_seek_required = true;
		}
		SpeedChanged (); /* EMIT SIGNAL */
	}

	return _buffer_reallocation_required || _seek_required;
}

void
Diskstream::prepare ()
{
	_processed = false;
	playback_distance = 0;
}

void
Diskstream::recover ()
{
	state_lock.unlock();
	_processed = false;
}

void
Diskstream::set_capture_offset ()
{
	if (_io == 0) {
		/* can't capture, so forget it */
		return;
	}

	_capture_offset = _io->input_latency();
}

void
Diskstream::set_align_style (AlignStyle a)
{
	if (record_enabled() && _session.actively_recording()) {
		return;
	}


	if (a != _alignment_style) {
		_alignment_style = a;
		AlignmentStyleChanged ();
	}
}

int
Diskstream::set_loop (Location *location)
{
	if (location) {
		if (location->start() >= location->end()) {
			error << string_compose(_("Location \"%1\" not valid for track loop (start >= end)"), location->name()) << endl;
			return -1;
		}
	}

	loop_location = location;

	 LoopSet (location); /* EMIT SIGNAL */
	return 0;
}

jack_nframes_t
Diskstream::get_capture_start_frame (uint32_t n)
{
	Glib::Mutex::Lock lm (capture_info_lock);

	if (capture_info.size() > n) {
		return capture_info[n]->start;
	}
	else {
		return capture_start_frame;
	}
}

jack_nframes_t
Diskstream::get_captured_frames (uint32_t n)
{
	Glib::Mutex::Lock lm (capture_info_lock);

	if (capture_info.size() > n) {
		return capture_info[n]->frames;
	}
	else {
		return capture_captured;
	}
}

void
Diskstream::set_roll_delay (jack_nframes_t nframes)
{
	_roll_delay = nframes;
}

void
Diskstream::set_speed (double sp)
{
	_session.request_diskstream_speed (*this, sp);

	/* to force a rebuffering at the right place */
	playlist_modified();
}

void
Diskstream::playlist_changed (Change ignored)
{
	playlist_modified ();
}

void
Diskstream::playlist_modified ()
{
	if (!i_am_the_modifier && !overwrite_queued) {
		_session.request_overwrite_buffer (this);
		overwrite_queued = true;
	} 
}

int
Diskstream::set_name (string str, void *src)
{
	if (str != _name) {
		assert(playlist());
		playlist()->set_name (str);
		_name = str;
		
		if (!in_set_state && recordable()) {
			/* rename existing capture files so that they have the correct name */
			return rename_write_sources ();
		} else {
			return -1;
		}
	}

	return 0;
}

void
Diskstream::set_destructive (bool yn)
{
	if (yn != destructive()) {
		reset_write_sources (true, true);
		if (yn) {
			_flags |= Destructive;
		} else {
			_flags &= ~Destructive;
		}
	}
}
