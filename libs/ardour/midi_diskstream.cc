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

#include "pbd/error.h"
#include "pbd/basename.h"
#include <glibmm/thread.h>
#include "pbd/xml++.h"
#include "pbd/memento_command.h"
#include "pbd/enumwriter.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/stacktrace.h"

#include "ardour/ardour.h"
#include "ardour/audioengine.h"
#include "ardour/butler.h"
#include "ardour/configuration.h"
#include "ardour/cycle_timer.h"
#include "ardour/debug.h"
#include "ardour/io.h"
#include "ardour/midi_diskstream.h"
#include "ardour/midi_model.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_port.h"
#include "ardour/midi_region.h"
#include "ardour/playlist_factory.h"
#include "ardour/region_factory.h"
#include "ardour/route.h"
#include "ardour/send.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/smf_source.h"
#include "ardour/utils.h"

#include "midi++/types.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

framecnt_t MidiDiskstream::midi_readahead = 4096;

MidiDiskstream::MidiDiskstream (Session &sess, const string &name, Diskstream::Flag flag)
	: Diskstream(sess, name, flag)
	, _playback_buf(0)
	, _capture_buf(0)
	, _note_mode(Sustained)
	, _frames_written_to_ringbuffer(0)
	, _frames_read_from_ringbuffer(0)
	, _gui_feed_buffer(AudioEngine::instance()->raw_buffer_size (DataType::MIDI))
{
	in_set_state = true;

	init ();
	use_new_playlist ();
	use_new_write_source (0);

	in_set_state = false;

	assert(!destructive());
}

MidiDiskstream::MidiDiskstream (Session& sess, const XMLNode& node)
	: Diskstream(sess, node)
	, _playback_buf(0)
	, _capture_buf(0)
	, _note_mode(Sustained)
	, _frames_written_to_ringbuffer(0)
	, _frames_read_from_ringbuffer(0)
	, _gui_feed_buffer(AudioEngine::instance()->raw_buffer_size (DataType::MIDI))
{
	in_set_state = true;

	init ();

	if (set_state (node, Stateful::loading_state_version)) {
		in_set_state = false;
		throw failed_constructor();
	}

	use_new_write_source (0);

	in_set_state = false;
}

void
MidiDiskstream::init ()
{
	/* there are no channels at this point, so these
	   two calls just get speed_buffer_size and wrap_buffer
	   size setup without duplicating their code.
	*/

	set_block_size (_session.get_block_size());
	allocate_temporary_buffers ();

	const size_t size = _session.butler()->midi_diskstream_buffer_size();
	_playback_buf = new MidiRingBuffer<framepos_t>(size);
	_capture_buf = new MidiRingBuffer<framepos_t>(size);

	_n_channels = ChanCount(DataType::MIDI, 1);

	assert(recordable());
}

MidiDiskstream::~MidiDiskstream ()
{
	Glib::Mutex::Lock lm (state_lock);
}


void
MidiDiskstream::non_realtime_locate (framepos_t position)
{
	if (_write_source) {
		_write_source->set_timeline_position (position);
	}
	seek (position, false);
}


void
MidiDiskstream::non_realtime_input_change ()
{
	{
		Glib::Mutex::Lock lm (state_lock);

		if (input_change_pending.type == IOChange::NoChange) {
			return;
		}

		if (input_change_pending.type & IOChange::ConfigurationChanged) {
			uint32_t ni = _io->n_ports().n_midi();

			if (ni != _n_channels.n_midi()) {
				error << string_compose (_("%1: I/O configuration change %4 requested to use %2, but channel setup is %3"),
				                         name(),
				                         _io->n_ports(),
				                         _n_channels, input_change_pending.type)
				      << endmsg;
			}

			if (ni == 0) {
				_source_port.reset ();
			} else {
				_source_port = _io->midi(0);
			}
		}

		if (input_change_pending.type & IOChange::ConnectionsChanged) {
			set_capture_offset ();
			set_align_style_from_io ();
		}

		input_change_pending.type = IOChange::NoChange;

		/* implicit unlock */
	}

	/* unlike with audio, there is never any need to reset write sources
	   based on input configuration changes because ... a MIDI track
	   has just 1 MIDI port as input, always.
	*/

	/* now refill channel buffers */

	if (speed() != 1.0f || speed() != -1.0f) {
		seek ((framepos_t) (_session.transport_frame() * (double) speed()));
	}
	else {
		seek (_session.transport_frame());
	}

	if (_write_source) {
		_write_source->set_last_write_end (_session.transport_frame());
	}
}

