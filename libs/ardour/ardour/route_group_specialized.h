/*
 * Copyright (C) 2006-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_route_group_specialized_h__
#define __ardour_route_group_specialized_h__

#include "ardour/route_group.h"
#include "ardour/track.h"

namespace ARDOUR {

template<class T> void
RouteGroup::apply (void (Track::*func)(T, PBD::Controllable::GroupControlDisposition), T val, PBD::Controllable::GroupControlDisposition group_override)
{
	for (RouteList::iterator i = routes->begin(); i != routes->end(); i++) {
		boost::shared_ptr<Track> at;

		if ((at = boost::dynamic_pointer_cast<Track>(*i)) != 0) {
			(at.get()->*func)(val, group_override);
		}
	}
}

} /* namespace ARDOUR */

#endif /* __ardour_route_group_specialized_h__ */

