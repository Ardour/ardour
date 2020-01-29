/*
 * Copyright (C) 2017-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017-2019 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <boost/smart_ptr/scoped_array.hpp>

#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/playback_buffer.h"

#include "evoral/Range.h"

#include "ardour/amp.h"
#include "ardour/audioengine.h"
#include "ardour/audioplaylist.h"
#include "ardour/audio_buffer.h"
#include "ardour/butler.h"
#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_track.h"
#include "ardour/pannable.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

ARDOUR::samplecnt_t DiskReader::_chunk_samples = default_chunk_samples ();
PBD::Signal0<void> DiskReader::Underrun;
Sample* DiskReader::_sum_buffer = 0;
Sample* DiskReader::_mixdown_buffer = 0;
gain_t* DiskReader::_gain_buffer = 0;
gint DiskReader::_no_disk_output (0);
DiskReader::Declicker DiskReader::loop_declick_in;
DiskReader::Declicker DiskReader::loop_declick_out;
samplecnt_t DiskReader::loop_fade_length (0);

DiskReader::DiskReader (Session& s, string const & str, DiskIOProcessor::Flag f)
	: DiskIOProcessor (s, str, f)
	, overwrite_sample (0)
	, run_must_resolve (false)
	, _declick_amp (s.nominal_sample_rate ())
	, _declick_offs (0)
{
	file_sample[DataType::AUDIO] = 0;
	file_sample[DataType::MIDI] = 0;
	g_atomic_int_set (&_pending_overwrite, 0);
}

DiskReader::~DiskReader ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("DiskReader %1 @ %2 deleted\n", _name, this));
}

void
DiskReader::ReaderChannelInfo::resize (samplecnt_t bufsize)
{
	delete rbuf; rbuf = 0;

	rbuf = new PlaybackBuffer<Sample> (bufsize);
	/* touch memory to lock it */
	memset (rbuf->buffer(), 0, sizeof (Sample) * rbuf->bufsize());
}

void
DiskReader::ReaderChannelInfo::resize_preloop (samplecnt_t bufsize)
{
	if (bufsize == 0) {
		return;
	}

	if (bufsize > pre_loop_buffer_size) {
		delete [] pre_loop_buffer;
		pre_loop_buffer = new Sample[bufsize];
		pre_loop_buffer_size = bufsize;
	}
}

int
DiskReader::add_channel_to (boost::shared_ptr<ChannelList> c, uint32_t how_many)
{
	while (how_many--) {
		c->push_back (new ReaderChannelInfo (_session.butler()->audio_playback_buffer_size(), loop_fade_length));
		DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: new reader channel, write space = %2 read = %3\n",
		                                            name(),
		                                            c->back()->rbuf->write_space(),
		                                            c->back()->rbuf->read_space()));
	}

	return 0;
}

void
DiskReader::allocate_working_buffers()
{
	/* with varifill buffer refilling, we compute the read size in bytes (to optimize
	   for disk i/o bandwidth) and then convert back into samples. These buffers
	   need to reflect the maximum size we could use, which is 4MB reads, or 2M samples
	   using 16 bit samples.
	*/
	_sum_buffer           = new Sample[2*1048576];
	_mixdown_buffer       = new Sample[2*1048576];
	_gain_buffer          = new gain_t[2*1048576];
}

void
DiskReader::free_working_buffers()
{
	delete [] _sum_buffer;
	delete [] _mixdown_buffer;
	delete [] _gain_buffer;
	_sum_buffer     = 0;
	_mixdown_buffer = 0;
	_gain_buffer    = 0;
}

samplecnt_t
DiskReader::default_chunk_samples()
{
	return 65536;
}

bool
DiskReader::set_name (string const & str)
{
	string my_name = X_("player:");
	my_name += str;

	if (_name != my_name) {
		SessionObject::set_name (my_name);
	}

	return true;
}

XMLNode&
DiskReader::state ()
{
	XMLNode& node (DiskIOProcessor::state ());
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
	/* can't do the resolve here because we don't have a place to put the
	 * note resolving data. Defer to
	 * MidiTrack::realtime_handle_transport_stopped() which will call
	 * ::resolve_tracker() and put the output in its _immediate_events store.
	 */
}

void
DiskReader::realtime_locate (bool for_loop_end)
{
	if (!for_loop_end) {
		std::cerr << name() << " DO note resolve on locate\n";
		boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack>(_track);
		_tracker.resolve_notes (mt->immediate_events(), 0);
	} else {
		std::cerr << name() << " skip note resolve on locate for loop\n";
	}
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

	PBD::PlaybackBuffer<Sample>* b = c->front()->rbuf;
	return (float) ((double) b->read_space() / (double) b->bufsize());
}

void
DiskReader::adjust_buffering ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->resize (_session.butler()->audio_playback_buffer_size());
	}
}

void
DiskReader::playlist_modified ()
{
	_session.request_overwrite_buffer (_track, PlaylistModified);
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

        if (!(g_atomic_int_get (&_pending_overwrite) & PlaylistChanged) || prior_playlist) {
	        _session.request_overwrite_buffer (_track, PlaylistChanged);
	}

	return 0;
}

