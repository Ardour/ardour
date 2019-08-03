/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2012 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#ifndef __ardour_session_route_h__
#define __ardour_session_route_h__

#include <iostream>

#include <glibmm/threads.h>

#include "ardour/session.h"
#include "ardour/route.h"

namespace ARDOUR {

template<class T> void
Session::foreach_route (T *obj, void (T::*func)(Route&), bool sort)
{
	boost::shared_ptr<RouteList> r = routes.reader();
	RouteList public_order (*r);

	if (sort) {
		public_order.sort (Stripable::Sorter ());
	}

	for (RouteList::iterator i = public_order.begin(); i != public_order.end(); i++) {
		(obj->*func) (**i);
	}
}

template<class T> void
Session::foreach_route (T *obj, void (T::*func)(boost::shared_ptr<Route>), bool sort)
{
	boost::shared_ptr<RouteList> r = routes.reader();
	RouteList public_order (*r);

	if (sort) {
		public_order.sort (Stripable::Sorter ());
	}

	for (RouteList::iterator i = public_order.begin(); i != public_order.end(); i++) {
		(obj->*func) (*i);
	}
}

template<class T, class A> void
Session::foreach_route (T *obj, void (T::*func)(Route&, A), A arg1, bool sort)
{
	boost::shared_ptr<RouteList> r = routes.reader();
	RouteList public_order (*r);

	if (sort) {
		public_order.sort (Stripable::Sorter ());
	}

	for (RouteList::iterator i = public_order.begin(); i != public_order.end(); i++) {
		(obj->*func) (**i, arg1);
	}
}

} /* namespace */

#endif /* __ardour_session_route_h__ */
