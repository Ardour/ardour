/*
    Copyright (C) 2002 Paul Davis 

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

#include <algorithm>
#include <sigc++/bind.h>
#include "pbd/error.h"

#include "ardour/playlist.h"
#include "ardour/processor.h"
#include "ardour/route.h"

#include "route_processor_selection.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace sigc;

RouteRedirectSelection&
RouteRedirectSelection::operator= (const RouteRedirectSelection& other)
{
	if (&other != this) {
		processors = other.processors;
		routes = other.routes;
	}
	return *this;
}

bool
operator== (const RouteRedirectSelection& a, const RouteRedirectSelection& b)
{
	// XXX MUST TEST PROCESSORS SOMEHOW
	return a.routes == b.routes;
}

void
RouteRedirectSelection::clear ()
{
	clear_processors ();
	clear_routes ();
}

void
RouteRedirectSelection::clear_processors ()
{
	processors.clear ();
	ProcessorsChanged ();
}

void
RouteRedirectSelection::clear_routes ()
{
	routes.clear ();
	RoutesChanged ();
}

void
RouteRedirectSelection::add (XMLNode* node)
{
	// XXX check for duplicate
	processors.add (node);
	ProcessorsChanged();
}

void
RouteRedirectSelection::set (XMLNode* node)
{
	clear_processors ();
	processors.set (node);
	ProcessorsChanged ();
}

void
RouteRedirectSelection::add (boost::shared_ptr<Route> r)
{
	if (find (routes.begin(), routes.end(), r) == routes.end()) {
		routes.push_back (r);

		// XXX SHAREDPTR FIXME
		// void (RouteRedirectSelection::*pmf)(Route*) = &RouteRedirectSelection::remove;
		// r->GoingAway.connect (bind (mem_fun(*this, pmf), r));

		RoutesChanged();
	}
}

void
RouteRedirectSelection::remove (boost::shared_ptr<Route> r)
{
	list<boost::shared_ptr<Route> >::iterator i;
	if ((i = find (routes.begin(), routes.end(), r)) != routes.end()) {
		routes.erase (i);
		RoutesChanged ();
	}
}

void
RouteRedirectSelection::set (boost::shared_ptr<Route> r)
{
	clear_routes ();
	add (r);
}

bool
RouteRedirectSelection::selected (boost::shared_ptr<Route> r)
{
	return find (routes.begin(), routes.end(), r) != routes.end();
}

bool
RouteRedirectSelection::empty ()
{
	return processors.empty () && routes.empty ();
}
		
