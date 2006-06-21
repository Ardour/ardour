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

    $Id$
*/

#include <algorithm>
#include <sigc++/bind.h>
#include <pbd/error.h>

#include <ardour/playlist.h>
#include <ardour/redirect.h>
#include <ardour/route.h>

#include "route_redirect_selection.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace sigc;

RouteRedirectSelection&
RouteRedirectSelection::operator= (const RouteRedirectSelection& other)
{
	if (&other != this) {
		redirects = other.redirects;
		routes = other.routes;
	}
	return *this;
}

bool
operator== (const RouteRedirectSelection& a, const RouteRedirectSelection& b)
{
	return a.redirects == b.redirects &&
		a.routes == b.routes;
}

void
RouteRedirectSelection::clear ()
{
	clear_redirects ();
	clear_routes ();
}

void
RouteRedirectSelection::clear_redirects ()
{
	for (RedirectSelection::iterator i = redirects.begin(); i != redirects.end(); ) {
		RedirectSelection::iterator tmp;

		tmp = i;
		++tmp;

		delete *i;

		i = tmp;
	}

	redirects.clear ();
	RedirectsChanged ();
}

void
RouteRedirectSelection::clear_routes ()
{
	routes.clear ();
	RoutesChanged ();
}

void
RouteRedirectSelection::add (Redirect* r)
{
	if (find (redirects.begin(), redirects.end(), r) == redirects.end()) {
		redirects.push_back (r);
		
		void (RouteRedirectSelection::*pmf)(Redirect*) = &RouteRedirectSelection::remove;
		r->GoingAway.connect (mem_fun(*this, pmf));

		RedirectsChanged();
	}
}

void
RouteRedirectSelection::add (const vector<Redirect*>& rlist)
{
	bool changed = false;

	for (vector<Redirect*>::const_iterator i = rlist.begin(); i != rlist.end(); ++i) {
		if (find (redirects.begin(), redirects.end(), *i) == redirects.end()) {
			redirects.push_back (*i);
			
			void (RouteRedirectSelection::*pmf)(Redirect*) = &RouteRedirectSelection::remove;
			(*i)->GoingAway.connect (mem_fun(*this, pmf));
			changed = true;
		}
	}

	if (changed) {
		RedirectsChanged();
	}
}

void
RouteRedirectSelection::remove (Redirect* r)
{
	list<Redirect*>::iterator i;
	if ((i = find (redirects.begin(), redirects.end(), r)) != redirects.end()) {
		redirects.erase (i);
		RedirectsChanged ();
	}
}

void
RouteRedirectSelection::set (Redirect *r)
{
	clear_redirects ();
	add (r);
}

void
RouteRedirectSelection::set (const vector<Redirect*>& rlist)
{
	clear_redirects ();
	add (rlist);
}

void
RouteRedirectSelection::add (Route* r)
{
	if (find (routes.begin(), routes.end(), r) == routes.end()) {
		routes.push_back (r);

		void (RouteRedirectSelection::*pmf)(Route*) = &RouteRedirectSelection::remove;
		r->GoingAway.connect (bind (mem_fun(*this, pmf), r));

		RoutesChanged();
	}
}

void
RouteRedirectSelection::remove (Route* r)
{
	list<Route*>::iterator i;
	if ((i = find (routes.begin(), routes.end(), r)) != routes.end()) {
		routes.erase (i);
		RoutesChanged ();
	}
}

void
RouteRedirectSelection::set (Route *r)
{
	clear_routes ();
	add (r);
}

bool
RouteRedirectSelection::selected (Route* ms)
{
	return find (routes.begin(), routes.end(), ms) != routes.end();
}

bool
RouteRedirectSelection::empty ()
{
	return redirects.empty () && routes.empty ();
}
		
