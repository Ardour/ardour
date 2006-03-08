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

    $Id$
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
#include <pbd/lockmonitor.h>
#include <pbd/xml++.h>

#include <ardour/ardour.h>
#include <ardour/audioengine.h>
#include <ardour/diskstream.h>
#include <ardour/utils.h>
#include <ardour/configuration.h>
#include <ardour/filesource.h>
#include <ardour/destructive_filesource.h>
#include <ardour/send.h>
#include <ardour/audioplaylist.h>
#include <ardour/cycle_timer.h>
#include <ardour/audioregion.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;

jack_nframes_t DiskStream::disk_io_chunk_frames;

sigc::signal<void,DiskStream*>    DiskStream::DiskStreamCreated;
sigc::signal<void,DiskStream*>    DiskStream::CannotRecordNoInput;
sigc::signal<void,list<Source*>*> DiskStream::DeleteSources;
sigc::signal<void>                DiskStream::DiskOverrun;
sigc::signal<void>                DiskStream::DiskUnderrun;

DiskStream::DiskStream (Session &sess, const string &name, Flag flag)
	: _name (name),
	  _session (sess)
{
	/* prevent any write sources from being created */

	in_set_state = true;
	init (flag);
	use_new_playlist ();
	in_set_state = false;

	if (destructive()) {
		setup_destructive_playlist ();
	}

	DiskStreamCreated (this); /* EMIT SIGNAL */
}
	
DiskStream::DiskStream (Session& sess, const XMLNode& node)
	: _session (sess)
	
{
	in_set_state = true;
	init (Recordable);

	if (set_state (node)) {
		in_set_state = false;
		throw failed_constructor();
	}

	in_set_state = false;

	if (destructive()) {
		use_destructive_playlist ();
	}

	DiskStreamCreated (this); /* EMIT SIGNAL */
}

void
DiskStream::init_channel (ChannelInfo &chan)
{
	chan.playback_wrap_buffer = 0;
	chan.capture_wrap_buffer = 0;
	chan.speed_buffer = 0;
	chan.peak_power = 0.0f;
	chan.write_source = 0;
	chan.source = 0;
	chan.current_capture_buffer = 0;
	chan.current_playback_buffer = 0;
	chan.curr_capture_cnt = 0;
	
	chan.playback_buf = new RingBufferNPT<Sample> (_session.diskstream_buffer_size());
	chan.capture_buf = new RingBufferNPT<Sample> (_session.diskstream_buffer_size());
	chan.capture_transition_buf = new RingBufferNPT<CaptureTransition> (128);
	
	
	/* touch the ringbuffer buffers, which will cause
	   them to be mapped into locked physical RAM if
	   we're running with mlockall(). this doesn't do
	   much if we're not.  
	*/
	memset (chan.playback_buf->buffer(), 0, sizeof (Sample) * chan.playback_buf->bufsize());
	memset (chan.capture_buf->buffer(), 0, sizeof (Sample) * chan.capture_buf->bufsize());
	memset (chan.capture_transition_buf->buffer(), 0, sizeof (CaptureTransition) * chan.capture_transition_buf->bufsize());
}


void
DiskStream::init (Flag f)
{
	_id = new_id();
	_refcnt = 0;
	_flags = f;
	_io = 0;
	_alignment_style = ExistingMaterial;
	_persistent_alignment_style = ExistingMaterial;
	first_input_change = true;
	_playlist = 0;
	i_am_the_modifier = 0;
	atomic_set (&_record_enabled, 0);
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
	deprecated_io_node = 0;

	/* there are no channels at this point, so these
	   two calls just get speed_buffer_size and wrap_buffer
	   size setup without duplicating their code.
	*/

	set_block_size (_session.get_block_size());
	allocate_temporary_buffers ();

	pending_overwrite = false;
	overwrite_frame = 0;
	overwrite_queued = false;
	input_change_pending = NoChange;

	add_channel ();
	_n_channels = 1;
}

void
DiskStream::destroy_channel (ChannelInfo &chan)
{
	if (chan.write_source) {
		chan.write_source->release ();
		chan.write_source = 0;
	}
		
	if (chan.speed_buffer) {
		delete [] chan.speed_buffer;
	}

	if (chan.playback_wrap_buffer) {
		delete [] chan.playback_wrap_buffer;
	}
	if (chan.capture_wrap_buffer) {
		delete [] chan.capture_wrap_buffer;
	}
	
	delete chan.playback_buf;
	delete chan.capture_buf;
	delete chan.capture_transition_buf;
	
	chan.playback_buf = 0;
	chan.capture_buf = 0;
}

DiskStream::~DiskStream ()
{
	LockMonitor lm (state_lock, __LINE__, __FILE__);

	if (_playlist) {
		_playlist->unref ();
	}

	for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
		destroy_channel((*chan));
	}
	
	channels.clear();
}

void
DiskStream::handle_input_change (IOChange change, void *src)
{
	LockMonitor lm (state_lock, __LINE__, __FILE__);

	if (!(input_change_pending & change)) {
		input_change_pending = IOChange (input_change_pending|change);
		_session.request_input_change_handling ();
	}
}

void
DiskStream::non_realtime_input_change ()
{
	{ 
		LockMonitor lm (state_lock, __LINE__, __FILE__);

		if (input_change_pending == NoChange) {
			return;
		}

		if (input_change_pending & ConfigurationChanged) {

			if (_io->n_inputs() > _n_channels) {
				
				// we need to add new channel infos
				
				int diff = _io->n_inputs() - channels.size();
				
				for (int i = 0; i < diff; ++i) {
					add_channel ();
				}
				
		} else if (_io->n_inputs() < _n_channels) {
				
				// we need to get rid of channels
				
				int diff = channels.size() - _io->n_inputs();
				
				for (int i = 0; i < diff; ++i) {
					remove_channel ();
				}
			}
		} 

		get_input_sources ();
		set_capture_offset ();
		
		if (first_input_change) {
			set_align_style (_persistent_alignment_style);
			first_input_change = false;
		} else {
			set_align_style_from_io ();
		}

		input_change_pending = NoChange;
	}

	/* reset capture files */

	reset_write_sources (false);

	/* now refill channel buffers */

	if (speed() != 1.0f || speed() != -1.0f) {
		seek ((jack_nframes_t) (_session.transport_frame() * (double) speed()));
	}
	else {
		seek (_session.transport_frame());
	}
}

void
DiskStream::get_input_sources ()
{
	uint32_t ni = _io->n_inputs();
	
	for (uint32_t n = 0; n < ni; ++n) {
		
		const char **connections = _io->input(n)->get_connections ();
		ChannelInfo& chan = channels[n];
		
		if (connections == 0 || connections[0] == 0) {
			
			if (chan.source) {
				// _source->disable_metering ();
			}
			
			chan.source = 0;
			
		} else {
			chan.source = _session.engine().get_port_by_name (connections[0]);
		}
		
		if (connections) {
			free (connections);
		}
	}
}		

int
DiskStream::find_and_use_playlist (const string& name)
{
	Playlist* pl;
	AudioPlaylist* playlist;
		
	if ((pl = _session.get_playlist (name)) == 0) {
		error << string_compose(_("DiskStream: Session doesn't know about a Playlist called \"%1\""), name) << endmsg;
		return -1;
	}

	if ((playlist = dynamic_cast<AudioPlaylist*> (pl)) == 0) {
		error << string_compose(_("DiskStream: Playlist \"%1\" isn't an audio playlist"), name) << endmsg;
		return -1;
	}

	return use_playlist (playlist);
}

