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

#include "pbd/i18n.h"
#include "pbd/memento_command.h"

#include "ardour/audioplaylist.h"
#include "ardour/audio_buffer.h"
#include "ardour/butler.h"
#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

ARDOUR::framecnt_t DiskReader::_chunk_frames = default_chunk_frames ();

DiskReader::DiskReader (Session& s, string const & str, DiskIOProcessor::Flag f)
	: DiskIOProcessor (s, str, f)
	, _roll_delay (0)
	, overwrite_frame (0)
        , overwrite_offset (0)
        , _pending_overwrite (false)
        , overwrite_queued (false)
        , file_frame (0)
        , playback_sample (0)
	, _monitoring_choice (MonitorDisk)
	, channels (new ChannelList)
{
}

DiskReader::~DiskReader ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("DiskReader %1 deleted\n", _name));

	if (_playlist) {
		_playlist->release ();
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
	if (_name != str) {
		assert (_playlist);
		_playlist->set_name (str);
		SessionObject::set_name(str);
	}

	return true;
}

void
DiskReader::set_roll_delay (ARDOUR::framecnt_t nframes)
{
	_roll_delay = nframes;
}

int
DiskReader::set_state (const XMLNode& node, int version)
{
	XMLProperty const * prop;

	if (DiskIOProcessor::set_state (node, version)) {
		return -1;
	}

	if ((prop = node.property ("playlist")) == 0) {
		return -1;
	}

	if (find_and_use_playlist (prop->value())) {
		return -1;
	}

	return 0;
}

/* Processor interface */

bool
DiskReader::configure_io (ChanCount in, ChanCount out)
{
	return true;
}

bool
DiskReader::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	return true;
}

void
DiskReader::realtime_handle_transport_stopped ()
{
}

void
DiskReader::realtime_locate ()
{
}

int
DiskReader::add_channel_to (boost::shared_ptr<ChannelList> c, uint32_t how_many)
{
	while (how_many--) {
		c->push_back (new ChannelInfo(
			              _session.butler()->audio_diskstream_playback_buffer_size(),
			              speed_buffer_size, wrap_buffer_size));
		interpolation.add_channel_to (
			_session.butler()->audio_diskstream_playback_buffer_size(),
			speed_buffer_size);
	}

	_n_channels.set (DataType::AUDIO, c->size());

	return 0;
}

int
DiskReader::add_channel (uint32_t how_many)
{
	RCUWriter<ChannelList> writer (channels);
	boost::shared_ptr<ChannelList> c = writer.get_copy();

	return add_channel_to (c, how_many);
}

int
DiskReader::remove_channel_from (boost::shared_ptr<ChannelList> c, uint32_t how_many)
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
DiskReader::remove_channel (uint32_t how_many)
{
	RCUWriter<ChannelList> writer (channels);
	boost::shared_ptr<ChannelList> c = writer.get_copy();

	return remove_channel_from (c, how_many);
}

float
DiskReader::buffer_load () const
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	if (c->empty ()) {
		return 1.0;
	}

	return (float) ((double) c->front()->buf->read_space()/
	                   (double) c->front()->buf->bufsize());
}

void
DiskReader::adjust_buffering ()
{
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->resize (_session.butler()->audio_diskstream_playback_buffer_size());
	}
}

DiskReader::ChannelInfo::ChannelInfo (framecnt_t bufsize, framecnt_t speed_size, framecnt_t wrap_size)
{
	current_buffer = 0;

	speed_buffer = new Sample[speed_size];
	wrap_buffer = new Sample[wrap_size];

	buf = new RingBufferNPT<Sample> (bufsize);

	/* touch the ringbuffer buffer, which will cause
	   them to be mapped into locked physical RAM if
	   we're running with mlockall(). this doesn't do
	   much if we're not.
	*/

	memset (buf->buffer(), 0, sizeof (Sample) * buf->bufsize());
}

void
DiskReader::ChannelInfo::resize (framecnt_t bufsize)
{
	delete buf;
	buf = new RingBufferNPT<Sample> (bufsize);
	memset (buf->buffer(), 0, sizeof (Sample) * buf->bufsize());
}

DiskReader::ChannelInfo::~ChannelInfo ()
{
	delete [] speed_buffer;
	speed_buffer = 0;

	delete [] wrap_buffer;
	wrap_buffer = 0;

	delete buf;
	buf = 0;
}

int
DiskReader::set_block_size (pframes_t /*nframes*/)
{
	if (_session.get_block_size() > speed_buffer_size) {
		speed_buffer_size = _session.get_block_size();
		boost::shared_ptr<ChannelList> c = channels.reader();

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			delete [] (*chan)->speed_buffer;
			(*chan)->speed_buffer = new Sample[speed_buffer_size];
		}
	}
	allocate_temporary_buffers ();
	return 0;
}

