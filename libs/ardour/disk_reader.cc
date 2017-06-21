/*
    Copyright (C) 2009-2016 Paul Davis

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

#include "pbd/enumwriter.h"
#include "pbd/i18n.h"
#include "pbd/memento_command.h"

#include "ardour/audioengine.h"
#include "ardour/audioplaylist.h"
#include "ardour/audio_buffer.h"
#include "ardour/butler.h"
#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_playlist.h"
#include "ardour/pannable.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

ARDOUR::framecnt_t DiskReader::_chunk_frames = default_chunk_frames ();
PBD::Signal0<void> DiskReader::Underrun;
Sample* DiskReader::_mixdown_buffer = 0;
gain_t* DiskReader::_gain_buffer = 0;
framecnt_t DiskReader::midi_readahead = 4096;

DiskReader::DiskReader (Session& s, string const & str, DiskIOProcessor::Flag f)
	: DiskIOProcessor (s, str, f)
	, _roll_delay (0)
	, overwrite_frame (0)
        , overwrite_offset (0)
        , _pending_overwrite (false)
        , overwrite_queued (false)
	, _gui_feed_buffer (AudioEngine::instance()->raw_buffer_size (DataType::MIDI))
{
}

DiskReader::~DiskReader ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("DiskReader %1 @ %2 deleted\n", _name, this));

	for (uint32_t n = 0; n < DataType::num_types; ++n) {
		if (_playlists[n]) {
			_playlists[n]->release ();
		}
	}

	{
		RCUWriter<ChannelList> writer (channels);
		boost::shared_ptr<ChannelList> c = writer.get_copy();

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			delete *chan;
		}

		c->clear();
	}

	channels.flush ();

	delete _midi_buf;
}

void
DiskReader::allocate_working_buffers()
{
	/* with varifill buffer refilling, we compute the read size in bytes (to optimize
	   for disk i/o bandwidth) and then convert back into samples. These buffers
	   need to reflect the maximum size we could use, which is 4MB reads, or 2M samples
	   using 16 bit samples.
	*/
	_mixdown_buffer       = new Sample[2*1048576];
	_gain_buffer          = new gain_t[2*1048576];
}

void
DiskReader::free_working_buffers()
{
	delete [] _mixdown_buffer;
	delete [] _gain_buffer;
	_mixdown_buffer       = 0;
	_gain_buffer          = 0;
}

framecnt_t
DiskReader::default_chunk_frames()
{
	return 65536;
}

bool
DiskReader::set_name (string const & str)
{
	string my_name = X_("reader:");
	my_name += str;

	if (_name != my_name) {
		SessionObject::set_name (my_name);
	}

	return true;
}

void
DiskReader::set_roll_delay (ARDOUR::framecnt_t nframes)
{
	_roll_delay = nframes;
}

XMLNode&
DiskReader::state (bool full)
{
	XMLNode& node (DiskIOProcessor::state (full));
	node.set_property(X_("type"), X_("diskreader"));
	return node;
}

int
DiskReader::set_state (const XMLNode& node, int version)
{
	if (DiskIOProcessor::set_state (node, version)) {
		return -1;
	}

	return 0;
}

void
DiskReader::realtime_handle_transport_stopped ()
{
	realtime_speed_change ();
}

void
DiskReader::realtime_locate ()
{
}

float
DiskReader::buffer_load () const
{
	/* Note: for MIDI it's not trivial to differentiate the following two cases:

	   1.  The playback buffer is empty because the system has run out of time to fill it.
	   2.  The playback buffer is empty because there is no more data on the playlist.

	   If we use a simple buffer load computation, we will report that the MIDI diskstream
	   cannot keep up when #2 happens, when in fact it can.  Since MIDI data rates
	   are so low compared to audio, just use the audio value here.
	*/

	boost::shared_ptr<ChannelList> c = channels.reader();

	if (c->empty ()) {
		/* no channels, so no buffers, so completely full and ready to playback, sir! */
		return 1.0;
	}

	PBD::RingBufferNPT<Sample> * b = c->front()->buf;
	return (float) ((double) b->read_space() / (double) b->bufsize());
}

void
DiskReader::adjust_buffering ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->resize (_session.butler()->audio_diskstream_playback_buffer_size());
	}
}

void
DiskReader::playlist_changed (const PropertyChange&)
{
	playlist_modified ();
}

void
DiskReader::playlist_modified ()
{
	if (!i_am_the_modifier && !overwrite_queued) {
		_session.request_overwrite_buffer (_route);
		overwrite_queued = true;
	}
}

