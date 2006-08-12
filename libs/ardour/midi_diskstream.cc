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
#include <ardour/midi_diskstream.h>
#include <ardour/utils.h>
#include <ardour/configuration.h>
#include <ardour/smf_source.h>
#include <ardour/destructive_filesource.h>
#include <ardour/send.h>
#include <ardour/midi_playlist.h>
#include <ardour/cycle_timer.h>
#include <ardour/midi_region.h>
#include <ardour/midi_port.h>

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

MidiDiskstream::MidiDiskstream (Session &sess, const string &name, Diskstream::Flag flag)
	: Diskstream(sess, name, flag)
	, _playback_buf(0)
	, _capture_buf(0)
	, _current_playback_buffer(0)
	, _current_capture_buffer(0)
	, _playback_wrap_buffer(0)
	, _capture_wrap_buffer(0)
	, _source_port(0)
	, _write_source(0)
	, _capture_transition_buf(0)
{
	/* prevent any write sources from being created */

	in_set_state = true;

	init(flag);
	use_new_playlist ();

	in_set_state = false;

	DiskstreamCreated (this); /* EMIT SIGNAL */
}
	
MidiDiskstream::MidiDiskstream (Session& sess, const XMLNode& node)
	: Diskstream(sess, node)
	, _playback_buf(0)
	, _capture_buf(0)
	, _current_playback_buffer(0)
	, _current_capture_buffer(0)
	, _playback_wrap_buffer(0)
	, _capture_wrap_buffer(0)
	, _source_port(0)
	, _write_source(0)
	, _capture_transition_buf(0)
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

	DiskstreamCreated (this); /* EMIT SIGNAL */
}

void
MidiDiskstream::init (Diskstream::Flag f)
{
	Diskstream::init(f);

	/* there are no channels at this point, so these
	   two calls just get speed_buffer_size and wrap_buffer
	   size setup without duplicating their code.
	*/

	set_block_size (_session.get_block_size());
	allocate_temporary_buffers ();

	_playback_wrap_buffer = new RawMidi[wrap_buffer_size];
	_capture_wrap_buffer = new RawMidi[wrap_buffer_size];
	_playback_buf = new RingBufferNPT<RawMidi> (_session.diskstream_buffer_size());
	_capture_buf = new RingBufferNPT<RawMidi> (_session.diskstream_buffer_size());
	_capture_transition_buf = new RingBufferNPT<CaptureTransition> (128);
	
	_n_channels = ChanCount(DataType::MIDI, 1);
}

MidiDiskstream::~MidiDiskstream ()
{
	Glib::Mutex::Lock lm (state_lock);
}

