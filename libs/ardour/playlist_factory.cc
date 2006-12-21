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

    $Id$
*/

#include <pbd/error.h>

#include <ardour/playlist.h>
#include <ardour/audioplaylist.h>
#include <ardour/playlist_factory.h>

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;

sigc::signal<void,boost::shared_ptr<Playlist> > PlaylistFactory::PlaylistCreated;

boost::shared_ptr<Playlist> 
PlaylistFactory::create (Session& s, const XMLNode& node, bool hidden) 
{
	boost::shared_ptr<Playlist> pl;

	pl = boost::shared_ptr<Playlist> (new AudioPlaylist (s, node, hidden));

	pl->set_region_ownership ();

	if (!hidden) {
		PlaylistCreated (pl);
	}
	return pl;
}

boost::shared_ptr<Playlist> 
PlaylistFactory::create (Session& s, string name, bool hidden) 
{
	boost::shared_ptr<Playlist> pl;

	pl = boost::shared_ptr<Playlist> (new AudioPlaylist (s, name, hidden));

	if (!hidden) {
		PlaylistCreated (pl);
	}

	return pl;
}

boost::shared_ptr<Playlist> 
PlaylistFactory::create (boost::shared_ptr<const Playlist> old, string name, bool hidden) 
{
	boost::shared_ptr<Playlist> pl;
	boost::shared_ptr<const AudioPlaylist> apl;

	if ((apl = boost::dynamic_pointer_cast<const AudioPlaylist> (old)) != 0) {
		pl = boost::shared_ptr<Playlist> (new AudioPlaylist (apl, name, hidden));
		pl->set_region_ownership ();
	}

	if (!hidden) {
		PlaylistCreated (pl);
	}

	return pl;
}

boost::shared_ptr<Playlist> 
PlaylistFactory::create (boost::shared_ptr<const Playlist> old, nframes_t start, nframes_t cnt, string name, bool hidden) 
{
	boost::shared_ptr<Playlist> pl;
	boost::shared_ptr<const AudioPlaylist> apl;
	
	if ((apl = boost::dynamic_pointer_cast<const AudioPlaylist> (old)) != 0) {
		pl = boost::shared_ptr<Playlist> (new AudioPlaylist (apl, start, cnt, name, hidden));
		pl->set_region_ownership ();
	}

	/* this factory method does NOT notify others */

	return pl;
}
