/*
    Copyright (C) 2000-2006 Paul Davis

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
#include <cassert>
#include <string>
#include <climits>
#include <fcntl.h>
#include <cstdlib>
#include <ctime>
#include <sys/stat.h>
#include <sys/mman.h>

#include "pbd/error.h"
#include "pbd/xml++.h"
#include "pbd/memento_command.h"
#include "pbd/enumwriter.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/analyser.h"
#include "ardour/audio_buffer.h"
#include "ardour/audio_diskstream.h"
#include "ardour/audio_port.h"
#include "ardour/audioengine.h"
#include "ardour/audiofilesource.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/butler.h"
#include "ardour/debug.h"
#include "ardour/io.h"
#include "ardour/playlist_factory.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/source_factory.h"
#include "ardour/track.h"
#include "ardour/types.h"
#include "ardour/utils.h"

#include "i18n.h"
#include <locale.h>

using namespace std;
using namespace ARDOUR;
using namespace PBD;

size_t  AudioDiskstream::_working_buffers_size = 0;
Sample* AudioDiskstream::_mixdown_buffer       = 0;
gain_t* AudioDiskstream::_gain_buffer          = 0;

AudioDiskstream::AudioDiskstream (Session &sess, const string &name, Diskstream::Flag flag)
	: Diskstream(sess, name, flag)
	, channels (new ChannelList)
{
	/* prevent any write sources from being created */

	in_set_state = true;
	use_new_playlist ();
	in_set_state = false;
}

AudioDiskstream::AudioDiskstream (Session& sess, const XMLNode& node)
	: Diskstream(sess, node)
	, channels (new ChannelList)
{
	in_set_state = true;
	init ();

	if (set_state (node, Stateful::loading_state_version)) {
		in_set_state = false;
		throw failed_constructor();
	}

	in_set_state = false;

	if (destructive()) {
		use_destructive_playlist ();
	}
}

void
AudioDiskstream::init ()
{
	/* there are no channels at this point, so these
	   two calls just get speed_buffer_size and wrap_buffer
	   size setup without duplicating their code.
	*/

	set_block_size (_session.get_block_size());
	allocate_temporary_buffers ();
}

AudioDiskstream::~AudioDiskstream ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("Audio Diskstream %1 destructor\n", _name));

	{
		RCUWriter<ChannelList> writer (channels);
		boost::shared_ptr<ChannelList> c = writer.get_copy();

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			delete *chan;
		}

		c->clear();
	}

	channels.flush ();
}

void
AudioDiskstream::allocate_working_buffers()
{
	assert(disk_io_frames() > 0);

	_working_buffers_size = disk_io_frames();
	_mixdown_buffer       = new Sample[_working_buffers_size];
	_gain_buffer          = new gain_t[_working_buffers_size];
}

void
AudioDiskstream::free_working_buffers()
{
	delete [] _mixdown_buffer;
	delete [] _gain_buffer;
	_working_buffers_size = 0;
	_mixdown_buffer       = 0;
	_gain_buffer          = 0;
}

void
AudioDiskstream::non_realtime_input_change ()
{
	{
		Glib::Threads::Mutex::Lock lm (state_lock);

		if (input_change_pending.type == IOChange::NoChange) {
			return;
		}

		if (input_change_pending.type == IOChange::ConfigurationChanged) {
			RCUWriter<ChannelList> writer (channels);
			boost::shared_ptr<ChannelList> c = writer.get_copy();

			_n_channels.set(DataType::AUDIO, c->size());

			if (_io->n_ports().n_audio() > _n_channels.n_audio()) {
				add_channel_to (c, _io->n_ports().n_audio() - _n_channels.n_audio());
			} else if (_io->n_ports().n_audio() < _n_channels.n_audio()) {
				remove_channel_from (c, _n_channels.n_audio() - _io->n_ports().n_audio());
			}
		}

		if (input_change_pending.type & IOChange::ConnectionsChanged) {
			get_input_sources ();
			set_capture_offset ();
			set_align_style_from_io ();
		}

		input_change_pending = IOChange::NoChange;

		/* implicit unlock */
	}

	/* reset capture files */

	reset_write_sources (false);

	/* now refill channel buffers */

	if (speed() != 1.0f || speed() != -1.0f) {
		seek ((framepos_t) (_session.transport_frame() * (double) speed()));
	} else {
		seek (_session.transport_frame());
	}
}

void
AudioDiskstream::non_realtime_locate (framepos_t location)
{
	/* now refill channel buffers */

	if (speed() != 1.0f || speed() != -1.0f) {
		seek ((framepos_t) (location * (double) speed()));
	} else {
		seek (location);
	}
}

void
AudioDiskstream::get_input_sources ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	uint32_t n;
	ChannelList::iterator chan;
	uint32_t ni = _io->n_ports().n_audio();
	vector<string> connections;

	for (n = 0, chan = c->begin(); chan != c->end() && n < ni; ++chan, ++n) {

		connections.clear ();

		if (_io->nth (n)->get_connections (connections) == 0) {
			if (!(*chan)->source.name.empty()) {
				// _source->disable_metering ();
			}
			(*chan)->source.name = string();
		} else {
			(*chan)->source.name = connections[0];
		}
	}
}

int
AudioDiskstream::find_and_use_playlist (const string& name)
{
	boost::shared_ptr<AudioPlaylist> playlist;

	if ((playlist = boost::dynamic_pointer_cast<AudioPlaylist> (_session.playlists->by_name (name))) == 0) {
		playlist = boost::dynamic_pointer_cast<AudioPlaylist> (PlaylistFactory::create (DataType::AUDIO, _session, name));
	}

	if (!playlist) {
		error << string_compose(_("AudioDiskstream: Playlist \"%1\" isn't an audio playlist"), name) << endmsg;
		return -1;
	}

	return use_playlist (playlist);
}

int
AudioDiskstream::use_playlist (boost::shared_ptr<Playlist> playlist)
{
	assert(boost::dynamic_pointer_cast<AudioPlaylist>(playlist));

	Diskstream::use_playlist(playlist);

	return 0;
}

int
AudioDiskstream::use_new_playlist ()
{
	string newname;
	boost::shared_ptr<AudioPlaylist> playlist;

	if (!in_set_state && destructive()) {
		return 0;
	}

	if (_playlist) {
		newname = Playlist::bump_name (_playlist->name(), _session);
	} else {
		newname = Playlist::bump_name (_name, _session);
	}

	if ((playlist = boost::dynamic_pointer_cast<AudioPlaylist> (PlaylistFactory::create (DataType::AUDIO, _session, newname, hidden()))) != 0) {

		return use_playlist (playlist);

	} else {
		return -1;
	}
}