int
MidiDiskstream::find_and_use_playlist (const string& name)
{
	boost::shared_ptr<MidiPlaylist> playlist;

	if ((playlist = boost::dynamic_pointer_cast<MidiPlaylist> (_session.playlists->by_name (name))) == 0) {
		playlist = boost::dynamic_pointer_cast<MidiPlaylist> (PlaylistFactory::create (DataType::MIDI, _session, name));
	}

	if (!playlist) {
		error << string_compose(_("MidiDiskstream: Playlist \"%1\" isn't an midi playlist"), name) << endmsg;
		return -1;
	}

	return use_playlist (playlist);
}

int
MidiDiskstream::use_playlist (boost::shared_ptr<Playlist> playlist)
{
	assert(boost::dynamic_pointer_cast<MidiPlaylist>(playlist));

	Diskstream::use_playlist(playlist);

	return 0;
}

int
MidiDiskstream::use_new_playlist ()
{
	string newname;
	boost::shared_ptr<MidiPlaylist> playlist;

	if (!in_set_state && destructive()) {
		return 0;
	}

	if (_playlist) {
		newname = Playlist::bump_name (_playlist->name(), _session);
	} else {
		newname = Playlist::bump_name (_name, _session);
	}

	if ((playlist = boost::dynamic_pointer_cast<MidiPlaylist> (PlaylistFactory::create (
			DataType::MIDI, _session, newname, hidden()))) != 0) {

		return use_playlist (playlist);

	} else {
		return -1;
	}
}

int
MidiDiskstream::use_copy_playlist ()
{
	assert(midi_playlist());

	if (destructive()) {
		return 0;
	}

	if (_playlist == 0) {
		error << string_compose(_("MidiDiskstream %1: there is no existing playlist to make a copy of!"), _name) << endmsg;
		return -1;
	}

	string newname;
	boost::shared_ptr<MidiPlaylist> playlist;

	newname = Playlist::bump_name (_playlist->name(), _session);

	if ((playlist  = boost::dynamic_pointer_cast<MidiPlaylist>(PlaylistFactory::create (midi_playlist(), newname))) != 0) {
		return use_playlist (playlist);
	} else {
		return -1;
	}
}

/** Overloaded from parent to die horribly
 */
int
MidiDiskstream::set_destructive (bool yn)
{
	yn = 0; // stop pedantic gcc complaints about unused parameter
	assert( ! destructive());
	assert( ! yn);
	return -1;
}

void
MidiDiskstream::set_note_mode (NoteMode m)
{
	_note_mode = m;
	midi_playlist()->set_note_mode(m);
	if (_write_source && _write_source->model())
		_write_source->model()->set_note_mode(m);
}

