/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2012 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#ifndef __ardour_session_playlist_h__
#define __ardour_session_playlist_h__

#include "ardour/session.h"
#include "ardour/playlist.h"
#include "ardour/session_playlists.h"

namespace ARDOUR {

template<class T> void
SessionPlaylists::foreach (T *obj, void (T::*func)(boost::shared_ptr<Playlist>))
{
	Glib::Threads::Mutex::Lock lm (lock);
	for (List::iterator i = playlists.begin(); i != playlists.end(); i++) {
		if (!(*i)->hidden()) {
			(obj->*func) (*i);
		}
	}
	for (List::iterator i = unused_playlists.begin(); i != unused_playlists.end(); i++) {
		if (!(*i)->hidden()) {
			(obj->*func) (*i);
		}
	}
}

} /* namespace */

#endif /* __ardour_session_playlist_h__ */