int
DiskStream::use_playlist (AudioPlaylist* playlist)
{
	{
		LockMonitor lm (state_lock, __LINE__, __FILE__);

		if (playlist == _playlist) {
			return 0;
		}

		plstate_connection.disconnect();
		plmod_connection.disconnect ();
		plgone_connection.disconnect ();

		if (_playlist) {
			_playlist->unref();
		}
			
		_playlist = playlist;
		_playlist->ref();

		if (!in_set_state && recordable()) {
			reset_write_sources (false);
		}
		
		plstate_connection = _playlist->StateChanged.connect (mem_fun (*this, &DiskStream::playlist_changed));
		plmod_connection = _playlist->Modified.connect (mem_fun (*this, &DiskStream::playlist_modified));
		plgone_connection = _playlist->GoingAway.connect (mem_fun (*this, &DiskStream::playlist_deleted));
	}

	if (!overwrite_queued) {
		_session.request_overwrite_buffer (this);
		overwrite_queued = true;
	}
	
	PlaylistChanged (); /* EMIT SIGNAL */
	_session.set_dirty ();

	return 0;
}

void
DiskStream::playlist_deleted (Playlist* pl)
{
	/* this catches an ordering issue with session destruction. playlists 
	   are destroyed before diskstreams. we have to invalidate any handles
	   we have to the playlist.
	*/

	_playlist = 0;
}

int
DiskStream::use_new_playlist ()
{
	string newname;
	AudioPlaylist* playlist;

	if (!in_set_state && destructive()) {
		return 0;
	}

	if (_playlist) {
		newname = Playlist::bump_name (_playlist->name(), _session);
	} else {
		newname = Playlist::bump_name (_name, _session);
	}

	if ((playlist = new AudioPlaylist (_session, newname, hidden())) != 0) {
		playlist->set_orig_diskstream_id (id());
		return use_playlist (playlist);
	} else { 
		return -1;
	}
}

int
DiskStream::use_copy_playlist ()
{
	if (destructive()) {
		return 0;
	}

	if (_playlist == 0) {
		error << string_compose(_("DiskStream %1: there is no existing playlist to make a copy of!"), _name) << endmsg;
		return -1;
	}

	string newname;
	AudioPlaylist* playlist;

	newname = Playlist::bump_name (_playlist->name(), _session);
	
	if ((playlist  = new AudioPlaylist (*_playlist, newname)) != 0) {
		playlist->set_orig_diskstream_id (id());
		return use_playlist (playlist);
	} else { 
		return -1;
	}
}

void
DiskStream::setup_destructive_playlist ()
{
	AudioRegion::SourceList srcs;

	/* make sure we have sources for every channel */

	reset_write_sources (true);

	for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
		srcs.push_back ((*chan).write_source);
	}

	/* a single full-sized region */

	AudioRegion* region = new AudioRegion (srcs, 0, max_frames, _name);
	_playlist->add_region (*region, 0);		
}

void
DiskStream::use_destructive_playlist ()
{
	/* use the sources associated with the single full-extent region */

	AudioRegion* region = dynamic_cast<AudioRegion*> (_playlist->regions_at (0)->front());
	uint32_t n;
	ChannelList::iterator chan;

	for (n = 0, chan = channels.begin(); chan != channels.end(); ++chan, ++n) {
		(*chan).write_source = dynamic_cast<FileSource*>(&region->source (n));
	}

	/* the source list will never be reset for a destructive track */
}

void
DiskStream::set_io (IO& io)
{
	_io = &io;
	set_align_style_from_io ();
}

void
DiskStream::set_name (string str, void *src)
{
	if (str != _name) {
		_playlist->set_name (str);
		_name = str;
		
		if (!in_set_state && recordable()) {

			/* open new capture files so that they have the correct name */

			reset_write_sources (false);
		}
	}
}

void
DiskStream::set_speed (double sp)
{
	_session.request_diskstream_speed (*this, sp);

	/* to force a rebuffering at the right place */
	playlist_modified();
}

bool
DiskStream::realtime_set_speed (double sp, bool global)
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
		 speed_changed (); /* EMIT SIGNAL */
	}

	return _buffer_reallocation_required || _seek_required;
}

void
DiskStream::non_realtime_set_speed ()
{
	if (_buffer_reallocation_required)
	{
		LockMonitor lm (state_lock, __LINE__, __FILE__);
		allocate_temporary_buffers ();

		_buffer_reallocation_required = false;
	}

	if (_seek_required) {
		if (speed() != 1.0f || speed() != -1.0f) {
			seek ((jack_nframes_t) (_session.transport_frame() * (double) speed()), true);
		}
		else {
			seek (_session.transport_frame(), true);
		}

		_seek_required = false;
	}
}

void
DiskStream::prepare ()
{
	_processed = false;
	playback_distance = 0;
}

void
DiskStream::check_record_status (jack_nframes_t transport_frame, jack_nframes_t nframes, bool can_record)
{
	int possibly_recording;
	int rolling;
	int change;
	const int transport_rolling = 0x4;
	const int track_rec_enabled = 0x2;
	const int global_rec_enabled = 0x1;

	/* merge together the 3 factors that affect record status, and compute
	   what has changed.
	*/

	rolling = _session.transport_speed() != 0.0f;
	possibly_recording = (rolling << 2) | (record_enabled() << 1) | can_record;
	change = possibly_recording ^ last_possibly_recording;

	if (possibly_recording == last_possibly_recording) {
		return;
	}

	/* change state */
	
	/* if per-track or global rec-enable turned on while the other was already on, we've started recording */

	if ((change & track_rec_enabled) && record_enabled() && (!(change & global_rec_enabled) && can_record) || 
	    ((change & global_rec_enabled) && can_record && (!(change & track_rec_enabled) && record_enabled()))) {
		
		/* starting to record: compute first+last frames */

		first_recordable_frame = transport_frame + _capture_offset;
		last_recordable_frame = max_frames;
		capture_start_frame = transport_frame;

		if (!(last_possibly_recording & transport_rolling) && (possibly_recording & transport_rolling)) {

			/* was stopped, now rolling (and recording) */

			if (_alignment_style == ExistingMaterial) {
				first_recordable_frame += _session.worst_output_latency();
			} else {
				first_recordable_frame += _roll_delay;
  			}

		} else {

			/* was rolling, but record state changed */

			if (_alignment_style == ExistingMaterial) {


				if (!_session.get_punch_in()) {

					/* manual punch in happens at the correct transport frame
					   because the user hit a button. but to get alignment correct 
					   we have to back up the position of the new region to the 
					   appropriate spot given the roll delay.
					*/

					capture_start_frame -= _roll_delay;

					/* XXX paul notes (august 2005): i don't know why
					   this is needed.
					*/

					first_recordable_frame += _capture_offset;

				} else {

					/* autopunch toggles recording at the precise
					   transport frame, and then the DS waits
					   to start recording for a time that depends
					   on the output latency.
					*/

					first_recordable_frame += _session.worst_output_latency();
				}

			} else {

				if (_session.get_punch_in()) {
					first_recordable_frame += _roll_delay;
				} else {
					capture_start_frame -= _roll_delay;
				}
			}
			
		}

		if (_flags & Recordable) {
			for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
				
				RingBufferNPT<CaptureTransition>::rw_vector transvec;
				(*chan).capture_transition_buf->get_write_vector(&transvec);
				
				if (transvec.len[0] > 0) {
					transvec.buf[0]->type = CaptureStart;
					transvec.buf[0]->capture_val = capture_start_frame;
					(*chan).capture_transition_buf->increment_write_ptr(1);
				}
				else {
					// bad!
					fatal << X_("programming error: capture_transition_buf is full on rec start!  inconceivable!") 
					      << endmsg;
				}
 			}
		}

	} else if (!record_enabled() || !can_record) {
		
		/* stop recording */
		
		last_recordable_frame = transport_frame + _capture_offset;
		
		if (_alignment_style == ExistingMaterial) {
			last_recordable_frame += _session.worst_output_latency();
		} else {
			last_recordable_frame += _roll_delay;
		}
	}

	last_possibly_recording = possibly_recording;
}

