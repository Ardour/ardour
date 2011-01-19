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

#ifndef __osc_oscrouteobserver_h__
#define __osc_oscrouteobserver_h__

#include <string>
#include <boost/shared_ptr.hpp>
#include <sigc++/sigc++.h>
#include <lo/lo.h>

#include "pbd/controllable.h"
#include "pbd/stateful.h"
#include "ardour/types.h"

class OSCRouteObserver
{

  public:
	OSCRouteObserver (boost::shared_ptr<ARDOUR::Route>, lo_address addr);
	~OSCRouteObserver ();

	boost::shared_ptr<ARDOUR::Route> route () const { return _route; }
	lo_address address() const { return addr; };

  private:
	boost::shared_ptr<ARDOUR::Route> _route;
	//boost::shared_ptr<Controllable> _controllable;
	
	PBD::ScopedConnection name_changed_connection;
	PBD::ScopedConnection rec_changed_connection;
	PBD::ScopedConnection mute_changed_connection;
	PBD::ScopedConnection solo_changed_connection;
	PBD::ScopedConnection gain_changed_connection;
	
	lo_address addr;
	std::string path;

	void name_changed (const PBD::PropertyChange& what_changed);
	void send_change_message (std::string path, boost::shared_ptr<PBD::Controllable> controllable);
};

#endif /* __osc_oscrouteobserver_h__ */