int
AudioDiskstream::use_copy_playlist ()
{
	assert(audio_playlist());

	if (destructive()) {
		return 0;
	}

	if (_playlist == 0) {
		error << string_compose(_("AudioDiskstream %1: there is no existing playlist to make a copy of!"), _name) << endmsg;
		return -1;
	}

	string newname;
	boost::shared_ptr<AudioPlaylist> playlist;

	newname = Playlist::bump_name (_playlist->name(), _session);

	if ((playlist = boost::dynamic_pointer_cast<AudioPlaylist>(PlaylistFactory::create (audio_playlist(), newname))) != 0) {
		return use_playlist (playlist);
	} else {
		return -1;
	}
}

void
AudioDiskstream::setup_destructive_playlist ()
{
	SourceList srcs;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		srcs.push_back ((*chan)->write_source);
	}

	/* a single full-sized region */

	assert (!srcs.empty ());

	PropertyList plist;
	plist.add (Properties::name, _name.val());
	plist.add (Properties::start, 0);
	plist.add (Properties::length, max_framepos - (max_framepos - srcs.front()->natural_position()));

	boost::shared_ptr<Region> region (RegionFactory::create (srcs, plist));
	_playlist->add_region (region, srcs.front()->natural_position());
}

void
AudioDiskstream::use_destructive_playlist ()
{
	/* this is called from the XML-based constructor or ::set_destructive. when called,
	   we already have a playlist and a region, but we need to
	   set up our sources for write. we use the sources associated
	   with the (presumed single, full-extent) region.
	*/

	boost::shared_ptr<Region> rp = _playlist->find_next_region (_session.current_start_frame(), Start, 1);

	if (!rp) {
		reset_write_sources (false, true);
		return;
	}

	boost::shared_ptr<AudioRegion> region = boost::dynamic_pointer_cast<AudioRegion> (rp);

	if (region == 0) {
		throw failed_constructor();
	}

	/* be sure to stretch the region out to the maximum length */

	region->set_length (max_framepos - region->position());

	uint32_t n;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->write_source = boost::dynamic_pointer_cast<AudioFileSource>(region->source (n));
		assert((*chan)->write_source);
		(*chan)->write_source->set_allow_remove_if_empty (false);

		/* this might be false if we switched modes, so force it */

		(*chan)->write_source->set_destructive (true);
	}

	/* the source list will never be reset for a destructive track */
}

void
AudioDiskstream::prepare_record_status(framepos_t capture_start_frame)
{
	if (recordable() && destructive()) {
		boost::shared_ptr<ChannelList> c = channels.reader();
		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

			RingBufferNPT<CaptureTransition>::rw_vector transitions;
			(*chan)->capture_transition_buf->get_write_vector (&transitions);

			if (transitions.len[0] > 0) {
				transitions.buf[0]->type = CaptureStart;
				transitions.buf[0]->capture_val = capture_start_frame;
				(*chan)->capture_transition_buf->increment_write_ptr(1);
			} else {
				// bad!
				fatal << X_("programming error: capture_transition_buf is full on rec start!  inconceivable!")
					<< endmsg;
			}
		}
	}
}


/** Do some record stuff [not described in this comment!]
 *
 *  Also:
 *    - Setup playback_distance with the nframes, or nframes adjusted
 *      for current varispeed, if appropriate.
 *    - Setup current_playback_buffer in each ChannelInfo to point to data
 *      that someone can read playback_distance worth of data from.
 */