int
DiskStream::process (jack_nframes_t transport_frame, jack_nframes_t nframes, jack_nframes_t offset, bool can_record, bool rec_monitors_input)
{
	uint32_t n;
	ChannelList::iterator c;
	int ret = -1;
	jack_nframes_t rec_offset = 0;
	jack_nframes_t rec_nframes = 0;
	bool nominally_recording;
	bool re = record_enabled ();
	bool collect_playback = false;

	/* if we've already processed the frames corresponding to this call,
	   just return. this allows multiple routes that are taking input
	   from this diskstream to call our ::process() method, but have
	   this stuff only happen once. more commonly, it allows both
	   the AudioTrack that is using this DiskStream *and* the Session
	   to call process() without problems.
	*/

	if (_processed) {
		return 0;
	}

	check_record_status (transport_frame, nframes, can_record);

	nominally_recording = (can_record && re);

	if (nframes == 0) {
		_processed = true;
		return 0;
	}

	/* This lock is held until the end of DiskStream::commit, so these two functions
	   must always be called as a pair. The only exception is if this function
	   returns a non-zero value, in which case, ::commit should not be called.
	*/

	if (pthread_mutex_trylock (state_lock.mutex())) {
		return 1;
	}

	adjust_capture_position = 0;

	for (c = channels.begin(); c != channels.end(); ++c) {
		(*c).current_capture_buffer = 0;
		(*c).current_playback_buffer  = 0;
	}

	if (nominally_recording || (_session.get_record_enabled() && _session.get_punch_in())) {
		OverlapType ot;
		
		ot = coverage (first_recordable_frame, last_recordable_frame, transport_frame, transport_frame + nframes);

		switch (ot) {
		case OverlapNone:
			rec_nframes = 0;
			break;
			
		case OverlapInternal:
		/*     ----------    recrange
                         |---|       transrange
		*/
			rec_nframes = nframes;
			rec_offset = 0;
			break;
			
		case OverlapStart:
			/*    |--------|    recrange
                            -----|          transrange
			*/
			rec_nframes = transport_frame + nframes - first_recordable_frame;
			if (rec_nframes) {
				rec_offset = first_recordable_frame - transport_frame;
			}
			break;
			
		case OverlapEnd:
			/*    |--------|    recrange
                                 |--------  transrange
			*/
			rec_nframes = last_recordable_frame - transport_frame;
			rec_offset = 0;
			break;
			
		case OverlapExternal:
			/*    |--------|    recrange
                            --------------  transrange
			*/
			rec_nframes = last_recordable_frame - last_recordable_frame;
			rec_offset = first_recordable_frame - transport_frame;
			break;
		}

		if (rec_nframes && !was_recording) {
			capture_captured = 0;
			was_recording = true;
		}
	}


	if (can_record && !_last_capture_regions.empty()) {
		_last_capture_regions.clear ();
	}

	if (nominally_recording || rec_nframes) {

		for (n = 0, c = channels.begin(); c != channels.end(); ++c, ++n) {
			
			ChannelInfo& chan (*c);
		
			chan.capture_buf->get_write_vector (&chan.capture_vector);

			if (rec_nframes <= chan.capture_vector.len[0]) {
				
				chan.current_capture_buffer = chan.capture_vector.buf[0];

				/* note: grab the entire port buffer, but only copy what we were supposed to for recording, and use
				   rec_offset
				*/

				memcpy (chan.current_capture_buffer, _io->input(n)->get_buffer (rec_nframes) + offset + rec_offset, sizeof (Sample) * rec_nframes);

			} else {

				jack_nframes_t total = chan.capture_vector.len[0] + chan.capture_vector.len[1];

				if (rec_nframes > total) {
					DiskOverrun ();
					goto out;
				}

				Sample* buf = _io->input (n)->get_buffer (nframes) + offset;
				jack_nframes_t first = chan.capture_vector.len[0];

				memcpy (chan.capture_wrap_buffer, buf, sizeof (Sample) * first);
				memcpy (chan.capture_vector.buf[0], buf, sizeof (Sample) * first);
				memcpy (chan.capture_wrap_buffer+first, buf + first, sizeof (Sample) * (rec_nframes - first));
				memcpy (chan.capture_vector.buf[1], buf + first, sizeof (Sample) * (rec_nframes - first));
				
				chan.current_capture_buffer = chan.capture_wrap_buffer;
			}
		}

	} else {

		if (was_recording) {
			finish_capture (rec_monitors_input);
		}

	}
	
	if (rec_nframes) {
		
		/* data will be written to disk */

		if (rec_nframes == nframes && rec_offset == 0) {

			for (c = channels.begin(); c != channels.end(); ++c) {
				(*c).current_playback_buffer = (*c).current_capture_buffer;
			}

			playback_distance = nframes;

		} else {


			/* we can't use the capture buffer as the playback buffer, because
			   we recorded only a part of the current process' cycle data
			   for capture.
			*/

			collect_playback = true;
		}

		adjust_capture_position = rec_nframes;

	} else if (nominally_recording) {

		/* can't do actual capture yet - waiting for latency effects to finish before we start*/

		for (c = channels.begin(); c != channels.end(); ++c) {
			(*c).current_playback_buffer = (*c).current_capture_buffer;
		}

		playback_distance = nframes;

	} else {

		collect_playback = true;
	}

	if (collect_playback) {

		/* we're doing playback */

		jack_nframes_t necessary_samples;

		/* no varispeed playback if we're recording, because the output .... TBD */

		if (rec_nframes == 0 && _actual_speed != 1.0f) {
			necessary_samples = (jack_nframes_t) floor ((nframes * fabs (_actual_speed))) + 1;
		} else {
			necessary_samples = nframes;
		}
		
		for (c = channels.begin(); c != channels.end(); ++c) {
			(*c).playback_buf->get_read_vector (&(*c).playback_vector);
		}

		n = 0;			

		for (c = channels.begin(); c != channels.end(); ++c, ++n) {
		
			ChannelInfo& chan (*c);

			if (necessary_samples <= chan.playback_vector.len[0]) {

				chan.current_playback_buffer = chan.playback_vector.buf[0];

			} else {
				jack_nframes_t total = chan.playback_vector.len[0] + chan.playback_vector.len[1];
				
				if (necessary_samples > total) {
					DiskUnderrun ();
					goto out;
					
				} else {
					
					memcpy ((char *) chan.playback_wrap_buffer, chan.playback_vector.buf[0],
						chan.playback_vector.len[0] * sizeof (Sample));
					memcpy (chan.playback_wrap_buffer + chan.playback_vector.len[0], chan.playback_vector.buf[1], 
						(necessary_samples - chan.playback_vector.len[0]) * sizeof (Sample));
					
					chan.current_playback_buffer = chan.playback_wrap_buffer;
				}
			}
		} 

		if (rec_nframes == 0 && _actual_speed != 1.0f && _actual_speed != -1.0f) {
			
			uint64_t phase = last_phase;
			jack_nframes_t i = 0;

			// Linearly interpolate into the alt buffer
			// using 40.24 fixp maths (swh)

			for (c = channels.begin(); c != channels.end(); ++c) {

				float fr;
				ChannelInfo& chan (*c);

				i = 0;
				phase = last_phase;

				for (jack_nframes_t outsample = 0; outsample < nframes; ++outsample) {
					i = phase >> 24;
					fr = (phase & 0xFFFFFF) / 16777216.0f;
					chan.speed_buffer[outsample] = 
						chan.current_playback_buffer[i] * (1.0f - fr) +
						chan.current_playback_buffer[i+1] * fr;
					phase += phi;
				}
				
				chan.current_playback_buffer = chan.speed_buffer;
			}

			playback_distance = i + 1;
			last_phase = (phase & 0xFFFFFF);

		} else {
			playback_distance = nframes;
		}

	}

	ret = 0;

  out:
	_processed = true;

	if (ret) {

		/* we're exiting with failure, so ::commit will not
		   be called. unlock the state lock.
		*/
		
		pthread_mutex_unlock (state_lock.mutex());
	} 

	return ret;
}