int
DiskReader::use_playlist (DataType dt, boost::shared_ptr<Playlist> playlist)
{
        bool prior_playlist = false;

        if (_playlists[dt]) {
	        prior_playlist = true;
        }

        if (DiskIOProcessor::use_playlist (dt, playlist)) {
		return -1;
	}

	/* don't do this if we've already asked for it *or* if we are setting up
	   the diskstream for the very first time - the input changed handling will
	   take care of the buffer refill.
	*/

        if (!overwrite_queued && (prior_playlist || _session.loading())) {
		_session.request_overwrite_buffer (_route);
		overwrite_queued = true;
	}

	return 0;
}

void
DiskReader::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame,
                 double speed, pframes_t nframes, bool result_required)
{
	uint32_t n;
	boost::shared_ptr<ChannelList> c = channels.reader();
	ChannelList::iterator chan;
	frameoffset_t playback_distance = nframes;
	MonitorState ms = _route->monitoring_state ();

	if (_active) {
		if (!_pending_active) {
			_active = false;
			return;
		}
	} else {
		if (_pending_active) {
			_active = true;
		} else {
			return;
		}
	}

	_need_butler = false;

	if (speed == 0.0 && (ms == MonitoringDisk)) {
		/* stopped. Don't accidentally pass any data from disk
		 * into our outputs (e.g. via interpolation)
		 */
		bufs.silence (nframes, 0);
		return;
	}

	if (speed != 1.0f && speed != -1.0f) {
		interpolation.set_speed (speed);
		midi_interpolation.set_speed (speed);
		playback_distance = midi_interpolation.distance (nframes);
	}

	if (speed < 0.0) {
		playback_distance = -playback_distance;
	}

	BufferSet& scratch_bufs (_session.get_scratch_buffers (bufs.count()));

	if (!result_required || ((ms & MonitoringDisk) == 0)) {

		/* no need for actual disk data, just advance read pointer and return */

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			(*chan)->buf->increment_read_ptr (playback_distance);
		}

	} else {

		/* we need audio data from disk */

		size_t n_buffers = bufs.count().n_audio();
		size_t n_chans = c->size();
		gain_t scaling;

		if (n_chans > n_buffers) {
			scaling = ((float) n_buffers)/n_chans;
		} else {
			scaling = 1.0;
		}

		for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {

			ChannelInfo* chaninfo (*chan);
			AudioBuffer& buf (bufs.get_audio (n%n_buffers));
			Sample* disk_signal = 0; /* assignment not really needed but it keeps the compiler quiet and helps track bugs */

			if (ms & MonitoringInput) {
				/* put disk stream in scratch buffer, blend at end */
				disk_signal = scratch_bufs.get_audio(n).data ();
			} else {
				/* no input stream needed, just overwrite buffers */
				disk_signal = buf.data ();
			}

			chaninfo->buf->get_read_vector (&(*chan)->rw_vector);

			if (playback_distance <= (framecnt_t) chaninfo->rw_vector.len[0]) {

				if (fabsf (speed) != 1.0f) {
					(void) interpolation.interpolate (
						n, nframes,
						chaninfo->rw_vector.buf[0],
						disk_signal);
				} else if (speed != 0.0) {
					memcpy (disk_signal, chaninfo->rw_vector.buf[0], sizeof (Sample) * playback_distance);
				}

			} else {

				const framecnt_t total = chaninfo->rw_vector.len[0] + chaninfo->rw_vector.len[1];

				if (playback_distance <= total) {

					/* We have enough samples, but not in one lump.
					 */

					if (fabsf (speed) != 1.0f) {
						interpolation.interpolate (n, chaninfo->rw_vector.len[0],
						                           chaninfo->rw_vector.buf[0],
						                           disk_signal);
						disk_signal += chaninfo->rw_vector.len[0];
						interpolation.interpolate (n, playback_distance - chaninfo->rw_vector.len[0],
						                           chaninfo->rw_vector.buf[1],
						                           disk_signal);
					} else if (speed != 0.0) {
						memcpy (disk_signal,
						        chaninfo->rw_vector.buf[0],
						        chaninfo->rw_vector.len[0] * sizeof (Sample));
						disk_signal += chaninfo->rw_vector.len[0];
						memcpy (disk_signal,
						        chaninfo->rw_vector.buf[1],
						        (playback_distance - chaninfo->rw_vector.len[0]) * sizeof (Sample));
					}

				} else {

					cerr << _name << " Need " << playback_distance << " total = " << total << endl;
					cerr << "underrun for " << _name << endl;
					DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 underrun in %2, total space = %3\n",
					                                            DEBUG_THREAD_SELF, name(), total));
					Underrun ();
					return;

				}
			}

			if (scaling != 1.0f && speed != 0.0) {
				apply_gain_to_buffer (disk_signal, nframes, scaling);
			}

			chaninfo->buf->increment_read_ptr (playback_distance);

			if ((speed != 0.0) && (ms & MonitoringInput)) {
				/* mix the disk signal into the input signal (already in bufs) */
				mix_buffers_no_gain (buf.data(), disk_signal, speed == 0.0 ? nframes : playback_distance);
			}
		}
	}

	/* MIDI data handling */

	if (!_session.declick_out_pending()) {
		if (ms & MonitoringDisk) {
			get_midi_playback (bufs.get_midi (0), playback_distance, ms, scratch_bufs, speed, playback_distance);
		}
	}

	if (speed < 0.0) {
		playback_sample -= playback_distance;
	} else {
		playback_sample += playback_distance;
	}

	if (_playlists[DataType::AUDIO]) {
		if (!c->empty()) {
			if (_slaved) {
				if (c->front()->buf->write_space() >= c->front()->buf->bufsize() / 2) {
					DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: slaved, write space = %2 of %3\n", name(), c->front()->buf->write_space(),
					                                            c->front()->buf->bufsize()));
					_need_butler = true;
				}
			} else {
				if ((framecnt_t) c->front()->buf->write_space() >= _chunk_frames) {
					DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: write space = %2 of %3\n", name(), c->front()->buf->write_space(),
					                                            _chunk_frames));
					_need_butler = true;
				}
			}
		}
	}

	if (_playlists[DataType::MIDI]) {
		/* MIDI butler needed part */

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
				_need_butler = true;
			}
		} else {
			_need_butler = true;
		}

	}

	DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 reader run, needs butler = %2\n", name(), _need_butler));
}

