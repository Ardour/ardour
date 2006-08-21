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
#include <pbd/memento_command.h>

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
	//, _current_playback_buffer(0)
	//, _current_capture_buffer(0)
	//, _playback_wrap_buffer(0)
	//, _capture_wrap_buffer(0)
	, _source_port(0)
	, _write_source(0)
	, _capture_transition_buf(0)
{
	/* prevent any write sources from being created */

	in_set_state = true;

	init(flag);
	use_new_playlist ();

	in_set_state = false;

	assert(!destructive());
	DiskstreamCreated (this); /* EMIT SIGNAL */
}
	
MidiDiskstream::MidiDiskstream (Session& sess, const XMLNode& node)
	: Diskstream(sess, node)
	, _playback_buf(0)
	, _capture_buf(0)
	//, _current_playback_buffer(0)
	//, _current_capture_buffer(0)
	//, _playback_wrap_buffer(0)
	//, _capture_wrap_buffer(0)
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

	//_playback_wrap_buffer = new RawMidi[wrap_buffer_size];
	//_capture_wrap_buffer = new RawMidi[wrap_buffer_size];
	_playback_buf = new MidiRingBuffer (_session.diskstream_buffer_size());
	_capture_buf = new MidiRingBuffer (_session.diskstream_buffer_size());
	_capture_transition_buf = new RingBufferNPT<CaptureTransition> (128);
	
	_n_channels = ChanCount(DataType::MIDI, 1);

	assert(recordable());
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
	uint32_t ni = _io->n_inputs().get(DataType::MIDI);

	if (ni == 0) {
		return;
	}

	// This is all we do for now at least
	assert(ni == 1);

	_source_port = _io->midi_input(0);

	/* I don't get it....
	const char **connections = _io->input(0)->get_connections ();

	if (connections == 0 || connections[0] == 0) {

		if (_source_port) {
			// _source_port->disable_metering ();
		}

		_source_port = 0;

	} else {
		_source_port = dynamic_cast<MidiPort*>(
			_session.engine().get_port_by_name (connections[0]) );
	}

	if (connections) {
		free (connections);
	}*/
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

/** Overloaded from parent to die horribly
 */
void
MidiDiskstream::set_destructive (bool yn)
{
	assert( ! destructive());
	assert( ! yn);
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

	/*_current_capture_buffer = 0;
	  _current_playback_buffer = 0;*/

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

		assert(_source_port);

		// Pump entire port buffer into the ring buffer (FIXME!)
		_capture_buf->write(_source_port->get_midi_buffer(), transport_frame);

		// FIXME: hackitty hack, don't come back
		//_write_source->ViewDataRangeReady (_write_source->length(), rec_nframes); /* EMIT SIGNAL */
		/*
		   for (size_t i=0; i < _source_port->size(); ++i) {
		   cerr << "DISKSTREAM GOT EVENT(1) " << i << "!!\n";
		   }

		   if (_source_port->size() == 0)
		   cerr << "No events :/ (1)\n";
		   */
	} else {

		if (was_recording) {
			finish_capture (rec_monitors_input);
		}

	}

	if (rec_nframes) {

		/* XXX XXX XXX XXX XXX XXX XXX XXX */
		/* data will be written to disk */

		adjust_capture_position = rec_nframes;

	} else if (nominally_recording) {

		/* can't do actual capture yet - waiting for latency effects to finish before we start*/

		// Ummm.. well, I suppose we'll just hang out for a bit?

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

		// XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX
		// Write into playback buffer here, and whatnot

	}

	ret = 0;

	_processed = true;

	if (ret) {

		/* we're exiting with failure, so ::commit will not
		   be called. unlock the state lock.
		   */

		state_lock.unlock();
	} 

	return ret;

	_processed = true;

	return 0;
}