void
DiskStream::recover ()
{
	pthread_mutex_unlock (state_lock.mutex());
	_processed = false;
}

bool
DiskStream::commit (jack_nframes_t nframes)
{
	bool need_butler = false;

	if (_actual_speed < 0.0) {
		playback_sample -= playback_distance;
	} else {
		playback_sample += playback_distance;
	}

	for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {

		(*chan).playback_buf->increment_read_ptr (playback_distance);
		
		if (adjust_capture_position) {
			(*chan).capture_buf->increment_write_ptr (adjust_capture_position);
		}
	}
	
	if (adjust_capture_position != 0) {
		capture_captured += adjust_capture_position;
		adjust_capture_position = 0;
	}
	
	if (_slaved) {
		need_butler = channels[0].playback_buf->write_space() >= channels[0].playback_buf->bufsize() / 2;
	} else {
		need_butler = channels[0].playback_buf->write_space() >= disk_io_chunk_frames
			|| channels[0].capture_buf->read_space() >= disk_io_chunk_frames;
	}

	pthread_mutex_unlock (state_lock.mutex());

	_processed = false;

	return need_butler;
}

void
DiskStream::set_pending_overwrite (bool yn)
{
	/* called from audio thread, so we can use the read ptr and playback sample as we wish */
	
	pending_overwrite = yn;

	overwrite_frame = playback_sample;
	overwrite_offset = channels.front().playback_buf->get_read_ptr();
}

int
DiskStream::overwrite_existing_buffers ()
{
 	Sample* mixdown_buffer;
 	float* gain_buffer;
	char * workbuf;
 	int ret = -1;
	bool reversed = (_visible_speed * _session.transport_speed()) < 0.0f;

	overwrite_queued = false;

	/* assume all are the same size */
	jack_nframes_t size = channels[0].playback_buf->bufsize();
	
 	mixdown_buffer = new Sample[size];
 	gain_buffer = new float[size];
	workbuf = new char[size*4];
	
	/* reduce size so that we can fill the buffer correctly. */
	size--;
	
	uint32_t n=0;
	jack_nframes_t start;

	for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan, ++n) {

		start = overwrite_frame;
		jack_nframes_t cnt = size;
		
		/* to fill the buffer without resetting the playback sample, we need to
		   do it one or two chunks (normally two).

		   |----------------------------------------------------------------------|

		                       ^
				       overwrite_offset
		    |<- second chunk->||<----------------- first chunk ------------------>|
		   
		*/
		
		jack_nframes_t to_read = size - overwrite_offset;

		if (read ((*chan).playback_buf->buffer() + overwrite_offset, mixdown_buffer, gain_buffer, workbuf,
			  start, to_read, *chan, n, reversed)) {
			error << string_compose(_("DiskStream %1: when refilling, cannot read %2 from playlist at frame %3"),
					 _id, size, playback_sample) << endmsg;
			goto out;
		}
			
		if (cnt > to_read) {

			cnt -= to_read;
		
			if (read ((*chan).playback_buf->buffer(), mixdown_buffer, gain_buffer, workbuf,
				  start, cnt, *chan, n, reversed)) {
				error << string_compose(_("DiskStream %1: when refilling, cannot read %2 from playlist at frame %3"),
						 _id, size, playback_sample) << endmsg;
				goto out;
			}
		}
	}

	ret = 0;
 
  out:
	pending_overwrite = false;
 	delete [] gain_buffer;
 	delete [] mixdown_buffer;
	delete [] workbuf;
 	return ret;
}

int
DiskStream::seek (jack_nframes_t frame, bool complete_refill)
{
	LockMonitor lm (state_lock, __LINE__, __FILE__);
	uint32_t n;
	int ret;
	ChannelList::iterator chan;

	for (n = 0, chan = channels.begin(); chan != channels.end(); ++chan, ++n) {
		(*chan).playback_buf->reset ();
		(*chan).capture_buf->reset ();
		if ((*chan).write_source) {
			(*chan).write_source->seek (frame);
		}
	}
	
	playback_sample = frame;
	file_frame = frame;

	if (complete_refill) {
		while ((ret = do_refill (0, 0, 0)) > 0);
	} else {
		ret = do_refill (0, 0, 0);
	}

	return ret;
}

int
DiskStream::can_internal_playback_seek (jack_nframes_t distance)
{
	ChannelList::iterator chan;

	for (chan = channels.begin(); chan != channels.end(); ++chan) {
		if ((*chan).playback_buf->read_space() < distance) {
			return false;
		} 
	}
	return true;
}

int
DiskStream::internal_playback_seek (jack_nframes_t distance)
{
	ChannelList::iterator chan;

	for (chan = channels.begin(); chan != channels.end(); ++chan) {
		(*chan).playback_buf->increment_read_ptr (distance);
	}

	first_recordable_frame += distance;
	playback_sample += distance;
	
	return 0;
}

int
DiskStream::read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer, char * workbuf, jack_nframes_t& start, jack_nframes_t cnt, 
		  ChannelInfo& channel_info, int channel, bool reversed)
{
	jack_nframes_t this_read = 0;
	bool reloop = false;
	jack_nframes_t loop_end = 0;
	jack_nframes_t loop_start = 0;
	jack_nframes_t loop_length = 0;
	jack_nframes_t offset = 0;
	Location *loc = 0;

	if (!reversed) {
		/* Make the use of a Location atomic for this read operation.
		   
		   Note: Locations don't get deleted, so all we care about
		   when I say "atomic" is that we are always pointing to
		   the same one and using a start/length values obtained
		   just once.
		*/
		
		if ((loc = loop_location) != 0) {
			loop_start = loc->start();
			loop_end = loc->end();
			loop_length = loop_end - loop_start;
		}
		
		/* if we are looping, ensure that the first frame we read is at the correct
		   position within the loop.
		*/
		
		if (loc && start >= loop_end) {
			//cerr << "start adjusted from " << start;
			start = loop_start + ((start - loop_start) % loop_length);
			//cerr << "to " << start << endl;
		}
		//cerr << "start is " << start << "  loopstart: " << loop_start << "  loopend: " << loop_end << endl;
	}

	while (cnt) {

		/* take any loop into account. we can't read past the end of the loop. */

		if (loc && (loop_end - start < cnt)) {
			this_read = loop_end - start;
			//cerr << "reloop true: thisread: " << this_read << "  cnt: " << cnt << endl;
			reloop = true;
		} else {
			reloop = false;
			this_read = cnt;
		}

		if (this_read == 0) {
			break;
		}

		this_read = min(cnt,this_read);

		if (_playlist->read (buf+offset, mixdown_buffer, gain_buffer, workbuf, start, this_read, channel) != this_read) {
			error << string_compose(_("DiskStream %1: cannot read %2 from playlist at frame %3"), _id, this_read, 
					 start) << endmsg;
			return -1;
		}

		_read_data_count = _playlist->read_data_count();
		
		if (reversed) {

			/* don't adjust start, since caller has already done that
			 */

			swap_by_ptr (buf, buf + this_read - 1);
			
		} else {
			
			/* if we read to the end of the loop, go back to the beginning */
			
			if (reloop) {
				start = loop_start;
			} else {
				start += this_read;
			}
		} 

		cnt -= this_read;
		offset += this_read;
	}

	return 0;
}

