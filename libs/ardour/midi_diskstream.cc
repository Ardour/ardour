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

#include "pbd/error.h"
#include "pbd/ffs.h"
#include "pbd/basename.h"
#include <glibmm/threads.h>
#include "pbd/xml++.h"
#include "pbd/memento_command.h"
#include "pbd/enumwriter.h"
#include "pbd/stateful_diff_command.h"
#include "pbd/stacktrace.h"

#include "ardour/audioengine.h"
#include "ardour/butler.h"
#include "ardour/debug.h"
#include "ardour/io.h"
#include "ardour/midi_diskstream.h"
#include "ardour/midi_model.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_port.h"
#include "ardour/midi_region.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_track.h"
#include "ardour/playlist_factory.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/smf_source.h"
#include "ardour/types.h"
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
	, _frames_pending_write(0)
	, _num_captured_loops(0)
	, _accumulated_capture_offset(0)
	, _gui_feed_buffer(AudioEngine::instance()->raw_buffer_size (DataType::MIDI))
{
	in_set_state = true;

	init ();
	use_new_playlist ();
	use_new_write_source (0);

	in_set_state = false;

	if (destructive()) {
		throw failed_constructor();
	}
}

MidiDiskstream::MidiDiskstream (Session& sess, const XMLNode& node)
	: Diskstream(sess, node)
	, _playback_buf(0)
	, _capture_buf(0)
	, _note_mode(Sustained)
	, _frames_written_to_ringbuffer(0)
	, _frames_read_from_ringbuffer(0)
	, _frames_pending_write(0)
	, _num_captured_loops(0)
	, _accumulated_capture_offset(0)
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
	interpolation.add_channel_to (0,0);
}

MidiDiskstream::~MidiDiskstream ()
{
	Glib::Threads::Mutex::Lock lm (state_lock);
	delete _playback_buf;
	delete _capture_buf;
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
		Glib::Threads::Mutex::Lock lm (state_lock);

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

	g_atomic_int_set(const_cast<gint*> (&_frames_pending_write), 0);
	g_atomic_int_set(const_cast<gint*> (&_num_captured_loops), 0);
}

int
MidiDiskstream::find_and_use_playlist (const string& name)
{
	boost::shared_ptr<MidiPlaylist> playlist;

	if ((playlist = boost::dynamic_pointer_cast<MidiPlaylist> (_session.playlists->by_name (name))) == 0) {
		playlist = boost::dynamic_pointer_cast<MidiPlaylist> (PlaylistFactory::create (DataType::MIDI, _session, name));
	}

	if (!playlist) {
		error << string_compose(_("MidiDiskstream: Playlist \"%1\" isn't a midi playlist"), name) << endmsg;
		return -1;
	}

	return use_playlist (playlist);
}

int
MidiDiskstream::use_playlist (boost::shared_ptr<Playlist> playlist)
{
	if (boost::dynamic_pointer_cast<MidiPlaylist>(playlist)) {
		Diskstream::use_playlist(playlist);
	}

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
	return yn ? -1 : 0;
}

void
MidiDiskstream::set_note_mode (NoteMode m)
{
	_note_mode = m;
	midi_playlist()->set_note_mode(m);
	if (_write_source && _write_source->model())
		_write_source->model()->set_note_mode(m);
}

/** Get the start, end, and length of a location "atomically".
 *
 * Note: Locations don't get deleted, so all we care about when I say "atomic"
 * is that we are always pointing to the same one and using start/length values
 * obtained just once.  Use this function to achieve this since location being
 * a parameter achieves this.
 */
static void
get_location_times(const Location* location,
                   framepos_t*     start,
                   framepos_t*     end,
                   framepos_t*     length)
{
	if (location) {
		*start  = location->start();
		*end    = location->end();
		*length = *end - *start;
	}
}

