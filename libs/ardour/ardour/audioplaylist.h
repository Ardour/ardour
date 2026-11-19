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

#pragma once

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
	AudioPlaylist (std::shared_ptr<const AudioPlaylist>, std::string name, bool hidden = false);
	AudioPlaylist (std::shared_ptr<const AudioPlaylist>, timepos_t const & start, timepos_t const & cnt, std::string name, bool hidden = false);

	timecnt_t read (Sample *dst, Sample *mixdown, float *gain_buffer, timepos_t const & start, timecnt_t const & cnt, uint32_t chan_n=0);

	/** Fill the comping layer from the top takes. With a non-empty range only that range (where not already comped) is filled. */
	bool initialize_comping_layer (samplepos_t range_start = 0, samplepos_t range_end = 0);
	/** Add [start, end) of @p source to the comping layer, shortening overlapping comp fragments. */
	bool add_to_comping_layer (std::shared_ptr<Region> source, samplepos_t start, samplepos_t end);
	/** Turn the comp fragments (within the range, if given) into ordinary regions on the top layer. */
	bool finish_comping_layer (samplepos_t range_start = 0, samplepos_t range_end = 0);
	/** Delete the comp fragments (within the range, if given). */
	bool discard_comping_layer (samplepos_t range_start = 0, samplepos_t range_end = 0);
	/** Remove [start, end) of @p source from the comping layer, leaving a gap. */
	bool remove_from_comping_layer (std::shared_ptr<Region> source, samplepos_t start, samplepos_t end);

	bool destroy_region (std::shared_ptr<Region>);

protected:

	void pre_combine (std::vector<std::shared_ptr<Region> >&);
	void post_combine (std::vector<std::shared_ptr<Region> >&, std::shared_ptr<Region>);
	void pre_uncombine (std::vector<std::shared_ptr<Region> >&, std::shared_ptr<Region>);

private:
	void cut_comping_range (samplepos_t start, samplepos_t end, std::string const& only_source_id);
	void add_comping_slice (std::shared_ptr<Region> from, samplepos_t s, samplepos_t e, std::string const& id);
	int set_state (const XMLNode&, int version);
	void dump () const;
	bool region_changed (const PBD::PropertyChange&, std::shared_ptr<Region>);
	void source_offset_changed (std::shared_ptr<AudioRegion>);
        void load_legacy_crossfades (const XMLNode&, int version);
};

} /* namespace ARDOUR */
