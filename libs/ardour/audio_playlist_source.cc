/*
 * Copyright (C) 2011-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2014-2016 Robin Gareus <robin@gareus.org>
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

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

AudioPlaylistSource::AudioPlaylistSource (Session& s, const ID& orig, const std::string& name, boost::shared_ptr<AudioPlaylist> p,
                                          uint32_t chn, timepos_t const & begin, timepos_t const & len, Source::Flag flags)
	: Source (s, DataType::AUDIO, name)
	, PlaylistSource (s, orig, name, p, DataType::AUDIO, begin, len, flags)
	, AudioSource (s, name)
	, _playlist_channel (chn)
{
	AudioSource::_length = timecnt_t (len);
	ensure_buffers_for_level (_level, _session.sample_rate());
}

AudioPlaylistSource::AudioPlaylistSource (Session& s, const XMLNode& node)
	: Source (s, node)
	, PlaylistSource (s, node)
	, AudioSource (s, node)
{
	/* PlaylistSources are never writable, renameable or removable */
	_flags = Flag (_flags & ~(Writable|CanRename|Removable|RemovableIfEmpty|RemoveAtDestroy));

	/* ancestors have already called ::set_state() in their XML-based
	   constructors.
	*/

	if (set_state (node, Stateful::loading_state_version, false)) {
		throw failed_constructor ();
	}

	_length = timecnt_t (_playlist_length);
}

AudioPlaylistSource::~AudioPlaylistSource ()
{
}

XMLNode&
AudioPlaylistSource::get_state ()
{
	XMLNode& node (AudioSource::get_state ());

	/* merge PlaylistSource state */

	PlaylistSource::add_state (node);

	node.set_property ("channel", _playlist_channel);

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

	pair<timepos_t,timepos_t> extent = _playlist->get_extent();

	AudioSource::_length = extent.first.distance (extent.second);

	if (!node.get_property (X_("channel"), _playlist_channel)) {
		throw failed_constructor ();
	}

	return 0;
}

samplecnt_t
AudioPlaylistSource::read_unlocked (Sample* dst, samplepos_t start, samplecnt_t cnt) const
{
	samplecnt_t to_read;
	samplecnt_t to_zero;

	/* we must be careful not to read beyond the end of our "section" of
	 * the playlist, because otherwise we may read data that exists, but
	 * is not supposed be part of our data.
	 */

	if (cnt > _playlist_length.samples() - start) {
		to_read = _playlist_length.samples() - start;
		to_zero = cnt - to_read;
	} else {
		to_read = cnt;
		to_zero = 0;
	}

	boost::scoped_array<float> sbuf(new float[to_read]);
	boost::scoped_array<gain_t> gbuf(new gain_t[to_read]);

	boost::dynamic_pointer_cast<AudioPlaylist>(_playlist)->read (dst, sbuf.get(), gbuf.get(), timepos_t (start)+_playlist_offset, timecnt_t (to_read), _playlist_channel);

	if (to_zero) {
		memset (dst+to_read, 0, sizeof (Sample) * to_zero);
	}

	return cnt;
}

samplecnt_t
AudioPlaylistSource::write_unlocked (Sample *, samplecnt_t)
{
	fatal << string_compose (_("programming error: %1"), "AudioPlaylistSource::write() called - should be impossible") << endmsg;
	abort(); /*NOTREACHED*/
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

	boost::shared_ptr<Region> r = _playlist->region_list_property().front ();
	boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

	return ar->audio_source()->n_channels ();
}

float
AudioPlaylistSource::sample_rate () const
{
	/* use just the first region to decide */

	if (empty()) {
		_session.sample_rate ();
	}

	boost::shared_ptr<Region> r = _playlist->region_list_property().front ();
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
AudioPlaylistSource::construct_peak_filepath (const string& /*audio_path_*/, const bool /* in_session */, const bool /* old_peak_name */) const
{
	return _peak_path;
}