void
DiskReader::run (BufferSet& bufs, samplepos_t start_sample, samplepos_t end_sample, double speed, pframes_t nframes, bool result_required)
{
	uint32_t n;
	boost::shared_ptr<ChannelList> c = channels.reader();
	ChannelList::iterator chan;
	sampleoffset_t disk_samples_to_consume;
	MonitorState ms = _track->monitoring_state ();

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

	const gain_t target_gain = ((speed == 0.0) || ((ms & MonitoringDisk) == 0)) ? 0.0 : 1.0;
	const bool declicked_out = (_declick_amp.gain() == target_gain) && target_gain == 0.0;
	const bool declick_out = (_declick_amp.gain() != target_gain) && target_gain == 0.0;

	if (!_session.cfg ()->get_use_transport_fades ()) {
		_declick_amp.set_gain (target_gain);
	}

	if (declicked_out && (ms == MonitoringDisk)) {
		/* Stopped and declicking has finished. Don't accidentally pass
		 * any data from disk into our outputs (e.g. via interpolation)
		 */
		return;
	}

	BufferSet& scratch_bufs (_session.get_scratch_buffers (bufs.count()));
	const bool still_locating = _session.global_locate_pending();

	assert (speed == -1 || speed == 0 || speed == 1);

	if (speed == 0) {
		disk_samples_to_consume = 0;
	} else {
		disk_samples_to_consume = nframes;
	}

	if (c->empty() || !_playlists[DataType::AUDIO]) {
		/* do nothing with audio */
		goto midi;
	}

	if (declick_out) {
		/* fade-out */

		// printf ("DR fade-out speed=%.1f gain=%.3f off=%ld start=%ld playpos=%ld (%s)\n", speed, _declick_amp.gain (), _declick_offs, start_sample, playback_sample, owner()->name().c_str());

		ms = MonitorState (ms | MonitoringDisk);
		assert (result_required);
		result_required = true;
	} else {
		_declick_offs = 0;
	}

	if (!result_required || ((ms & MonitoringDisk) == 0) || still_locating || _no_disk_output) {

		/* no need for actual disk data, just advance read pointer */

		if (!still_locating || _no_disk_output) {
			for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
				assert ((*chan)->rbuf);
				(*chan)->rbuf->increment_read_ptr (disk_samples_to_consume);
			}
		}

		/* if monitoring disk but locating put silence in the buffers */

		if ((_no_disk_output || still_locating) && (ms == MonitoringDisk)) {
			bufs.silence (nframes, 0);
		}

	} else {

		/* we need audio data from disk */

		size_t n_buffers = bufs.count().n_audio();
		size_t n_chans = c->size();
		gain_t scaling;

		if (n_chans > n_buffers) {
			scaling = ((float) n_buffers) / n_chans;
		} else {
			scaling = 1.0;
		}

		const float initial_declick_gain = _declick_amp.gain ();
		const sampleoffset_t declick_offs = _declick_offs;

		for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {

			ChannelInfo* chaninfo (*chan);
			AudioBuffer& output (bufs.get_audio (n % n_buffers));

			AudioBuffer& disk_buf ((ms & MonitoringInput) ? scratch_bufs.get_audio(n) : output);

			if (start_sample != playback_sample && target_gain != 0) {
				if (can_internal_playback_seek (start_sample - playback_sample)) {
					internal_playback_seek (start_sample - playback_sample);
				} else {
					disk_samples_to_consume = 0; /* will force an underrun below */
				}
			}

			/* reset _declick_amp to the correct gain before processing this channel. */
			if (declick_out) {
				_declick_amp.set_gain (initial_declick_gain);
			}


			if (!declick_out) {

				const samplecnt_t total = chaninfo->rbuf->read (disk_buf.data(), disk_samples_to_consume);

				if (disk_samples_to_consume > total) {
					cerr << _name << " Need " << total << " have only " << disk_samples_to_consume << endl;
					cerr << "underrun for " << _name << endl;
					DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 underrun in %2, total space = %3\n",
					                                            DEBUG_THREAD_SELF, name(), total));
					Underrun ();
					return;
				}

			} else if (_declick_amp.gain() != target_gain) {

				assert (target_gain == 0);

				/* note that this is a non-committing read: it
				   retrieves data from the ringbuffer but does not
				   advance the read pointer. As a result,
				   subsequent calls (as we declick) need to
				   pass in an offset describing where to read
				   from. We maintain _declick_offs across calls
				   to ::run()
				*/

				const samplecnt_t total = chaninfo->rbuf->read (disk_buf.data(), nframes, false, declick_offs);

				if (n == 0) {
					_declick_offs += total;
				}
			}

			_declick_amp.apply_gain (disk_buf, nframes, target_gain);

			/* _declick_amp is now left with the correct gain after
			 * processing nframes
			 */

			Amp::apply_simple_gain (disk_buf, nframes, scaling);

			if (ms & MonitoringInput) {
				/* mix the disk signal into the input signal (already in bufs) */
				mix_buffers_no_gain (output.data(), disk_buf.data(), nframes);
			}
		}
	}


  midi:

	/* MIDI data handling */

	const bool midi_data_available = !(pending_overwrite() & PlaylistModified);

	if (bufs.count().n_midi()) {

		MidiBuffer& dst (bufs.get_midi (0));

		if (run_must_resolve) {
			resolve_tracker (dst, 0);
			run_must_resolve = false;
		}

		if (!_no_disk_output && !declick_in_progress() && (ms & MonitoringDisk) && !still_locating && midi_data_available && speed) {
			get_midi_playback (dst, start_sample, end_sample, ms, scratch_bufs, speed, disk_samples_to_consume);
		}
	}

	/* decide if we need the butler */

	if (!still_locating && midi_data_available) {

		bool butler_required = false;

		if (speed < 0.0) {
			playback_sample -= disk_samples_to_consume;
		} else {
			playback_sample += disk_samples_to_consume;
		}

		Location* loc = _loop_location;
		if (loc) {
			Evoral::Range<samplepos_t> loop_range (loc->start(), loc->end() - 1);
			playback_sample = loop_range.squish (playback_sample);
		}

		if (_playlists[DataType::AUDIO]) {
			if (!c->empty()) {
				if (_slaved) {
					if (c->front()->rbuf->write_space() >= c->front()->rbuf->bufsize() / 2) {
						DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: slaved, write space = %2 of %3\n", name(), c->front()->rbuf->write_space(), c->front()->rbuf->bufsize()));
						butler_required = true;
					}
				} else {
					if ((samplecnt_t) c->front()->rbuf->write_space() >= _chunk_samples) {
						DEBUG_TRACE (DEBUG::Butler, string_compose ("%1: write space = %2 of %3\n", name(), c->front()->rbuf->write_space(),
						                                            _chunk_samples));
						butler_required = true;
					}
				}
			}
		}

		/* All of MIDI is in RAM, no need to call the butler unless we
		 * have to overwrite buffers because of a playlist change.
		 */

		_need_butler = butler_required;
	}

	if (_need_butler) {
		DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 reader run, needs butler = %2\n", name(), _need_butler));
	}
}

bool
DiskReader::declick_in_progress () const
{
	return _declick_amp.gain() != 0; // declick-out
}

bool
DiskReader::pending_overwrite () const
{
	return g_atomic_int_get (&_pending_overwrite) != 0;
}

