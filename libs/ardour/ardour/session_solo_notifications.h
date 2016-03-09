/*
    Copyright (C) 2016 Paul Davis

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

#ifndef __libardour_session_solo_notifications_h__
#define __libardour_session_solo_notifications_h__

#include <boost/shared_ptr.hpp>
#include "pbd/controllable.h"

namespace ARDOUR {

class Route;

template<typename T>
class SessionSoloNotifications
{
    public:
	void solo_changed (bool self_solo_change, PBD::Controllable::GroupControlDisposition gcd, boost::shared_ptr<Route> route) {
		static_cast<T*>(this)->_route_solo_changed (self_solo_change, gcd, route);
	}

	void listen_changed (PBD::Controllable::GroupControlDisposition gcd, boost::shared_ptr<Route> route) {
		static_cast<T*>(this)->_route_listen_changed (gcd, route);
	}

	void mute_changed () {
		static_cast<T*>(this)->_route_mute_changed ();
	}

	void solo_isolated_changed (boost::shared_ptr<Route> route) {
		static_cast<T*>(this)->_route_solo_isolated_changed (route);
	}
};

} /* namespace */

#endif /* __libardour_session_solo_notifications_h__ */