int
AudioDiskstream::process (framepos_t transport_frame, pframes_t nframes, framecnt_t& playback_distance)
{
	uint32_t n;
	boost::shared_ptr<ChannelList> c = channels.reader();
	ChannelList::iterator chan;
	framecnt_t rec_offset = 0;
	framecnt_t rec_nframes = 0;
	bool collect_playback = false;
	bool can_record = _session.actively_recording ();

	playback_distance = 0;

	if (!_io || !_io->active()) {
		return 0;
	}

	check_record_status (transport_frame, can_record);

	if (nframes == 0) {
		return 0;
	}

	Glib::Threads::Mutex::Lock sm (state_lock, Glib::Threads::TRY_LOCK);

	if (!sm.locked()) {
		return 1;
	}

	adjust_capture_position = 0;

	for (chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->current_capture_buffer = 0;
		(*chan)->current_playback_buffer = 0;
	}

	// Safeguard against situations where process() goes haywire when autopunching
	// and last_recordable_frame < first_recordable_frame

	if (last_recordable_frame < first_recordable_frame) {
		last_recordable_frame = max_framepos;
	}

	if (record_enabled()) {

		Evoral::OverlapType ot = Evoral::coverage (first_recordable_frame, last_recordable_frame, transport_frame, transport_frame + nframes);
		calculate_record_range (ot, transport_frame, nframes, rec_nframes, rec_offset);

		if (rec_nframes && !was_recording) {
			capture_captured = 0;
			was_recording = true;
		}
	}

	if (can_record && !_last_capture_sources.empty()) {
		_last_capture_sources.clear ();
	}

	if (rec_nframes) {

		uint32_t limit = _io->n_ports ().n_audio();

		/* one or more ports could already have been removed from _io, but our
		   channel setup hasn't yet been updated. prevent us from trying to
		   use channels that correspond to missing ports. note that the
		   process callback (from which this is called) is always atomic
		   with respect to port removal/addition.
		*/

		for (n = 0, chan = c->begin(); chan != c->end() && n < limit; ++chan, ++n) {

			ChannelInfo* chaninfo (*chan);

			chaninfo->capture_buf->get_write_vector (&chaninfo->capture_vector);

			if (rec_nframes <= (framecnt_t) chaninfo->capture_vector.len[0]) {

				chaninfo->current_capture_buffer = chaninfo->capture_vector.buf[0];

				/* note: grab the entire port buffer, but only copy what we were supposed to
				   for recording, and use rec_offset
				*/

				boost::shared_ptr<AudioPort> const ap = _io->audio (n);
				assert(ap);
				assert(rec_nframes <= (framecnt_t) ap->get_audio_buffer(nframes).capacity());

				Sample *buf = ap->get_audio_buffer (nframes).data (rec_offset);
				memcpy (chaninfo->current_capture_buffer, buf, sizeof (Sample) * rec_nframes);

			} else {

				framecnt_t total = chaninfo->capture_vector.len[0] + chaninfo->capture_vector.len[1];

				if (rec_nframes > total) {
					DiskOverrun ();
					return -1;
				}

				boost::shared_ptr<AudioPort> const ap = _io->audio (n);
				assert(ap);

				Sample* buf = ap->get_audio_buffer(nframes).data (rec_offset);
				framecnt_t first = chaninfo->capture_vector.len[0];

				memcpy (chaninfo->capture_wrap_buffer, buf, sizeof (Sample) * first);
				memcpy (chaninfo->capture_vector.buf[0], buf, sizeof (Sample) * first);
				memcpy (chaninfo->capture_wrap_buffer+first, buf + first, sizeof (Sample) * (rec_nframes - first));
				memcpy (chaninfo->capture_vector.buf[1], buf + first, sizeof (Sample) * (rec_nframes - first));

				chaninfo->current_capture_buffer = chaninfo->capture_wrap_buffer;
			}
		}

	} else {

		if (was_recording) {
			finish_capture (c);
		}

	}

	if (rec_nframes) {

		/* data will be written to disk */

		if (rec_nframes == nframes && rec_offset == 0) {

			for (chan = c->begin(); chan != c->end(); ++chan) {
				(*chan)->current_playback_buffer = (*chan)->current_capture_buffer;
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

	} else if (can_record && record_enabled()) {

		/* can't do actual capture yet - waiting for latency effects to finish before we start*/

		for (chan = c->begin(); chan != c->end(); ++chan) {
			(*chan)->current_playback_buffer = (*chan)->current_capture_buffer;
		}

		playback_distance = nframes;

	} else {

		collect_playback = true;
	}

	if ((_track->monitoring_state () & MonitoringDisk) || collect_playback) {
		
		/* we're doing playback */

		framecnt_t necessary_samples;

		/* no varispeed playback if we're recording, because the output .... TBD */

		if (rec_nframes == 0 && _actual_speed != 1.0f) {
			necessary_samples = (framecnt_t) ceil ((nframes * fabs (_actual_speed))) + 2;
		} else {
			necessary_samples = nframes;
		}

		for (chan = c->begin(); chan != c->end(); ++chan) {
			(*chan)->playback_buf->get_read_vector (&(*chan)->playback_vector);
		}

		n = 0;

		/* Setup current_playback_buffer in each ChannelInfo to point to data that someone
		   can read necessary_samples (== nframes at a transport speed of 1) worth of data
		   from right now.
		*/

		for (chan = c->begin(); chan != c->end(); ++chan, ++n) {

			ChannelInfo* chaninfo (*chan);

			if (necessary_samples <= (framecnt_t) chaninfo->playback_vector.len[0]) {
				/* There are enough samples in the first part of the ringbuffer */
				chaninfo->current_playback_buffer = chaninfo->playback_vector.buf[0];

			} else {
				framecnt_t total = chaninfo->playback_vector.len[0] + chaninfo->playback_vector.len[1];

				if (necessary_samples > total) {
					cerr << _name << " Need " << necessary_samples << " total = " << total << endl;
					cerr << "underrun for " << _name << endl;
					DiskUnderrun ();
					return -1;

				} else {

					/* We have enough samples, but not in one lump.  Coalesce the two parts
					   into one in playback_wrap_buffer in our ChannelInfo, and specify that
					   as our current_playback_buffer.
					*/

					/* Copy buf[0] from playback_buf */
					memcpy ((char *) chaninfo->playback_wrap_buffer,
							chaninfo->playback_vector.buf[0],
							chaninfo->playback_vector.len[0] * sizeof (Sample));
					
					/* Copy buf[1] from playback_buf */
					memcpy (chaninfo->playback_wrap_buffer + chaninfo->playback_vector.len[0],
							chaninfo->playback_vector.buf[1],
							(necessary_samples - chaninfo->playback_vector.len[0])
									* sizeof (Sample));

					chaninfo->current_playback_buffer = chaninfo->playback_wrap_buffer;
				}
			}
		}

		if (rec_nframes == 0 && _actual_speed != 1.0f && _actual_speed != -1.0f) {

			interpolation.set_speed (_target_speed);

			int channel = 0;
			for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan, ++channel) {
				ChannelInfo* chaninfo (*chan);
				
				playback_distance = interpolation.interpolate (
					channel, nframes, chaninfo->current_playback_buffer, chaninfo->speed_buffer);
				
				chaninfo->current_playback_buffer = chaninfo->speed_buffer;
			}
			
		} else {
			playback_distance = nframes;
		}

		_speed = _target_speed;
	}

	return 0;
}

/** Update various things including playback_sample, read pointer on each channel's playback_buf
 *  and write pointer on each channel's capture_buf.  Also wout whether the butler is needed.
 *  @return true if the butler is required.
 */
bool
AudioDiskstream::commit (framecnt_t playback_distance)
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

	boost::shared_ptr<ChannelList> c = channels.reader();
	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

		(*chan)->playback_buf->increment_read_ptr (playback_distance);

		if (adjust_capture_position) {
			(*chan)->capture_buf->increment_write_ptr (adjust_capture_position);
		}
	}

	if (adjust_capture_position != 0) {
		capture_captured += adjust_capture_position;
		adjust_capture_position = 0;
	}

	if (c->empty()) {
		return false;
	}

	if (_slaved) {
		if (_io && _io->active()) {
			need_butler = c->front()->playback_buf->write_space() >= c->front()->playback_buf->bufsize() / 2;
		} else {
			need_butler = false;
		}
	} else {
		if (_io && _io->active()) {
			need_butler = ((framecnt_t) c->front()->playback_buf->write_space() >= disk_io_chunk_frames)
				|| ((framecnt_t) c->front()->capture_buf->read_space() >= disk_io_chunk_frames);
		} else {
			need_butler = ((framecnt_t) c->front()->capture_buf->read_space() >= disk_io_chunk_frames);
		}
	}

	return need_butler;
}

void
AudioDiskstream::set_pending_overwrite (bool yn)
{
	/* called from audio thread, so we can use the read ptr and playback sample as we wish */

	_pending_overwrite = yn;

	overwrite_frame = playback_sample;

	boost::shared_ptr<ChannelList> c = channels.reader ();
	if (!c->empty ()) {
		overwrite_offset = c->front()->playback_buf->get_read_ptr();
	}
}

int
AudioDiskstream::overwrite_existing_buffers ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();
	if (c->empty ()) {
		_pending_overwrite = false;
		return 0;
	}

	Sample* mixdown_buffer;
	float* gain_buffer;
	int ret = -1;
	bool reversed = (_visible_speed * _session.transport_speed()) < 0.0f;

	overwrite_queued = false;

	/* assume all are the same size */
	framecnt_t size = c->front()->playback_buf->bufsize();

	mixdown_buffer = new Sample[size];
	gain_buffer = new float[size];

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

		if (read ((*chan)->playback_buf->buffer() + overwrite_offset, mixdown_buffer, gain_buffer, start, to_read, n, reversed)) {
			error << string_compose(_("AudioDiskstream %1: when refilling, cannot read %2 from playlist at frame %3"),
						id(), size, playback_sample) << endmsg;
			goto out;
		}

		if (cnt > to_read) {

			cnt -= to_read;

			if (read ((*chan)->playback_buf->buffer(), mixdown_buffer, gain_buffer, start, cnt, n, reversed)) {
				error << string_compose(_("AudioDiskstream %1: when refilling, cannot read %2 from playlist at frame %3"),
							id(), size, playback_sample) << endmsg;
				goto out;
			}
		}
	}

	ret = 0;

  out:
	_pending_overwrite = false;
	delete [] gain_buffer;
	delete [] mixdown_buffer;
	return ret;
}

