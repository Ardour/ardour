/*
 * Copyright (C) 2012-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_control_protocol_types_h__
#define __ardour_control_protocol_types_h__

#include <vector>
#include <boost/smart_ptr.hpp>

namespace ARDOUR {
	class Route;
	class Stripable;

	typedef std::vector<boost::weak_ptr<ARDOUR::Route> >    RouteNotificationList;
	typedef boost::shared_ptr<RouteNotificationList>        RouteNotificationListPtr;
	typedef std::vector<boost::shared_ptr<ARDOUR::Route> >  StrongRouteNotificationList;

	typedef std::vector<boost::weak_ptr<ARDOUR::Stripable> >    StripableNotificationList;
	typedef boost::shared_ptr<StripableNotificationList>        StripableNotificationListPtr;
	typedef std::vector<boost::shared_ptr<ARDOUR::Stripable> >  StrongStripableNotificationList;
}

#endif /* __ardour_control_protocol_types_h__ */
