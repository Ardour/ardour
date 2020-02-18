/*
 * Copyright (C) 2009-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __osc_osccontrollable_h__
#define __osc_osccontrollable_h__

#include <string>
#include <boost/shared_ptr.hpp>
#include <sigc++/sigc++.h>
#include <lo/lo.h>

#include "pbd/controllable.h"
#include "pbd/stateful.h"
#include "ardour/types.h"

namespace ARDOUR {
	class Route;
}

class OSCControllable : public PBD::Stateful
{
  public:
	OSCControllable (lo_address addr, const std::string& path, boost::shared_ptr<PBD::Controllable>);
	virtual ~OSCControllable ();

	lo_address address() const { return addr; }

	XMLNode& get_state ();
	int set_state (const XMLNode& node, int version);

  protected:
	boost::shared_ptr<PBD::Controllable> controllable;
	PBD::ScopedConnection changed_connection;
	lo_address addr;
	std::string path;

	virtual void send_change_message ();
};

class OSCRouteControllable : public OSCControllable
{

  public:
	OSCRouteControllable (lo_address addr, const std::string& path,
			      boost::shared_ptr<PBD::Controllable>,
			      boost::shared_ptr<ARDOUR::Route>);
	~OSCRouteControllable ();

	boost::shared_ptr<ARDOUR::Route> route() const { return _route; }

  private:
	boost::shared_ptr<ARDOUR::Route> _route;

	void send_change_message ();
};

#endif /* __osc_osccontrollable_h__ */