int
DiskStream::do_refill (Sample* mixdown_buffer, float* gain_buffer, char * workbuf)
{
	int32_t ret = 0;
	jack_nframes_t to_read;
	RingBufferNPT<Sample>::rw_vector vector;
	bool free_mixdown;
	bool free_gain;
	bool free_workbuf;
	bool reversed = (_visible_speed * _session.transport_speed()) < 0.0f;
	jack_nframes_t total_space;
	jack_nframes_t zero_fill;
	uint32_t chan_n;
	ChannelList::iterator i;
	jack_nframes_t ts;

	channels.front().playback_buf->get_write_vector (&vector);
	
	if ((total_space = vector.len[0] + vector.len[1]) == 0) {
		return 0;
	}
	
	/* if there are 2+ chunks of disk i/o possible for
	   this track, let the caller know so that it can arrange
	   for us to be called again, ASAP.
	*/
	
	if (total_space >= (_slaved?3:2) * disk_io_chunk_frames) {
		ret = 1;
	}
	
	/* if we're running close to normal speed and there isn't enough 
	   space to do disk_io_chunk_frames of I/O, then don't bother.  
	   
	   at higher speeds, just do it because the sync between butler
	   and audio thread may not be good enough.
	*/
	
	if ((total_space < disk_io_chunk_frames) && fabs (_actual_speed) < 2.0f) {
		return 0;
	}
	
	/* when slaved, don't try to get too close to the read pointer. this
	   leaves space for the buffer reversal to have something useful to
	   work with.
	*/
	
	if (_slaved && total_space < (channels.front().playback_buf->bufsize() / 2)) {
		return 0;
	}

	total_space = min (disk_io_chunk_frames, total_space);

	if (reversed) {

		if (file_frame == 0) {

			/* at start: nothing to do but fill with silence */

			for (chan_n = 0, i = channels.begin(); i != channels.end(); ++i, ++chan_n) {
					
				ChannelInfo& chan (*i);
				chan.playback_buf->get_write_vector (&vector);
				memset (vector.buf[0], 0, sizeof(Sample) * vector.len[0]);
				if (vector.len[1]) {
					memset (vector.buf[1], 0, sizeof(Sample) * vector.len[1]);
				}
				chan.playback_buf->increment_write_ptr (vector.len[0] + vector.len[1]);
			}
			return 0;
		}

		if (file_frame < total_space) {

			/* too close to the start: read what we can, 
			   and then zero fill the rest 
			*/

			zero_fill = total_space - file_frame;
			total_space = file_frame;
			file_frame = 0;

		} else {
			
			/* move read position backwards because we are going
			   to reverse the data.
			*/
			
			file_frame -= total_space;
			zero_fill = 0;
		}

	} else {

		if (file_frame == max_frames) {

			/* at end: nothing to do but fill with silence */
			
			for (chan_n = 0, i = channels.begin(); i != channels.end(); ++i, ++chan_n) {
					
				ChannelInfo& chan (*i);
				chan.playback_buf->get_write_vector (&vector);
				memset (vector.buf[0], 0, sizeof(Sample) * vector.len[0]);
				if (vector.len[1]) {
					memset (vector.buf[1], 0, sizeof(Sample) * vector.len[1]);
				}
				chan.playback_buf->increment_write_ptr (vector.len[0] + vector.len[1]);
			}
			return 0;
		}
		
		if (file_frame > max_frames - total_space) {

			/* to close to the end: read what we can, and zero fill the rest */

			zero_fill = total_space - (max_frames - file_frame);
			total_space = max_frames - file_frame;

		} else {
			zero_fill = 0;
		}
	}

	/* Please note: the code to allocate buffers isn't run
	   during normal butler thread operation. Its there
	   for other times when we need to call do_refill()
	   from somewhere other than the butler thread.
	*/

	if (mixdown_buffer == 0) {
		mixdown_buffer = new Sample[disk_io_chunk_frames];
		free_mixdown = true;
	} else {
		free_mixdown = false;
	}

	if (gain_buffer == 0) {
		gain_buffer = new float[disk_io_chunk_frames];
		free_gain = true;
	} else {
		free_gain = false;
	}

	if (workbuf == 0) {
		workbuf = new char[disk_io_chunk_frames * 4];
		free_workbuf = true;
	} else {
		free_workbuf = false;
	}
	
	jack_nframes_t file_frame_tmp = 0;

	for (chan_n = 0, i = channels.begin(); i != channels.end(); ++i, ++chan_n) {

		ChannelInfo& chan (*i);
		Sample* buf1;
		Sample* buf2;
		jack_nframes_t len1, len2;

		chan.playback_buf->get_write_vector (&vector);

		ts = total_space;
		file_frame_tmp = file_frame;

		if (reversed) {
			buf1 = vector.buf[1];
			len1 = vector.len[1];
			buf2 = vector.buf[0];
			len2 = vector.len[0];
		} else {
			buf1 = vector.buf[0];
			len1 = vector.len[0];
			buf2 = vector.buf[1];
			len2 = vector.len[1];
		}


		to_read = min (ts, len1);
		to_read = min (to_read, disk_io_chunk_frames);

		if (to_read) {

			if (read (buf1, mixdown_buffer, gain_buffer, workbuf, file_frame_tmp, to_read, chan, chan_n, reversed)) {
				ret = -1;
				goto out;
			}
			
			chan.playback_buf->increment_write_ptr (to_read);
			ts -= to_read;
		}

		to_read = min (ts, len2);

		if (to_read) {

			
			/* we read all of vector.len[0], but it wasn't an entire disk_io_chunk_frames of data,
			   so read some or all of vector.len[1] as well.
			*/

			if (read (buf2, mixdown_buffer, gain_buffer, workbuf, file_frame_tmp, to_read, chan, chan_n, reversed)) {
				ret = -1;
				goto out;
			}
		
			chan.playback_buf->increment_write_ptr (to_read);
		}

		if (zero_fill) {
			/* do something */
		}

	}
	
	file_frame = file_frame_tmp;

  out:
	if (free_mixdown) {
		delete [] mixdown_buffer;
	}
	if (free_gain) {
		delete [] gain_buffer;
	}
	if (free_workbuf) {
		delete [] workbuf;
	}

	return ret;
}	

