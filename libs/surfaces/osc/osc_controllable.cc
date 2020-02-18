/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cstdio> /* for sprintf, sigh */
#include <climits>
#include "pbd/error.h"
#include "pbd/xml++.h"

#include "ardour/route.h"

#include "osc.h"
#include "osc_controllable.h"

using namespace PBD;
using namespace ARDOUR;
using namespace ArdourSurface;

OSCControllable::OSCControllable (lo_address a, const std::string& p, boost::shared_ptr<Controllable> c)
	: controllable (c)
	, path (p)
{
	addr = lo_address_new (lo_address_get_hostname(a) , lo_address_get_port(a));
	c->Changed.connect (changed_connection, MISSING_INVALIDATOR, boost::bind (&OSCControllable::send_change_message, this), OSC::instance());
}

OSCControllable::~OSCControllable ()
{
	changed_connection.disconnect();
	lo_address_free (addr);
}

XMLNode&
OSCControllable::get_state ()
{
	XMLNode& root (controllable->get_state());
	return root;
}

int
OSCControllable::set_state (const XMLNode& /*node*/, int /*version*/)
{
	return 0;
}

void
OSCControllable::send_change_message ()
{
	lo_message msg = lo_message_new ();

	lo_message_add_float (msg, (float) controllable->get_value());

	/* XXX thread issues */

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}

/*------------------------------------------------------------*/

OSCRouteControllable::OSCRouteControllable (lo_address a, const std::string& p,
					    boost::shared_ptr<Controllable> c, boost::shared_ptr<Route> r)
	: OSCControllable (a, p, c)
	, _route (r)
{
}

OSCRouteControllable::~OSCRouteControllable ()
{
}

void
OSCRouteControllable::send_change_message ()
{
	lo_message msg = lo_message_new ();

	/* Can only send ID part of RID at present */

	lo_message_add_int32 (msg, _route->presentation_info().order());
	lo_message_add_float (msg, (float) controllable->get_value());

	/* XXX thread issues */

	//std::cerr << "ORC: send " << path << " = " << controllable->get_value() << std::endl;

	lo_send_message (addr, path.c_str(), msg);
	lo_message_free (msg);
}