int
MidiDiskstream::process (framepos_t transport_frame, pframes_t nframes, framecnt_t& playback_distance)
{
	framecnt_t rec_offset = 0;
	framecnt_t rec_nframes = 0;
	bool      nominally_recording;
	bool      re = record_enabled ();
	bool      can_record = _session.actively_recording ();

	playback_distance = 0;

	check_record_status (transport_frame, can_record);

	nominally_recording = (can_record && re);

	if (nframes == 0) {
		return 0;
	}

	boost::shared_ptr<MidiPort> sp = _source_port.lock ();

	if (sp == 0) {
		return 1;
	}

	Glib::Mutex::Lock sm (state_lock, Glib::TRY_LOCK);

	if (!sm.locked()) {
		return 1;
	}

	adjust_capture_position = 0;

	if (nominally_recording || (re && was_recording && _session.get_record_enabled() && _session.config.get_punch_in())) {
		Evoral::OverlapType ot = Evoral::coverage (first_recordable_frame, last_recordable_frame, transport_frame, transport_frame + nframes);

		calculate_record_range(ot, transport_frame, nframes, rec_nframes, rec_offset);

		if (rec_nframes && !was_recording) {
			_write_source->mark_write_starting_now ();
			capture_captured = 0;
			was_recording = true;
		}
	}

	if (can_record && !_last_capture_sources.empty()) {
		_last_capture_sources.clear ();
	}

	if (nominally_recording || rec_nframes) {

		// Pump entire port buffer into the ring buffer (FIXME: split cycles?)
		MidiBuffer& buf = sp->get_midi_buffer(nframes);
		for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
			const Evoral::MIDIEvent<MidiBuffer::TimeType> ev(*i, false);
			assert(ev.buffer());
#ifndef NDEBUG
			if (DEBUG::MidiIO & PBD::debug_bits) {
				const uint8_t* __data = ev.buffer();
				DEBUG_STR_DECL(a);
				DEBUG_STR_APPEND(a, string_compose ("mididiskstream %1 capture event @ %2 + %3 sz %4 ", this, ev.time(), transport_frame, ev.size()));
				for (size_t i=0; i < ev.size(); ++i) {
					DEBUG_STR_APPEND(a,hex);
					DEBUG_STR_APPEND(a,"0x");
					DEBUG_STR_APPEND(a,(int)__data[i]);
					DEBUG_STR_APPEND(a,' ');
				}
				DEBUG_STR_APPEND(a,'\n');
				DEBUG_TRACE (DEBUG::MidiIO, DEBUG_STR(a).str());
			}
#endif
			_capture_buf->write(ev.time() + transport_frame, ev.type(), ev.size(), ev.buffer());
		}

		if (buf.size() != 0) {
			Glib::Mutex::Lock lm (_gui_feed_buffer_mutex, Glib::TRY_LOCK);

			if (lm.locked ()) {
				/* Copy this data into our GUI feed buffer and tell the GUI
				   that it can read it if it likes.
				*/
				_gui_feed_buffer.clear ();
				
				for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
					/* This may fail if buf is larger than _gui_feed_buffer, but it's not really
					   the end of the world if it does.
					*/
					_gui_feed_buffer.push_back ((*i).time() + transport_frame, (*i).size(), (*i).buffer());
				}
			}

			DataRecorded (_write_source); /* EMIT SIGNAL */
		}

	} else {

		if (was_recording) {
			finish_capture ();
		}

	}

	if (rec_nframes) {

		/* data will be written to disk */

		if (rec_nframes == nframes && rec_offset == 0) {
			playback_distance = nframes;
		}

		adjust_capture_position = rec_nframes;

	} else if (nominally_recording) {

		/* XXXX do this for MIDI !!!
		   can't do actual capture yet - waiting for latency effects to finish before we start
		   */

		playback_distance = nframes;

	} else {

		/* XXX: should be doing varispeed stuff here, similar to the code in AudioDiskstream::process */

		playback_distance = nframes;

	}

	return 0;
}

bool
MidiDiskstream::commit (framecnt_t playback_distance)
{
	bool need_butler = false;

	if (_actual_speed < 0.0) {
		playback_sample -= playback_distance;
	} else {
		playback_sample += playback_distance;
	}

	if (adjust_capture_position != 0) {
		capture_captured += adjust_capture_position;
		adjust_capture_position = 0;
	}

	uint32_t frames_read = g_atomic_int_get(&_frames_read_from_ringbuffer);
	uint32_t frames_written = g_atomic_int_get(&_frames_written_to_ringbuffer);

	/*
	  cerr << name() << " MDS written: " << frames_written << " - read: " << frames_read <<
	  " = " << frames_written - frames_read
	  << " + " << playback_distance << " < " << midi_readahead << " = " << need_butler << ")" << endl;
	*/

	/* frames_read will generally be less than frames_written, but
	 * immediately after an overwrite, we can end up having read some data
	 * before we've written any. we don't need to trip an assert() on this,
	 * but we do need to check so that the decision on whether or not we
	 * need the butler is done correctly.
	 */
	
	if (frames_read <= frames_written) {
		if ((frames_written - frames_read) + playback_distance < midi_readahead) {
			need_butler = true;
		}
	}


	return need_butler;
}