int
DiskStream::do_flush (char * workbuf, bool force_flush)
{
	uint32_t to_write;
	int32_t ret = 0;
	RingBufferNPT<Sample>::rw_vector vector;
	RingBufferNPT<CaptureTransition>::rw_vector transvec;
	jack_nframes_t total;
	
	/* important note: this function will write *AT MOST* 
	   disk_io_chunk_frames of data to disk. it will never 
	   write more than that. if its writes that much and there 
	   is more than that waiting to be written, it will return 1,
	   otherwise 0 on success or -1 on failure.

	   if there is less than disk_io_chunk_frames to be written, 
	   no data will be written at all unless `force_flush' is true.  
	*/

	_write_data_count = 0;

	for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
	
		(*chan).capture_buf->get_read_vector (&vector);

		total = vector.len[0] + vector.len[1];

		
		if (total == 0 || (total < disk_io_chunk_frames && !force_flush && was_recording)) {
			goto out;
		}

		
		/* if there are 2+ chunks of disk i/o possible for
		   this track, let the caller know so that it can arrange
		   for us to be called again, ASAP.
		   
		   if we are forcing a flush, then if there is* any* extra
		   work, let the caller know.

		   if we are no longer recording and there is any extra work,
		   let the caller know too.
		*/

		if (total >= 2 * disk_io_chunk_frames || ((force_flush || !was_recording) && total > disk_io_chunk_frames)) {
			ret = 1;
		} 

		to_write = min (disk_io_chunk_frames, (jack_nframes_t) vector.len[0]);
		
		
		// check the transition buffer when recording destructive
		// important that we get this after the capture buf

		if (destructive()) {
			(*chan).capture_transition_buf->get_read_vector(&transvec);
			size_t transcount = transvec.len[0] + transvec.len[1];
			bool have_start = false;
			size_t ti;

			for (ti=0; ti < transcount; ++ti) {
				CaptureTransition & captrans = (ti < transvec.len[0]) ? transvec.buf[0][ti] : transvec.buf[1][ti-transvec.len[0]];
				
				if (captrans.type == CaptureStart) {
					// by definition, the first data we got above represents the given capture pos

					(*chan).write_source->mark_capture_start (captrans.capture_val);
					(*chan).curr_capture_cnt = 0;

					have_start = true;
				}
				else if (captrans.type == CaptureEnd) {

					// capture end, the capture_val represents total frames in capture

					if (captrans.capture_val <= (*chan).curr_capture_cnt + to_write) {

						// shorten to make the write a perfect fit
						uint32_t nto_write = (captrans.capture_val - (*chan).curr_capture_cnt); 

						if (nto_write < to_write) {
							ret = 1; // should we?
						}
						to_write = nto_write;

						(*chan).write_source->mark_capture_end ();
						
						// increment past this transition, but go no further
						++ti;
						break;
					}
					else {
						// actually ends just beyond this chunk, so force more work
						ret = 1;
						break;
					}
				}
			}

			if (ti > 0) {
				(*chan).capture_transition_buf->increment_read_ptr(ti);
			}
		}


		
		if ((!(*chan).write_source) || (*chan).write_source->write (vector.buf[0], to_write, workbuf) != to_write) {
			error << string_compose(_("DiskStream %1: cannot write to disk"), _id) << endmsg;
			return -1;
		}

		(*chan).capture_buf->increment_read_ptr (to_write);
		(*chan).curr_capture_cnt += to_write;
		
		if ((to_write == vector.len[0]) && (total > to_write) && (to_write < disk_io_chunk_frames) && !destructive()) {
		
			/* we wrote all of vector.len[0] but it wasn't an entire
			   disk_io_chunk_frames of data, so arrange for some part 
			   of vector.len[1] to be flushed to disk as well.
			*/
		
			to_write = min ((jack_nframes_t)(disk_io_chunk_frames - to_write), (jack_nframes_t) vector.len[1]);
		
			if ((*chan).write_source->write (vector.buf[1], to_write, workbuf) != to_write) {
				error << string_compose(_("DiskStream %1: cannot write to disk"), _id) << endmsg;
				return -1;
			}

			_write_data_count += (*chan).write_source->write_data_count();
	
			(*chan).capture_buf->increment_read_ptr (to_write);
			(*chan).curr_capture_cnt += to_write;
		}
	}

  out:
	return ret;
}

void
DiskStream::playlist_changed (Change ignored)
{
	playlist_modified ();
}

void
DiskStream::playlist_modified ()
{
	if (!i_am_the_modifier && !overwrite_queued) {
		_session.request_overwrite_buffer (this);
		overwrite_queued = true;
	} 
}

void
DiskStream::transport_stopped (struct tm& when, time_t twhen, bool abort_capture)
{
	uint32_t buffer_position;
	bool more_work = true;
	int err = 0;
	AudioRegion* region = 0;
	jack_nframes_t total_capture;
	AudioRegion::SourceList srcs;
	AudioRegion::SourceList::iterator src;
	ChannelList::iterator chan;
	vector<CaptureInfo*>::iterator ci;
	uint32_t n = 0; 
	list<Source*>* deletion_list;
	bool mark_write_completed = false;

	finish_capture (true);

	/* butler is already stopped, but there may be work to do 
	   to flush remaining data to disk.
	*/

	while (more_work && !err) {
		switch (do_flush ( _session.conversion_buffer(Session::TransportContext), true)) {
		case 0:
			more_work = false;
			break;
		case 1:
			break;
		case -1:
			error << string_compose(_("DiskStream \"%1\": cannot flush captured data to disk!"), _name) << endmsg;
			err++;
		}
	}

	/* XXX is there anything we can do if err != 0 ? */
	LockMonitor lm (capture_info_lock, __LINE__, __FILE__);
	
	if (capture_info.empty()) {
		return;
	}

	if (abort_capture) {
		
		ChannelList::iterator chan;
		
		deletion_list = new list<Source*>;

		for ( chan = channels.begin(); chan != channels.end(); ++chan) {

			if ((*chan).write_source) {
				
				(*chan).write_source->mark_for_remove ();
				(*chan).write_source->release ();
				
				deletion_list->push_back ((*chan).write_source);

				(*chan).write_source = 0;
			}
			
			/* new source set up in "out" below */
		}
		
		if (!deletion_list->empty()) {
			DeleteSources (deletion_list);
		} else {
			delete deletion_list;
		}

		goto out;
	} 

	for (total_capture = 0, ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		total_capture += (*ci)->frames;
	}

	/* figure out the name for this take */

	for (n = 0, chan = channels.begin(); chan != channels.end(); ++chan, ++n) {

		Source* s = (*chan).write_source;
		
		if (s) {

			FileSource* fsrc;

			srcs.push_back (s);

			if ((fsrc = dynamic_cast<FileSource *>(s)) != 0) {
				fsrc->update_header (capture_info.front()->start, when, twhen);
			}

			s->set_captured_for (_name);
			
		}
	}

	/* destructive tracks have a single, never changing region */

	if (destructive()) {

		/* send a signal that any UI can pick up to do the right thing. there is 
		   a small problem here in that a UI may need the peak data to be ready
		   for the data that was recorded and this isn't interlocked with that
		   process. this problem is deferred to the UI.
		 */
		
		_playlist->Modified();

	} else {

		/* Register a new region with the Session that
		   describes the entire source. Do this first
		   so that any sub-regions will obviously be
		   children of this one (later!)
		*/
		
		try {
			region = new AudioRegion (srcs, channels[0].write_source->last_capture_start_frame(), total_capture, 
						  region_name_from_path (channels[0].write_source->name()), 
						  0, AudioRegion::Flag (AudioRegion::DefaultFlags|AudioRegion::Automatic|AudioRegion::WholeFile));
			
			region->special_set_position (capture_info.front()->start);
		}
		
		
		catch (failed_constructor& err) {
			error << string_compose(_("%1: could not create region for complete audio file"), _name) << endmsg;
			/* XXX what now? */
		}
		
		_last_capture_regions.push_back (region);

		// cerr << _name << ": there are " << capture_info.size() << " capture_info records\n";
		
		_session.add_undo (_playlist->get_memento());
		_playlist->freeze ();
		
		for (buffer_position = channels[0].write_source->last_capture_start_frame(), ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
			
			string region_name;
			_session.region_name (region_name, _name, false);
			
			// cerr << _name << ": based on ci of " << (*ci)->start << " for " << (*ci)->frames << " add a region\n";
			
			try {
				region = new AudioRegion (srcs, buffer_position, (*ci)->frames, region_name);
			}
			
			catch (failed_constructor& err) {
				error << _("DiskStream: could not create region for captured audio!") << endmsg;
				continue; /* XXX is this OK? */
			}
			
			_last_capture_regions.push_back (region);
			
			// cerr << "add new region, buffer position = " << buffer_position << " @ " << (*ci)->start << endl;
			
			i_am_the_modifier++;
			_playlist->add_region (*region, (*ci)->start);
			i_am_the_modifier--;
			
			buffer_position += (*ci)->frames;
		}

		_playlist->thaw ();
		_session.add_redo_no_execute (_playlist->get_memento());
	}

	mark_write_completed = true;

	reset_write_sources (mark_write_completed);

  out:
	for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		delete *ci;
	}

	capture_info.clear ();
	capture_start_frame = 0;
}

