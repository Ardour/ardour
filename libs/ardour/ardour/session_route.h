/*
    Copyright (C) 2000 Paul Davis

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

#ifndef __ardour_session_route_h__
#define __ardour_session_route_h__

#include <iostream>

#include <glibmm/threads.h>

#include "ardour/session.h"
#include "ardour/route.h"

namespace ARDOUR {

template<class T> void
Session::foreach_route (T *obj, void (T::*func)(Route&))
{
	boost::shared_ptr<RouteList> r = routes.reader();
	RouteList public_order (*r);
	RoutePublicOrderSorter cmp;

	public_order.sort (cmp);

	for (RouteList::iterator i = public_order.begin(); i != public_order.end(); i++) {
		(obj->*func) (**i);
	}
}

template<class T> void
Session::foreach_route (T *obj, void (T::*func)(boost::shared_ptr<Route>))
{
	boost::shared_ptr<RouteList> r = routes.reader();
	RouteList public_order (*r);
	RoutePublicOrderSorter cmp;

	public_order.sort (cmp);

	for (RouteList::iterator i = public_order.begin(); i != public_order.end(); i++) {
		(obj->*func) (*i);
	}
}

template<class T, class A> void
Session::foreach_route (T *obj, void (T::*func)(Route&, A), A arg1)
{
	boost::shared_ptr<RouteList> r = routes.reader();
	RouteList public_order (*r);
	RoutePublicOrderSorter cmp;

	public_order.sort (cmp);

	for (RouteList::iterator i = public_order.begin(); i != public_order.end(); i++) {
		(obj->*func) (**i, arg1);
	}
}

} /* namespace */

#endif /* __ardour_session_route_h__ */
