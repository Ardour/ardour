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

#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/playback_buffer.h"

#include "ardour/audioplaylist.h"
#include "ardour/butler.h"
#include "ardour/debug.h"
#include "ardour/disk_io.h"
#include "ardour/disk_reader.h"
#include "ardour/disk_writer.h"
#include "ardour/location.h"
#include "ardour/midi_ring_buffer.h"
#include "ardour/midi_playlist.h"
#include "ardour/playlist.h"
#include "ardour/playlist_factory.h"
#include "ardour/rc_configuration.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/track.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace std;

const string DiskIOProcessor::state_node_name = X_("DiskIOProcessor");

// PBD::Signal0<void> DiskIOProcessor::DiskOverrun;
// PBD::Signal0<void>  DiskIOProcessor::DiskUnderrun;

DiskIOProcessor::DiskIOProcessor (Session& s, Track& t, string const & str, Flag f, Temporal::TimeDomain td)
	: Processor (s, str, td)
	, _flags (f)
	, _slaved (false)
	, in_set_state (false)
	, playback_sample (0)
	, _need_butler (false)
	, _track (t)
	, channels (new ChannelList)
	, _midi_buf (0)
{
	set_display_to_user (false);
}

DiskIOProcessor::~DiskIOProcessor ()
{
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

	for (uint32_t n = 0; n < DataType::num_types; ++n) {
		if (_playlists[n]) {
			_playlists[n]->release ();
		}
	}
}


void
DiskIOProcessor::init ()
{
	set_block_size (_session.get_block_size());
}

void
DiskIOProcessor::set_buffering_parameters (BufferingPreset bp)
{
	samplecnt_t read_chunk_size;
	samplecnt_t read_buffer_size;
	samplecnt_t write_chunk_size;
	samplecnt_t write_buffer_size;

	if (!get_buffering_presets (bp, read_chunk_size, read_buffer_size, write_chunk_size, write_buffer_size)) {
		return;
	}

	DiskReader::set_chunk_samples (read_chunk_size);
	DiskWriter::set_chunk_samples (write_chunk_size);

	Config->set_audio_capture_buffer_seconds (write_buffer_size);
	Config->set_audio_playback_buffer_seconds (read_buffer_size);
}

bool
DiskIOProcessor::get_buffering_presets (BufferingPreset bp,
                                        samplecnt_t& read_chunk_size,
                                        samplecnt_t& read_buffer_size,
                                        samplecnt_t& write_chunk_size,
                                        samplecnt_t& write_buffer_size)
{
	switch (bp) {
	case Small:
		read_chunk_size = 65536;  /* samples */
		write_chunk_size = 65536; /* samples */
		read_buffer_size = 5;  /* seconds */
		write_buffer_size = 5; /* seconds */
		break;

	case Medium:
		read_chunk_size = 262144;  /* samples */
		write_chunk_size = 131072; /* samples */
		read_buffer_size = 10;  /* seconds */
		write_buffer_size = 10; /* seconds */
		break;

	case Large:
		read_chunk_size = 524288; /* samples */
		write_chunk_size = 131072; /* samples */
		read_buffer_size = 20; /* seconds */
		write_buffer_size = 20; /* seconds */
		break;

	default:
		return false;
	}

	return true;
}

bool
DiskIOProcessor::can_support_io_configuration (const ChanCount& in, ChanCount& out)
{
	if (in.n_midi() != 0 && in.n_midi() != 1) {
		/* we only support zero or 1 MIDI stream */
		return false;
	}

	/* currently no way to deliver different channels that we receive */
	out = in;

	return true;
}

bool
DiskIOProcessor::configure_io (ChanCount in, ChanCount out)
{
	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("Configuring %1 for in:%2 out:%3\n", name(), in, out));

	bool changed = false;

	{
		RCUWriter<ChannelList> writer (channels);
		boost::shared_ptr<ChannelList> c = writer.get_copy();

		uint32_t n_audio = in.n_audio();

		if (n_audio > c->size()) {
			add_channel_to (c, n_audio - c->size());
			changed = true;
		} else if (n_audio < c->size()) {
			remove_channel_from (c, c->size() - n_audio);
			changed = true;
		}

		/* writer leaves scope, actual channel list is updated */
	}

	if (in.n_midi() > 0 && !_midi_buf) {
		const size_t size = _session.butler()->midi_buffer_size();
		_midi_buf = new MidiRingBuffer<samplepos_t>(size);
		changed = true;
	}

	if (changed) {
		configuration_changed ();
	}

	return Processor::configure_io (in, out);
}