int
AudioDiskstream::seek (framepos_t frame, bool complete_refill)
{
	uint32_t n;
	int ret = -1;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	Glib::Threads::Mutex::Lock lm (state_lock);

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->playback_buf->reset ();
		(*chan)->capture_buf->reset ();
	}

	/* can't rec-enable in destructive mode if transport is before start */

	if (destructive() && record_enabled() && frame < _session.current_start_frame()) {
		disengage_record_enable ();
	}

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
AudioDiskstream::can_internal_playback_seek (framecnt_t distance)
{
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (chan = c->begin(); chan != c->end(); ++chan) {
		if ((*chan)->playback_buf->read_space() < (size_t) distance) {
			return false;
		}
	}
	return true;
}

int
AudioDiskstream::internal_playback_seek (framecnt_t distance)
{
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->playback_buf->increment_read_ptr (distance);
	}

	if (first_recordable_frame < max_framepos) {
		first_recordable_frame += distance;
	}
	playback_sample += distance;

	return 0;
}

/** Read some data for 1 channel from our playlist into a buffer.
 *  @param buf Buffer to write to.
 *  @param start Session frame to start reading from; updated to where we end up
 *         after the read.
 *  @param cnt Count of samples to read.
 *  @param reversed true if we are running backwards, otherwise false.
 */