int
MidiDiskstream::process (BufferSet& bufs, framepos_t transport_frame, pframes_t nframes, framecnt_t& playback_distance, bool need_disk_signal)
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

	Glib::Threads::Mutex::Lock sm (state_lock, Glib::Threads::TRY_LOCK);

	if (!sm.locked()) {
		return 1;
	}

	const Location* const loop_loc    = loop_location;
	framepos_t            loop_start  = 0;
	framepos_t            loop_end    = 0;
	framepos_t            loop_length = 0;
	get_location_times(loop_loc, &loop_start, &loop_end, &loop_length);

	adjust_capture_position = 0;

	if (nominally_recording || (re && was_recording && _session.get_record_enabled() && _session.config.get_punch_in())) {
		Evoral::OverlapType ot = Evoral::coverage (first_recordable_frame, last_recordable_frame, transport_frame, transport_frame + nframes);
		// XXX should this be transport_frame + nframes - 1 ? coverage() expects its parameter ranges to include their end points

		calculate_record_range(ot, transport_frame, nframes, rec_nframes, rec_offset);
		/* For audio: not writing frames to the capture ringbuffer offsets
		 * the recording. For midi: we need to keep track of the record range
		 * and subtract the accumulated difference from the event time.
		 */
		if (rec_nframes) {
			_accumulated_capture_offset += rec_offset;
		} else {
			_accumulated_capture_offset += nframes;
		}

		if (rec_nframes && !was_recording) {
			if (loop_loc) {
				/* Loop recording, so pretend the capture started at the loop
				   start rgardless of what time it is now, so the source starts
				   at the loop start and can handle time wrapping around.
				   Otherwise, start the source right now as usual.
				*/
				capture_captured    = transport_frame - loop_start;
				capture_start_frame = loop_start;
			}
			_write_source->mark_write_starting_now(
				capture_start_frame, capture_captured, loop_length);
			g_atomic_int_set(const_cast<gint*> (&_frames_pending_write), 0);
			g_atomic_int_set(const_cast<gint*> (&_num_captured_loops), 0);
			was_recording = true;
		}
	}

	if (can_record && !_last_capture_sources.empty()) {
		_last_capture_sources.clear ();
	}

	if (nominally_recording || rec_nframes) {
		// Pump entire port buffer into the ring buffer (TODO: split cycles?)
		MidiBuffer&        buf    = sp->get_midi_buffer(nframes);
		MidiTrack*         mt     = dynamic_cast<MidiTrack*>(_track);
		MidiChannelFilter* filter = mt ? &mt->capture_filter() : NULL;

		for (MidiBuffer::iterator i = buf.begin(); i != buf.end(); ++i) {
			Evoral::MIDIEvent<MidiBuffer::TimeType> ev(*i, false);
			if (ev.time() + rec_offset > rec_nframes) {
				break;
			}
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
			/* Write events to the capture buffer in frames from session start,
			   but ignoring looping so event time progresses monotonically.
			   The source knows the loop length so it knows exactly where the
			   event occurs in the series of recorded loops and can implement
			   any desirable behaviour.  We don't want to send event with
			   transport time here since that way the source can not
			   reconstruct their actual time; future clever MIDI looping should
			   probably be implemented in the source instead of here.
			*/
			const framecnt_t loop_offset = _num_captured_loops * loop_length;
			const framepos_t event_time = transport_frame + loop_offset - _accumulated_capture_offset + ev.time();
			if (event_time < 0 || event_time < first_recordable_frame) {
				/* Event out of range, skip */
				continue;
			}

			if (!filter || !filter->filter(ev.buffer(), ev.size())) {
				_capture_buf->write(event_time, ev.type(), ev.size(), ev.buffer());
			}
		}
		g_atomic_int_add(const_cast<gint*>(&_frames_pending_write), nframes);

		if (buf.size() != 0) {
			Glib::Threads::Mutex::Lock lm (_gui_feed_buffer_mutex, Glib::Threads::TRY_LOCK);

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
		_accumulated_capture_offset = 0;

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

	} else if (_actual_speed != 1.0f && _target_speed > 0) {

		interpolation.set_speed (_target_speed);

		playback_distance = interpolation.distance  (nframes);

	} else {
		playback_distance = nframes;
	}

	if (need_disk_signal) {
		/* copy the diskstream data to all output buffers */
		
		MidiBuffer& mbuf (bufs.get_midi (0));
		get_playback (mbuf, playback_distance);

		/* leave the audio count alone */
		ChanCount cnt (DataType::MIDI, 1);
		cnt.set (DataType::AUDIO, bufs.count().n_audio());
		bufs.set_count (cnt);

		/* vari-speed */
		if (_target_speed > 0 && _actual_speed != 1.0f) {
			MidiBuffer& mbuf (bufs.get_midi (0));
			for (MidiBuffer::iterator i = mbuf.begin(); i != mbuf.end(); ++i) {
				MidiBuffer::TimeType *tme = i.timeptr();
				*tme = (*tme) * nframes / playback_distance;
			}
		}
	}

	return 0;
}

