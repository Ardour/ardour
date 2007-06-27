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
#include <pbd/error.h>

#include <ardour/playlist.h>
#include <ardour/processor.h>
#include <ardour/route.h>

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
	return a.processors == b.processors &&
		a.routes == b.routes;
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
RouteRedirectSelection::add (boost::shared_ptr<Processor> r)
{
	if (find (processors.begin(), processors.end(), r) == processors.end()) {
		processors.push_back (r);

		// XXX SHAREDPTR FIXME
		// void (RouteRedirectSelection::*pmf)(Redirect*) = &RouteRedirectSelection::remove;
		// r->GoingAway.connect (mem_fun(*this, pmf));

		ProcessorsChanged();
	}
}

void
RouteRedirectSelection::add (const vector<boost::shared_ptr<Processor> >& rlist)
{
	bool changed = false;

	for (vector<boost::shared_ptr<Processor> >::const_iterator i = rlist.begin(); i != rlist.end(); ++i) {
		if (find (processors.begin(), processors.end(), *i) == processors.end()) {
			processors.push_back (*i);
			
			// XXX SHAREDPTR FIXME

			//void (RouteRedirectSelection::*pmf)(Redirect*) = &RouteRedirectSelection::remove;
			// (*i)->GoingAway.connect (mem_fun(*this, pmf));
			changed = true;
		}
	}

	if (changed) {
		ProcessorsChanged();
	}
}

void
RouteRedirectSelection::remove (boost::shared_ptr<Processor> r)
{
	list<boost::shared_ptr<Processor> >::iterator i;
	if ((i = find (processors.begin(), processors.end(), r)) != processors.end()) {
		processors.erase (i);
		ProcessorsChanged ();
	}
}

void
RouteRedirectSelection::set (boost::shared_ptr<Processor> r)
{
	clear_processors ();
	add (r);
}

void
RouteRedirectSelection::set (const vector<boost::shared_ptr<Processor> >& rlist)
{
	clear_processors ();
	add (rlist);
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
		