int
AudioDiskstream::read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer,
                       framepos_t& start, framecnt_t cnt,
                       int channel, bool reversed)
{
	framecnt_t this_read = 0;
	bool reloop = false;
	framepos_t loop_end = 0;
	framepos_t loop_start = 0;
	framecnt_t offset = 0;
	Location *loc = 0;

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
			error << string_compose(_("AudioDiskstream %1: cannot read %2 from playlist at frame %3"), id(), this_read,
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
AudioDiskstream::do_refill_with_alloc ()
{
	Sample* mix_buf  = new Sample[disk_io_chunk_frames];
	float*  gain_buf = new float[disk_io_chunk_frames];

	int ret = _do_refill(mix_buf, gain_buf);

	delete [] mix_buf;
	delete [] gain_buf;

	return ret;
}

/** Get some more data from disk and put it in our channels' playback_bufs,
 *  if there is suitable space in them.
 */
int
AudioDiskstream::_do_refill (Sample* mixdown_buffer, float* gain_buffer)
{
	int32_t ret = 0;
	framecnt_t to_read;
	RingBufferNPT<Sample>::rw_vector vector;
	bool const reversed = (_visible_speed * _session.transport_speed()) < 0.0f;
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

	c->front()->playback_buf->get_write_vector (&vector);

	if ((total_space = vector.len[0] + vector.len[1]) == 0) {
		/* nowhere to write to */
		return 0;
	}

	/* if there are 2+ chunks of disk i/o possible for
	   this track, let the caller know so that it can arrange
	   for us to be called again, ASAP.
	*/

	if (total_space >= (_slaved ? 3 : 2) * disk_io_chunk_frames) {
		ret = 1;
	}

	/* if we're running close to normal speed and there isn't enough
	   space to do disk_io_chunk_frames of I/O, then don't bother.

	   at higher speeds, just do it because the sync between butler
	   and audio thread may not be good enough.

	   Note: it is a design assumption that disk_io_chunk_frames is smaller
	   than the playback buffer size, so this check should never trip when
	   the playback buffer is empty.
	*/

	if ((total_space < disk_io_chunk_frames) && fabs (_actual_speed) < 2.0f) {
		return 0;
	}

	/* when slaved, don't try to get too close to the read pointer. this
	   leaves space for the buffer reversal to have something useful to
	   work with.
	*/

	if (_slaved && total_space < (framecnt_t) (c->front()->playback_buf->bufsize() / 2)) {
		return 0;
	}

	/* never do more than disk_io_chunk_frames worth of disk input per call (limit doesn't apply for memset) */

	total_space = min (disk_io_chunk_frames, total_space);

	if (reversed) {

		if (file_frame == 0) {

			/* at start: nothing to do but fill with silence */

			for (chan_n = 0, i = c->begin(); i != c->end(); ++i, ++chan_n) {

				ChannelInfo* chan (*i);
				chan->playback_buf->get_write_vector (&vector);
				memset (vector.buf[0], 0, sizeof(Sample) * vector.len[0]);
				if (vector.len[1]) {
					memset (vector.buf[1], 0, sizeof(Sample) * vector.len[1]);
				}
				chan->playback_buf->increment_write_ptr (vector.len[0] + vector.len[1]);
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
				chan->playback_buf->get_write_vector (&vector);
				memset (vector.buf[0], 0, sizeof(Sample) * vector.len[0]);
				if (vector.len[1]) {
					memset (vector.buf[1], 0, sizeof(Sample) * vector.len[1]);
				}
				chan->playback_buf->increment_write_ptr (vector.len[0] + vector.len[1]);
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

	for (chan_n = 0, i = c->begin(); i != c->end(); ++i, ++chan_n) {

		ChannelInfo* chan (*i);
		Sample* buf1;
		Sample* buf2;
		framecnt_t len1, len2;

		chan->playback_buf->get_write_vector (&vector);

		if ((framecnt_t) vector.len[0] > disk_io_chunk_frames) {

			/* we're not going to fill the first chunk, so certainly do not bother with the
			   other part. it won't be connected with the part we do fill, as in:

			   .... => writable space
			   ++++ => readable space
			   ^^^^ => 1 x disk_io_chunk_frames that would be filled

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
		to_read = min (to_read, disk_io_chunk_frames);

		assert (to_read >= 0);

		if (to_read) {

			if (read (buf1, mixdown_buffer, gain_buffer, file_frame_tmp, to_read, chan_n, reversed)) {
				ret = -1;
				goto out;
			}

			chan->playback_buf->increment_write_ptr (to_read);
			ts -= to_read;
		}

		to_read = min (ts, len2);

		if (to_read) {

			/* we read all of vector.len[0], but it wasn't an entire disk_io_chunk_frames of data,
			   so read some or all of vector.len[1] as well.
			*/

			if (read (buf2, mixdown_buffer, gain_buffer, file_frame_tmp, to_read, chan_n, reversed)) {
				ret = -1;
				goto out;
			}

			chan->playback_buf->increment_write_ptr (to_read);
		}

		if (zero_fill) {
			/* XXX: do something */
		}

	}

	file_frame = file_frame_tmp;
	assert (file_frame >= 0);

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
AudioDiskstream::do_flush (RunContext /*context*/, bool force_flush)
{
	uint32_t to_write;
	int32_t ret = 0;
	RingBufferNPT<Sample>::rw_vector vector;
	RingBufferNPT<CaptureTransition>::rw_vector transvec;
	framecnt_t total;

	transvec.buf[0] = 0;
	transvec.buf[1] = 0;
	vector.buf[0] = 0;
	vector.buf[1] = 0;

	boost::shared_ptr<ChannelList> c = channels.reader();
	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

		(*chan)->capture_buf->get_read_vector (&vector);

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

		to_write = min (disk_io_chunk_frames, (framecnt_t) vector.len[0]);

		// check the transition buffer when recording destructive
		// important that we get this after the capture buf

		if (destructive()) {
			(*chan)->capture_transition_buf->get_read_vector(&transvec);
			size_t transcount = transvec.len[0] + transvec.len[1];
			size_t ti;

			for (ti=0; ti < transcount; ++ti) {
				CaptureTransition & captrans = (ti < transvec.len[0]) ? transvec.buf[0][ti] : transvec.buf[1][ti-transvec.len[0]];

				if (captrans.type == CaptureStart) {
					// by definition, the first data we got above represents the given capture pos

					(*chan)->write_source->mark_capture_start (captrans.capture_val);
					(*chan)->curr_capture_cnt = 0;

				} else if (captrans.type == CaptureEnd) {

					// capture end, the capture_val represents total frames in capture

					if (captrans.capture_val <= (*chan)->curr_capture_cnt + to_write) {

						// shorten to make the write a perfect fit
						uint32_t nto_write = (captrans.capture_val - (*chan)->curr_capture_cnt);

						if (nto_write < to_write) {
							ret = 1; // should we?
						}
						to_write = nto_write;

						(*chan)->write_source->mark_capture_end ();

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
				(*chan)->capture_transition_buf->increment_read_ptr(ti);
			}
		}

		if ((!(*chan)->write_source) || (*chan)->write_source->write (vector.buf[0], to_write) != to_write) {
			error << string_compose(_("AudioDiskstream %1: cannot write to disk"), id()) << endmsg;
			return -1;
		}

		(*chan)->capture_buf->increment_read_ptr (to_write);
		(*chan)->curr_capture_cnt += to_write;

		if ((to_write == vector.len[0]) && (total > to_write) && (to_write < disk_io_chunk_frames) && !destructive()) {

			/* we wrote all of vector.len[0] but it wasn't an entire
			   disk_io_chunk_frames of data, so arrange for some part
			   of vector.len[1] to be flushed to disk as well.
			*/

			to_write = min ((framecnt_t)(disk_io_chunk_frames - to_write), (framecnt_t) vector.len[1]);

			if ((*chan)->write_source->write (vector.buf[1], to_write) != to_write) {
				error << string_compose(_("AudioDiskstream %1: cannot write to disk"), id()) << endmsg;
				return -1;
			}

			(*chan)->capture_buf->increment_read_ptr (to_write);
			(*chan)->curr_capture_cnt += to_write;
		}
	}

  out:
	return ret;
}

void
AudioDiskstream::transport_stopped_wallclock (struct tm& when, time_t twhen, bool abort_capture)
{
	uint32_t buffer_position;
	bool more_work = true;
	int err = 0;
	boost::shared_ptr<AudioRegion> region;
	framecnt_t total_capture;
	SourceList srcs;
	SourceList::iterator src;
	ChannelList::iterator chan;
	vector<CaptureInfo*>::iterator ci;
	boost::shared_ptr<ChannelList> c = channels.reader();
	uint32_t n = 0;
	bool mark_write_completed = false;

	finish_capture (c);

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
			error << string_compose(_("AudioDiskstream \"%1\": cannot flush captured data to disk!"), _name) << endmsg;
			err++;
		}
	}

	/* XXX is there anything we can do if err != 0 ? */
	Glib::Threads::Mutex::Lock lm (capture_info_lock);

	if (capture_info.empty()) {
		return;
	}

	if (abort_capture) {

		if (destructive()) {
			goto outout;
		}

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

			if ((*chan)->write_source) {

				(*chan)->write_source->mark_for_remove ();
				(*chan)->write_source->drop_references ();
				(*chan)->write_source.reset ();
			}

			/* new source set up in "out" below */
		}

		goto out;
	}

	for (total_capture = 0, ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		total_capture += (*ci)->frames;
	}

	/* figure out the name for this take */

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {

		boost::shared_ptr<AudioFileSource> s = (*chan)->write_source;

		if (s) {
			srcs.push_back (s);
			s->update_header (capture_info.front()->start, when, twhen);
			s->set_captured_for (_name.val());
			s->mark_immutable ();

			if (Config->get_auto_analyse_audio()) {
				Analyser::queue_source_for_analysis (s, true);
			}
		}
	}

	/* destructive tracks have a single, never changing region */

	if (destructive()) {

		/* send a signal that any UI can pick up to do the right thing. there is
		   a small problem here in that a UI may need the peak data to be ready
		   for the data that was recorded and this isn't interlocked with that
		   process. this problem is deferred to the UI.
		 */

		_playlist->LayeringChanged(); // XXX this may not get the UI to do the right thing

	} else {

		string whole_file_region_name;
		whole_file_region_name = region_name_from_path (c->front()->write_source->name(), true);

		/* Register a new region with the Session that
		   describes the entire source. Do this first
		   so that any sub-regions will obviously be
		   children of this one (later!)
		*/

		try {
			PropertyList plist;

			plist.add (Properties::start, c->front()->write_source->last_capture_start_frame());
			plist.add (Properties::length, total_capture);
			plist.add (Properties::name, whole_file_region_name);
			boost::shared_ptr<Region> rx (RegionFactory::create (srcs, plist));
			rx->set_automatic (true);
			rx->set_whole_file (true);

			region = boost::dynamic_pointer_cast<AudioRegion> (rx);
			region->special_set_position (capture_info.front()->start);
		}


		catch (failed_constructor& err) {
			error << string_compose(_("%1: could not create region for complete audio file"), _name) << endmsg;
			/* XXX what now? */
		}

		_last_capture_sources.insert (_last_capture_sources.end(), srcs.begin(), srcs.end());

		// cerr << _name << ": there are " << capture_info.size() << " capture_info records\n";

		_playlist->clear_changes ();
		_playlist->set_capture_insertion_in_progress (true);
		_playlist->freeze ();

		for (buffer_position = c->front()->write_source->last_capture_start_frame(), ci = capture_info.begin(); ci != capture_info.end(); ++ci) {

			string region_name;

			RegionFactory::region_name (region_name, whole_file_region_name, false);

			DEBUG_TRACE (DEBUG::CaptureAlignment, string_compose ("%1 capture start @ %2 length %3 add new region %4\n",
			                                                      _name, (*ci)->start, (*ci)->frames, region_name));

			try {

				PropertyList plist;

				plist.add (Properties::start, buffer_position);
				plist.add (Properties::length, (*ci)->frames);
				plist.add (Properties::name, region_name);

				boost::shared_ptr<Region> rx (RegionFactory::create (srcs, plist));
				region = boost::dynamic_pointer_cast<AudioRegion> (rx);
			}

			catch (failed_constructor& err) {
				error << _("AudioDiskstream: could not create region for captured audio!") << endmsg;
				continue; /* XXX is this OK? */
			}

			i_am_the_modifier++;

			_playlist->add_region (region, (*ci)->start, 1, non_layered());
			_playlist->set_layer (region, DBL_MAX);
			i_am_the_modifier--;

			buffer_position += (*ci)->frames;
		}

		_playlist->thaw ();
		_playlist->set_capture_insertion_in_progress (false);
		_session.add_command (new StatefulDiffCommand (_playlist));
	}

	mark_write_completed = true;

  out:
	reset_write_sources (mark_write_completed);

  outout:

	for (ci = capture_info.begin(); ci != capture_info.end(); ++ci) {
		delete *ci;
	}

	capture_info.clear ();
	capture_start_frame = 0;
}

void
AudioDiskstream::transport_looped (framepos_t transport_frame)
{
	if (was_recording) {
		// all we need to do is finish this capture, with modified capture length
		boost::shared_ptr<ChannelList> c = channels.reader();

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

		finish_capture (c);

		// the next region will start recording via the normal mechanism
		// we'll set the start position to the current transport pos
		// no latency adjustment or capture offset needs to be made, as that already happened the first time
		capture_start_frame = transport_frame;
		first_recordable_frame = transport_frame; // mild lie
		last_recordable_frame = max_framepos;
		was_recording = true;

		if (recordable() && destructive()) {
			for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

				RingBufferNPT<CaptureTransition>::rw_vector transvec;
				(*chan)->capture_transition_buf->get_write_vector(&transvec);

				if (transvec.len[0] > 0) {
					transvec.buf[0]->type = CaptureStart;
					transvec.buf[0]->capture_val = capture_start_frame;
					(*chan)->capture_transition_buf->increment_write_ptr(1);
				}
				else {
					// bad!
					fatal << X_("programming error: capture_transition_buf is full on rec loop!  inconceivable!")
					      << endmsg;
				}
			}
		}

	}
}

void
AudioDiskstream::finish_capture (boost::shared_ptr<ChannelList> c)
{
	was_recording = false;
	first_recordable_frame = max_framepos;
	last_recordable_frame = max_framepos;

	if (capture_captured == 0) {
		return;
	}

	if (recordable() && destructive()) {
		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {

			RingBufferNPT<CaptureTransition>::rw_vector transvec;
			(*chan)->capture_transition_buf->get_write_vector(&transvec);

			if (transvec.len[0] > 0) {
				transvec.buf[0]->type = CaptureEnd;
				transvec.buf[0]->capture_val = capture_captured;
				(*chan)->capture_transition_buf->increment_write_ptr(1);
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

	/* now we've finished a capture, reset first_recordable_frame for next time */
	first_recordable_frame = max_framepos;
}

void
AudioDiskstream::set_record_enabled (bool yn)
{
	if (!recordable() || !_session.record_enabling_legal() || _io->n_ports().n_audio() == 0) {
		return;
	}

	/* can't rec-enable in destructive mode if transport is before start */

	if (destructive() && yn && _session.transport_frame() < _session.current_start_frame()) {
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
	}
}

bool
AudioDiskstream::prep_record_enable ()
{
	if (!recordable() || !_session.record_enabling_legal() || _io->n_ports().n_audio() == 0) {
		return false;
	}

	/* can't rec-enable in destructive mode if transport is before start */

	if (destructive() && _session.transport_frame() < _session.current_start_frame()) {
		return false;
	}

	bool rolling = _session.transport_speed() != 0.0f;
	boost::shared_ptr<ChannelList> c = channels.reader();

	capturing_sources.clear ();

	if (Config->get_monitoring_model() == HardwareMonitoring) {

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			(*chan)->source.request_jack_monitors_input (!(_session.config.get_auto_input() && rolling));
			capturing_sources.push_back ((*chan)->write_source);
			(*chan)->write_source->mark_streaming_write_started ();
		}

	} else {
		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			capturing_sources.push_back ((*chan)->write_source);
			(*chan)->write_source->mark_streaming_write_started ();
		}
	}

	RecordEnableChanged (); /* EMIT SIGNAL */

	return true;
}

bool
AudioDiskstream::prep_record_disable ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();
	if (Config->get_monitoring_model() == HardwareMonitoring) {
		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			(*chan)->source.request_jack_monitors_input (false);
		}
	}
	capturing_sources.clear ();
	RecordEnableChanged (); /* EMIT SIGNAL */

	return true;
}

XMLNode&
AudioDiskstream::get_state ()
{
	XMLNode& node (Diskstream::get_state());
	char buf[64] = "";
	LocaleGuard lg (X_("POSIX"));

	boost::shared_ptr<ChannelList> c = channels.reader();
	snprintf (buf, sizeof(buf), "%zd", c->size());
	node.add_property ("channels", buf);

	if (!capturing_sources.empty() && _session.get_record_enabled()) {

		XMLNode* cs_child = new XMLNode (X_("CapturingSources"));
		XMLNode* cs_grandchild;

		for (vector<boost::shared_ptr<AudioFileSource> >::iterator i = capturing_sources.begin(); i != capturing_sources.end(); ++i) {
			cs_grandchild = new XMLNode (X_("file"));
			cs_grandchild->add_property (X_("path"), (*i)->path());
			cs_child->add_child_nocopy (*cs_grandchild);
		}

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
AudioDiskstream::set_state (const XMLNode& node, int version)
{
	const XMLProperty* prop;
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	uint32_t nchans = 1;
	XMLNode* capture_pending_node = 0;
	LocaleGuard lg (X_("POSIX"));

	/* prevent write sources from being created */

	in_set_state = true;

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == IO::state_node_name) {
			deprecated_io_node = new XMLNode (**niter);
		}

		if ((*niter)->name() == X_("CapturingSources")) {
			capture_pending_node = *niter;
		}
	}

	if (Diskstream::set_state (node, version)) {
		return -1;
	}

	if ((prop = node.property ("channels")) != 0) {
		nchans = atoi (prop->value().c_str());
	}

	// create necessary extra channels
	// we are always constructed with one and we always need one

	_n_channels.set(DataType::AUDIO, channels.reader()->size());

	if (nchans > _n_channels.n_audio()) {

		add_channel (nchans - _n_channels.n_audio());
		IO::PortCountChanged(_n_channels);

	} else if (nchans < _n_channels.n_audio()) {

		remove_channel (_n_channels.n_audio() - nchans);
	}



	if (!destructive() && capture_pending_node) {
		/* destructive streams have one and only one source per channel,
		   and so they never end up in pending capture in any useful
		   sense.
		*/
		use_pending_capture_data (*capture_pending_node);
	}

	in_set_state = false;

	/* make sure this is clear before we do anything else */

	capturing_sources.clear ();

	/* write sources are handled when we handle the input set
	   up of the IO that owns this DS (::non_realtime_input_change())
	*/

	return 0;
}

int
AudioDiskstream::use_new_write_source (uint32_t n)
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	if (!recordable()) {
		return 1;
	}

	if (n >= c->size()) {
		error << string_compose (_("AudioDiskstream: channel %1 out of range"), n) << endmsg;
		return -1;
	}

	ChannelInfo* chan = (*c)[n];

	try {
		if ((chan->write_source = _session.create_audio_source_for_session (
			     n_channels().n_audio(), name(), n, destructive())) == 0) {
			throw failed_constructor();
		}
	}

	catch (failed_constructor &err) {
		error << string_compose (_("%1:%2 new capture file not initialized correctly"), _name, n) << endmsg;
		chan->write_source.reset ();
		return -1;
	}

	/* do not remove destructive files even if they are empty */

	chan->write_source->set_allow_remove_if_empty (!destructive());

	return 0;
}

list<boost::shared_ptr<Source> >
AudioDiskstream::steal_write_sources()
{
	/* not possible to steal audio write sources */
	list<boost::shared_ptr<Source> > ret;
	return ret;
}

void
AudioDiskstream::reset_write_sources (bool mark_write_complete, bool /*force*/)
{
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();
	uint32_t n;

	if (!_session.writable() || !recordable()) {
		return;
	}

	capturing_sources.clear ();

	for (chan = c->begin(), n = 0; chan != c->end(); ++chan, ++n) {

		if (!destructive()) {

			if ((*chan)->write_source) {

				if (mark_write_complete) {
					(*chan)->write_source->mark_streaming_write_completed ();
					(*chan)->write_source->done_with_peakfile_writes ();
				}

				if ((*chan)->write_source->removable()) {
					(*chan)->write_source->mark_for_remove ();
					(*chan)->write_source->drop_references ();
				}

				(*chan)->write_source.reset ();
			}

			use_new_write_source (n);

			if (record_enabled()) {
				capturing_sources.push_back ((*chan)->write_source);
			}

		} else {

			if ((*chan)->write_source == 0) {
				use_new_write_source (n);
			}
		}
	}

	if (destructive() && !c->empty ()) {

		/* we now have all our write sources set up, so create the
		   playlist's single region.
		*/

		if (_playlist->empty()) {
			setup_destructive_playlist ();
		}
	}
}

void
AudioDiskstream::set_block_size (pframes_t /*nframes*/)
{
	if (_session.get_block_size() > speed_buffer_size) {
		speed_buffer_size = _session.get_block_size();
		boost::shared_ptr<ChannelList> c = channels.reader();

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			if ((*chan)->speed_buffer)
				delete [] (*chan)->speed_buffer;
			(*chan)->speed_buffer = new Sample[speed_buffer_size];
		}
	}
	allocate_temporary_buffers ();
}