void
DiskReader::set_pending_overwrite (bool yn)
{
	/* called from audio thread, so we can use the read ptr and playback sample as we wish */

	_pending_overwrite = yn;

	overwrite_frame = playback_sample;

	boost::shared_ptr<ChannelList> c = channels.reader ();
	if (!c->empty ()) {
		overwrite_offset = c->front()->buf->get_read_ptr();
	}
}

int
DiskReader::overwrite_existing_buffers ()
{
	int ret = -1;

	boost::shared_ptr<ChannelList> c = channels.reader();

	overwrite_queued = false;

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1 overwriting existing buffers at %2\n", overwrite_frame));

	if (!c->empty ()) {

		/* AUDIO */

		const bool reversed = _session.transport_speed() < 0.0f;

		/* assume all are the same size */
		framecnt_t size = c->front()->buf->bufsize();

		std::auto_ptr<Sample> mixdown_buffer (new Sample[size]);
		std::auto_ptr<float> gain_buffer (new float[size]);

		/* reduce size so that we can fill the buffer correctly (ringbuffers
		   can only handle size-1, otherwise they appear to be empty)
		*/
		size--;

		uint32_t n=0;
		framepos_t start;

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan, ++n) {

			start = overwrite_frame;
			framecnt_t cnt = size;

			/* to fill the buffer without resetting the playback sample, we need to
			   do it one or two chunks (normally two).

			   |----------------------------------------------------------------------|

			   ^
			   overwrite_offset
			   |<- second chunk->||<----------------- first chunk ------------------>|

			*/

			framecnt_t to_read = size - overwrite_offset;

			if (audio_read ((*chan)->buf->buffer() + overwrite_offset, mixdown_buffer.get(), gain_buffer.get(), start, to_read, n, reversed)) {
				error << string_compose(_("DiskReader %1: when refilling, cannot read %2 from playlist at frame %3"),
				                        id(), size, playback_sample) << endmsg;
				goto midi;
			}

			if (cnt > to_read) {

				cnt -= to_read;

				if (audio_read ((*chan)->buf->buffer(), mixdown_buffer.get(), gain_buffer.get(), start, cnt, n, reversed)) {
					error << string_compose(_("DiskReader %1: when refilling, cannot read %2 from playlist at frame %3"),
					                        id(), size, playback_sample) << endmsg;
					goto midi;
				}
			}
		}

		ret = 0;

	}

  midi:

	if (_midi_buf && _playlists[DataType::MIDI]) {

		/* Clear the playback buffer contents.  This is safe as long as the butler
		   thread is suspended, which it should be.
		*/
		_midi_buf->reset ();
		_midi_buf->reset_tracker ();

		g_atomic_int_set (&_frames_read_from_ringbuffer, 0);
		g_atomic_int_set (&_frames_written_to_ringbuffer, 0);

		/* Resolve all currently active notes in the playlist.  This is more
		   aggressive than it needs to be: ideally we would only resolve what is
		   absolutely necessary, but this seems difficult and/or impossible without
		   having the old data or knowing what change caused the overwrite.
		*/
		midi_playlist()->resolve_note_trackers (*_midi_buf, overwrite_frame);

		midi_read (overwrite_frame, _chunk_frames, false);

		file_frame = overwrite_frame; // it was adjusted by ::midi_read()
	}

	_pending_overwrite = false;

	return ret;
}