void
DiskReader::allocate_temporary_buffers ()
{
	/* make sure the wrap buffer is at least large enough to deal
	   with the speeds up to 1.2, to allow for micro-variation
	   when slaving to MTC, Timecode etc.
	*/

	double const sp = max (fabs (_actual_speed), 1.2);
	framecnt_t required_wrap_size = (framecnt_t) ceil (_session.get_block_size() * sp) + 2;

	if (required_wrap_size > wrap_buffer_size) {

		boost::shared_ptr<ChannelList> c = channels.reader();

		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
			if ((*chan)->wrap_buffer) {
				delete [] (*chan)->wrap_buffer;
			}
			(*chan)->wrap_buffer = new Sample[required_wrap_size];
		}

		wrap_buffer_size = required_wrap_size;
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
		// !!!! _session.request_overwrite_buffer (this);
		overwrite_queued = true;
	}
}

void
DiskReader::playlist_deleted (boost::weak_ptr<Playlist> wpl)
{
	boost::shared_ptr<Playlist> pl (wpl.lock());

	if (pl == _playlist) {

		/* this catches an ordering issue with session destruction. playlists
		   are destroyed before disk readers. we have to invalidate any handles
		   we have to the playlist.
		*/

		if (_playlist) {
			_playlist.reset ();
		}
	}
}

int
DiskReader::use_playlist (boost::shared_ptr<Playlist> playlist)
{
        if (!playlist) {
                return 0;
        }

        bool prior_playlist = false;

	{
		Glib::Threads::Mutex::Lock lm (state_lock);

		if (playlist == _playlist) {
			return 0;
		}

		playlist_connections.drop_connections ();

		if (_playlist) {
			_playlist->release();
                        prior_playlist = true;
		}

		_playlist = playlist;
		_playlist->use();

		_playlist->ContentsChanged.connect_same_thread (playlist_connections, boost::bind (&DiskReader::playlist_modified, this));
		_playlist->LayeringChanged.connect_same_thread (playlist_connections, boost::bind (&DiskReader::playlist_modified, this));
		_playlist->DropReferences.connect_same_thread (playlist_connections, boost::bind (&DiskReader::playlist_deleted, this, boost::weak_ptr<Playlist>(_playlist)));
		_playlist->RangesMoved.connect_same_thread (playlist_connections, boost::bind (&DiskReader::playlist_ranges_moved, this, _1, _2));
	}

	/* don't do this if we've already asked for it *or* if we are setting up
	   the diskstream for the very first time - the input changed handling will
	   take care of the buffer refill.
	*/

	if (!overwrite_queued && prior_playlist) {
		// !!! _session.request_overwrite_buffer (this);
		overwrite_queued = true;
	}

	PlaylistChanged (); /* EMIT SIGNAL */
	_session.set_dirty ();

	return 0;
}

int
DiskReader::find_and_use_playlist (const string& name)
{
	boost::shared_ptr<AudioPlaylist> playlist;

	if ((playlist = boost::dynamic_pointer_cast<AudioPlaylist> (_session.playlists->by_name (name))) == 0) {
		playlist = boost::dynamic_pointer_cast<AudioPlaylist> (PlaylistFactory::create (DataType::AUDIO, _session, name));
	}

	if (!playlist) {
		error << string_compose(_("DiskReader: Playlist \"%1\" isn't an audio playlist"), name) << endmsg;
		return -1;
	}

	return use_playlist (playlist);
}