void
DiskStream::finish_capture (bool rec_monitors_input)
{
	was_recording = false;
	
	if (capture_captured == 0) {
		return;
	}

	if (recordable() && destructive()) {
		for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
			
			RingBufferNPT<CaptureTransition>::rw_vector transvec;
			(*chan).capture_transition_buf->get_write_vector(&transvec);
			
			
			if (transvec.len[0] > 0) {
				transvec.buf[0]->type = CaptureEnd;
				transvec.buf[0]->capture_val = capture_captured;
				(*chan).capture_transition_buf->increment_write_ptr(1);
			}
			else {
				// bad!
				fatal << string_compose (_("programmer error: %1"), X_("capture_transition_buf is full when stopping record!  inconceivable!")) << endmsg;
			}
		}
	}
	
	
	CaptureInfo* ci = new CaptureInfo;
	
	ci->start =  capture_start_frame;
	ci->frames = capture_captured;
	
	/* XXX theoretical race condition here. Need atomic exchange ? 
	   However, the circumstances when this is called right 
	   now (either on record-disable or transport_stopped)
	   mean that no actual race exists. I think ...
	   We now have a capture_info_lock, but it is only to be used
	   to synchronize in the transport_stop and the capture info
	   accessors, so that invalidation will not occur (both non-realtime).
	*/

	// cerr << "Finish capture, add new CI, " << ci->start << '+' << ci->frames << endl;

	capture_info.push_back (ci);
	capture_captured = 0;
}

void
DiskStream::set_record_enabled (bool yn, void* src)
{
        bool rolling = _session.transport_speed() != 0.0f;

	if (!recordable() || !_session.record_enabling_legal()) {
		return;
	}
	
	/* if we're turning on rec-enable, there needs to be an
	   input connection.
	 */

	if (yn && channels[0].source == 0) {

		/* pick up connections not initiated *from* the IO object
		   we're associated with.
		*/

		get_input_sources ();

		if (channels[0].source == 0) {
		
			if (yn) {
				CannotRecordNoInput (this); /* emit signal */
			}
			return;
		}
	}

	/* yes, i know that this not proof against race conditions, but its
	   good enough. i think.
	*/

	if (record_enabled() != yn) {
		if (yn) {
			atomic_set (&_record_enabled, 1);
			capturing_sources.clear ();
			if (Config->get_use_hardware_monitoring())  {
				for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
					if ((*chan).source) {
						(*chan).source->request_monitor_input (!(_session.get_auto_input() && rolling));
					}
					capturing_sources.push_back ((*chan).write_source);
				}
			} else {
				for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
					capturing_sources.push_back ((*chan).write_source);
				}
			}

		} else {
			atomic_set (&_record_enabled, 0);
			if (Config->get_use_hardware_monitoring()) {
				for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
					if ((*chan).source) {
						(*chan).source->request_monitor_input (false);
					}
				}
			}
			capturing_sources.clear ();
		}

		record_enable_changed (src); /* EMIT SIGNAL */
	}
}

XMLNode&
DiskStream::get_state ()
{
	XMLNode* node = new XMLNode ("DiskStream");
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	snprintf (buf, sizeof(buf), "%zd", channels.size());
	node->add_property ("channels", buf);

	node->add_property ("playlist", _playlist->name());
	
	snprintf (buf, sizeof(buf), "%f", _visible_speed);
	node->add_property ("speed", buf);

	node->add_property("name", _name);
	snprintf (buf, sizeof(buf), "%" PRIu64, id());
	node->add_property("id", buf);

	if (!capturing_sources.empty() && _session.get_record_enabled()) {

		XMLNode* cs_child = new XMLNode (X_("CapturingSources"));
		XMLNode* cs_grandchild;

		for (vector<FileSource*>::iterator i = capturing_sources.begin(); i != capturing_sources.end(); ++i) {
			cs_grandchild = new XMLNode (X_("file"));
			cs_grandchild->add_property (X_("path"), (*i)->path());
			cs_child->add_child_nocopy (*cs_grandchild);
		}

		/* store the location where capture will start */

		Location* pi;

		if (_session.get_punch_in() && ((pi = _session.locations()->auto_punch_location()) != 0)) {
			snprintf (buf, sizeof (buf), "%" PRIu32, pi->start());
		} else {
			snprintf (buf, sizeof (buf), "%" PRIu32, _session.transport_frame());
		}

		cs_child->add_property (X_("at"), buf);
		node->add_child_nocopy (*cs_child);
	}

	if (_extra_xml) {
		node->add_child_copy (*_extra_xml);
	}

	return* node;
}

int
DiskStream::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	uint32_t nchans = 1;
	XMLNode* capture_pending_node = 0;
	LocaleGuard lg (X_("POSIX"));

	in_set_state = true;

 	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
 		if ((*niter)->name() == IO::state_node_name) {
			deprecated_io_node = new XMLNode (**niter);
 		}

		if ((*niter)->name() == X_("CapturingSources")) {
			capture_pending_node = *niter;
		}
 	}

	/* prevent write sources from being created */
	
	in_set_state = true;
	
	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	} 

	if (deprecated_io_node) {
		if ((prop = deprecated_io_node->property ("id")) != 0) {
			sscanf (prop->value().c_str(), "%" PRIu64, &_id);
		}
	} else {
		if ((prop = node.property ("id")) != 0) {
			sscanf (prop->value().c_str(), "%" PRIu64, &_id);
		}
	}

	if ((prop = node.property ("channels")) != 0) {
		nchans = atoi (prop->value().c_str());
	}
	
	// create necessary extra channels
	// we are always constructed with one
	// and we always need one

	if (nchans > _n_channels) {

		// we need to add new channel infos
		//LockMonitor lm (state_lock, __LINE__, __FILE__);

		int diff = nchans - channels.size();

		for (int i=0; i < diff; ++i) {
			add_channel ();
		}

	} else if (nchans < _n_channels) {

		// we need to get rid of channels
		//LockMonitor lm (state_lock, __LINE__, __FILE__);

		int diff = channels.size() - nchans;
		
		for (int i = 0; i < diff; ++i) {
			remove_channel ();
		}
	}

	if ((prop = node.property ("playlist")) == 0) {
		return -1;
	}

	{
		bool had_playlist = (_playlist != 0);
	
		if (find_and_use_playlist (prop->value())) {
			return -1;
		}

		if (!had_playlist) {
			_playlist->set_orig_diskstream_id (_id);
		}

		if (capture_pending_node) {
			use_pending_capture_data (*capture_pending_node);
		}

	}

	if ((prop = node.property ("speed")) != 0) {
		double sp = atof (prop->value().c_str());

		if (realtime_set_speed (sp, false)) {
			non_realtime_set_speed ();
		}
	}

	_n_channels = channels.size();

	in_set_state = false;

	/* make sure this is clear before we do anything else */

	capturing_sources.clear ();

	/* write sources are handled elsewhere; 
  	      for destructive tracks: in {setup,use}_destructive_playlist()
	      for non-destructive: when we handle the input set up of the IO that owns this DS
	*/
		
	in_set_state = false;

	return 0;
}

int
DiskStream::use_new_write_source (uint32_t n)
{
	if (!recordable()) {
		return 1;
	}

	if (n >= channels.size()) {
		error << string_compose (_("DiskStream: channel %1 out of range"), n) << endmsg;
		return -1;
	}

	ChannelInfo &chan = channels[n];
	
	if (chan.write_source) {

		if (FileSource::is_empty (chan.write_source->path())) {
			chan.write_source->mark_for_remove ();
			chan.write_source->release();
			delete chan.write_source;
		} else {
			chan.write_source->release();
			chan.write_source = 0;
		}
	}

	try {
		if ((chan.write_source = _session.create_file_source (*this, n, destructive())) == 0) {
			throw failed_constructor();
		}
	} 

	catch (failed_constructor &err) {
		error << string_compose (_("%1:%2 new capture file not initialized correctly"), _name, n) << endmsg;
		chan.write_source = 0;
		return -1;
	}

	chan.write_source->use ();

	return 0;
}