void
DiskReader::set_pending_overwrite (OverwriteReason why)
{
	boost::shared_ptr<ChannelList> c = channels.reader ();

	/* called from audio thread, so we can use the read ptr and playback sample as we wish */

	if (!c->empty ()) {

		const samplecnt_t reserved_size = c->front()->rbuf->reserved_size();
		const samplecnt_t bufsize = c->front()->rbuf->bufsize ();

		overwrite_sample = playback_sample - reserved_size;

		overwrite_offset = c->front()->rbuf->read_ptr();

		if (overwrite_offset > reserved_size) {

			/*
			  |----------------------------------------------------------------------|
	                               ^               ^
	                               RRRRRRRRRRRRRRRRoverwrite_offset  (old read_ptr)
	                  |<- second ->|<------------------ first chunk ------------------------>|

	                  Fill the the end of the buffer ("first chunk"), above
			*/

			overwrite_offset -= reserved_size;

		} else {

			/*
			  |----------------------------------------------------------------------|
	                  RRRRRRRRE^                                                     RRRRRRRRR
	                           overwrite_offset  (old read_ptr)
	                  |<                second chunk                                >|<first>|

	                  Fill the end of the buffer ("R1R1R1" aka "first" above)
			*/

			overwrite_offset = bufsize - (reserved_size - overwrite_offset);
		}
	}

	if (why & (LoopChanged|PlaylistModified|PlaylistChanged)) {
		run_must_resolve = true;
	}

	while (true) {
		OverwriteReason current = OverwriteReason (g_atomic_int_get (&_pending_overwrite));
		OverwriteReason next = OverwriteReason (current | why);
		if (g_atomic_int_compare_and_exchange (&_pending_overwrite, current, next)) {
			break;
		}
	}

}

bool
DiskReader::overwrite_existing_audio ()
{
	/* This is a tricky and/or clever little method. Let's try to describe
	 * precisely what it does.
	 *
	 * Our goal is to completely overwrite the playback buffers for each
	 * audio channel with new data. The wrinkle is that we want to preserve
	 * the EXACT mapping between a given timeline position and buffer
	 * offset that existed when we requested an overwrite. That is, if the
	 * Nth position in the buffer contained the sample corresponding to
	 * timeline position T, then once this is complete that condition
	 * should still hold. The actual value of the sample (and even when it
	 * corresponds to any actual material on disk - it may just be silence)
	 * may change, but this buffer_offset<->timeline_position mapping must
	 * remain constant.
	 *
	 * Why do this? There are many reasons. A trivial example is that the
	 * region gain level for one region has been changed, and the user
	 * should be able to hear the result.
	 *
	 * In ::set_pending_overwrite() (above) we stored a sample and a buffer
	 * offset. These corresponded to the next sample to be played and the
	 * buffer position holding that sample. We were able to determine this
	 * pair atomically because ::set_pending_overwrite() is called from
	 * within process context, and thus neither playback_sample nor the
	 * buffer read ptr can change while it runs. We computed the earliest
	 * sample/timeline position in the buffer (at the start of the reserved
	 * zone, if any) and its corresponding buffer offset.
	 *
	 * Here, we will refill the buffer, starting with the sample and buffer
	 * offset computed by ::set_pending_overwrite(). Typically this will
	 * take two reads from the playlist, because our read will be "split"
	 * by the end of the buffer (i.e. we fill from some mid-buffer point to
	 * the end, then fill from the start to the mid-buffer point, as is
	 * common with ring buffers).
	 *
	 * Note that the process thread may indeed access the buffer while we
	 * are doing this. There is a strong likelihood of colliding read/write
	 * between this thread (the butler) and a process thread. But we don't
	 * care: we know that the samples being read/written will correspond to
	 * the same timeline position, and that the user has just done
	 * something forcing us to update the value(s). Given that a Sample is
	 * currently (and likely forever) a floating point value, and that on
	 * many/most architectures, a store for a floating point value is
	 * non-atomic, there is some chance of the process read reading a
	 * sample value while it is being written. This could theoretically
	 * cause a brief glitch, but no more or less than any other
	 * "discontinuity" in the sample's value will.
	 *
	 * It goes without saying that this relies on being serialized within
	 * the butler thread with respect any other buffer write operation
	 * (e.g. via ::refill()). It should also be noted that it has no effect
	 * at all on the write-related members of the playback buffer - we
	 * simply replace the contents of the buffer.
	 */


	boost::shared_ptr<ChannelList> c = channels.reader();

	if (c->empty () || !_playlists[DataType::AUDIO]) {
		return true;
	}

	const bool reversed = _session.transport_speed() < 0.0f;

	sampleoffset_t chunk1_offset;
	samplecnt_t    chunk1_cnt;
	samplecnt_t    chunk2_cnt;

	const samplecnt_t bufsize = c->front()->rbuf->bufsize ();

	chunk1_offset = overwrite_offset;
	chunk1_cnt = bufsize - overwrite_offset;

	if (chunk1_cnt == bufsize) {
		chunk1_cnt--;
		chunk2_cnt = 0;
	} else {
		chunk2_cnt = bufsize - 1 - chunk1_cnt;
	}

	/* this is a complete buffer fill */
	assert (chunk1_cnt + chunk2_cnt == bufsize - 1);

	boost::scoped_array<Sample> mixdown_buffer (new Sample[bufsize]);
	boost::scoped_array<float> gain_buffer (new float[bufsize]);
	uint32_t n = 0;
	bool ret = true;

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan, ++n) {

		samplepos_t start = overwrite_sample;

		Sample* buf = (*chan)->rbuf->buffer();
		ReaderChannelInfo* rci = dynamic_cast<ReaderChannelInfo*> (*chan);

		if (chunk1_cnt) {
			if (audio_read (buf + chunk1_offset, mixdown_buffer.get(), gain_buffer.get(), start, chunk1_cnt, rci, n, reversed) != chunk1_cnt)  {
				error << string_compose(_("DiskReader %1: when overwriting(1), cannot read %2 from playlist at sample %3"), id(), chunk1_cnt, overwrite_sample) << endmsg;
				ret = false;
				continue;
			}
		}

		if (chunk2_cnt) {
			if (audio_read (buf, mixdown_buffer.get(), gain_buffer.get(), start, chunk2_cnt, rci, n, reversed) != chunk2_cnt) {
				error << string_compose(_("DiskReader %1: when overwriting(2), cannot read %2 from playlist at sample %3"), id(), chunk2_cnt, overwrite_sample) << endmsg;
				ret = false;
			}
		}
	}

	return ret;
}