int
DiskReader::use_new_playlist ()
{
	string newname;
	boost::shared_ptr<AudioPlaylist> playlist;

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
DiskReader::use_copy_playlist ()
{
	assert(audio_playlist());

	if (_playlist == 0) {
		error << string_compose(_("DiskReader %1: there is no existing playlist to make a copy of!"), _name) << endmsg;
		return -1;
	}

	string newname;
	boost::shared_ptr<AudioPlaylist> playlist;

	newname = Playlist::bump_name (_playlist->name(), _session);

	if ((playlist = boost::dynamic_pointer_cast<AudioPlaylist>(PlaylistFactory::create (audio_playlist(), newname))) != 0) {
		playlist->reset_shares();
		return use_playlist (playlist);
	} else {
		return -1;
	}
}


/** Do some record stuff [not described in this comment!]
 *
 *  Also:
 *    - Setup playback_distance with the nframes, or nframes adjusted
 *      for current varispeed, if appropriate.
 *    - Setup current_buffer in each ChannelInfo to point to data
 *      that someone can read playback_distance worth of data from.
 */
void
DiskReader::run (BufferSet& bufs, framepos_t start_frame, framepos_t end_frame,
                 double speed, pframes_t nframes, bool result_required)
/*
	int
	DiskReader::process (BufferSet& bufs, framepos_t transport_frame, pframes_t nframes,
	framecnt_t& playback_distance, bool need_disk_signal)
*/
{
	uint32_t n;
	boost::shared_ptr<ChannelList> c = channels.reader();
	ChannelList::iterator chan;
	framecnt_t playback_distance = 0;

	Glib::Threads::Mutex::Lock sm (state_lock, Glib::Threads::TRY_LOCK);

	if (!sm.locked()) {
		return;
	}

	for (chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->current_buffer = 0;
	}

	if (result_required || _monitoring_choice == MonitorDisk || _monitoring_choice == MonitorCue) {

		/* we're doing playback */

		framecnt_t necessary_samples;

		if (_actual_speed != 1.0) {
			necessary_samples = (framecnt_t) ceil ((nframes * fabs (_actual_speed))) + 2;
		} else {
			necessary_samples = nframes;
		}

		for (chan = c->begin(); chan != c->end(); ++chan) {
			(*chan)->buf->get_read_vector (&(*chan)->read_vector);
		}

		n = 0;

		/* Setup current_buffer in each ChannelInfo to point to data that someone
		   can read necessary_samples (== nframes at a transport speed of 1) worth of data
		   from right now.
		*/

		for (chan = c->begin(); chan != c->end(); ++chan, ++n) {

			ChannelInfo* chaninfo (*chan);

			if (necessary_samples <= (framecnt_t) chaninfo->read_vector.len[0]) {
				/* There are enough samples in the first part of the ringbuffer */
				chaninfo->current_buffer = chaninfo->read_vector.buf[0];

			} else {
				framecnt_t total = chaninfo->read_vector.len[0] + chaninfo->read_vector.len[1];

				if (necessary_samples > total) {
					cerr << _name << " Need " << necessary_samples << " total = " << total << endl;
					cerr << "underrun for " << _name << endl;
                                        DEBUG_TRACE (DEBUG::Butler, string_compose ("%1 underrun in %2, total space = %3\n",
                                                                                    DEBUG_THREAD_SELF, name(), total));
					Underrun ();
					return;

				} else {

					/* We have enough samples, but not in one lump.  Coalesce the two parts
					   into one in wrap_buffer in our ChannelInfo, and specify that
					   as our current_buffer.
					*/

					assert(wrap_buffer_size >= necessary_samples);

					/* Copy buf[0] from buf */
					memcpy ((char *) chaninfo->wrap_buffer,
							chaninfo->read_vector.buf[0],
							chaninfo->read_vector.len[0] * sizeof (Sample));

					/* Copy buf[1] from buf */
					memcpy (chaninfo->wrap_buffer + chaninfo->read_vector.len[0],
							chaninfo->read_vector.buf[1],
							(necessary_samples - chaninfo->read_vector.len[0])
									* sizeof (Sample));

					chaninfo->current_buffer = chaninfo->wrap_buffer;
				}
			}
		}

		if (_actual_speed != 1.0f && _actual_speed != -1.0f) {

			interpolation.set_speed (_target_speed);

			int channel = 0;
			for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan, ++channel) {
				ChannelInfo* chaninfo (*chan);

				playback_distance = interpolation.interpolate (
					channel, nframes, chaninfo->current_buffer, chaninfo->speed_buffer);

				chaninfo->current_buffer = chaninfo->speed_buffer;
			}

		} else {
			playback_distance = nframes;
		}

		_speed = _target_speed;
	}

	if (result_required || _monitoring_choice == MonitorDisk || _monitoring_choice == MonitorCue) {

		/* copy data over to buffer set */

		size_t n_buffers = bufs.count().n_audio();
		size_t n_chans = c->size();
		gain_t scaling = 1.0f;

		if (n_chans > n_buffers) {
			scaling = ((float) n_buffers)/n_chans;
		}

		for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {

			AudioBuffer& buf (bufs.get_audio (n%n_buffers));
			ChannelInfo* chaninfo (*chan);

			if (n < n_chans) {
				if (scaling != 1.0f) {
					buf.read_from_with_gain (chaninfo->current_buffer, nframes, scaling);
				} else {
					buf.read_from (chaninfo->current_buffer, nframes);
				}
			} else {
				if (scaling != 1.0f) {
					buf.accumulate_with_gain_from (chaninfo->current_buffer, nframes, scaling);
				} else {
					buf.accumulate_from (chaninfo->current_buffer, nframes);
				}
			}
		}

		/* leave the MIDI count alone */
		ChanCount cnt (DataType::AUDIO, n_chans);
		cnt.set (DataType::MIDI, bufs.count().n_midi());
		bufs.set_count (cnt);

		/* extra buffers will already be silent, so leave them alone */
	}

	bool need_butler = false;

	if (_actual_speed < 0.0) {
		playback_sample -= playback_distance;
	} else {
		playback_sample += playback_distance;
	}

	for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan) {
		(*chan)->buf->increment_read_ptr (playback_distance);
	}

	if (!c->empty()) {
		if (_slaved) {
			need_butler = c->front()->buf->write_space() >= c->front()->buf->bufsize() / 2;
		} else {
			need_butler = (framecnt_t) c->front()->buf->write_space() >= _chunk_frames;
		}
	}

	//return need_butler;
}