bool
MidiDiskstream::commit (jack_nframes_t nframes)
{
	bool need_butler = false;

	if (_actual_speed < 0.0) {
		playback_sample -= playback_distance;
	} else {
		playback_sample += playback_distance;
	}

	/* XXX XXX XXX XXX XXX XXX XXX XXX XXX XXX */

	/*
	_playback_buf->increment_read_ptr (playback_distance);

	if (adjust_capture_position) {
		_capture_buf->increment_write_ptr (adjust_capture_position);
	}
*/
	if (adjust_capture_position != 0) {
		capture_captured += adjust_capture_position;
		adjust_capture_position = 0;
	}

	if (_slaved) {
		need_butler = _playback_buf->write_space() >= _playback_buf->capacity() / 2;
	} else {
		need_butler = _playback_buf->write_space() >= disk_io_chunk_frames
			|| _capture_buf->read_space() >= disk_io_chunk_frames;
	}
	
	state_lock.unlock();

	_processed = false;

	return need_butler;
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
	Glib::Mutex::Lock lm (state_lock);
	int ret = -1;

	_playback_buf->reset();
	_capture_buf->reset();

	playback_sample = frame;
	file_frame = frame;

	if (complete_refill) {
		while ((ret = do_refill_with_alloc ()) > 0) ;
	} else {
		ret = do_refill_with_alloc ();
	}

	return ret;
}

int
MidiDiskstream::can_internal_playback_seek (jack_nframes_t distance)
{
	if (_playback_buf->read_space() < distance) {
		return false;
	} else {
		return true;
	}
}

int
MidiDiskstream::internal_playback_seek (jack_nframes_t distance)
{
	first_recordable_frame += distance;
	playback_sample += distance;

	return 0;
}

/** @a start is set to the new frame position (TIME) read up to */
int
MidiDiskstream::read (jack_nframes_t& start, jack_nframes_t dur, bool reversed)
{	
	jack_nframes_t this_read = 0;
	bool reloop = false;
	jack_nframes_t loop_end = 0;
	jack_nframes_t loop_start = 0;
	jack_nframes_t loop_length = 0;
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

	while (dur) {

		/* take any loop into account. we can't read past the end of the loop. */

		if (loc && (loop_end - start < dur)) {
			this_read = loop_end - start;
			//cerr << "reloop true: thisread: " << this_read << "  dur: " << dur << endl;
			reloop = true;
		} else {
			reloop = false;
			this_read = dur;
		}

		if (this_read == 0) {
			break;
		}

		this_read = min(dur,this_read);

		if (midi_playlist()->read (*_playback_buf, start, this_read) != this_read) {
			error << string_compose(_("MidiDiskstream %1: cannot read %2 from playlist at frame %3"), _id, this_read, 
					 start) << endmsg;
			return -1;
		}

		_read_data_count = _playlist->read_data_count();
		
		if (reversed) {

			cerr << "Reversed MIDI.. that's just crazy talk." << endl;
			// Swap note ons with note offs here

		} else {
			
			/* if we read to the end of the loop, go back to the beginning */
			
			if (reloop) {
				start = loop_start;
			} else {
				start += this_read;
			}
		} 

		dur -= this_read;
	}

	return 0;
}

int
MidiDiskstream::do_refill_with_alloc ()
{
	return do_refill();
}