bool
DiskReader::overwrite_existing_midi ()
{
	RTMidiBuffer* mbuf = rt_midibuffer ();

	if (mbuf) {
		boost::shared_ptr<MidiTrack> mt = boost::dynamic_pointer_cast<MidiTrack>(_track);
		MidiChannelFilter* filter = mt ? &mt->playback_filter() : 0;

		PBD::Timing minsert;
		minsert.start();

		midi_playlist()->render (filter);

		minsert.update();
		assert (midi_playlist()->rendered());
		cerr << "Reading " << name()  << " took " << minsert.elapsed() << " microseconds, final size = " << midi_playlist()->rendered()->size() << endl;
	}

	return true;
}

bool
DiskReader::overwrite_existing_buffers ()
{
	/* called from butler thread */

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1 overwriting existing buffers at %2 (because %3%4%5\n", owner()->name(), overwrite_sample, std::hex, g_atomic_int_get (&_pending_overwrite), std::dec));

	bool ret = true;

	if (g_atomic_int_get (&_pending_overwrite) & (PlaylistModified|LoopDisabled|LoopChanged|PlaylistChanged)) {
		if (!overwrite_existing_audio ()) {
			ret = false;
		}
	}

	if (g_atomic_int_get (&_pending_overwrite) & (PlaylistModified|PlaylistChanged)) {
		if (!overwrite_existing_midi ()) {
			ret = false;
		}
	}

	g_atomic_int_set (&_pending_overwrite, 0);

	return ret;
}

int
DiskReader::seek (samplepos_t sample, bool complete_refill)
{
	/* called via non_realtime_locate() from butler thread */

	uint32_t n;
	int ret = -1;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	if (c->empty()) {
		return 0;
	}

	if (sample == playback_sample && !complete_refill) {
		return 0;
	}

	if (abs (sample - playback_sample) < (c->front()->rbuf->reserved_size() / 6)) {
		/* we're close enough. Note: this is a heuristic */
		return 0;
	}

	g_atomic_int_set (&_pending_overwrite, 0);

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("DiskReader::seek %1 %2 -> %3 refill=%4\n", owner()->name().c_str(), playback_sample, sample, complete_refill));

	const samplecnt_t distance = sample - playback_sample;
	if (!complete_refill && can_internal_playback_seek (distance)) {
		internal_playback_seek (distance);
		return 0;
	}

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->rbuf->reset ();
	}

	playback_sample = sample;
	file_sample[DataType::AUDIO] = sample;
	file_sample[DataType::MIDI] = sample;

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

bool
DiskReader::can_internal_playback_seek (sampleoffset_t distance)
{
	/* 1. Audio */

	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (chan = c->begin(); chan != c->end(); ++chan) {
		if (!(*chan)->rbuf->can_seek (distance)) {
			return false;
		}
	}

	/* 2. MIDI can always seek any distance */

	return true;
}

