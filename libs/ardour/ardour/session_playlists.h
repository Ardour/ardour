/*
    Copyright (C) 2009 Paul Davis

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

#ifndef __ardour_session_playlists_h__
#define __ardour_session_playlists_h__

#include <set>
#include <vector>
#include <string>
#include <glibmm/thread.h>
#include <boost/shared_ptr.hpp>
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
	
class SessionPlaylists : public PBD::ScopedConnectionList
{
public:
	~SessionPlaylists ();
	
	boost::shared_ptr<Playlist> by_name (std::string name);
	boost::shared_ptr<Playlist> by_id (const PBD::ID&);
	uint32_t source_use_count (boost::shared_ptr<const Source> src) const;
	template<class T> void foreach (T *obj, void (T::*func)(boost::shared_ptr<Playlist>));
	void get (std::vector<boost::shared_ptr<Playlist> >&);
	void unassigned (std::list<boost::shared_ptr<Playlist> > & list);

private:
	friend class Session;
	
	bool add (boost::shared_ptr<Playlist>);
	void remove (boost::shared_ptr<Playlist>);
	void track (bool, boost::weak_ptr<Playlist>);
	
	uint32_t n_playlists() const;
	void find_equivalent_playlist_regions (boost::shared_ptr<Region>, std::vector<boost::shared_ptr<Region> >& result);
	void update_after_tempo_map_change ();
	void add_state (XMLNode *, bool);
	bool maybe_delete_unused (boost::function<int(boost::shared_ptr<Playlist>)>);
	int load (Session &, const XMLNode&);
	int load_unused (Session &, const XMLNode&);
	boost::shared_ptr<Playlist> XMLPlaylistFactory (Session &, const XMLNode&);

	mutable Glib::Mutex lock;
	typedef std::set<boost::shared_ptr<Playlist> > List;
	List playlists;
	List unused_playlists;
};

}

#endif