int
DiskReader::seek (framepos_t frame, bool complete_refill)
{
	uint32_t n;
	int ret = -1;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->buf->reset ();
	}

	if (g_atomic_int_get (&_frames_read_from_ringbuffer) == 0) {
		/* we haven't read anything since the last seek,
		   so flush all note trackers to prevent
		   wierdness
		*/
		reset_tracker ();
	}

	_midi_buf->reset();
	g_atomic_int_set(&_frames_read_from_ringbuffer, 0);
	g_atomic_int_set(&_frames_written_to_ringbuffer, 0);

	playback_sample = frame;
	file_frame = frame;

	if (complete_refill) {
		/* call _do_refill() to refill the entire buffer, using
		   the largest reads possible.
		*/
		while ((ret = do_refill_with_alloc (false)) > 0) ;
	} else {
		/* call _do_refill() to refill just one chunk, and then
		   return.
		*/
		ret = do_refill_with_alloc (true);
	}


	return ret;
}

int
DiskReader::can_internal_playback_seek (framecnt_t distance)
{
	/* 1. Audio */

	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (chan = c->begin(); chan != c->end(); ++chan) {
		if ((*chan)->buf->read_space() < (size_t) distance) {
			return false;
		}
	}

	/* 2. MIDI */

	uint32_t frames_read    = g_atomic_int_get(&_frames_read_from_ringbuffer);
	uint32_t frames_written = g_atomic_int_get(&_frames_written_to_ringbuffer);

	return ((frames_written - frames_read) < distance);
}

int
DiskReader::internal_playback_seek (framecnt_t distance)
{
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->buf->increment_read_ptr (::llabs(distance));
	}

	playback_sample += distance;

	return 0;
}

static
void swap_by_ptr (Sample *first, Sample *last)
{
	while (first < last) {
		Sample tmp = *first;
		*first++ = *last;
		*last-- = tmp;
	}
}

/** Read some data for 1 channel from our playlist into a buffer.
 *  @param buf Buffer to write to.
 *  @param start Session frame to start reading from; updated to where we end up
 *         after the read.
 *  @param cnt Count of samples to read.
 *  @param reversed true if we are running backwards, otherwise false.
 */