frameoffset_t
MidiDiskstream::calculate_playback_distance (pframes_t nframes)
{
	frameoffset_t playback_distance = nframes;

	if (!record_enabled() && _actual_speed != 1.0f && _actual_speed > 0.f) {
		interpolation.set_speed (_target_speed);
		playback_distance = interpolation.distance (nframes, false);
	}

	if (_actual_speed < 0.0) {
		return -playback_distance;
	} else {
		return playback_distance;
	}
}

bool
MidiDiskstream::commit (framecnt_t playback_distance)
{
	bool need_butler = false;

	if (!_io || !_io->active()) {
		return false;
	}

	if (_actual_speed < 0.0) {
		playback_sample -= playback_distance;
	} else {
		playback_sample += playback_distance;
	}

	if (adjust_capture_position != 0) {
		capture_captured += adjust_capture_position;
		adjust_capture_position = 0;
	}

	uint32_t frames_read = g_atomic_int_get(const_cast<gint*>(&_frames_read_from_ringbuffer));
	uint32_t frames_written = g_atomic_int_get(const_cast<gint*>(&_frames_written_to_ringbuffer));

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
	
	/* furthermore..
	 *
	 * Doing heavy GUI operations[1] can stall also the butler.
	 * The RT-thread meanwhile will happily continue and
	 * ‘frames_read’ (from buffer to output) will become larger
	 * than ‘frames_written’ (from disk to buffer).
	 *
	 * The disk-stream is now behind..
	 *
	 * In those cases the butler needs to be summed to refill the buffer (done now)
	 * AND we need to skip (frames_read - frames_written). ie remove old events
	 * before playback_sample from the rinbuffer.
	 *
	 * [1] one way to do so is described at #6170.
	 * For me just popping up the context-menu on a MIDI-track header
	 * of a track with a large (think beethoven :) midi-region also did the
	 * trick. The playhead stalls for 2 or 3 sec, until the context-menu shows.
	 *
	 * In both cases the root cause is that redrawing MIDI regions on the GUI is still very slow
	 * and can stall
	 */
	if (frames_read <= frames_written) {
		if ((frames_written - frames_read) + playback_distance < midi_readahead) {
			need_butler = true;
		}
	} else {
		need_butler = true;
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
	/* Clear the playback buffer contents.  This is safe as long as the butler
	   thread is suspended, which it should be. */
	_playback_buf->reset ();
	_playback_buf->reset_tracker ();

	g_atomic_int_set (&_frames_read_from_ringbuffer, 0);
	g_atomic_int_set (&_frames_written_to_ringbuffer, 0);

	/* Resolve all currently active notes in the playlist.  This is more
	   aggressive than it needs to be: ideally we would only resolve what is
	   absolutely necessary, but this seems difficult and/or impossible without
	   having the old data or knowing what change caused the overwrite. */
	midi_playlist()->resolve_note_trackers (*_playback_buf, overwrite_frame);

	read (overwrite_frame, disk_read_chunk_frames, false);
	file_frame = overwrite_frame; // it was adjusted by ::read()
	overwrite_queued = false;
	_pending_overwrite = false;

	return 0;
}

int
MidiDiskstream::seek (framepos_t frame, bool complete_refill)
{
	Glib::Threads::Mutex::Lock lm (state_lock);
	int ret = -1;

	if (g_atomic_int_get (&_frames_read_from_ringbuffer) == 0) {
		/* we haven't read anything since the last seek,
		   so flush all note trackers to prevent
		   wierdness
		*/
		reset_tracker ();
	}

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
	framecnt_t this_read   = 0;
	bool       reloop      = false;
	framepos_t loop_end    = 0;
	framepos_t loop_start  = 0;
	framecnt_t loop_length = 0;
	Location*  loc         = 0;

	MidiTrack*         mt     = dynamic_cast<MidiTrack*>(_track);
	MidiChannelFilter* filter = mt ? &mt->playback_filter() : NULL;

	if (!reversed) {

		loc = loop_location;
		get_location_times(loc, &loop_start, &loop_end, &loop_length);

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

		if (midi_playlist()->read (*_playback_buf, start, this_read, 0, filter) != this_read) {
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

	/* no space to write */
	if (_playback_buf->write_space() == 0) {
		return 0;
	}

	uint32_t frames_read = g_atomic_int_get(&_frames_read_from_ringbuffer);
	uint32_t frames_written = g_atomic_int_get(&_frames_written_to_ringbuffer);
	if ((frames_read < frames_written) && (frames_written - frames_read) >= midi_readahead) {
		return 0;
	}

	framecnt_t to_read = midi_readahead - ((framecnt_t)frames_written - (framecnt_t)frames_read);

	//cout << "MDS read for midi_readahead " << to_read << "  rb_contains: "
	//	<< frames_written - frames_read << endl;

	to_read = min (to_read, (framecnt_t) (max_framepos - file_frame));
	to_read = min (to_read, (framecnt_t) write_space);

	if (read (file_frame, to_read, reversed)) {
		ret = -1;
	}

	return ret;
}

/** Flush pending data to disk.
 *
 * Important note: this function will write *AT MOST* disk_write_chunk_frames
 * of data to disk. it will never write more than that.  If it writes that
 * much and there is more than that waiting to be written, it will return 1,
 * otherwise 0 on success or -1 on failure.
 *
 * If there is less than disk_write_chunk_frames to be written, no data will be
 * written at all unless @a force_flush is true.
 */
int
MidiDiskstream::do_flush (RunContext /*context*/, bool force_flush)
{
	framecnt_t to_write;
	int32_t ret = 0;

	if (!_write_source) {
		return 0;
	}

	const framecnt_t total = g_atomic_int_get(const_cast<gint*> (&_frames_pending_write));

	if (total == 0 || 
	    _capture_buf->read_space() == 0 || 
	    (!force_flush && (total < disk_write_chunk_frames) && was_recording)) {
		goto out;
	}

	/* if there are 2+ chunks of disk i/o possible for
	   this track), let the caller know so that it can arrange
	   for us to be called again, ASAP.

	   if we are forcing a flush, then if there is* any* extra
	   work, let the caller know.

	   if we are no longer recording and there is any extra work,
	   let the caller know too.
	   */

	if (total >= 2 * disk_write_chunk_frames || ((force_flush || !was_recording) && total > disk_write_chunk_frames)) {
		ret = 1;
	}

	if (force_flush) {
		/* push out everything we have, right now */
		to_write = max_framecnt;
	} else {
		to_write = disk_write_chunk_frames;
	}

	if (record_enabled() && ((total > disk_write_chunk_frames) || force_flush)) {
		Source::Lock lm(_write_source->mutex());
		if (_write_source->midi_write (lm, *_capture_buf, get_capture_start_frame (0), to_write) != to_write) {
			error << string_compose(_("MidiDiskstream %1: cannot write to disk"), id()) << endmsg;
			return -1;
		} 
		g_atomic_int_add(const_cast<gint*> (&_frames_pending_write), -to_write);
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
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

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

		framecnt_t total_capture = 0;
		for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
			total_capture += (*ci)->frames;
		}

		if (_write_source->length (capture_info.front()->start) != 0) {

			/* phew, we have data */

			Source::Lock source_lock(_write_source->mutex());

			/* figure out the name for this take */

			srcs.push_back (_write_source);

			_write_source->set_timeline_position (capture_info.front()->start);
			_write_source->set_captured_for (_name);

			/* set length in beats to entire capture length */

			BeatsFramesConverter converter (_session.tempo_map(), capture_info.front()->start);
			const Evoral::Beats total_capture_beats = converter.from (total_capture);
			_write_source->set_length_beats (total_capture_beats);

			/* flush to disk: this step differs from the audio path,
			   where all the data is already on disk.
			*/

			_write_source->mark_midi_streaming_write_completed (source_lock, Evoral::Sequence<Evoral::Beats>::ResolveStuckNotes, total_capture_beats);

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

				DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 capture start @ %2 length %3 add new region %4\n",
							_name, (*ci)->start, (*ci)->frames, region_name));


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
MidiDiskstream::transport_looped (framepos_t)
{
	/* Here we only keep track of the number of captured loops so monotonic
	   event times can be delivered to the write source in process().  Trying
	   to be clever here is a world of trouble, it is better to simply record
	   the input in a straightforward non-destructive way.  In the future when
	   we want to implement more clever MIDI looping modes it should be done in
	   the Source and/or entirely after the capture is finished.
	*/
	if (was_recording) {
		g_atomic_int_add(const_cast<gint*> (&_num_captured_loops), 1);
	}
}

void
MidiDiskstream::finish_capture ()
{
	was_recording = false;

	if (capture_captured == 0) {
		return;
	}

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
	if (!recordable() || !_session.record_enabling_legal() || _io->n_ports().n_midi() == 0) {
		return;
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
		
		RecordEnableChanged (); /* EMIT SIGNAL */
	}
}

bool
MidiDiskstream::prep_record_enable ()
{
	if (!recordable() || !_session.record_enabling_legal() || _io->n_ports().n_midi() == 0) {
		return false;
	}

	bool const rolling = _session.transport_speed() != 0.0f;

	boost::shared_ptr<MidiPort> sp = _source_port.lock ();
	
	if (sp && Config->get_monitoring_model() == HardwareMonitoring) {
		sp->request_input_monitoring (!(_session.config.get_auto_input() && rolling));
	}

	return true;
}

bool
MidiDiskstream::prep_record_disable ()
{

	return true;
}

XMLNode&
MidiDiskstream::get_state ()
{
	XMLNode& node (Diskstream::get_state());
	char buf[64];
	LocaleGuard lg (X_("C"));

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
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	XMLNode* capture_pending_node = 0;
	LocaleGuard lg (X_("C"));

	/* prevent write sources from being created */

	in_set_state = true;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == X_("CapturingSources")) {
			capture_pending_node = *niter;
		}
	}

	if (Diskstream::set_state (node, version)) {
		return -1;
	}

	if (capture_pending_node) {
		use_pending_capture_data (*capture_pending_node);
	}

	in_set_state = false;

	return 0;
}

int
MidiDiskstream::use_new_write_source (uint32_t n)
{
	if (!_session.writable() || !recordable()) {
		return 1;
	}

	_accumulated_capture_offset = 0;
	_write_source.reset();

	try {
		_write_source = boost::dynamic_pointer_cast<SMFSource>(
			_session.create_midi_source_for_session (write_source_name ()));

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
/**
 * We want to use the name of the existing write source (the one that will be
 * used by the next capture) for another purpose. So change the name of the
 * current source, and return its current name.
 *
 * Return an empty string if the change cannot be accomplished.
 */
std::string
MidiDiskstream::steal_write_source_name ()
{
	string our_old_name = _write_source->name();

	/* this will bump the name of the current write source to the next one
	 * (e.g. "MIDI 1-1" gets renamed to "MIDI 1-2"), thus leaving the
	 * current write source name (e.g. "MIDI 1-1" available). See the
	 * comments in Session::create_midi_source_by_stealing_name() about why
	 * we do this.
	 */

	try {
		string new_path = _session.new_midi_source_path (name());
		
		if (_write_source->rename (new_path)) {
			return string();
		}
	} catch (...) {
		return string ();
	}
	
	return our_old_name;
}

void
MidiDiskstream::reset_write_sources (bool mark_write_complete, bool /*force*/)
{
	if (!_session.writable() || !recordable()) {
		return;
	}

	if (_write_source && mark_write_complete) {
		Source::Lock lm(_write_source->mutex());
		_write_source->mark_streaming_write_completed (lm);
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
MidiDiskstream::ensure_input_monitoring (bool yn)
{
	boost::shared_ptr<MidiPort> sp = _source_port.lock ();
	
	if (sp) {
		sp->ensure_input_monitoring (yn);
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

void
MidiDiskstream::flush_playback (framepos_t start, framepos_t end)
{
	_playback_buf->flush (start, end);
	g_atomic_int_add (&_frames_read_from_ringbuffer, end - start);
}

/** Writes playback events from playback_sample for nframes to dst, translating time stamps
 *  so that an event at playback_sample has time = 0
 */
void
MidiDiskstream::get_playback (MidiBuffer& dst, framecnt_t nframes)
{
	dst.clear();

	Location* loc = loop_location;

	DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose (
		             "%1 MDS pre-read read %8 @ %4..%5 from %2 write to %3, LOOPED ? %6-%7\n", _name,
		             _playback_buf->get_read_ptr(), _playback_buf->get_write_ptr(), playback_sample, playback_sample + nframes, 
			     (loc ? loc->start() : -1), (loc ? loc->end() : -1), nframes));

        // cerr << "================\n";
        // _playback_buf->dump (cerr);
        // cerr << "----------------\n";

	size_t events_read = 0;	

	if (loc) {
		framepos_t effective_start;

		if (playback_sample >= loc->end()) {
			effective_start = loc->start() + ((playback_sample - loc->end()) % loc->length());
		} else {
			effective_start = playback_sample;
		}
		
		DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("looped, effective start adjusted to %1\n", effective_start));

		if (effective_start == loc->start()) {
			/* We need to turn off notes that may extend
			   beyond the loop end.
			*/

			_playback_buf->resolve_tracker (dst, 0);
		}

		_playback_buf->skip_to (effective_start);

		if (loc->end() >= effective_start && loc->end() < effective_start + nframes) {
			/* end of loop is within the range we are reading, so
			   split the read in two, and lie about the location
			   for the 2nd read
			*/
			framecnt_t first, second;

			first = loc->end() - effective_start;
			second = nframes - first;

			DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("loop read for eff %1 end %2: %3 and %4\n",
									      effective_start, loc->end(), first, second));

			if (first) {
				DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("loop read #1, from %1 for %2\n",
										      effective_start, first));
				events_read = _playback_buf->read (dst, effective_start, first);
			} 

			if (second) {
				DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("loop read #2, from %1 for %2\n",
										      loc->start(), second));
				events_read += _playback_buf->read (dst, loc->start(), second);
			}
								    
		} else {
			DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("loop read #3, adjusted start as %1 for %2\n",
									      effective_start, nframes));
			events_read = _playback_buf->read (dst, effective_start, effective_start + nframes);
		}
	} else {
		_playback_buf->skip_to (playback_sample);
		events_read = _playback_buf->read (dst, playback_sample, playback_sample + nframes);
	}

	DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose (
		             "%1 MDS events read %2 range %3 .. %4 rspace %5 wspace %6 r@%7 w@%8\n",
		             _name, events_read, playback_sample, playback_sample + nframes,
		             _playback_buf->read_space(), _playback_buf->write_space(),
			     _playback_buf->get_read_ptr(), _playback_buf->get_write_ptr()));

	g_atomic_int_add (&_frames_read_from_ringbuffer, nframes);
}

