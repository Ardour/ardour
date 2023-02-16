/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2019 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_session_playlists_h__
#define __ardour_session_playlists_h__

#include <memory>
#include <set>
#include <vector>
#include <string>

#include <glibmm/threads.h>

#include <boost/function.hpp>

#include "pbd/signals.h"

class XMLNode;

namespace PBD {
	class ID;
}

namespace ARDOUR {

class Playlist;
class Region;
class Source;
class Session;
class Crossfade;
class Track;

class LIBARDOUR_API SessionPlaylists : public PBD::ScopedConnectionList
{
public:
	~SessionPlaylists ();

	std::shared_ptr<Playlist> for_pgroup (std::string name, const PBD::ID& for_track);
	std::shared_ptr<Playlist> by_name (std::string name);
	std::shared_ptr<Playlist> by_id (const PBD::ID&);
	uint32_t source_use_count (std::shared_ptr<const Source> src) const;
	uint32_t region_use_count (std::shared_ptr<Region> region) const;
	template<class T> void foreach (T *obj, void (T::*func)(std::shared_ptr<Playlist>));
	void foreach (boost::function<void(std::shared_ptr<const Playlist>)> functor, bool incl_unused = true);
	void get (std::vector<std::shared_ptr<Playlist> >&) const;
	void unassigned (std::list<std::shared_ptr<Playlist> > & list);
	void destroy_region (std::shared_ptr<Region>);
	std::shared_ptr<Crossfade> find_crossfade (const PBD::ID &);
	void sync_all_regions_with_regions ();
	std::vector<std::shared_ptr<Playlist> > playlists_for_pgroup (std::string pgroup);
	std::vector<std::shared_ptr<Playlist> > playlists_for_track (std::shared_ptr<Track>) const;
	std::vector<std::shared_ptr<Playlist> > get_used () const;
	std::vector<std::shared_ptr<Playlist> > get_unused () const;
	uint32_t n_playlists() const;

private:
	friend class Session;

	bool add (std::shared_ptr<Playlist>);
	void remove (std::shared_ptr<Playlist>);
	void remove_weak (std::weak_ptr<Playlist>);
	void track (bool, std::weak_ptr<Playlist>);
	void update_tracking ();

	void update_orig_2X (PBD::ID, PBD::ID);

	void find_equivalent_playlist_regions (std::shared_ptr<Region>, std::vector<std::shared_ptr<Region> >& result);
	void update_after_tempo_map_change ();
	void add_state (XMLNode*, bool save_template, bool include_unused) const;
	bool maybe_delete_unused (boost::function<int(std::shared_ptr<Playlist>)>);
	int load (Session &, const XMLNode&);
	int load_unused (Session &, const XMLNode&);
	std::shared_ptr<Playlist> XMLPlaylistFactory (Session &, const XMLNode&);

	mutable Glib::Threads::Mutex lock;
	typedef std::set<std::shared_ptr<Playlist> > List;
	List playlists;
	List unused_playlists;
};

}

#endif