void
AudioDiskstream::allocate_temporary_buffers ()
{
	/* make sure the wrap buffer is at least large enough to deal
	   with the speeds up to 1.2, to allow for micro-variation
	   when slaving to MTC, Timecode etc.
	*/

	double const sp = max (fabsf (_actual_speed), 1.2f);
	framecnt_t required_wrap_size = (framecnt_t) floor (_session.get_block_size() * sp) + 1;

	if (required_wrap_size > wrap_buffer_size) {

		boost::shared_ptr<ChannelList> c = channels.reader();

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			if ((*chan)->playback_wrap_buffer) {
				delete [] (*chan)->playback_wrap_buffer;
			}
			(*chan)->playback_wrap_buffer = new Sample[required_wrap_size];
			if ((*chan)->capture_wrap_buffer) {
				delete [] (*chan)->capture_wrap_buffer;
			}
			(*chan)->capture_wrap_buffer = new Sample[required_wrap_size];
		}

		wrap_buffer_size = required_wrap_size;
	}
}

void
AudioDiskstream::request_jack_monitors_input (bool yn)
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->source.request_jack_monitors_input (yn);
	}
}

void
AudioDiskstream::set_align_style_from_io ()
{
	bool have_physical = false;

	if (_alignment_choice != Automatic) {
		return;
	}

	if (_io == 0) {
		return;
	}

	get_input_sources ();

	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		if ((*chan)->source.is_physical ()) {
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

int
AudioDiskstream::add_channel_to (boost::shared_ptr<ChannelList> c, uint32_t how_many)
{
	while (how_many--) {
		c->push_back (new ChannelInfo(
			              _session.butler()->audio_diskstream_playback_buffer_size(),
			              _session.butler()->audio_diskstream_capture_buffer_size(),
			              speed_buffer_size, wrap_buffer_size));
		interpolation.add_channel_to (
			_session.butler()->audio_diskstream_playback_buffer_size(),
			speed_buffer_size);
	}

	_n_channels.set(DataType::AUDIO, c->size());

	return 0;
}

int
AudioDiskstream::add_channel (uint32_t how_many)
{
	RCUWriter<ChannelList> writer (channels);
	boost::shared_ptr<ChannelList> c = writer.get_copy();

	return add_channel_to (c, how_many);
}

int
AudioDiskstream::remove_channel_from (boost::shared_ptr<ChannelList> c, uint32_t how_many)
{
	while (how_many-- && !c->empty()) {
		delete c->back();
		c->pop_back();
		interpolation.remove_channel_from ();
	}

	_n_channels.set(DataType::AUDIO, c->size());

	return 0;
}

int
AudioDiskstream::remove_channel (uint32_t how_many)
{
	RCUWriter<ChannelList> writer (channels);
	boost::shared_ptr<ChannelList> c = writer.get_copy();

	return remove_channel_from (c, how_many);
}

float
AudioDiskstream::playback_buffer_load () const
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	if (c->empty ()) {
		return 0;
	}

	return (float) ((double) c->front()->playback_buf->read_space()/
			(double) c->front()->playback_buf->bufsize());
}

float
AudioDiskstream::capture_buffer_load () const
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	if (c->empty ()) {
		return 0;
	}

	return (float) ((double) c->front()->capture_buf->write_space()/
			(double) c->front()->capture_buf->bufsize());
}