int
MidiDiskstream::do_refill ()
{
	int32_t        ret = 0;
	size_t         write_space = _playback_buf->write_space();

	bool reversed = (_visible_speed * _session.transport_speed()) < 0.0f;

	if (write_space == 0) {
		return 0;
	}

	/* if there are 2+ chunks of disk i/o possible for
	   this track, let the caller know so that it can arrange
	   for us to be called again, ASAP.
	   */

	// FIXME: using disk_io_chunk_frames as an event count, not good
	if (_playback_buf->write_space() >= (_slaved?3:2) * disk_io_chunk_frames) {
		ret = 1;
	}

	/* if we're running close to normal speed and there isn't enough 
	   space to do disk_io_chunk_frames of I/O, then don't bother.  

	   at higher speeds, just do it because the sync between butler
	   and audio thread may not be good enough.
	   */

	if ((write_space < disk_io_chunk_frames) && fabs (_actual_speed) < 2.0f) {
		cerr << "No refill 1\n";
		return 0;
	}

	/* when slaved, don't try to get too close to the read pointer. this
	   leaves space for the buffer reversal to have something useful to
	   work with.
	   */

	if (_slaved && write_space < (_playback_buf->capacity() / 2)) {
		cerr << "No refill 2\n";
		return 0;
	}

	if (reversed) {
		cerr << "No refill 3 (reverse)\n";
		return 0;
	}

	if (file_frame == max_frames) {
		//cerr << "No refill 4 (EOF)\n";

		/* at end: nothing to do */

		return 0;
	}

#if 0
	// or this
	if (file_frame > max_frames - total_space) {

		/* to close to the end: read what we can, and zero fill the rest */

		zero_fill = total_space - (max_frames - file_frame);
		total_space = max_frames - file_frame;

	} else {
		zero_fill = 0;
	}
#endif

	// At this point we:
	assert(_playback_buf->write_space() > 0); // ... have something to write to, and
	assert(file_frame <= max_frames); // ... something to write

	// So (read it, then) write it:
	
	jack_nframes_t file_frame_tmp = file_frame;
	jack_nframes_t to_read = min(disk_io_chunk_frames, (max_frames - file_frame));
	
	// FIXME: read count?
	if (read (file_frame_tmp, to_read, reversed)) {
		ret = -1;
		goto out;
	}

	file_frame = file_frame_tmp;

out:

	return ret;
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
	uint32_t to_write;
	int32_t ret = 0;
	// FIXME: I'd be lying if I said I knew what this thing was
	//RingBufferNPT<CaptureTransition>::rw_vector transvec;
	jack_nframes_t total;

	_write_data_count = 0;

	total = _capture_buf->read_space();


	// FIXME: put this condition back in! (removed for testing)
	if (total == 0) { // || (total < disk_io_chunk_frames && !force_flush && was_recording)) {
		//cerr << "MDS - no flush 1\n";
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

	//to_write = min (disk_io_chunk_frames, (jack_nframes_t) vector.len[0]);
	to_write = disk_io_chunk_frames;

	assert(!destructive());

	if ((!_write_source) || _write_source->write (*_capture_buf, to_write) != to_write) {
		//cerr << "MDS - no flush 2\n";
		error << string_compose(_("AudioDiskstream %1: cannot write to disk"), _id) << endmsg;
		return -1;
	} else {
		//cerr << "MDS - flushed\n";
	}

	//(*chan).curr_capture_cnt += to_write;

out:
	//return ret;
	return 0;
}

void
MidiDiskstream::transport_stopped (struct tm& when, time_t twhen, bool abort_capture)
{
	uint32_t buffer_position;
	bool more_work = true;
	int err = 0;
	MidiRegion* region = 0;
	jack_nframes_t total_capture;
	MidiRegion::SourceList srcs;
	MidiRegion::SourceList::iterator src;
	vector<CaptureInfo*>::iterator ci;
	bool mark_write_completed = false;

	finish_capture (true);

	/* butler is already stopped, but there may be work to do 
	   to flush remaining data to disk.
	   */

	while (more_work && !err) {
		switch (do_flush (Session::TransportContext, true)) {
			case 0:
				more_work = false;
				break;
			case 1:
				break;
			case -1:
				error << string_compose(_("MidiDiskstream \"%1\": cannot flush captured data to disk!"), _name) << endmsg;
				err++;
		}
	}

	/* XXX is there anything we can do if err != 0 ? */
	Glib::Mutex::Lock lm (capture_info_lock);

	if (capture_info.empty()) {
		return;
	}

	if (abort_capture) {

		list<Source*>* deletion_list = new list<Source*>;

		if (_write_source) {
			_write_source->mark_for_remove ();
			_write_source->release ();

			deletion_list->push_back (_write_source);

			_write_source = 0;
		}

		/* new source set up in "out" below */

		if (!deletion_list->empty()) {
			DeleteSources (deletion_list);
		} else {
			delete deletion_list;
		}

	} else {

		assert(_write_source);

		for (total_capture = 0, ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
			total_capture += (*ci)->frames;
		}

		/* figure out the name for this take */

		SMFSource* s = _write_source;

		if (s) {

			srcs.push_back (s);

			cerr << "MidiDiskstream: updating source after capture\n";
			s->update_header (capture_info.front()->start, when, twhen);

			s->set_captured_for (_name);

		}

		/* Register a new region with the Session that
		   describes the entire source. Do this first
		   so that any sub-regions will obviously be
		   children of this one (later!)
		   */
		try {
			assert(_write_source);
			region = new MidiRegion (srcs, _write_source->last_capture_start_frame(), total_capture, 
					region_name_from_path (_write_source->name()), 
					0, Region::Flag (Region::DefaultFlags|Region::Automatic|Region::WholeFile));

			region->special_set_position (capture_info.front()->start);
		}


		catch (failed_constructor& err) {
			error << string_compose(_("%1: could not create region for complete midi file"), _name) << endmsg;
			/* XXX what now? */
		}

		_last_capture_regions.push_back (region);

		// cerr << _name << ": there are " << capture_info.size() << " capture_info records\n";

		XMLNode &before = _playlist->get_state();
		_playlist->freeze ();

		for (buffer_position = _write_source->last_capture_start_frame(), ci = capture_info.begin(); ci != capture_info.end(); ++ci) {

			string region_name;
			_session.region_name (region_name, _write_source->name(), false);

			// cerr << _name << ": based on ci of " << (*ci)->start << " for " << (*ci)->frames << " add a region\n";

			try {
				region = new MidiRegion (srcs, buffer_position, (*ci)->frames, region_name);
			}

			catch (failed_constructor& err) {
				error << _("MidiDiskstream: could not create region for captured audio!") << endmsg;
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
		XMLNode &after = _playlist->get_state();
		_session.add_command (new MementoCommand<Playlist>(*_playlist, before, after));

		mark_write_completed = true;

		reset_write_sources (mark_write_completed);

	}

	for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		delete *ci;
	}

	capture_info.clear ();
	capture_start_frame = 0;
}

void
MidiDiskstream::finish_capture (bool rec_monitors_input)
{
	was_recording = false;
	
	if (capture_captured == 0) {
		return;
	}

	// Why must we destroy?
	assert(!destructive());

	CaptureInfo* ci = new CaptureInfo;
	
	ci->start  = capture_start_frame;
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
MidiDiskstream::set_record_enabled (bool yn)
{
	if (!recordable() || !_session.record_enabling_legal()) {
		return;
	}

	assert(!destructive());
	
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
	_write_source->set_allow_remove_if_empty (true);

	return 0;
}

void
MidiDiskstream::reset_write_sources (bool mark_write_complete, bool force)
{
	if (!recordable()) {
		return;
	}

	if (_write_source && mark_write_complete) {
		_write_source->mark_streaming_write_completed ();
	}

	if (!_write_source) {
		use_new_write_source ();
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
			(double) _playback_buf->capacity());
}

float
MidiDiskstream::capture_buffer_load () const
{
	return (float) ((double) _capture_buf->write_space()/
			(double) _capture_buf->capacity());
}


int
MidiDiskstream::use_pending_capture_data (XMLNode& node)
{
	return 0;
}

/** Writes playback events in the given range to dst, translating time stamps
 * so that an event at start has time = 0
 */
void
MidiDiskstream::get_playback(MidiBuffer& dst, jack_nframes_t start, jack_nframes_t end)
{
	assert(end > start);
	dst.clear();
	assert(dst.size() == 0);
/*
	cerr << "MIDI Diskstream pretending to read" << endl;

	MidiEvent ev;
	RawMidi data[4];

	const char note = rand()%30 + 30;
	
	ev.buffer = data;
	ev.time = 0;
	ev.size = 3;

	data[0] = 0x90;
	data[1] = note;
	data[2] = 120;

	dst.push_back(ev);
	
	ev.buffer = data;
	ev.time = (end - start) / 2;
	ev.size = 3;

	data[0] = 0x80;
	data[1] = note;
	data[2] = 64;
*/
	_playback_buf->read(dst, start, end);

	// Translate time stamps to be relative to the start of this cycle
	for (size_t i=0; i < dst.size(); ++i) {
		assert(dst[i].time >= start);
		assert(dst[i].time <= end);
		cerr << "Translating event stamp " << dst[i].time << " to ";
		dst[i].time -= start;
		cerr << dst[i].time << endl;

	}
}