int
DiskReader::audio_read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer,
                        framepos_t& start, framecnt_t cnt,
                        int channel, bool reversed)
{
	framecnt_t this_read = 0;
	bool reloop = false;
	framepos_t loop_end = 0;
	framepos_t loop_start = 0;
	framecnt_t offset = 0;
	Location *loc = 0;

	if (!_playlists[DataType::AUDIO]) {
		memset (buf, 0, sizeof (Sample) * cnt);
		return 0;
	}

	/* XXX we don't currently play loops in reverse. not sure why */

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

		if (loc && start >= loop_end) {
			start = loop_start + ((start - loop_start) % loop_length);
		}

	}

	if (reversed) {
		start -= cnt;
	}

	/* We need this while loop in case we hit a loop boundary, in which case our read from
	   the playlist must be split into more than one section.
	*/

	while (cnt) {

		/* take any loop into account. we can't read past the end of the loop. */

		if (loc && (loop_end - start < cnt)) {
			this_read = loop_end - start;
			reloop = true;
		} else {
			reloop = false;
			this_read = cnt;
		}

		if (this_read == 0) {
			break;
		}

		this_read = min(cnt,this_read);

		if (audio_playlist()->read (buf+offset, mixdown_buffer, gain_buffer, start, this_read, channel) != this_read) {
			error << string_compose(_("DiskReader %1: cannot read %2 from playlist at frame %3"), id(), this_read,
					 start) << endmsg;
			return -1;
		}

		if (reversed) {

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
DiskReader::_do_refill_with_alloc (bool partial_fill)
{
	/* We limit disk reads to at most 4MB chunks, which with floating point
	   samples would be 1M samples. But we might use 16 or 14 bit samples,
	   in which case 4MB is more samples than that. Therefore size this for
	   the smallest sample value .. 4MB = 2M samples (16 bit).
	*/

	{
		std::auto_ptr<Sample> mix_buf (new Sample[2*1048576]);
		std::auto_ptr<float>  gain_buf (new float[2*1048576]);

		int ret = refill_audio (mix_buf.get(), gain_buf.get(), (partial_fill ? _chunk_frames : 0));

		if (ret) {
			return ret;
		}
	}

	return refill_midi ();
}

int
DiskReader::refill (Sample* mixdown_buffer, float* gain_buffer, framecnt_t fill_level)
{
	int ret = refill_audio (mixdown_buffer, gain_buffer, fill_level);

	if (ret) {
		return ret;
	}

	return refill_midi ();
}


/** Get some more data from disk and put it in our channels' bufs,
 *  if there is suitable space in them.
 *
 * If fill_level is non-zero, then we will refill the buffer so that there is
 * still at least fill_level samples of space left to be filled. This is used
 * after locates so that we do not need to wait to fill the entire buffer.
 *
 */

int
DiskReader::refill_audio (Sample* mixdown_buffer, float* gain_buffer, framecnt_t fill_level)
{
	/* do not read from disk while session is marked as Loading, to avoid
	   useless redundant I/O.
	*/

	if (_session.loading()) {
		return 0;
	}

	int32_t ret = 0;
	framecnt_t to_read;
	RingBufferNPT<Sample>::rw_vector vector;
	bool const reversed = _session.transport_speed() < 0.0f;
	framecnt_t total_space;
	framecnt_t zero_fill;
	uint32_t chan_n;
	ChannelList::iterator i;
	boost::shared_ptr<ChannelList> c = channels.reader();
	framecnt_t ts;

	if (c->empty()) {
		return 0;
	}

	assert(mixdown_buffer);
	assert(gain_buffer);

	vector.buf[0] = 0;
	vector.len[0] = 0;
	vector.buf[1] = 0;
	vector.len[1] = 0;

	c->front()->buf->get_write_vector (&vector);

	if ((total_space = vector.len[0] + vector.len[1]) == 0) {
		DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: no space to refill\n", name()));
		/* nowhere to write to */
		return 0;
	}

	if (fill_level) {
		if (fill_level < total_space) {
			total_space -= fill_level;
		} else {
			/* we can't do anything with it */
			fill_level = 0;
		}
	}

	/* if we're running close to normal speed and there isn't enough
	   space to do disk_read_chunk_frames of I/O, then don't bother.

	   at higher speeds, just do it because the sync between butler
	   and audio thread may not be good enough.

	   Note: it is a design assumption that disk_read_chunk_frames is smaller
	   than the playback buffer size, so this check should never trip when
	   the playback buffer is empty.
	*/

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: space to refill %2 vs. chunk %3 (speed = %4)\n", name(), total_space, _chunk_frames, _session.transport_speed()));
	if ((total_space < _chunk_frames) && fabs (_session.transport_speed()) < 2.0f) {
		return 0;
	}

	/* when slaved, don't try to get too close to the read pointer. this
	   leaves space for the buffer reversal to have something useful to
	   work with.
	*/

	if (_slaved && total_space < (framecnt_t) (c->front()->buf->bufsize() / 2)) {
		DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: not enough to refill while slaved\n", this));
		return 0;
	}

	if (reversed) {

		if (file_frame == 0) {

			/* at start: nothing to do but fill with silence */

			for (chan_n = 0, i = c->begin(); i != c->end(); ++i, ++chan_n) {

				ChannelInfo* chan (*i);
				chan->buf->get_write_vector (&vector);
				memset (vector.buf[0], 0, sizeof(Sample) * vector.len[0]);
				if (vector.len[1]) {
					memset (vector.buf[1], 0, sizeof(Sample) * vector.len[1]);
				}
				chan->buf->increment_write_ptr (vector.len[0] + vector.len[1]);
			}
			return 0;
		}

		if (file_frame < total_space) {

			/* too close to the start: read what we can,
			   and then zero fill the rest
			*/

			zero_fill = total_space - file_frame;
			total_space = file_frame;

		} else {

			zero_fill = 0;
		}

	} else {

		if (file_frame == max_framepos) {

			/* at end: nothing to do but fill with silence */

			for (chan_n = 0, i = c->begin(); i != c->end(); ++i, ++chan_n) {

				ChannelInfo* chan (*i);
				chan->buf->get_write_vector (&vector);
				memset (vector.buf[0], 0, sizeof(Sample) * vector.len[0]);
				if (vector.len[1]) {
					memset (vector.buf[1], 0, sizeof(Sample) * vector.len[1]);
				}
				chan->buf->increment_write_ptr (vector.len[0] + vector.len[1]);
			}
			return 0;
		}

		if (file_frame > max_framepos - total_space) {

			/* to close to the end: read what we can, and zero fill the rest */

			zero_fill = total_space - (max_framepos - file_frame);
			total_space = max_framepos - file_frame;

		} else {
			zero_fill = 0;
		}
	}

	framepos_t file_frame_tmp = 0;

	/* total_space is in samples. We want to optimize read sizes in various sizes using bytes */

	const size_t bits_per_sample = format_data_width (_session.config.get_native_file_data_format());
	size_t total_bytes = total_space * bits_per_sample / 8;

	/* chunk size range is 256kB to 4MB. Bigger is faster in terms of MB/sec, but bigger chunk size always takes longer
	 */
	size_t byte_size_for_read = max ((size_t) (256 * 1024), min ((size_t) (4 * 1048576), total_bytes));

	/* find nearest (lower) multiple of 16384 */

	byte_size_for_read = (byte_size_for_read / 16384) * 16384;

	/* now back to samples */

	framecnt_t samples_to_read = byte_size_for_read / (bits_per_sample / 8);

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: will refill %2 channels with %3 samples\n", name(), c->size(), total_space));

	// uint64_t before = g_get_monotonic_time ();
	// uint64_t elapsed;

	for (chan_n = 0, i = c->begin(); i != c->end(); ++i, ++chan_n) {

		ChannelInfo* chan (*i);
		Sample* buf1;
		Sample* buf2;
		framecnt_t len1, len2;

		chan->buf->get_write_vector (&vector);

		if ((framecnt_t) vector.len[0] > samples_to_read) {

			/* we're not going to fill the first chunk, so certainly do not bother with the
			   other part. it won't be connected with the part we do fill, as in:

			   .... => writable space
			   ++++ => readable space
			   ^^^^ => 1 x disk_read_chunk_frames that would be filled

			   |......|+++++++++++++|...............................|
			   buf1                buf0
			                        ^^^^^^^^^^^^^^^


			   So, just pretend that the buf1 part isn't there.

			*/

			vector.buf[1] = 0;
			vector.len[1] = 0;

		}

		ts = total_space;
		file_frame_tmp = file_frame;

		buf1 = vector.buf[0];
		len1 = vector.len[0];
		buf2 = vector.buf[1];
		len2 = vector.len[1];

		to_read = min (ts, len1);
		to_read = min (to_read, (framecnt_t) samples_to_read);

		assert (to_read >= 0);

		if (to_read) {

			if (audio_read (buf1, mixdown_buffer, gain_buffer, file_frame_tmp, to_read, chan_n, reversed)) {
				ret = -1;
				goto out;
			}

			chan->buf->increment_write_ptr (to_read);
			ts -= to_read;
		}

		to_read = min (ts, len2);

		if (to_read) {

			/* we read all of vector.len[0], but it wasn't the
			   entire samples_to_read of data, so read some or
			   all of vector.len[1] as well.
			*/

			if (audio_read (buf2, mixdown_buffer, gain_buffer, file_frame_tmp, to_read, chan_n, reversed)) {
				ret = -1;
				goto out;
			}

			chan->buf->increment_write_ptr (to_read);
		}

		if (zero_fill) {
			/* XXX: do something */
		}

	}

	// elapsed = g_get_monotonic_time () - before;
	// cerr << "\tbandwidth = " << (byte_size_for_read / 1048576.0) / (elapsed/1000000.0) << "MB/sec\n";

	file_frame = file_frame_tmp;
	assert (file_frame >= 0);

	ret = ((total_space - samples_to_read) > _chunk_frames);

	c->front()->buf->get_write_vector (&vector);

  out:
	return ret;
}

void
DiskReader::playlist_ranges_moved (list< Evoral::RangeMove<framepos_t> > const & movements_frames, bool from_undo)
{
	/* If we're coming from an undo, it will have handled
	   automation undo (it must, since automation-follows-regions
	   can lose automation data).  Hence we can do nothing here.
	*/

	if (from_undo) {
		return;
	}

	if (!_route || Config->get_automation_follows_regions () == false) {
		return;
	}

	list< Evoral::RangeMove<double> > movements;

	for (list< Evoral::RangeMove<framepos_t> >::const_iterator i = movements_frames.begin();
	     i != movements_frames.end();
	     ++i) {

		movements.push_back(Evoral::RangeMove<double>(i->from, i->length, i->to));
	}

	/* move panner automation */
	boost::shared_ptr<Pannable> pannable = _route->pannable();
        Evoral::ControlSet::Controls& c (pannable->controls());

        for (Evoral::ControlSet::Controls::iterator ci = c.begin(); ci != c.end(); ++ci) {
                boost::shared_ptr<AutomationControl> ac = boost::dynamic_pointer_cast<AutomationControl>(ci->second);
                if (!ac) {
                        continue;
                }
                boost::shared_ptr<AutomationList> alist = ac->alist();
		if (!alist->size()) {
			continue;
		}
                XMLNode & before = alist->get_state ();
                bool const things_moved = alist->move_ranges (movements);
                if (things_moved) {
                        _session.add_command (new MementoCommand<AutomationList> (
                                                      *alist.get(), &before, &alist->get_state ()));
                }
        }
	/* move processor automation */
        _route->foreach_processor (boost::bind (&DiskReader::move_processor_automation, this, _1, movements_frames));
}

void
DiskReader::move_processor_automation (boost::weak_ptr<Processor> p, list< Evoral::RangeMove<framepos_t> > const & movements_frames)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}

	list< Evoral::RangeMove<double> > movements;
	for (list< Evoral::RangeMove<framepos_t> >::const_iterator i = movements_frames.begin(); i != movements_frames.end(); ++i) {
		movements.push_back(Evoral::RangeMove<double>(i->from, i->length, i->to));
	}

	set<Evoral::Parameter> const a = processor->what_can_be_automated ();

	for (set<Evoral::Parameter>::const_iterator i = a.begin (); i != a.end (); ++i) {
		boost::shared_ptr<AutomationList> al = processor->automation_control(*i)->alist();
		if (!al->size()) {
			continue;
		}
		XMLNode & before = al->get_state ();
		bool const things_moved = al->move_ranges (movements);
		if (things_moved) {
			_session.add_command (
				new MementoCommand<AutomationList> (
					*al.get(), &before, &al->get_state ()
					)
				);
		}
	}
}

