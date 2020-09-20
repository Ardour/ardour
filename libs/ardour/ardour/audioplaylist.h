/*
 * Copyright (C) 2006-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
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

#ifndef __ardour_audio_playlist_h__
#define __ardour_audio_playlist_h__

#include <vector>
#include <list>

#include "ardour/ardour.h"
#include "ardour/playlist.h"

namespace ARDOUR  {

class Session;
class AudioRegion;
class Source;
class AudioPlaylist;

class LIBARDOUR_API AudioPlaylist : public ARDOUR::Playlist
{
public:
	AudioPlaylist (Session&, const XMLNode&, bool hidden = false);
	AudioPlaylist (Session&, std::string name, bool hidden = false);
	AudioPlaylist (boost::shared_ptr<const AudioPlaylist>, std::string name, bool hidden = false);
	AudioPlaylist (boost::shared_ptr<const AudioPlaylist>, timepos_t const & start, timepos_t const & cnt, std::string name, bool hidden = false);

	timecnt_t read (Sample *dst, Sample *mixdown, float *gain_buffer, timepos_t const & start, timecnt_t const & cnt, uint32_t chan_n=0);

	bool destroy_region (boost::shared_ptr<Region>);

protected:

	void pre_combine (std::vector<boost::shared_ptr<Region> >&);
	void post_combine (std::vector<boost::shared_ptr<Region> >&, boost::shared_ptr<Region>);
	void pre_uncombine (std::vector<boost::shared_ptr<Region> >&, boost::shared_ptr<Region>);

private:
	int set_state (const XMLNode&, int version);
	void dump () const;
	bool region_changed (const PBD::PropertyChange&, boost::shared_ptr<Region>);
	void source_offset_changed (boost::shared_ptr<AudioRegion>);
        void load_legacy_crossfades (const XMLNode&, int version);
};

} /* namespace ARDOUR */

#endif /* __ardour_audio_playlist_h__ */