void
DiskStream::reset_write_sources (bool mark_write_complete, bool force)
{
	ChannelList::iterator chan;
	uint32_t n;

	if (!recordable()) {
		return;
	}
	
	if (!force && destructive()) {

		/* make sure we always have enough sources for the current channel count */

		for (chan = channels.begin(), n = 0; chan != channels.end(); ++chan, ++n) {
			if ((*chan).write_source == 0) {
				break;
			}
		}

		if (chan == channels.end()) {
			return;
		}

		/* some channels do not have a write source */
	}

	capturing_sources.clear ();
	
	for (chan = channels.begin(), n = 0; chan != channels.end(); ++chan, ++n) {
		if ((*chan).write_source && mark_write_complete) {
			(*chan).write_source->mark_streaming_write_completed ();
		}
		use_new_write_source (n);
		if (record_enabled()) {
			capturing_sources.push_back ((*chan).write_source);
		}
	}
}

void
DiskStream::set_block_size (jack_nframes_t nframes)
{
	if (_session.get_block_size() > speed_buffer_size) {
		speed_buffer_size = _session.get_block_size();

		for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
			if ((*chan).speed_buffer) delete [] (*chan).speed_buffer;
			(*chan).speed_buffer = new Sample[speed_buffer_size];
		}
	}
	allocate_temporary_buffers ();
}

void
DiskStream::allocate_temporary_buffers ()
{
	/* make sure the wrap buffer is at least large enough to deal
	   with the speeds up to 1.2, to allow for micro-variation
	   when slaving to MTC, SMPTE etc.
	*/

	double sp = max (fabsf (_actual_speed), 1.2f);
	jack_nframes_t required_wrap_size = (jack_nframes_t) floor (_session.get_block_size() * sp) + 1;

	if (required_wrap_size > wrap_buffer_size) {

		for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
			if ((*chan).playback_wrap_buffer) delete [] (*chan).playback_wrap_buffer;
			(*chan).playback_wrap_buffer = new Sample[required_wrap_size];	
			if ((*chan).capture_wrap_buffer) delete [] (*chan).capture_wrap_buffer;
			(*chan).capture_wrap_buffer = new Sample[required_wrap_size];	
		}

		wrap_buffer_size = required_wrap_size;
	}
}

void
DiskStream::monitor_input (bool yn)
{
	for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
		
		if ((*chan).source) {
			(*chan).source->request_monitor_input (yn);
		}
	}
}

void
DiskStream::set_capture_offset ()
{
	if (_io == 0) {
		/* can't capture, so forget it */
		return;
	}

	_capture_offset = _io->input_latency();
}

void
DiskStream::set_persistent_align_style (AlignStyle a)
{
	_persistent_alignment_style = a;
}

void
DiskStream::set_align_style_from_io ()
{
	bool have_physical = false;

	if (_io == 0) {
		return;
	}

	get_input_sources ();
	
	for (ChannelList::iterator chan = channels.begin(); chan != channels.end(); ++chan) {
		if ((*chan).source && (*chan).source->flags() & JackPortIsPhysical) {
			have_physical = true;
			break;
		}
	}

	if (have_physical) {
		set_align_style (ExistingMaterial);
	} else {
		set_align_style (CaptureTime);
	}
}

void
DiskStream::set_align_style (AlignStyle a)
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
DiskStream::add_channel ()
{
	/* XXX need to take lock??? */

	ChannelInfo chan;

	init_channel (chan);

	chan.speed_buffer = new Sample[speed_buffer_size];
	chan.playback_wrap_buffer = new Sample[wrap_buffer_size];
	chan.capture_wrap_buffer = new Sample[wrap_buffer_size];

	channels.push_back (chan);

	_n_channels = channels.size();

	return 0;
}

int
DiskStream::remove_channel ()
{
	if (channels.size() > 1) {
		/* XXX need to take lock??? */
		ChannelInfo & chan = channels.back();
		destroy_channel (chan);
		channels.pop_back();

		_n_channels = channels.size();
		return 0;
	}

	return -1;
}

float
DiskStream::playback_buffer_load () const
{
	return (float) ((double) channels.front().playback_buf->read_space()/
			(double) channels.front().playback_buf->bufsize());
}

float
DiskStream::capture_buffer_load () const
{
	return (float) ((double) channels.front().capture_buf->write_space()/
			(double) channels.front().capture_buf->bufsize());
}

int
DiskStream::set_loop (Location *location)
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
DiskStream::get_capture_start_frame (uint32_t n)
{
	LockMonitor lm (capture_info_lock, __LINE__, __FILE__);

	if (capture_info.size() > n) {
		return capture_info[n]->start;
	}
	else {
		return capture_start_frame;
	}
}

jack_nframes_t
DiskStream::get_captured_frames (uint32_t n)
{
	LockMonitor lm (capture_info_lock, __LINE__, __FILE__);

	if (capture_info.size() > n) {
		return capture_info[n]->frames;
	}
	else {
		return capture_captured;
	}
}

void
DiskStream::punch_in ()
{
}

void
DiskStream::punch_out ()
{
}

int
DiskStream::use_pending_capture_data (XMLNode& node)
{
	const XMLProperty* prop;
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	FileSource* fs;
	FileSource* first_fs = 0;
	AudioRegion::SourceList pending_sources;
	jack_nframes_t position;

	if ((prop = node.property (X_("at"))) == 0) {
		return -1;
	}

	if (sscanf (prop->value().c_str(), "%" PRIu32, &position) != 1) {
		return -1;
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == X_("file")) {

			if ((prop = (*niter)->property (X_("path"))) == 0) {
				continue;
			}

			try {
				fs = new FileSource (prop->value(), _session.frame_rate(), true);
			}

			catch (failed_constructor& err) {
				error << string_compose (_("%1: cannot restore pending capture source file %2"),
						  _name, prop->value())
				      << endmsg;
				return -1;
			}

			pending_sources.push_back (fs);
			
			if (first_fs == 0) {
				first_fs = fs;
			}

			fs->set_captured_for (_name);
		}
	}

	if (pending_sources.size() == 0) {
		/* nothing can be done */
		return 1;
	}

	if (pending_sources.size() != _n_channels) {
		error << string_compose (_("%1: incorrect number of pending sources listed - ignoring them all"), _name)
		      << endmsg;
		return -1;
	}

	AudioRegion* region;
	
	try {
		region = new AudioRegion (pending_sources, 0, first_fs->length(),
					  region_name_from_path (first_fs->name()), 
					  0, AudioRegion::Flag (AudioRegion::DefaultFlags|AudioRegion::Automatic|AudioRegion::WholeFile));
		
		region->special_set_position (0);
	}

	catch (failed_constructor& err) {
		error << string_compose (_("%1: cannot create whole-file region from pending capture sources"),
				  _name)
		      << endmsg;
		
		return -1;
	}

	try {
		region = new AudioRegion (pending_sources, 0, first_fs->length(), region_name_from_path (first_fs->name()));
	}

	catch (failed_constructor& err) {
		error << string_compose (_("%1: cannot create region from pending capture sources"),
				  _name)
		      << endmsg;
		
		return -1;
	}

	_playlist->add_region (*region, position);

	return 0;
}

void
DiskStream::set_roll_delay (jack_nframes_t nframes)
{
	_roll_delay = nframes;
}

void
DiskStream::set_destructive (bool yn)
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