boost::shared_ptr<MidiBuffer>
DiskReader::get_gui_feed_buffer () const
{
	boost::shared_ptr<MidiBuffer> b (new MidiBuffer (AudioEngine::instance()->raw_buffer_size (DataType::MIDI)));

	Glib::Threads::Mutex::Lock lm (_gui_feed_buffer_mutex);
	b->copy (_gui_feed_buffer);
	return b;
}

void
DiskReader::reset_tracker ()
{
	_midi_buf->reset_tracker ();

	boost::shared_ptr<MidiPlaylist> mp (midi_playlist());

	if (mp) {
		mp->reset_note_trackers ();
	}
}

void
DiskReader::resolve_tracker (Evoral::EventSink<framepos_t>& buffer, framepos_t time)
{
	_midi_buf->resolve_tracker(buffer, time);

	boost::shared_ptr<MidiPlaylist> mp (midi_playlist());

	if (mp) {
		mp->reset_note_trackers ();
	}
}

/** Writes playback events from playback_sample for nframes to dst, translating time stamps
 *  so that an event at playback_sample has time = 0
 */
void
DiskReader::get_midi_playback (MidiBuffer& dst, framecnt_t nframes, MonitorState ms, BufferSet& scratch_bufs, double speed, framecnt_t playback_distance)
{
	MidiBuffer* target;

	if ((ms & MonitoringInput) == 0) {
		dst.clear();
		target = &dst;
	} else {
		target = &scratch_bufs.get_midi (0);
	}

	if (ms & MonitoringDisk) {
		/* no disk data needed */

		Location* loc = loop_location;

		DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose (
			             "%1 MDS pre-read read %8 offset = %9 @ %4..%5 from %2 write to %3, LOOPED ? %6 .. %7\n", _name,
			             _midi_buf->get_read_ptr(), _midi_buf->get_write_ptr(), playback_sample, playback_sample + nframes,
			             (loc ? loc->start() : -1), (loc ? loc->end() : -1), nframes, Port::port_offset()));

		//cerr << "======== PRE ========\n";
		//_midi_buf->dump (cerr);
		//cerr << "----------------\n";

		size_t events_read = 0;

		if (loc) {
			framepos_t effective_start;

			Evoral::Range<framepos_t> loop_range (loc->start(), loc->end() - 1);
			effective_start = loop_range.squish (playback_sample);

			DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("looped, effective start adjusted to %1\n", effective_start));

			if (effective_start == loc->start()) {
				/* We need to turn off notes that may extend
				   beyond the loop end.
				*/

				_midi_buf->resolve_tracker (*target, 0);
			}

			/* for split-cycles we need to offset the events */

			if (loc->end() >= effective_start && loc->end() < effective_start + nframes) {

				/* end of loop is within the range we are reading, so
				   split the read in two, and lie about the location
				   for the 2nd read
				*/

				framecnt_t first, second;

				first = loc->end() - effective_start;
				second = nframes - first;

				DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("loop read for eff %1 end %2: %3 and %4, cycle offset %5\n",
				                                                      effective_start, loc->end(), first, second));

				if (first) {
					DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("loop read #1, from %1 for %2\n",
					                                                      effective_start, first));
					events_read = _midi_buf->read (*target, effective_start, first);
				}

				if (second) {
					DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("loop read #2, from %1 for %2\n",
					                                                      loc->start(), second));
					events_read += _midi_buf->read (*target, loc->start(), second);
				}

			} else {
				DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("loop read #3, adjusted start as %1 for %2\n",
				                                                      effective_start, nframes));
				events_read = _midi_buf->read (*target, effective_start, effective_start + nframes);
			}
		} else {
			const size_t n_skipped = _midi_buf->skip_to (playback_sample);
			if (n_skipped > 0) {
				warning << string_compose(_("MidiDiskstream %1: skipped %2 events, possible underflow"), id(), n_skipped) << endmsg;
			}
			DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("playback buffer read, from %1 to %2 (%3)", playback_sample, playback_sample + nframes, nframes));
			events_read = _midi_buf->read (*target, playback_sample, playback_sample + nframes);
		}

		DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose (
			             "%1 MDS events read %2 range %3 .. %4 rspace %5 wspace %6 r@%7 w@%8\n",
			             _name, events_read, playback_sample, playback_sample + nframes,
			             _midi_buf->read_space(), _midi_buf->write_space(),
			             _midi_buf->get_read_ptr(), _midi_buf->get_write_ptr()));
	}

	g_atomic_int_add (&_frames_read_from_ringbuffer, nframes);

	/* vari-speed */

	if (speed != 0.0 && fabsf (speed) != 1.0f) {
		for (MidiBuffer::iterator i = target->begin(); i != target->end(); ++i) {
			MidiBuffer::TimeType *tme = i.timeptr();
			*tme = (*tme) * nframes / playback_distance;
		}
	}

	if (ms & MonitoringInput) {
		dst.merge_from (*target, nframes);
	}

	//cerr << "======== POST ========\n";
	//_midi_buf->dump (cerr);
	//cerr << "----------------\n";
}

