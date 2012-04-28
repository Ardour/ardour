/*
    Copyright (C) 2006 Paul Davis 

    This program is free software; you can redistribute it
    and/or modify it under the terms of the GNU Lesser
    General Public License as published by the Free Software
    Foundation; either version 2 of the License, or (at your
    option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __ardour_control_protocol_types_h__
#define __ardour_control_protocol_types_h__

#include <vector>
#include <boost/smart_ptr.hpp>

namespace ARDOUR {
	class Route;
	
	typedef std::vector<boost::weak_ptr<ARDOUR::Route> >    RouteNotificationList;
	typedef boost::shared_ptr<RouteNotificationList>        RouteNotificationListPtr;

	typedef std::vector<boost::shared_ptr<ARDOUR::Route> >  StrongRouteNotificationList;
}

#endif /* __ardour_control_protocol_types_h__ */
