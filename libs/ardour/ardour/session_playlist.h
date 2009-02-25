/*
    Copyright (C) 2002 Paul Davis 

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

#ifndef __ardour_session_playlist_h__
#define __ardour_session_playlist_h__

#include "ardour/session.h"
#include "ardour/playlist.h"

namespace ARDOUR {

template<class T> void 
Session::foreach_playlist (T *obj, void (T::*func)(boost::shared_ptr<Playlist>)) 
{
	Glib::Mutex::Lock lm (playlist_lock);
	for (PlaylistList::iterator i = playlists.begin(); i != playlists.end(); i++) {
		if (!(*i)->hidden()) {
			(obj->*func) (*i);
		}
	}
	for (PlaylistList::iterator i = unused_playlists.begin(); i != unused_playlists.end(); i++) {
		if (!(*i)->hidden()) {
			(obj->*func) (*i);
		}
	}
}

} /* namespace */

#endif /* __ardour_session_playlist_h__ */