/** @a start is set to the new frame position (TIME) read up to */
int
DiskReader::midi_read (framepos_t& start, framecnt_t dur, bool reversed)
{
	framecnt_t this_read   = 0;
	framepos_t loop_end    = 0;
	framepos_t loop_start  = 0;
	framecnt_t loop_length = 0;
	Location*  loc         = loop_location;
	framepos_t effective_start = start;
	Evoral::Range<framepos_t>*  loop_range (0);

//	MidiTrack*         mt     = dynamic_cast<MidiTrack*>(_track);
//	MidiChannelFilter* filter = mt ? &mt->playback_filter() : 0;
	MidiChannelFilter* filter = 0;

	frameoffset_t loop_offset = 0;

	if (!reversed && loc) {
		get_location_times (loc, &loop_start, &loop_end, &loop_length);
	}

	while (dur) {

		/* take any loop into account. we can't read past the end of the loop. */

		if (loc && !reversed) {

			if (!loop_range) {
				loop_range = new Evoral::Range<framepos_t> (loop_start, loop_end-1); // inclusive semantics require -1
			}

			/* if we are (seamlessly) looping, ensure that the first frame we read is at the correct
			   position within the loop.
			*/

			effective_start = loop_range->squish (effective_start);

			if ((loop_end - effective_start) <= dur) {
				/* too close to end of loop to read "dur", so
				   shorten it.
				*/
				this_read = loop_end - effective_start;
			} else {
				this_read = dur;
			}

		} else {
			this_read = dur;
		}

		if (this_read == 0) {
			break;
		}

		this_read = min (dur,this_read);

		DEBUG_TRACE (DEBUG::MidiDiskstreamIO, string_compose ("MDS ::read at %1 for %2 loffset %3\n", effective_start, this_read, loop_offset));

		if (midi_playlist()->read (*_midi_buf, effective_start, this_read, loop_range, 0, filter) != this_read) {
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

			/* adjust passed-by-reference argument (note: this is
			   monotonic and does not reflect looping.
			*/
			start += this_read;

			/* similarly adjust effective_start, but this may be
			   readjusted for seamless looping as we continue around
			   the loop.
			*/
			effective_start += this_read;
		}

		dur -= this_read;
		//offset += this_read;
	}

	return 0;
}

int
DiskReader::refill_midi ()
{
	if (!_playlists[DataType::MIDI]) {
		return 0;
	}

	size_t  write_space = _midi_buf->write_space();
	const bool reversed    = _session.transport_speed() < 0.0f;

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("MIDI refill, write space = %1 file frame = %2\n", write_space, file_frame));

	/* no space to write */
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

	int ret = 0;
	const uint32_t frames_read = g_atomic_int_get (&_frames_read_from_ringbuffer);
	const uint32_t frames_written = g_atomic_int_get (&_frames_written_to_ringbuffer);

	if ((frames_read < frames_written) && (frames_written - frames_read) >= midi_readahead) {
		return 0;
	}

	framecnt_t to_read = midi_readahead - ((framecnt_t)frames_written - (framecnt_t)frames_read);

	to_read = min (to_read, (framecnt_t) (max_framepos - file_frame));
	to_read = min (to_read, (framecnt_t) write_space);

	if (midi_read (file_frame, to_read, reversed)) {
		ret = -1;
	}

	return ret;
}
