/*
    Copyright (C) 2009 Paul Davis
 
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

#include <cstdio> /* for sprintf, sigh */
#include <climits>

#include "boost/lambda/lambda.hpp"

#include "pbd/error.h"
#include "pbd/xml++.h"

#include "ardour/route.h"

#include "osc.h"
#include "osc_route_observer.h"

#define ui_bind(f, ...) boost::protect (boost::bind (f, __VA_ARGS__))

using namespace sigc;
using namespace PBD;
using namespace ARDOUR;


OSCRouteObserver::OSCRouteObserver (lo_address a, const std::string& p, boost::shared_ptr<Route> r)
	: _route (r)
{
	addr = lo_address_new (lo_address_get_hostname(a) , lo_address_get_port(a));
	_route->PropertyChanged.connect (changed_connection, MISSING_INVALIDATOR, ui_bind (&OSCRouteObserver::name_changed, this, boost::lambda::_1), OSC::instance());
}

OSCRouteObserver::~OSCRouteObserver ()
{
	changed_connection.disconnect();
	lo_address_free (addr);
}

void
OSCRouteObserver::name_changed (const PBD::PropertyChange& what_changed)
{
	if (!what_changed.contains (ARDOUR::Properties::name)) {
	    return;
	}
	
	if (!_route) {
		return;
	}
	
	lo_message msg = lo_message_new ();

	lo_message_add_int32 (msg, _route->remote_control_id());
	lo_message_add_string (msg, _route->name().c_str());

	lo_send_message (addr, "/route/name", msg);
	lo_message_free (msg);
}