int
AudioDiskstream::use_pending_capture_data (XMLNode& node)
{
	const XMLProperty* prop;
	XMLNodeList nlist = node.children();
	XMLNodeIterator niter;
	boost::shared_ptr<AudioFileSource> fs;
	boost::shared_ptr<AudioFileSource> first_fs;
	SourceList pending_sources;
	framepos_t position;

	if ((prop = node.property (X_("at"))) == 0) {
		return -1;
	}

	if (sscanf (prop->value().c_str(), "%" PRIu64, &position) != 1) {
		return -1;
	}

	for (niter = nlist.begin(); niter != nlist.end(); ++niter) {
		if ((*niter)->name() == X_("file")) {

			if ((prop = (*niter)->property (X_("path"))) == 0) {
				continue;
			}

			// This protects sessions from errant CapturingSources in stored sessions
			struct stat sbuf;
			if (stat (prop->value().c_str(), &sbuf)) {
				continue;
			}

			try {
				fs = boost::dynamic_pointer_cast<AudioFileSource> (
					SourceFactory::createWritable (
						DataType::AUDIO, _session,
						prop->value(), string(), false, _session.frame_rate()));
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

			fs->set_captured_for (_name.val());
		}
	}

	if (pending_sources.size() == 0) {
		/* nothing can be done */
		return 1;
	}

	if (pending_sources.size() != _n_channels.n_audio()) {
		error << string_compose (_("%1: incorrect number of pending sources listed - ignoring them all"), _name)
		      << endmsg;
		return -1;
	}

	boost::shared_ptr<AudioRegion> region;

	try {

		PropertyList plist;

		plist.add (Properties::start, 0);
		plist.add (Properties::length, first_fs->length (first_fs->timeline_position()));
		plist.add (Properties::name, region_name_from_path (first_fs->name(), true));

		region = boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (pending_sources, plist));

		region->set_automatic (true);
		region->set_whole_file (true);
		region->special_set_position (0);
	}

	catch (failed_constructor& err) {
		error << string_compose (
				_("%1: cannot create whole-file region from pending capture sources"),
				_name) << endmsg;

		return -1;
	}

	_playlist->add_region (region, position);

	return 0;
}