bool
MidiDiskstream::set_name (string const & name)
{
	if (_name == name) {
		return true;
	}
	Diskstream::set_name (name);

	/* get a new write source so that its name reflects the new diskstream name */
	use_new_write_source (0);

	return true;
}

bool
MidiDiskstream::set_write_source_name (const std::string& str) {
	if (_write_source_name == str) {
		return true;
	}
	Diskstream::set_write_source_name (str);
	if (_write_source_name == name()) {
		return true;
	}
	use_new_write_source (0);
	return true;
}

boost::shared_ptr<MidiBuffer>
MidiDiskstream::get_gui_feed_buffer () const
{
	boost::shared_ptr<MidiBuffer> b (new MidiBuffer (AudioEngine::instance()->raw_buffer_size (DataType::MIDI)));
	
	Glib::Threads::Mutex::Lock lm (_gui_feed_buffer_mutex);
	b->copy (_gui_feed_buffer);
	return b;
}

void
MidiDiskstream::reset_tracker ()
{
	_playback_buf->reset_tracker ();

	boost::shared_ptr<MidiPlaylist> mp (midi_playlist());

	if (mp) {
		mp->reset_note_trackers ();
	}
}

void
MidiDiskstream::resolve_tracker (Evoral::EventSink<framepos_t>& buffer, framepos_t time)
{
	_playback_buf->resolve_tracker(buffer, time);

	boost::shared_ptr<MidiPlaylist> mp (midi_playlist());

	if (mp) {
		mp->reset_note_trackers ();
	}
}


boost::shared_ptr<MidiPlaylist>
MidiDiskstream::midi_playlist ()
{
	return boost::dynamic_pointer_cast<MidiPlaylist>(_playlist);
}