void
MidiDiskstream::set_pending_overwrite (bool yn)
{
	/* called from audio thread, so we can use the read ptr and playback sample as we wish */

	_pending_overwrite = yn;
	overwrite_frame = playback_sample;
}

int
MidiDiskstream::overwrite_existing_buffers ()
{
	/* This is safe as long as the butler thread is suspended, which it should be */
	_playback_buf->reset ();

	g_atomic_int_set (&_frames_read_from_ringbuffer, 0);
	g_atomic_int_set (&_frames_written_to_ringbuffer, 0);

	read (overwrite_frame, disk_io_chunk_frames, false);
	file_frame = overwrite_frame; // it was adjusted by ::read()
	overwrite_queued = false;
	_pending_overwrite = false;

	return 0;
}

int
MidiDiskstream::seek (framepos_t frame, bool complete_refill)
{
	Glib::Mutex::Lock lm (state_lock);
	int ret = -1;

	_playback_buf->reset();
	_capture_buf->reset();
	g_atomic_int_set(&_frames_read_from_ringbuffer, 0);
	g_atomic_int_set(&_frames_written_to_ringbuffer, 0);

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
MidiDiskstream::can_internal_playback_seek (framecnt_t distance)
{
	uint32_t frames_read    = g_atomic_int_get(&_frames_read_from_ringbuffer);
	uint32_t frames_written = g_atomic_int_get(&_frames_written_to_ringbuffer);
	return ((frames_written - frames_read) < distance);
}

int
MidiDiskstream::internal_playback_seek (framecnt_t distance)
{
	first_recordable_frame += distance;
	playback_sample += distance;

	return 0;
}

/** @a start is set to the new frame position (TIME) read up to */
int
MidiDiskstream::read (framepos_t& start, framecnt_t dur, bool reversed)
{
	framecnt_t this_read = 0;
	bool reloop = false;
	framepos_t loop_end = 0;
	framepos_t loop_start = 0;
	Location *loc = 0;

	if (!reversed) {

		framecnt_t loop_length = 0;

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

		if (loc && (start >= loop_end)) {
			//cerr << "start adjusted from " << start;
			start = loop_start + ((start - loop_start) % loop_length);
			//cerr << "to " << start << endl;
		}
		// cerr << "start is " << start << " end " << start+dur << "  loopstart: " << loop_start << "  loopend: " << loop_end << endl;
	}

	while (dur) {

		/* take any loop into account. we can't read past the end of the loop. */

		if (loc && (loop_end - start <= dur)) {
			this_read = loop_end - start;
			// cerr << "reloop true: thisread: " << this_read << "  dur: " << dur << endl;
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
			error << string_compose(
					_("MidiDiskstream %1: cannot read %2 from playlist at frame %3"),
					id(), this_read, start) << endmsg;
			return -1;
		}

		g_atomic_int_add (&_frames_written_to_ringbuffer, this_read);

		if (reversed) {

			// Swap note ons with note offs here.  etc?
			// Fully reversing MIDI requires look-ahead (well, behind) to find previous
			// CC values etc.  hard.

		} else {

			/* if we read to the end of the loop, go back to the beginning */

			if (reloop) {
				// Synthesize LoopEvent here, because the next events
				// written will have non-monotonic timestamps.
				_playback_buf->write(loop_end - 1, LoopEventType, sizeof (framepos_t), (uint8_t *) &loop_start);
				start = loop_start;
			} else {
				start += this_read;
			}
		}

		dur -= this_read;
		//offset += this_read;
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
	int     ret         = 0;
	size_t  write_space = _playback_buf->write_space();
	bool    reversed    = (_visible_speed * _session.transport_speed()) < 0.0f;

	if (write_space == 0) {
		return 0;
	}

	if (reversed) {
		return 0;
	}

	/* at end: nothing to do */
	if (file_frame == max_framepos) {
		return 0;
	}

	// At this point we...
	assert(_playback_buf->write_space() > 0); // ... have something to write to, and
	assert(file_frame <= max_framepos); // ... something to write

	// now calculate how much time is in the ringbuffer.
	// and lets write as much as we need to get this to be midi_readahead;
	uint32_t frames_read = g_atomic_int_get(&_frames_read_from_ringbuffer);
	uint32_t frames_written = g_atomic_int_get(&_frames_written_to_ringbuffer);
	if ((frames_written - frames_read) >= midi_readahead) {
		return 0;
	}

	framecnt_t to_read = midi_readahead - (frames_written - frames_read);

	//cout << "MDS read for midi_readahead " << to_read << "  rb_contains: "
	//	<< frames_written - frames_read << endl;

	to_read = (framecnt_t) min ((framecnt_t) to_read, (framecnt_t) (max_framepos - file_frame));

	if (read (file_frame, to_read, reversed)) {
		ret = -1;
	}

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
MidiDiskstream::do_flush (RunContext /*context*/, bool force_flush)
{
	framecnt_t to_write;
	framecnt_t total;
	int32_t ret = 0;

	if (!_write_source) {
		return 0;
	}

	assert (!destructive());

	total = _session.transport_frame() - _write_source->last_write_end();

	if (total == 0 || 
	    _capture_buf->read_space() == 0 || 
	    (!force_flush && (total < disk_io_chunk_frames) && was_recording)) {
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

	if (force_flush) {
		/* push out everything we have, right now */
		to_write = max_framecnt;
	} else {
		to_write = disk_io_chunk_frames;
	}

	if (record_enabled() && ((total > disk_io_chunk_frames) || force_flush)) {
		if (_write_source->midi_write (*_capture_buf, get_capture_start_frame (0), to_write) != to_write) {
			error << string_compose(_("MidiDiskstream %1: cannot write to disk"), id()) << endmsg;
			return -1;
		} 
	}

out:
	return ret;
}

void
MidiDiskstream::transport_stopped_wallclock (struct tm& /*when*/, time_t /*twhen*/, bool abort_capture)
{
	bool more_work = true;
	int err = 0;
	boost::shared_ptr<MidiRegion> region;
	MidiRegion::SourceList srcs;
	MidiRegion::SourceList::iterator src;
	vector<CaptureInfo*>::iterator ci;

	finish_capture ();

	/* butler is already stopped, but there may be work to do
	   to flush remaining data to disk.
	   */

	while (more_work && !err) {
		switch (do_flush (TransportContext, true)) {
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
		goto no_capture_stuff_to_do;
	}

	if (abort_capture) {

		if (_write_source) {
			_write_source->mark_for_remove ();
			_write_source->drop_references ();
			_write_source.reset();
		}

		/* new source set up in "out" below */

	} else {

		assert(_write_source);

		framecnt_t total_capture = 0;
		for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
			total_capture += (*ci)->frames;
		}

		if (_write_source->length (capture_info.front()->start) != 0) {

			/* phew, we have data */

			/* figure out the name for this take */

			srcs.push_back (_write_source);

			_write_source->set_timeline_position (capture_info.front()->start);
			_write_source->set_captured_for (_name);

			/* set length in beats to entire capture length */

			BeatsFramesConverter converter (_session.tempo_map(), capture_info.front()->start);
			const double total_capture_beats = converter.from (total_capture);
			_write_source->set_length_beats (total_capture_beats);

			/* flush to disk: this step differs from the audio path,
			   where all the data is already on disk.
			*/

			_write_source->mark_midi_streaming_write_completed (Evoral::Sequence<Evoral::MusicalTime>::ResolveStuckNotes, total_capture_beats);

			/* we will want to be able to keep (over)writing the source
			   but we don't want it to be removable. this also differs
			   from the audio situation, where the source at this point
			   must be considered immutable. luckily, we can rely on
			   MidiSource::mark_streaming_write_completed() to have
			   already done the necessary work for that.
			*/

			string whole_file_region_name;
			whole_file_region_name = region_name_from_path (_write_source->name(), true);

			/* Register a new region with the Session that
			   describes the entire source. Do this first
			   so that any sub-regions will obviously be
			   children of this one (later!)
			*/

			try {
				PropertyList plist;

				plist.add (Properties::name, whole_file_region_name);
				plist.add (Properties::whole_file, true);
				plist.add (Properties::automatic, true);
				plist.add (Properties::start, 0);
				plist.add (Properties::length, total_capture);
				plist.add (Properties::layer, 0);

				boost::shared_ptr<Region> rx (RegionFactory::create (srcs, plist));

				region = boost::dynamic_pointer_cast<MidiRegion> (rx);
				region->special_set_position (capture_info.front()->start);
			}


			catch (failed_constructor& err) {
				error << string_compose(_("%1: could not create region for complete midi file"), _name) << endmsg;
				/* XXX what now? */
			}

			_last_capture_sources.insert (_last_capture_sources.end(), srcs.begin(), srcs.end());

			_playlist->clear_changes ();
			_playlist->freeze ();

			/* Session frame time of the initial capture in this pass, which is where the source starts */
			framepos_t initial_capture = 0;
			if (!capture_info.empty()) {
				initial_capture = capture_info.front()->start;
			}

			for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {

				string region_name;

				RegionFactory::region_name (region_name, _write_source->name(), false);

				// cerr << _name << ": based on ci of " << (*ci)->start << " for " << (*ci)->frames << " add a region\n";

				try {
					PropertyList plist;

					/* start of this region is the offset between the start of its capture and the start of the whole pass */
					plist.add (Properties::start, (*ci)->start - initial_capture);
					plist.add (Properties::length, (*ci)->frames);
					plist.add (Properties::length_beats, converter.from((*ci)->frames));
					plist.add (Properties::name, region_name);

					boost::shared_ptr<Region> rx (RegionFactory::create (srcs, plist));
					region = boost::dynamic_pointer_cast<MidiRegion> (rx);
				}

				catch (failed_constructor& err) {
					error << _("MidiDiskstream: could not create region for captured midi!") << endmsg;
					continue; /* XXX is this OK? */
				}

				// cerr << "add new region, buffer position = " << buffer_position << " @ " << (*ci)->start << endl;

				i_am_the_modifier++;
				_playlist->add_region (region, (*ci)->start);
				i_am_the_modifier--;
			}

			_playlist->thaw ();
			_session.add_command (new StatefulDiffCommand(_playlist));

		} else {

			/* No data was recorded, so this capture will
			   effectively be aborted; do the same as we
			   do for an explicit abort.
			*/

			if (_write_source) {
				_write_source->mark_for_remove ();
				_write_source->drop_references ();
				_write_source.reset();
			}
 		}

	}

	use_new_write_source (0);

	for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		delete *ci;
	}

	capture_info.clear ();
	capture_start_frame = 0;

  no_capture_stuff_to_do:

	reset_tracker ();
}

void
MidiDiskstream::transport_looped (framepos_t transport_frame)
{
	if (was_recording) {

		// adjust the capture length knowing that the data will be recorded to disk
		// only necessary after the first loop where we're recording
		if (capture_info.size() == 0) {
			capture_captured += _capture_offset;

			if (_alignment_style == ExistingMaterial) {
				capture_captured += _session.worst_output_latency();
			} else {
				capture_captured += _roll_delay;
			}
		}

		finish_capture ();

		// the next region will start recording via the normal mechanism
		// we'll set the start position to the current transport pos
		// no latency adjustment or capture offset needs to be made, as that already happened the first time
		capture_start_frame = transport_frame;
		first_recordable_frame = transport_frame; // mild lie
		last_recordable_frame = max_framepos;
		was_recording = true;
	}

	if (!Config->get_seamless_loop()) {
		reset_tracker ();
	}
}

void
MidiDiskstream::finish_capture ()
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
	bool const rolling = _session.transport_speed() != 0.0f;

	g_atomic_int_set (&_record_enabled, 1);

	boost::shared_ptr<MidiPort> sp = _source_port.lock ();
	
	if (sp && Config->get_monitoring_model() == HardwareMonitoring) {
		sp->request_jack_monitors_input (!(_session.config.get_auto_input() && rolling));
	}

	RecordEnableChanged (); /* EMIT SIGNAL */
}

void
MidiDiskstream::disengage_record_enable ()
{
	g_atomic_int_set (&_record_enabled, 0);
	RecordEnableChanged (); /* EMIT SIGNAL */
}

XMLNode&
MidiDiskstream::get_state ()
{
	XMLNode& node (Diskstream::get_state());
	char buf[64];
	LocaleGuard lg (X_("POSIX"));

	node.add_property("channel-mode", enum_2_string(get_channel_mode()));
	snprintf (buf, sizeof(buf), "0x%x", get_channel_mask());
	node.add_property("channel-mask", buf);

	if (_write_source && _session.get_record_enabled()) {

		XMLNode* cs_child = new XMLNode (X_("CapturingSources"));
		XMLNode* cs_grandchild;

		cs_grandchild = new XMLNode (X_("file"));
		cs_grandchild->add_property (X_("path"), _write_source->path());
		cs_child->add_child_nocopy (*cs_grandchild);

		/* store the location where capture will start */

		Location* pi;

		if (_session.config.get_punch_in() && ((pi = _session.locations()->auto_punch_location()) != 0)) {
			snprintf (buf, sizeof (buf), "%" PRId64, pi->start());
		} else {
			snprintf (buf, sizeof (buf), "%" PRId64, _session.transport_frame());
		}

		cs_child->add_property (X_("at"), buf);
		node.add_child_nocopy (*cs_child);
	}

	return node;
}

int
MidiDiskstream::set_state (const XMLNode& node, int version)
{
	const XMLProperty* prop;
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLNode* capture_pending_node = 0;
	LocaleGuard lg (X_("POSIX"));

	/* prevent write sources from being created */

	in_set_state = true;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		assert ((*niter)->name() != IO::state_node_name);

		if ((*niter)->name() == X_("CapturingSources")) {
			capture_pending_node = *niter;
		}
	}

	if (Diskstream::set_state (node, version)) {
		return -1;
	}

	ChannelMode channel_mode = AllChannels;
	if ((prop = node.property ("channel-mode")) != 0) {
		channel_mode = ChannelMode (string_2_enum(prop->value(), channel_mode));
	}

	unsigned int channel_mask = 0xFFFF;
	if ((prop = node.property ("channel-mask")) != 0) {
		sscanf (prop->value().c_str(), "0x%x", &channel_mask);
		if (channel_mask & (~0xFFFF)) {
			warning << _("MidiDiskstream: XML property channel-mask out of range") << endmsg;
		}
	}


	if (capture_pending_node) {
		use_pending_capture_data (*capture_pending_node);
	}

	set_channel_mode (channel_mode, channel_mask);

	in_set_state = false;

	return 0;
}

int
MidiDiskstream::use_new_write_source (uint32_t n)
{
	if (!_session.writable() || !recordable()) {
		return 1;
	}

	assert(n == 0);

	_write_source.reset();

	try {
		_write_source = boost::dynamic_pointer_cast<SMFSource>(
			_session.create_midi_source_for_session (0, name ()));

		if (!_write_source) {
			throw failed_constructor();
		}
	}

	catch (failed_constructor &err) {
		error << string_compose (_("%1:%2 new capture file not initialized correctly"), _name, n) << endmsg;
		_write_source.reset();
		return -1;
	}

	return 0;
}

list<boost::shared_ptr<Source> >
MidiDiskstream::steal_write_sources()
{
	list<boost::shared_ptr<Source> > ret;

	/* put some data on the disk, even if its just a header for an empty file */
	boost::dynamic_pointer_cast<SMFSource> (_write_source)->ensure_disk_file ();

	/* never let it go away */
	_write_source->mark_nonremovable ();

	ret.push_back (_write_source);

	/* get a new one */

	use_new_write_source (0);

	return ret;
}

void
MidiDiskstream::reset_write_sources (bool mark_write_complete, bool /*force*/)
{
	if (!_session.writable() || !recordable()) {
		return;
	}

	if (_write_source && mark_write_complete) {
		_write_source->mark_streaming_write_completed ();
	}
	use_new_write_source (0);
}

void
MidiDiskstream::set_block_size (pframes_t /*nframes*/)
{
}

void
MidiDiskstream::allocate_temporary_buffers ()
{
}

void
MidiDiskstream::ensure_jack_monitors_input (bool yn)
{
	boost::shared_ptr<MidiPort> sp = _source_port.lock ();
	
	if (sp) {
		sp->ensure_jack_monitors_input (yn);
	}
}

void
MidiDiskstream::set_align_style_from_io ()
{
	if (_alignment_choice != Automatic) {
		return;
	}

	/* XXX Not sure what, if anything we can do with MIDI
	   as far as capture alignment etc.
	*/

	set_align_style (ExistingMaterial);
}


float
MidiDiskstream::playback_buffer_load () const
{
	/* For MIDI it's not trivial to differentiate the following two cases:
	   
	   1.  The playback buffer is empty because the system has run out of time to fill it.
	   2.  The playback buffer is empty because there is no more data on the playlist.

	   If we use a simple buffer load computation, we will report that the MIDI diskstream
	   cannot keep up when #2 happens, when in fact it can.  Since MIDI data rates
	   are so low compared to audio, just give a pretend answer here.
	*/
	
	return 1;
}

float
MidiDiskstream::capture_buffer_load () const
{
	/* We don't report playback buffer load, so don't report capture load either */
	
	return 1;
}

int
MidiDiskstream::use_pending_capture_data (XMLNode& /*node*/)
{
	return 0;
}

/** Writes playback events from playback_sample for nframes to dst, translating time stamps
 *  so that an event at playback_sample has time = 0
 */
void
MidiDiskstream::get_playback (MidiBuffer& dst, framecnt_t nframes)
{
	dst.clear();
	assert(dst.size() == 0);

#ifndef NDEBUG
	DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose (
		             "%1 MDS pre-read read %4..%5 from %2 write to %3\n", _name,
		             _playback_buf->get_read_ptr(), _playback_buf->get_write_ptr(), playback_sample, playback_sample + nframes));
//        cerr << "================\n";
//        _playback_buf->dump (cerr);
//        cerr << "----------------\n";

	const size_t events_read = _playback_buf->read (dst, playback_sample, playback_sample + nframes);
	DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose (
		             "%1 MDS events read %2 range %3 .. %4 rspace %5 wspace %6 r@%7 w@%8\n",
		             _name, events_read, playback_sample, playback_sample + nframes,
		             _playback_buf->read_space(), _playback_buf->write_space(),
			     _playback_buf->get_read_ptr(), _playback_buf->get_write_ptr()));
#else
	_playback_buf->read (dst, playback_sample, playback_sample + nframes);
#endif

	g_atomic_int_add (&_frames_read_from_ringbuffer, nframes);
}

bool
MidiDiskstream::set_name (string const & name)
{
	Diskstream::set_name (name);

	/* get a new write source so that its name reflects the new diskstream name */
	use_new_write_source (0);

	return true;
}

boost::shared_ptr<MidiBuffer>
MidiDiskstream::get_gui_feed_buffer () const
{
	boost::shared_ptr<MidiBuffer> b (new MidiBuffer (AudioEngine::instance()->raw_buffer_size (DataType::MIDI)));
	
	Glib::Mutex::Lock lm (_gui_feed_buffer_mutex);
	b->copy (_gui_feed_buffer);
	return b;
}

void
MidiDiskstream::reset_tracker ()
{
	_playback_buf->reset_tracker ();

	boost::shared_ptr<MidiPlaylist> mp (midi_playlist());

	if (mp) {
		mp->clear_note_trackers ();
	}
}