frameoffset_t
DiskReader::calculate_playback_distance (pframes_t nframes)
{
	frameoffset_t playback_distance = nframes;

	if (_actual_speed != 1.0f && _actual_speed != -1.0f) {
		interpolation.set_speed (_target_speed);
		boost::shared_ptr<ChannelList> c = channels.reader();
		int channel = 0;
		for (ChannelList::iterator chan = c->begin(); chan != c->end(); ++chan, ++channel) {
			playback_distance = interpolation.interpolate (channel, nframes, NULL, NULL);
		}
	} else {
		playback_distance = nframes;
	}

	if (_actual_speed < 0.0) {
		return -playback_distance;
	} else {
		return playback_distance;
	}
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
	framecnt_t size = c->front()->buf->bufsize();

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

		if (read ((*chan)->buf->buffer() + overwrite_offset, mixdown_buffer, gain_buffer, start, to_read, n, reversed)) {
			error << string_compose(_("DiskReader %1: when refilling, cannot read %2 from playlist at frame %3"),
						id(), size, playback_sample) << endmsg;
			goto out;
		}

		if (cnt > to_read) {

			cnt -= to_read;

			if (read ((*chan)->buf->buffer(), mixdown_buffer, gain_buffer, start, cnt, n, reversed)) {
				error << string_compose(_("DiskReader %1: when refilling, cannot read %2 from playlist at frame %3"),
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

void
DiskReader::non_realtime_locate (framepos_t location)
{
	/* now refill channel buffers */

	if (speed() != 1.0f || speed() != -1.0f) {
		seek ((framepos_t) (location * (double) speed()), true);
	} else {
		seek (location, true);
	}
}

int
DiskReader::seek (framepos_t frame, bool complete_refill)
{
	uint32_t n;
	int ret = -1;
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	Glib::Threads::Mutex::Lock lm (state_lock);

	for (n = 0, chan = c->begin(); chan != c->end(); ++chan, ++n) {
		(*chan)->buf->reset ();
	}

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
	ChannelList::iterator chan;
	boost::shared_ptr<ChannelList> c = channels.reader();

	for (chan = c->begin(); chan != c->end(); ++chan) {
		if ((*chan)->buf->read_space() < (size_t) distance) {
			return false;
		}
	}
	return true;
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
DiskReader::read (Sample* buf, Sample* mixdown_buffer, float* gain_buffer,
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

	Sample* mix_buf  = new Sample[2*1048576];
	float*  gain_buf = new float[2*1048576];

	int ret = _do_refill (mix_buf, gain_buf, (partial_fill ? _chunk_frames : 0));

	delete [] mix_buf;
	delete [] gain_buf;

	return ret;
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
DiskReader::_do_refill (Sample* mixdown_buffer, float* gain_buffer, framecnt_t fill_level)
{
	if (_session.state_of_the_state() & Session::Loading) {
		return 0;
	}

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

	/* do not read from disk while session is marked as Loading, to avoid
	   useless redundant I/O.
	*/

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

	if ((total_space < _chunk_frames) && fabs (_actual_speed) < 2.0f) {
		return 0;
	}

	/* when slaved, don't try to get too close to the read pointer. this
	   leaves space for the buffer reversal to have something useful to
	   work with.
	*/

	if (_slaved && total_space < (framecnt_t) (c->front()->buf->bufsize() / 2)) {
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

	//cerr << name() << " will read " << byte_size_for_read << " out of total bytes " << total_bytes << " in buffer of "
	// << c->front()->buf->bufsize() * bits_per_sample / 8 << " bps = " << bits_per_sample << endl;
	// cerr << name () << " read samples = " << samples_to_read << " out of total space " << total_space << " in buffer of " << c->front()->buf->bufsize() << " samples\n";

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

			if (read (buf1, mixdown_buffer, gain_buffer, file_frame_tmp, to_read, chan_n, reversed)) {
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

			if (read (buf2, mixdown_buffer, gain_buffer, file_frame_tmp, to_read, chan_n, reversed)) {
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

#if 0
	if (!_track || Config->get_automation_follows_regions () == false) {
		return;
	}

	list< Evoral::RangeMove<double> > movements;

	for (list< Evoral::RangeMove<framepos_t> >::const_iterator i = movements_frames.begin();
	     i != movements_frames.end();
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
	_track->foreach_processor (boost::bind (&Diskstream::move_processor_automation, this, _1, movements_frames));
#endif
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

