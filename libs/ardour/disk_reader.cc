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

#include "ardour/debug.h"
#include "ardour/disk_reader.h"
#include "ardour/playlist.h"
#include "ardour/session.h"

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
{
}

DiskReader::~DiskReader ()
{
	DEBUG_TRACE (DEBUG::Destruction, string_compose ("DiskReader %1 deleted\n", _name));

	if (_playlist) {
		_playlist->release ();
	}
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