int
DiskIOProcessor::set_block_size (pframes_t nframes)
{
	return 0;
}

void
DiskIOProcessor::non_realtime_locate (samplepos_t location)
{
	/* now refill channel buffers */

	seek (location, true);
}

int
DiskIOProcessor::set_state (const XMLNode& node, int version)
{
	XMLProperty const * prop;

	Processor::set_state (node, version);

	if ((prop = node.property ("flags")) != 0) {
		_flags = Flag (string_2_enum (prop->value(), _flags));
	}

	return 0;
}

int
DiskIOProcessor::add_channel (uint32_t how_many)
{
	RCUWriter<ChannelList> writer (channels);
	boost::shared_ptr<ChannelList> c = writer.get_copy();

	return add_channel_to (c, how_many);
}

int
DiskIOProcessor::remove_channel_from (boost::shared_ptr<ChannelList> c, uint32_t how_many)
{
	while (how_many-- && !c->empty()) {
		delete c->back();
		c->pop_back();
	}

	return 0;
}

int
DiskIOProcessor::remove_channel (uint32_t how_many)
{
	RCUWriter<ChannelList> writer (channels);
	boost::shared_ptr<ChannelList> c = writer.get_copy();

	return remove_channel_from (c, how_many);
}

void
DiskIOProcessor::playlist_deleted (boost::weak_ptr<Playlist> wpl)
{
	boost::shared_ptr<Playlist> pl (wpl.lock());

	if (!pl) {
		return;
	}

	for (uint32_t n = 0; n < DataType::num_types; ++n) {
		if (pl == _playlists[n]) {

			/* this catches an ordering issue with session destruction. playlists
			   are destroyed before disk readers. we have to invalidate any handles
			   we have to the playlist.
			*/
			_playlists[n].reset ();
			break;
		}
	}
}

boost::shared_ptr<AudioPlaylist>
DiskIOProcessor::audio_playlist () const
{
	return boost::dynamic_pointer_cast<AudioPlaylist> (_playlists[DataType::AUDIO]);
}

boost::shared_ptr<MidiPlaylist>
DiskIOProcessor::midi_playlist () const
{
	return boost::dynamic_pointer_cast<MidiPlaylist> (_playlists[DataType::MIDI]);
}

int
DiskIOProcessor::use_playlist (DataType dt, boost::shared_ptr<Playlist> playlist)
{
	if (!playlist) {
		return 0;
	}

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: set to use playlist %2 (%3)\n", name(), playlist->name(), dt.to_string()));

	if (playlist == _playlists[dt]) {
		DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1: already using that playlist\n", name()));
		return 0;
	}

	playlist_connections.drop_connections ();

	if (_playlists[dt]) {
		_playlists[dt]->release();
	}

	_playlists[dt] = playlist;
	playlist->use();

	playlist->ContentsChanged.connect_same_thread (playlist_connections, boost::bind (&DiskIOProcessor::playlist_modified, this));
	playlist->LayeringChanged.connect_same_thread (playlist_connections, boost::bind (&DiskIOProcessor::playlist_modified, this));
	playlist->DropReferences.connect_same_thread (playlist_connections, boost::bind (&DiskIOProcessor::playlist_deleted, this, boost::weak_ptr<Playlist>(playlist)));
	playlist->RangesMoved.connect_same_thread (playlist_connections, boost::bind (&DiskIOProcessor::playlist_ranges_moved, this, _1, _2));

	DEBUG_TRACE (DEBUG::DiskIO, string_compose ("%1 now using playlist %1 (%2)\n", name(), playlist->name(), playlist->id()));

	return 0;
}

DiskIOProcessor::ChannelInfo::ChannelInfo (samplecnt_t bufsize)
	: rbuf (0)
	, wbuf (0)
	, capture_transition_buf (0)
	, curr_capture_cnt (0)
{
}

DiskIOProcessor::ChannelInfo::~ChannelInfo ()
{
	delete rbuf;
	delete wbuf;
	delete capture_transition_buf;
	rbuf = 0;
	wbuf = 0;
	capture_transition_buf = 0;
}

/** Get the start, end, and length of a location "atomically".
 *
 * Note: Locations don't get deleted, so all we care about when I say "atomic"
 * is that we are always pointing to the same one and using start/length values
 * obtained just once.  Use this function to achieve this since location being
 * a parameter achieves this.
 */
void
DiskIOProcessor::get_location_times(const Location* location,
                   timepos_t*     start,
                   timepos_t*     end,
                   timecnt_t*     length)
{
	if (location) {
		*start  = location->start();
		*end    = location->end();
		*length = location->length();
	}
}

