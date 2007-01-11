/*
    Copyright (C) 2003 Paul Davis 

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

#ifndef __ardour_playlist_templates_h__
#define __ardour_playlist_templates_h__

namespace ARDOUR {

template<class T> void AudioPlaylist::foreach_crossfade (T *t, void (T::*func)(boost::shared_ptr<Crossfade>)) {
	RegionLock rlock (this, false);
	for (Crossfades::iterator i = _crossfades.begin(); i != _crossfades.end(); i++) {	
		(t->*func) (*i);
	}
}

template<class T> void Playlist::foreach_region (T *t, void (T::*func)(boost::shared_ptr<Region>, void *), void *arg) {
	RegionLock rlock (this, false);
	for (RegionList::iterator i = regions.begin(); i != regions.end(); i++) {	
		(t->*func) ((*i), arg);
	}
	}

template<class T> void Playlist::foreach_region (T *t, void (T::*func)(boost::shared_ptr<Region>)) {
	RegionLock rlock (this, false);
	for (RegionList::const_iterator i = regions.begin(); i != regions.end(); i++) {
		(t->*func) (*i);
	}
}

}

#endif /* __ardour_playlist_templates_h__ */
