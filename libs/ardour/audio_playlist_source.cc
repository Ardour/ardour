/*
    Copyright (C) 2011 Paul Davis

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

#ifdef WAF_BUILD
#include "libardour-config.h"
#endif

#include <vector>
#include <cstdio>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"

#include "ardour/audioplaylist.h"
#include "ardour/audio_playlist_source.h"
#include "ardour/audioregion.h"
#include "ardour/filename_extensions.h"
#include "ardour/session.h"
#include "ardour/session_directory.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioPlaylistSource::AudioPlaylistSource (Session& s, const ID& orig, const std::string& name, boost::shared_ptr<AudioPlaylist> p,
					  uint32_t chn, frameoffset_t begin, framecnt_t len, Source::Flag flags)
	: Source (s, DataType::AUDIO, name)
	, PlaylistSource (s, orig, name, p, DataType::AUDIO, begin, len, flags)
	, AudioSource (s, name)
	, _playlist_channel (chn)
{
	AudioSource::_length = len;
	ensure_buffers_for_level (_level, _session.frame_rate());
}

AudioPlaylistSource::AudioPlaylistSource (Session& s, const XMLNode& node)
	: Source (s, node)
	, PlaylistSource (s, node)
	, AudioSource (s, node)
{
	/* PlaylistSources are never writable, renameable, removable or destructive */
	_flags = Flag (_flags & ~(Writable|CanRename|Removable|RemovableIfEmpty|RemoveAtDestroy|Destructive));

	/* ancestors have already called ::set_state() in their XML-based
	   constructors.
	*/

	if (set_state (node, Stateful::loading_state_version, false)) {
		throw failed_constructor ();
	}

	AudioSource::_length = _playlist_length;
}

AudioPlaylistSource::~AudioPlaylistSource ()
{
}

XMLNode&
AudioPlaylistSource::get_state ()
{
	XMLNode& node (AudioSource::get_state ());
	char buf[64];

	/* merge PlaylistSource state */

	PlaylistSource::add_state (node);

	snprintf (buf, sizeof (buf), "%" PRIu32, _playlist_channel);
	node.add_property ("channel", buf);

	return node;
}

int
AudioPlaylistSource::set_state (const XMLNode& node, int version)
{
	return set_state (node, version, true);
}

int
AudioPlaylistSource::set_state (const XMLNode& node, int version, bool with_descendants)
{
	if (with_descendants) {
		if (Source::set_state (node, version) ||
		    PlaylistSource::set_state (node, version) ||
		    AudioSource::set_state (node, version)) {
			return -1;
		}
	}

	const XMLProperty* prop;
	pair<framepos_t,framepos_t> extent = _playlist->get_extent();

	AudioSource::_length = extent.second - extent.first;

	if ((prop = node.property (X_("channel"))) == 0) {
		throw failed_constructor ();
	}

	sscanf (prop->value().c_str(), "%" PRIu32, &_playlist_channel);

	ensure_buffers_for_level (_level, _session.frame_rate());

	return 0;
}

framecnt_t
AudioPlaylistSource::read_unlocked (Sample* dst, framepos_t start, framecnt_t cnt) const
{
	boost::shared_array<Sample> sbuf;
	boost::shared_array<gain_t> gbuf;
	framecnt_t to_read;
	framecnt_t to_zero;

	/* we must be careful not to read beyond the end of our "section" of
	 * the playlist, because otherwise we may read data that exists, but
	 * is not supposed be part of our data.
	 */

	if (cnt > _playlist_length - start) {
		to_read = _playlist_length - start;
		to_zero = cnt - to_read;
	} else {
		to_read = cnt;
		to_zero = 0;
	}

	{
		/* Don't need to hold the lock for the actual read, and
		   actually, we cannot, but we do want to interlock
		   with any changes to the list of buffers caused
		   by creating new nested playlists/sources
		*/
		Glib::Threads::Mutex::Lock lm (_level_buffer_lock);
		sbuf = _mixdown_buffers[_level-1];
		gbuf = _gain_buffers[_level-1];
	}

	boost::dynamic_pointer_cast<AudioPlaylist>(_playlist)->read (dst, sbuf.get(), gbuf.get(), start+_playlist_offset, to_read, _playlist_channel);

	if (to_zero) {
		memset (dst+to_read, 0, sizeof (Sample) * to_zero);
	}

	return cnt;
}

framecnt_t
AudioPlaylistSource::write_unlocked (Sample *, framecnt_t)
{
	fatal << string_compose (_("programming error: %1"), "AudioPlaylistSource::write() called - should be impossible") << endmsg;
	/*NOTREACHED*/
	return 0;
}

bool
AudioPlaylistSource::empty () const
{
	return !_playlist || _playlist->empty();
}

uint32_t
AudioPlaylistSource::n_channels () const
{
	/* use just the first region to decide */

	if (empty()) {
		return 1;
	}

	boost::shared_ptr<Region> r = _playlist->region_list().front ();
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	return ar->audio_source()->n_channels ();
}

float
AudioPlaylistSource::sample_rate () const
{
	/* use just the first region to decide */

	if (empty()) {
		_session.frame_rate ();
	}

	boost::shared_ptr<Region> r = _playlist->region_list().front ();
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	return ar->audio_source()->sample_rate ();
}

int
AudioPlaylistSource::setup_peakfile ()
{
	_peak_path = Glib::build_filename (_session.session_directory().peak_path(), name() + ARDOUR::peakfile_suffix);
	return initialize_peakfile (string());
}

string
AudioPlaylistSource::peak_path (string /*audio_path_IGNORED*/)
{
	return _peak_path;
}