void
DiskReader::internal_playback_seek (sampleoffset_t distance)
{
	if (distance == 0) {
		return;
	}

	sampleoffset_t off = distance;

	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();
	for (chan = c->begin(); chan != c->end(); ++chan) {
		if (distance < 0) {
			off = 0 - (sampleoffset_t) (*chan)->rbuf->decrement_read_ptr (::llabs (distance));
		} else {
			off = (*chan)->rbuf->increment_read_ptr (distance);
		}
	}

	playback_sample += off;
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
 *  @param start Session sample to start reading from; updated to where we end up
 *         after the read.
 *  @param cnt Count of samples to read.
 *  @param reversed true if we are running backwards, otherwise false.
 */
samplecnt_t
DiskReader::audio_read (Sample* sum_buffer,
                        Sample* mixdown_buffer,
                        float* gain_buffer,
                        samplepos_t& start,
                        samplecnt_t cnt,
                        ReaderChannelInfo* rci,
                        int channel,
                        bool reversed)
{
	samplecnt_t this_read = 0;
	bool reloop = false;
	samplepos_t loop_end = 0;
	samplepos_t loop_start = 0;
	Location *loc = 0;
	const samplecnt_t rcnt = cnt;

	/* XXX we don't currently play loops in reverse. not sure why */

	if (!reversed) {

		samplecnt_t loop_length = 0;

		/* Make the use of a Location atomic for this read operation.

		   Note: Locations don't get deleted, so all we care about
		   when I say "atomic" is that we are always pointing to
		   the same one and using a start/length values obtained
		   just once.
		*/

		if ((loc = _loop_location) != 0) {
			loop_start = loc->start();
			loop_end = loc->end();
			loop_length = loop_end - loop_start;
		}

		/* if we are looping, ensure that the first sample we read is at the correct
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

		this_read = min (cnt, this_read);

		/* note that the mixdown and gain buffers are purely for the
		 * internal use of the playlist, and cannot be considered
		 * useful after the return from AudioPlayback::read()
		 */

		if (audio_playlist()->read (sum_buffer, mixdown_buffer, gain_buffer, start, this_read, channel) != this_read) {
			error << string_compose(_("DiskReader %1: cannot read %2 from playlist at sample %3"), id(), this_read, start) << endmsg;
			return 0;
		}

		if (loc) {

			/* Looping: do something (maybe) about the loop boundaries */

			switch (Config->get_loop_fade_choice()) {
			case NoLoopFade:
				break;
			case BothLoopFade:
				loop_declick_in.run (sum_buffer, start, start + this_read);
				loop_declick_out.run (sum_buffer, start, start + this_read);
				break;
			case EndLoopFade:
				loop_declick_out.run (sum_buffer, start, start + this_read);
				break;
			case XFadeLoop:
				maybe_xfade_loop (sum_buffer, start, start + this_read, rci);
				break;
			}
		}

		if (reversed) {

			swap_by_ptr (sum_buffer, sum_buffer + this_read - 1);

		} else {

			/* if we read to the end of the loop, go back to the beginning */

			if (reloop) {
				start = loop_start;
			} else {
				start += this_read;
			}
		}

		cnt -= this_read;
		sum_buffer += this_read;
	}

	return rcnt;
}

int
DiskReader::_do_refill_with_alloc (bool partial_fill)
{
	/* We limit disk reads to at most 4MB chunks, which with floating point
	   samples would be 1M samples. But we might use 16 or 14 bit samples,
	   in which case 4MB is more samples than that. Therefore size this for
	   the smallest sample value .. 4MB = 2M samples (16 bit).
	*/

	boost::scoped_array<Sample> sum_buf (new Sample[2*1048576]);
	boost::scoped_array<Sample> mix_buf (new Sample[2*1048576]);
	boost::scoped_array<float>  gain_buf (new float[2*1048576]);

	return refill_audio (sum_buf.get(), mix_buf.get(), gain_buf.get(), (partial_fill ? _chunk_samples : 0));
}

int
DiskReader::refill (Sample* sum_buffer, Sample* mixdown_buffer, float* gain_buffer, samplecnt_t fill_level)
{
	if (refill_audio (sum_buffer, mixdown_buffer, gain_buffer, fill_level)) {
		return -1;
	}

	if (rt_midibuffer() && (_session.transport_speed() < 0.0f) != rt_midibuffer()->reversed()) {
		rt_midibuffer()->reverse ();
	}

	return 0;
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
DiskReader::refill_audio (Sample* sum_buffer, Sample* mixdown_buffer, float* gain_buffer, samplecnt_t fill_level)
{
	/* do not read from disk while session is marked as Loading, to avoid
	   useless redundant I/O.
	*/

	if (_session.loading()) {
		return 0;
	}

	int32_t ret = 0;
	bool const reversed = _session.transport_speed() < 0.0f;
	samplecnt_t zero_fill;
	uint32_t chan_n;
	ChannelList::iterator i;
	boost::shared_ptr<ChannelList> c = channels.reader();

	if (c->empty()) {
		return 0;
	}

	assert(mixdown_buffer);
	assert(gain_buffer);

	samplecnt_t total_space = c->front()->rbuf->write_space();

	if (total_space == 0) {
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
	   space to do disk_read_chunk_samples of I/O, then don't bother.

	   at higher speeds, just do it because the sync between butler
	   and audio thread may not be good enough.

	   Note: it is a design assumption that disk_read_chunk_samples is smaller
	   than the playback buffer size, so this check should never trip when
	   the playback buffer is empty.
	*/

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: space to refill %2 vs. chunk %3 (speed = %4)\n", name(), total_space, _chunk_samples, _session.transport_speed()));
	if ((total_space < _chunk_samples) && fabs (_session.transport_speed()) < 2.0f) {
		return 0;
	}

	/* when slaved, don't try to get too close to the read pointer. this
	   leaves space for the buffer reversal to have something useful to
	   work with.
	*/

	if (_slaved && total_space < (samplecnt_t) (c->front()->rbuf->bufsize() / 2)) {
		DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: not enough to refill while slaved\n", this));
		return 0;
	}

	samplepos_t ffa = file_sample[DataType::AUDIO];

	if (reversed) {

		if (ffa == 0) {
			/* at start: nothing to do but fill with silence */
			for (chan_n = 0, i = c->begin(); i != c->end(); ++i, ++chan_n) {
				ChannelInfo* chan (*i);
				chan->rbuf->write_zero (chan->rbuf->write_space ());
			}
			return 0;
		}

		if (ffa < total_space) {
			/* too close to the start: read what we can, and then zero fill the rest */
			zero_fill = total_space - ffa;
			total_space = ffa;
		} else {
			zero_fill = 0;
		}

	} else {

		if (ffa == max_samplepos) {
			/* at end: nothing to do but fill with silence */
			for (chan_n = 0, i = c->begin(); i != c->end(); ++i, ++chan_n) {
				ChannelInfo* chan (*i);
				chan->rbuf->write_zero (chan->rbuf->write_space ());
			}
			return 0;
		}

		if (ffa > max_samplepos - total_space) {
			/* to close to the end: read what we can, and zero fill the rest */
			zero_fill = total_space - (max_samplepos - ffa);
			total_space = max_samplepos - ffa;

		} else {
			zero_fill = 0;
		}
	}

	/* total_space is in samples. We want to optimize read sizes in various sizes using bytes */
	const size_t bits_per_sample = format_data_width (_session.config.get_native_file_data_format());
	size_t total_bytes = total_space * bits_per_sample / 8;

	/* chunk size range is 256kB to 4MB. Bigger is faster in terms of MB/sec, but bigger chunk size always takes longer */
	size_t byte_size_for_read = max ((size_t) (256 * 1024), min ((size_t) (4 * 1048576), total_bytes));

	/* find nearest (lower) multiple of 16384 */

	byte_size_for_read = (byte_size_for_read / 16384) * 16384;

	/* now back to samples */
	samplecnt_t samples_to_read = byte_size_for_read / (bits_per_sample / 8);

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: will refill %2 channels with %3 samples\n", name(), c->size(), total_space));

	samplepos_t file_sample_tmp = ffa;

	// int64_t before = g_get_monotonic_time ();
	// int64_t elapsed;

	for (chan_n = 0, i = c->begin(); i != c->end(); ++i, ++chan_n) {

		ChannelInfo* chan (*i);
		file_sample_tmp = ffa;
		samplecnt_t ts = total_space;

		samplecnt_t to_read = min (ts, (samplecnt_t) chan->rbuf->write_space ());
		to_read = min (to_read, samples_to_read);
		assert (to_read >= 0);

		// cerr << owner()->name() << " to-read: " << to_read << endl;

		if (to_read) {
			ReaderChannelInfo* rci = dynamic_cast<ReaderChannelInfo*> (chan);
			samplecnt_t nread;

			if (!_playlists[DataType::AUDIO]) {

				chan->rbuf->write_zero (to_read);

			} else {

				if ((nread = audio_read (sum_buffer, mixdown_buffer, gain_buffer, file_sample_tmp, to_read, rci, chan_n, reversed)) != to_read) {
					error << string_compose(_("DiskReader %1: when refilling, cannot read %2 from playlist at sample %3"), name(), to_read, ffa) << endmsg;
					ret = -1;
					goto out;
				}

				if (chan->rbuf->write (sum_buffer, nread) != nread) {
					error << string_compose(_("DiskReader %1: when refilling, cannot write %2 into buffer"), name(), nread) << endmsg;
					ret = -1;
				}
			}
		}

		if (zero_fill) {
			/* not sure if action is needed,
			 * we'll later hit the "to close to the end" case
			 */
			//chan->rbuf->write_zero (zero_fill);
		}
	}

	// elapsed = g_get_monotonic_time () - before;
	// cerr << '\t' << name() << ": bandwidth = " << (byte_size_for_read / 1048576.0) / (elapsed/1000000.0) << "MB/sec\n";

	file_sample[DataType::AUDIO] = file_sample_tmp;
	assert (file_sample[DataType::AUDIO] >= 0);

	ret = ((total_space - samples_to_read) > _chunk_samples);

  out:
	return ret;
}

void
DiskReader::playlist_ranges_moved (list< Evoral::RangeMove<samplepos_t> > const & movements_samples, bool from_undo_or_shift)
{
	/* If we're coming from an undo, it will have handled
	 * automation undo (it must, since automation-follows-regions
	 * can lose automation data).  Hence we can do nothing here.
	 *
	 * Likewise when shifting regions (insert/remove time)
	 * automation is taken care of separately (busses with
	 * automation have no disk-reader).
	 */

	if (from_undo_or_shift) {
		return;
	}

	if (!_track || Config->get_automation_follows_regions () == false) {
		return;
	}

	list< Evoral::RangeMove<double> > movements;

	for (list< Evoral::RangeMove<samplepos_t> >::const_iterator i = movements_samples.begin();
	     i != movements_samples.end();
	     ++i) {

		movements.push_back(Evoral::RangeMove<double>(i->from, i->length, i->to));
	}

	/* move panner automation */
	boost::shared_ptr<Pannable> pannable = _track->pannable();
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
        _track->foreach_processor (boost::bind (&DiskReader::move_processor_automation, this, _1, movements_samples));
}

void
DiskReader::move_processor_automation (boost::weak_ptr<Processor> p, list< Evoral::RangeMove<samplepos_t> > const & movements_samples)
{
	boost::shared_ptr<Processor> processor (p.lock ());
	if (!processor) {
		return;
	}

	list< Evoral::RangeMove<double> > movements;
	for (list< Evoral::RangeMove<samplepos_t> >::const_iterator i = movements_samples.begin(); i != movements_samples.end(); ++i) {
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

void
DiskReader::reset_tracker ()
{
	_tracker.reset ();
}

void
DiskReader::resolve_tracker (Evoral::EventSink<samplepos_t>& buffer, samplepos_t time)
{
	_tracker.resolve_notes (buffer, time);
}

/** Writes playback events from playback_sample for nframes to dst, translating time stamps
 *  so that an event at playback_sample has time = 0
 */
void
DiskReader::get_midi_playback (MidiBuffer& dst, samplepos_t start_sample, samplepos_t end_sample, MonitorState ms, BufferSet& scratch_bufs, double speed, samplecnt_t disk_samples_to_consume)
{
	RTMidiBuffer* rtmb = rt_midibuffer();

	if (!rtmb || (rtmb->size() == 0)) {
		/* no data to read, so do nothing */
		return;
	}

	MidiBuffer* target;

	if (ms & MonitoringInput) {
		/* data from disk needs to be *merged* not written into the
		   dst, because it may contain input data that we want to
		   monitor. Since RTMidiBuffer currently (Oct 2019) has no
		   suitable method, put the disk data into a scratch buffer and
		   then merge later.
		*/

		target = &scratch_bufs.get_midi (0);
	} else {
		/* No need to preserve the contents of the input buffer. But
		 * Route::process_output_buffers() clears the buffer as-needed
		 * so know we do not need to clear it.
		 */
		target = &dst;
	}

	if (!_no_disk_output) {

		const samplecnt_t nframes = abs (end_sample - start_sample);

		if (ms & MonitoringDisk) {

			/* disk data needed
			 */

			Location* loc = _loop_location;

			if (loc) {
				/* Evoral::Range has inclusive range semantics. Ugh. Hence the -1 */
				const Evoral::Range<samplepos_t> loop_range (loc->start(), loc->end() - 1);
				samplepos_t effective_start = start_sample;
				samplecnt_t cnt = nframes;
				sampleoffset_t offset = 0;

				DEBUG_TRACE (DEBUG::MidiDiskIO, string_compose ("LOOP read, loop is %1..%2 range is %3..%4 nf %5\n", loc->start(), loc->end(), start_sample, end_sample, nframes));

				do {

					samplepos_t effective_end;

					effective_start = loop_range.squish (effective_start);
					effective_end = min (effective_start + cnt, loc->end());
					assert (effective_end > effective_start);

					const samplecnt_t this_read = effective_end - effective_start;

					DEBUG_TRACE (DEBUG::MidiDiskIO, string_compose ("playback buffer LOOP read, from %1 to %2 (%3)\n", effective_start, effective_end, this_read));

					size_t events_read = rtmb->read (*target, effective_start, effective_end, _tracker, offset);
					cnt -= this_read;
					effective_start += this_read;
					offset += this_read;

					DEBUG_TRACE (DEBUG::MidiDiskIO, string_compose ("%1 MDS events LOOP read %2 cnt now %3\n", _name, events_read, cnt));

					if (cnt) {
						/* We re going to have to read across the loop end. Resolve any notes the extend across the loop end.
						 * Time is relative to start_sample.
						 */
						DEBUG_TRACE (DEBUG::MidiDiskIO, string_compose ("read crosses loop end, resolve @ %1\n", effective_end - start_sample));
						_tracker.resolve_notes (*target, effective_end - start_sample);
					}

				} while (cnt);

			} else {
				DEBUG_TRACE (DEBUG::MidiDiskIO, string_compose ("playback buffer read, from %1 to %2 (%3)\n", start_sample, end_sample, nframes));
				size_t events_read = rtmb->read (*target, start_sample, end_sample, _tracker);
				DEBUG_TRACE (DEBUG::MidiDiskIO, string_compose ("%1 MDS events read %2 range %3 .. %4\n", _name, events_read, playback_sample, playback_sample + nframes));
			}
		}

		if (ms & MonitoringInput) {
			/* merges data from disk (in "target", which is a scratch
			   buffer in this case) into the actual destination buffer
			   (which holds existing input data).
			*/
			dst.merge_from (*target, nframes);
		}
	}

#if 0
	if (!target->empty ()) {
		cerr << "======== MIDI OUT ========\n";
		for (MidiBuffer::iterator i = target->begin(); i != target->end(); ++i) {
			const Evoral::Event<MidiBuffer::TimeType> ev (*i, false);
			cerr << "MIDI EVENT (from disk) @ " << ev.time();
			for (size_t xx = 0; xx < ev.size(); ++xx) {
				cerr << ' ' << hex << (int) ev.buffer()[xx];
			}
			cerr << dec << endl;
		}
		cerr << "----------------\n";
	}
#endif
}
void
DiskReader::inc_no_disk_output ()
{
	g_atomic_int_inc (&_no_disk_output);
}

void
DiskReader::dec_no_disk_output ()
{
	/* this is called unconditionally when things happen that ought to end
	   a period of "no disk output". It's OK for that to happen when there
	   was no corresponding call to ::inc_no_disk_output(), but we must
	   stop the value from becoming negative.
	*/

	do {
		gint v  = g_atomic_int_get (&_no_disk_output);
		if (v > 0) {
			if (g_atomic_int_compare_and_exchange (&_no_disk_output, v, v - 1)) {
				break;
			}
		} else {
			break;
		}
	} while (true);
}

DiskReader::DeclickAmp::DeclickAmp (samplecnt_t sample_rate)
{
	_a = 4550.f / (gain_t)sample_rate;
	_l = -log1p (_a);
	_g = 0;
}

void
DiskReader::DeclickAmp::apply_gain (AudioBuffer& buf, samplecnt_t n_samples, const float target, sampleoffset_t buffer_offset)
{
	if (n_samples == 0) {
		return;
	}
	float g = _g;

	if (g == target) {
		assert (buffer_offset == 0);
		Amp::apply_simple_gain (buf, n_samples, target, 0);
		return;
	}

	const float a = _a;
	Sample* const buffer = buf.data ();

	const int max_nproc = 16;
	uint32_t remain = n_samples;
	uint32_t offset = buffer_offset;

	while (remain > 0) {
		uint32_t n_proc = remain > max_nproc ? max_nproc : remain;
		for (uint32_t i = 0; i < n_proc; ++i) {
			buffer[offset + i] *= g;
		}
#if 1
		g += a * (target - g);
#else /* accurate exponential fade */
		if (n_proc == max_nproc) {
			g += a * (target - g);
		} else {
			g = target - (target - g) * expf (_l * n_proc / max_nproc);
		}
#endif
		remain -= n_proc;
		offset += n_proc;
	}

	if (fabsf (g - target) < /* GAIN_COEFF_DELTA */ 1e-5) {
		_g = target;
	} else {
		_g = g;
	}
}

DiskReader::Declicker::Declicker ()
	: fade_start (0)
	, fade_end (0)
	, fade_length (0)
	, vec (0)
{
}

DiskReader::Declicker::~Declicker ()
{
	delete [] vec;
}

void
DiskReader::Declicker::alloc (samplecnt_t sr, bool fadein)
{
	delete [] vec;
	vec = new Sample[loop_fade_length];

	const float a = 1024.0f / sr;

	/* build a psuedo-exponential (linear-volume) shape for the fade */

	samplecnt_t n;

#define GAIN_COEFF_DELTA (1e-5)

	if (fadein) {
		gain_t g = 0.0;
		for (n = 0; (n < sr) && ((1.0 - g) > GAIN_COEFF_DELTA); ++n) {
			vec[n] = g;
			g += a * (1.0 - g);
		}
	} else {
		gain_t g = 1.0;
		for (n = 0; (n < sr) && (g > GAIN_COEFF_DELTA); ++n) {
			vec[n] = g;
			g += a * -g;
		}
	}

	fade_length = n;

	/* zero out the rest just to be safe */

	memset (&vec[n], 0, sizeof (gain_t) * (loop_fade_length - n));

#undef GAIN_COEFF_DELTA
}

void
DiskReader::Declicker::reset (samplepos_t loop_start, samplepos_t loop_end, bool fadein, samplecnt_t sr)
{
	if (loop_start == loop_end) {
		fade_start = 0;
		fade_end = 0;
		return;
	}

	/* adjust the position of the fade (this is absolute (global) timeline units) */

	if (fadein) {
		fade_start = loop_start;
		fade_end = loop_start + fade_length;
	} else {
		fade_start = loop_end - fade_length;
		fade_end = loop_end;
	}

}

void
DiskReader::Declicker::run (Sample* buf, samplepos_t read_start, samplepos_t read_end)
{
	samplecnt_t n;     /* how many samples to process */
	sampleoffset_t bo; /* offset into buffer */
	sampleoffset_t vo; /* offset into gain vector */

	if (fade_start == fade_end) {
		return;
	}

	/* Determine how the read range overlaps with the fade range, so we can determine which part of the fade gain vector
	   to apply to which part of the buffer.
	*/

	switch (Evoral::coverage (fade_start, fade_end, read_start, read_end)) {

	case Evoral::OverlapInternal:
		/* note: start and end points cannot coincide (see evoral/Range.h)
		 *
		 * read range is entirely within fade range
		 */
		bo = 0;
		vo = read_start - fade_start;
		n = read_end - read_start;
		break;

	case Evoral::OverlapExternal:
		/* read range extends on either side of fade range
		 *
		 * External allows coincidental start & end points, so check for that
		 */
		if (fade_start == read_start && fade_end == read_end) {
			/* fade entire read ... this is SO unlikely ! */
			bo = 0;
			vo = 0;
			n = fade_end - fade_start;
		} else {
			bo = fade_start - read_start;
			vo = 0;
			n = fade_end - fade_start;
		}
		break;

	case Evoral::OverlapStart:
		/* read range starts before and ends within fade or at same end as fade */
		n = fade_end - read_start;
		vo = 0;
		bo = fade_start - read_start;
		break;

	case Evoral::OverlapEnd:
		/* read range starts within fade range, but possibly at it's end, so check */
		if (read_start == fade_end) {
			/* nothing to do */
			return;
		}
		bo = 0;
		vo = read_start - fade_start;
		n = fade_end - read_start;
		break;

	case Evoral::OverlapNone:
		/* no overlap ... nothing to do */
		return;
	}

	Sample* b = &buf[bo];
	gain_t* g = &vec[vo];

	for (sampleoffset_t i = 0; i < n; ++i) {
		b[i] *= g[i];
	}
}

void
DiskReader::maybe_xfade_loop (Sample* buf, samplepos_t read_start, samplepos_t read_end, ReaderChannelInfo* chan)
{
	samplecnt_t n;     /* how many samples to process */
	sampleoffset_t bo; /* offset into buffer */
	sampleoffset_t vo; /* offset into gain vector */

	const samplepos_t fade_start = loop_declick_out.fade_start;
	const samplepos_t fade_end = loop_declick_out.fade_end;

	if (fade_start == fade_end) {
		return;
	}

	/* Determine how the read range overlaps with the fade range, so we can determine which part of the fade gain vector
	   to apply to which part of the buffer.
	*/

	switch (Evoral::coverage (fade_start, fade_end, read_start, read_end)) {

	case Evoral::OverlapInternal:
		/* note: start and end points cannot coincide (see evoral/Range.h)
		 *
		 * read range is entirely within fade range
		 */
		bo = 0;
		vo = read_start - fade_start;
		n = read_end - read_start;
		break;

	case Evoral::OverlapExternal:
		/* read range extends on either side of fade range
		 *
		 * External allows coincidental start & end points, so check for that
		 */
		if (fade_start == read_start && fade_end == read_end) {
			/* fade entire read ... this is SO unlikely ! */
			bo = 0;
			vo = 0;
			n = fade_end - fade_start;
		} else {
			bo = fade_start - read_start;
			vo = 0;
			n = fade_end - fade_start;
		}
		break;

	case Evoral::OverlapStart:
		/* read range starts before and ends within fade or at same end as fade */
		n = fade_end - read_start;
		vo = 0;
		bo = fade_start - read_start;
		break;

	case Evoral::OverlapEnd:
		/* read range starts within fade range, but possibly at it's end, so check */
		if (read_start == fade_end) {
			/* nothing to do */
			return;
		}
		bo = 0;
		vo = read_start - fade_start;
		n = fade_end - read_start;
		break;

	case Evoral::OverlapNone:
		/* no overlap ... nothing to do */
		return;
	}

	Sample* b    = &buf[bo];                   /* data to be faded out */
	Sample* sbuf = &chan->pre_loop_buffer[vo]; /* pre-loop (maybe silence) to be faded in */
	gain_t* og   = &loop_declick_out.vec[vo];  /* fade out gain vector */
	gain_t* ig   = &loop_declick_in.vec[vo];   /* fade in gain vector */

	for (sampleoffset_t i = 0; i < n; ++i) {
		b[i] = (b[i] * og[i]) + (sbuf[i] * ig[i]);
	}
}

RTMidiBuffer*
DiskReader::rt_midibuffer ()
{
	boost::shared_ptr<Playlist> pl = _playlists[DataType::MIDI];

	if (!pl) {
		return 0;
	}

	boost::shared_ptr<MidiPlaylist> mpl = boost::dynamic_pointer_cast<MidiPlaylist> (pl);

	if (!mpl) {
		/* error, but whatever ... */
		return 0;
	}

	return mpl->rendered();
}

void
DiskReader::alloc_loop_declick (samplecnt_t sr)
{
	loop_fade_length = lrintf (ceil (-log (1e-5) / (1024.f/sr)));
	loop_declick_in.alloc (sr, true);
	loop_declick_out.alloc (sr, false);
}

void
DiskReader::reset_loop_declick (Location* loc, samplecnt_t sr)
{
	if (loc) {
		loop_declick_in.reset (loc->start(), loc->end(), true, sr);
		loop_declick_out.reset (loc->start(), loc->end(), false, sr);
	} else {
		loop_declick_in.reset (0, 0, true, sr);
		loop_declick_out.reset (0, 0, false, sr);
	}
}

void
DiskReader::set_loop (Location* loc)
{
	Processor::set_loop (loc);

	if (!loc) {
		return;
	}

	reload_loop ();
}

void
DiskReader::reload_loop ()
{
	if (!_loop_location) {
		return;
	}

	boost::shared_ptr<ChannelList> c = channels.reader();

	if (c->empty() || !_playlists[DataType::AUDIO]) {
		return;
	}

	Location* loc = _loop_location;
	boost::scoped_array<Sample> mix_buf (new Sample [loop_fade_length]);
	boost::scoped_array<Sample> gain_buf (new Sample [loop_fade_length]);

	uint32_t channel = 0;

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan, ++channel) {

		ReaderChannelInfo* rci = dynamic_cast<ReaderChannelInfo*> (*chan);

		rci->resize_preloop (loop_fade_length);

		if (loc->start() > loop_fade_length) {
			audio_playlist()->read (rci->pre_loop_buffer, mix_buf.get(), gain_buf.get(), loc->start() - loop_declick_out.fade_length, loop_declick_out.fade_length, channel);
		} else {
			memset (rci->pre_loop_buffer, 0, sizeof (Sample) * loop_fade_length);
		}

	}
}

bool
DiskReader::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	if (!DiskIOProcessor::can_support_io_configuration (in, out)) {
		return false;
	}

	/* DiskIO might have done this too, but do it again anyway as a
	 * starting point.
	 */

	out == in;

	if (_playlists[DataType::AUDIO]) {
		ChannelList::size_type naudio = max (ChannelList::size_type (1), channels.reader()->size());
		if (out.n_audio() < naudio) {
			out.set (DataType::AUDIO, naudio);
		}
	}

	if (_playlists[DataType::MIDI]) {
		if (out.n_midi() != 1) {
			out.set (DataType::MIDI, 1);
		}
	}

	return true;
}