int
AudioDiskstream::set_non_layered (bool yn)
{
	if (yn != non_layered()) {

		if (yn) {
			_flags = Flag (_flags | NonLayered);
		} else {
			_flags = Flag (_flags & ~NonLayered);
		}
	}

	return 0;
}

int
AudioDiskstream::set_destructive (bool yn)
{
	if (yn != destructive()) {

		if (yn) {
			bool bounce_ignored;
			/* requestor should already have checked this and
			   bounced if necessary and desired
			*/
			if (!can_become_destructive (bounce_ignored)) {
				return -1;
			}
			_flags = Flag (_flags | Destructive);
			use_destructive_playlist ();
		} else {
			_flags = Flag (_flags & ~Destructive);
			reset_write_sources (true, true);
		}
	}

	return 0;
}

bool
AudioDiskstream::can_become_destructive (bool& requires_bounce) const
{
	if (!_playlist) {
		requires_bounce = false;
		return false;
	}

	/* is there only one region ? */

	if (_playlist->n_regions() != 1) {
		requires_bounce = true;
		return false;
	}

	boost::shared_ptr<Region> first = _playlist->find_next_region (_session.current_start_frame(), Start, 1);
	if (!first) {
		requires_bounce = false;
		return true;
	}

	/* do the source(s) for the region cover the session start position ? */

	if (first->position() != _session.current_start_frame()) {
		if (first->start() > _session.current_start_frame()) {
			requires_bounce = true;
			return false;
		}
	}

	/* is the source used by only 1 playlist ? */

	boost::shared_ptr<AudioRegion> afirst = boost::dynamic_pointer_cast<AudioRegion> (first);

	assert (afirst);

	if (_session.playlists->source_use_count (afirst->source()) > 1) {
		requires_bounce = true;
		return false;
	}

	requires_bounce = false;
	return true;
}

void
AudioDiskstream::adjust_playback_buffering ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->resize_playback (_session.butler()->audio_diskstream_playback_buffer_size());
	}
}

void
AudioDiskstream::adjust_capture_buffering ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->resize_capture (_session.butler()->audio_diskstream_capture_buffer_size());
	}
}

bool
AudioDiskstream::ChannelSource::is_physical () const
{
	if (name.empty()) {
		return false;
	}

	return AudioEngine::instance()->port_is_physical (name);
}

void
AudioDiskstream::ChannelSource::request_jack_monitors_input (bool yn) const
{
	if (name.empty()) {
		return;
	}

	return AudioEngine::instance()->request_jack_monitors_input (name, yn);
}

AudioDiskstream::ChannelInfo::ChannelInfo (framecnt_t playback_bufsize, framecnt_t capture_bufsize, framecnt_t speed_size, framecnt_t wrap_size)
{
	current_capture_buffer = 0;
	current_playback_buffer = 0;
	curr_capture_cnt = 0;

	speed_buffer = new Sample[speed_size];
	playback_wrap_buffer = new Sample[wrap_size];
	capture_wrap_buffer = new Sample[wrap_size];

	playback_buf = new RingBufferNPT<Sample> (playback_bufsize);
	capture_buf = new RingBufferNPT<Sample> (capture_bufsize);
	capture_transition_buf = new RingBufferNPT<CaptureTransition> (256);

	/* touch the ringbuffer buffers, which will cause
	   them to be mapped into locked physical RAM if
	   we're running with mlockall(). this doesn't do
	   much if we're not.
	*/

	memset (playback_buf->buffer(), 0, sizeof (Sample) * playback_buf->bufsize());
	memset (capture_buf->buffer(), 0, sizeof (Sample) * capture_buf->bufsize());
	memset (capture_transition_buf->buffer(), 0, sizeof (CaptureTransition) * capture_transition_buf->bufsize());
}

void
AudioDiskstream::ChannelInfo::resize_playback (framecnt_t playback_bufsize)
{
	delete playback_buf;
	playback_buf = new RingBufferNPT<Sample> (playback_bufsize);
	memset (playback_buf->buffer(), 0, sizeof (Sample) * playback_buf->bufsize());
}

void
AudioDiskstream::ChannelInfo::resize_capture (framecnt_t capture_bufsize)
{
	delete capture_buf;

	capture_buf = new RingBufferNPT<Sample> (capture_bufsize);
	memset (capture_buf->buffer(), 0, sizeof (Sample) * capture_buf->bufsize());
}

AudioDiskstream::ChannelInfo::~ChannelInfo ()
{
	write_source.reset ();

	delete [] speed_buffer;
	speed_buffer = 0;

	delete [] playback_wrap_buffer;
	playback_wrap_buffer = 0;

	delete [] capture_wrap_buffer;
	capture_wrap_buffer = 0;

	delete playback_buf;
	playback_buf = 0;

	delete capture_buf;
	capture_buf = 0;

	delete capture_transition_buf;
	capture_transition_buf = 0;
}


bool
AudioDiskstream::set_name (string const & name)
{
	Diskstream::set_name (name);

	/* get a new write source so that its name reflects the new diskstream name */

	boost::shared_ptr<ChannelList> c = channels.reader();
	ChannelList::iterator i;
	int n = 0;

	for (n = 0, i = c->begin(); i != c->end(); ++i, ++n) {
		use_new_write_source (n);
	}

	return true;
}
