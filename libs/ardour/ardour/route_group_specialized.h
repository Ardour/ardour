/*
    Copyright (C) 2000-2007 Paul Davis 

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

#ifndef __ardour_route_group_specialized_h__
#define __ardour_route_group_specialized_h__

#include "ardour/route_group.h"
#include "ardour/track.h"

namespace ARDOUR {

template<class T> void 
RouteGroup::apply (void (Track::*func)(T, void *), T val, void *src) 
{
	for (std::list<Route *>::iterator i = routes.begin(); i != routes.end(); i++) {
		Track *at;
		if ((at = dynamic_cast<Track*>(*i)) != 0) {
			(at->*func)(val, this);
		}
	}
}
 
} /* namespace ARDOUR */

#endif /* __ardour_route_group_specialized_h__ */