void
MidiDiskstream::non_realtime_input_change ()
{
	{ 
		Glib::Mutex::Lock lm (state_lock);

		if (input_change_pending == NoChange) {
			return;
		}

		if (input_change_pending & ConfigurationChanged) {

			assert(_io->n_inputs() == _n_channels);
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
MidiDiskstream::get_input_sources ()
{
#if 0
	if (_io->n_inputs() == 0) {
		cerr << "MidiDiskstream NO INPUTS?\n";
		return;
	} else {
		cerr << "INPUTS!\n";
	}

	// FIXME this is weird and really different from AudioDiskstream
	
	assert(_io->n_inputs() == 1);
	assert(_io->midi_input(0));
	_source_port = _io->midi_input(0);

	const char **connections = _io->input(0)->get_connections ();

	if (connections == 0 || connections[0] == 0) {

		if (_source_port) {
			// _source_port->disable_metering ();
		}

		_source_port = 0;

	} else {
		_source_port = dynamic_cast<MidiPort*>(
			_session.engine().get_port_by_name (connections[0]));
		assert(_source_port);
	}

	if (_source_port) {
		cerr << "SOURCE PORT!\n";
	} else {
		cerr << "NO SOURCE PORT?!\n";
	}

	if (connections) {
		free (connections);
	}
#endif
}		

int
MidiDiskstream::find_and_use_playlist (const string& name)
{
	Playlist* pl;
	MidiPlaylist* playlist;
		
	if ((pl = _session.playlist_by_name (name)) == 0) {
		playlist = new MidiPlaylist(_session, name);
		pl = playlist;
	}

	if ((playlist = dynamic_cast<MidiPlaylist*> (pl)) == 0) {
		error << string_compose(_("MidiDiskstream: Playlist \"%1\" isn't a midi playlist"), name) << endmsg;
		return -1;
	}

	return use_playlist (playlist);
}

int
MidiDiskstream::use_playlist (Playlist* playlist)
{
	assert(dynamic_cast<MidiPlaylist*>(playlist));

	return Diskstream::use_playlist(playlist);
}

int
MidiDiskstream::use_new_playlist ()
{
	string newname;
	MidiPlaylist* playlist;

	if (!in_set_state && destructive()) {
		return 0;
	}

	if (_playlist) {
		newname = Playlist::bump_name (_playlist->name(), _session);
	} else {
		newname = Playlist::bump_name (_name, _session);
	}

	if ((playlist = new MidiPlaylist (_session, newname, hidden())) != 0) {
		playlist->set_orig_diskstream_id (id());
		return use_playlist (playlist);
	} else { 
		return -1;
	}
}

int
MidiDiskstream::use_copy_playlist ()
{
	if (destructive()) {
		return 0;
	}

	if (_playlist == 0) {
		error << string_compose(_("MidiDiskstream %1: there is no existing playlist to make a copy of!"), _name) << endmsg;
		return -1;
	}

	string newname;
	MidiPlaylist* playlist;

	newname = Playlist::bump_name (_playlist->name(), _session);
	
	if ((playlist  = new MidiPlaylist (*midi_playlist(), newname)) != 0) {
		playlist->set_orig_diskstream_id (id());
		return use_playlist (playlist);
	} else { 
		return -1;
	}
}

void
MidiDiskstream::setup_destructive_playlist ()
{
	Region::SourceList srcs;

	srcs.push_back (_write_source);
	/* a single full-sized region */

	cerr << "Setup MIDI DS using " << srcs.front()->natural_position () << endl;

	MidiRegion* region = new MidiRegion (srcs, 0, max_frames, _name);
	_playlist->add_region (*region, srcs.front()->natural_position());		
}

void
MidiDiskstream::use_destructive_playlist ()
{
	/* use the sources associated with the single full-extent region */
	
	Playlist::RegionList* rl = _playlist->regions_at (0);

	if (rl->empty()) {
		reset_write_sources (false, true);
		return;
	}

	MidiRegion* region = dynamic_cast<MidiRegion*> (rl->front());

	if (region == 0) {
		throw failed_constructor();
	}

	delete rl;

	assert(region->n_channels() == 1);
	_write_source = dynamic_cast<SMFSource*>(&region->source (0));
	assert(_write_source);
	_write_source->set_allow_remove_if_empty (false);

	/* the source list will never be reset for a destructive track */
}

void
MidiDiskstream::check_record_status (jack_nframes_t transport_frame, jack_nframes_t nframes, bool can_record)
{
	// FIXME: waaay too much code to duplicate (AudioDiskstream)
	
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
			RingBufferNPT<CaptureTransition>::rw_vector transvec;
			_capture_transition_buf->get_write_vector(&transvec);

			if (transvec.len[0] > 0) {
				transvec.buf[0]->type = CaptureStart;
				transvec.buf[0]->capture_val = capture_start_frame;
				_capture_transition_buf->increment_write_ptr(1);
			} else {
				// bad!
				fatal << X_("programming error: capture_transition_buf is full on rec start!  inconceivable!") 
					<< endmsg;
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
MidiDiskstream::process (jack_nframes_t transport_frame, jack_nframes_t nframes, jack_nframes_t offset, bool can_record, bool rec_monitors_input)
{
	// FIXME: waay too much code to duplicate (AudioDiskstream::process)
	int            ret = -1;
	jack_nframes_t rec_offset = 0;
	jack_nframes_t rec_nframes = 0;
	bool           nominally_recording;
	bool           re = record_enabled ();
	bool           collect_playback = false;
	
	_current_capture_buffer = 0;
	_current_playback_buffer = 0;

	/* if we've already processed the frames corresponding to this call,
	   just return. this allows multiple routes that are taking input
	   from this diskstream to call our ::process() method, but have
	   this stuff only happen once. more commonly, it allows both
	   the AudioTrack that is using this AudioDiskstream *and* the Session
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

	/* This lock is held until the end of AudioDiskstream::commit, so these two functions
	   must always be called as a pair. The only exception is if this function
	   returns a non-zero value, in which case, ::commit should not be called.
	*/

	// If we can't take the state lock return.
	if (!state_lock.trylock()) {
		return 1;
	}
	
	adjust_capture_position = 0;

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
		_capture_buf->get_write_vector (&_capture_vector);

		if (rec_nframes <= _capture_vector.len[0]) {

			_current_capture_buffer = _capture_vector.buf[0];

			/* note: grab the entire port buffer, but only copy what we were supposed to for recording, and use
			   rec_offset
			   */

			// FIXME: midi buffer size?

			// FIXME: reading from a MIDI port is different, can't just memcpy
			//memcpy (_current_capture_buffer, _io->input(0)->get_buffer (rec_nframes) + offset + rec_offset, sizeof (RawMidi) * rec_nframes);
			assert(_source_port);
			for (size_t i=0; i < _source_port->size(); ++i) {
				cerr << "DISKSTREAM GOT EVENT " << i << "!!\n";
			}

			if (_source_port->size() == 0)
				cerr << "No events :/ (1)\n";


		} else {

			jack_nframes_t total = _capture_vector.len[0] + _capture_vector.len[1];

			if (rec_nframes > total) {
				cerr << "DiskOverrun\n";
				//DiskOverrun (); // FIXME
				goto out;
			}

			// FIXME (see above)
			//RawMidi* buf = _io->input (0)->get_buffer (nframes) + offset;
			assert(_source_port);
			for (size_t i=0; i < _source_port->size(); ++i) {
				cerr << "DISKSTREAM GOT EVENT " << i << "!!\n";
			}
			if (_source_port->size() == 0)
				cerr << "No events :/ (2)\n";
			RawMidi* buf = NULL; // FIXME FIXME FIXME (make it compile)
			assert(false);
			jack_nframes_t first = _capture_vector.len[0];

			memcpy (_capture_wrap_buffer, buf, sizeof (RawMidi) * first);
			memcpy (_capture_vector.buf[0], buf, sizeof (RawMidi) * first);
			memcpy (_capture_wrap_buffer+first, buf + first, sizeof (RawMidi) * (rec_nframes - first));
			memcpy (_capture_vector.buf[1], buf + first, sizeof (RawMidi) * (rec_nframes - first));

			_current_capture_buffer = _capture_wrap_buffer;
		}
	} else {

		if (was_recording) {
			finish_capture (rec_monitors_input);
		}

	}
	
	if (rec_nframes) {
		
		/* data will be written to disk */

		if (rec_nframes == nframes && rec_offset == 0) {

			_current_playback_buffer = _current_capture_buffer;
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

		_current_playback_buffer = _current_capture_buffer;

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
		
		_playback_buf->get_read_vector (&_playback_vector);
		
		if (necessary_samples <= _playback_vector.len[0]) {

			_current_playback_buffer = _playback_vector.buf[0];

		} else {
			jack_nframes_t total = _playback_vector.len[0] + _playback_vector.len[1];
			
			if (necessary_samples > total) {
				cerr << "DiskUnderrun\n";
				//DiskUnderrun (); // FIXME
				goto out;
				
			} else {
				
				memcpy (_playback_wrap_buffer, _playback_vector.buf[0],
					_playback_vector.len[0] * sizeof (RawMidi));
				memcpy (_playback_wrap_buffer + _playback_vector.len[0], _playback_vector.buf[1], 
					(necessary_samples - _playback_vector.len[0]) * sizeof (RawMidi));
				
				_current_playback_buffer = _playback_wrap_buffer;
			}
		}

#if 0
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
						chan._current_playback_buffer[i] * (1.0f - fr) +
						chan._current_playback_buffer[i+1] * fr;
					phase += phi;
				}
				
				chan._current_playback_buffer = chan.speed_buffer;
			}

			playback_distance = i + 1;
			last_phase = (phase & 0xFFFFFF);

		} else {
			playback_distance = nframes;
		}
#endif

		playback_distance = nframes;

	}

	ret = 0;

  out:
	_processed = true;

	if (ret) {

		/* we're exiting with failure, so ::commit will not
		   be called. unlock the state lock.
		*/
		
		state_lock.unlock();
	} 

	return ret;
}

bool
MidiDiskstream::commit (jack_nframes_t nframes)
{
	return 0;
}

void
MidiDiskstream::set_pending_overwrite (bool yn)
{
	/* called from audio thread, so we can use the read ptr and playback sample as we wish */
	
	pending_overwrite = yn;

	overwrite_frame = playback_sample;
	//overwrite_offset = channels.front().playback_buf->get_read_ptr();
}

int
MidiDiskstream::overwrite_existing_buffers ()
{
	return 0;
}

int
MidiDiskstream::seek (jack_nframes_t frame, bool complete_refill)
{
	return 0;
}

int
MidiDiskstream::can_internal_playback_seek (jack_nframes_t distance)
{
	return 0;
}

int
MidiDiskstream::internal_playback_seek (jack_nframes_t distance)
{
	return 0;
}

int
MidiDiskstream::read (RawMidi* buf, jack_nframes_t& start, jack_nframes_t cnt, bool reversed)
{
	return 0;
}

int
MidiDiskstream::do_refill_with_alloc ()
{
	return 0;
}

int
MidiDiskstream::do_refill ()
{
	return 0;
}

/** Flush pending data to disk.
 *
 * Important note: this function will write *AT MOST* disk_io_chunk_frames
 * of data to disk. it will never write more than that.  If it writes that
 * much and there is more than that waiting to be written, it will return 1,
 * otherwise 0 on success or -1 on failure.
 * 
 * If there is less than disk_io_chunk_frames to be written, no data will be
 * written at all unless @a force_flush is true.
 */
int
MidiDiskstream::do_flush (Session::RunContext context, bool force_flush)
{
	return 0;
}

void
MidiDiskstream::transport_stopped (struct tm& when, time_t twhen, bool abort_capture)
{
}

void
MidiDiskstream::finish_capture (bool rec_monitors_input)
{
}

void
MidiDiskstream::set_record_enabled (bool yn)
{
	if (!recordable() || !_session.record_enabling_legal()) {
		return;
	}

	/* can't rec-enable in destructive mode if transport is before start */

	if (destructive() && yn && _session.transport_frame() < _session.current_start_frame()) {
		return;
	}

	if (yn && _source_port == 0) {

		/* pick up connections not initiated *from* the IO object
		   we're associated with.
		*/

		get_input_sources ();
	}

	/* yes, i know that this not proof against race conditions, but its
	   good enough. i think.
	*/

	if (record_enabled() != yn) {
		if (yn) {
			engage_record_enable ();
		} else {
			disengage_record_enable ();
		}
	}
}

void
MidiDiskstream::engage_record_enable ()
{
    bool rolling = _session.transport_speed() != 0.0f;

	g_atomic_int_set (&_record_enabled, 1);
	
	if (Config->get_use_hardware_monitoring() && _source_port) {
		_source_port->request_monitor_input (!(_session.get_auto_input() && rolling));
	}

	RecordEnableChanged (); /* EMIT SIGNAL */
}

void
MidiDiskstream::disengage_record_enable ()
{
	g_atomic_int_set (&_record_enabled, 0);
	if (Config->get_use_hardware_monitoring()) {
		if (_source_port) {
			_source_port->request_monitor_input (false);
		}
	}

	RecordEnableChanged (); /* EMIT SIGNAL */
}

XMLNode&
MidiDiskstream::get_state ()
{
	XMLNode* node = new XMLNode ("MidiDiskstream");
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	snprintf (buf, sizeof(buf), "0x%x", _flags);
	node->add_property ("flags", buf);

	node->add_property ("playlist", _playlist->name());
	
	snprintf (buf, sizeof(buf), "%f", _visible_speed);
	node->add_property ("speed", buf);

	node->add_property("name", _name);
	id().print(buf);
	node->add_property("id", buf);

	if (_write_source && _session.get_record_enabled()) {

		XMLNode* cs_child = new XMLNode (X_("CapturingSources"));
		XMLNode* cs_grandchild;

		cs_grandchild = new XMLNode (X_("file"));
		cs_grandchild->add_property (X_("path"), _write_source->path());
		cs_child->add_child_nocopy (*cs_grandchild);

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
MidiDiskstream::set_state (const XMLNode& node)
{
	const XMLProperty* prop;
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	uint32_t nchans = 1;
	XMLNode* capture_pending_node = 0;
	LocaleGuard lg (X_("POSIX"));

	in_set_state = true;

 	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
 		/*if ((*niter)->name() == IO::state_node_name) {
			deprecated_io_node = new XMLNode (**niter);
 		}*/
 		assert ((*niter)->name() != IO::state_node_name);

		if ((*niter)->name() == X_("CapturingSources")) {
			capture_pending_node = *niter;
		}
 	}

	/* prevent write sources from being created */
	
	in_set_state = true;
	
	if ((prop = node.property ("name")) != 0) {
		_name = prop->value();
	} 

	if ((prop = node.property ("id")) != 0) {
		_id = prop->value ();
	}

	if ((prop = node.property ("flags")) != 0) {
		_flags = strtol (prop->value().c_str(), 0, 0);
	}

	if ((prop = node.property ("channels")) != 0) {
		nchans = atoi (prop->value().c_str());
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
		
		if (!destructive() && capture_pending_node) {
			/* destructive streams have one and only one source per channel,
			   and so they never end up in pending capture in any useful
			   sense.
			*/
			use_pending_capture_data (*capture_pending_node);
		}

	}

	if ((prop = node.property ("speed")) != 0) {
		double sp = atof (prop->value().c_str());

		if (realtime_set_speed (sp, false)) {
			non_realtime_set_speed ();
		}
	}

	in_set_state = false;

	/* make sure this is clear before we do anything else */

	// FIXME?
	//_capturing_source = 0;

	/* write sources are handled when we handle the input set 
	   up of the IO that owns this DS (::non_realtime_input_change())
	*/
		
	in_set_state = false;

	return 0;
}

int
MidiDiskstream::use_new_write_source (uint32_t n)
{
	if (!recordable()) {
		return 1;
	}

	assert(n == 0);

	if (_write_source) {

		if (SMFSource::is_empty (_write_source->path())) {
			_write_source->mark_for_remove ();
			_write_source->release();
			delete _write_source;
		} else {
			_write_source->release();
			_write_source = 0;
		}
	}

	try {
		_write_source = dynamic_cast<SMFSource*>(_session.create_midi_source_for_session (*this));
		if (!_write_source) {
			throw failed_constructor();
		}
	} 

	catch (failed_constructor &err) {
		error << string_compose (_("%1:%2 new capture file not initialized correctly"), _name, n) << endmsg;
		_write_source = 0;
		return -1;
	}

	_write_source->use ();

	/* do not remove destructive files even if they are empty */

	_write_source->set_allow_remove_if_empty (!destructive());

	return 0;
}

void
MidiDiskstream::reset_write_sources (bool mark_write_complete, bool force)
{
	if (!recordable()) {
		return;
	}

	if (!destructive()) {

		if (_write_source && mark_write_complete) {
			_write_source->mark_streaming_write_completed ();
		}
		use_new_write_source ();

	} else {
		if (_write_source == 0) {
			use_new_write_source ();
		}
	}

	if (destructive()) {

		/* we now have all our write sources set up, so create the
		   playlist's single region.
		   */

		if (_playlist->empty()) {
			setup_destructive_playlist ();
		}
	}
}

int
MidiDiskstream::rename_write_sources ()
{
	if (_write_source != 0) {
		_write_source->set_name (_name, destructive());
		/* XXX what to do if this fails ? */
	}
	return 0;
}

void
MidiDiskstream::set_block_size (jack_nframes_t nframes)
{
}

void
MidiDiskstream::allocate_temporary_buffers ()
{
}

void
MidiDiskstream::monitor_input (bool yn)
{
	if (_source_port)
		_source_port->request_monitor_input (yn);
	else
		cerr << "MidiDiskstream NO SOURCE PORT TO MONITOR\n";
}

void
MidiDiskstream::set_align_style_from_io ()
{
	bool have_physical = false;

	if (_io == 0) {
		return;
	}

	get_input_sources ();
	
	if (_source_port && _source_port->flags() & JackPortIsPhysical) {
		have_physical = true;
	}

	if (have_physical) {
		set_align_style (ExistingMaterial);
	} else {
		set_align_style (CaptureTime);
	}
}


float
MidiDiskstream::playback_buffer_load () const
{
	return (float) ((double) _playback_buf->read_space()/
			(double) _playback_buf->bufsize());
}

float
MidiDiskstream::capture_buffer_load () const
{
	return (float) ((double) _capture_buf->write_space()/
			(double) _capture_buf->bufsize());
}


int
MidiDiskstream::use_pending_capture_data (XMLNode& node)
{
	return 0;
}
