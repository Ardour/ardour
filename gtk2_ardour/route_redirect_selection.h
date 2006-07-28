/*
    Copyright (C) 2004 Paul Davis 

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

#ifndef __ardour_gtk_route_redirect_selection_h__
#define __ardour_gtk_route_redirect_selection_h__

#include <vector>
#include <sigc++/signal.h>

#include "redirect_selection.h"
#include "route_selection.h"

class RouteRedirectSelection : public sigc::trackable 
{
  public:
	RedirectSelection    redirects;
	RouteSelection       routes;

	RouteRedirectSelection() {}

	RouteRedirectSelection& operator= (const RouteRedirectSelection& other);

	sigc::signal<void> RedirectsChanged;
	sigc::signal<void> RoutesChanged;

	void clear ();
	bool empty();

	void set (boost::shared_ptr<ARDOUR::Redirect>);
	void set (const std::vector<boost::shared_ptr<ARDOUR::Redirect> >&);
	void add (boost::shared_ptr<ARDOUR::Redirect>);
	void add (const std::vector<boost::shared_ptr<ARDOUR::Redirect> >&);
	void remove (boost::shared_ptr<ARDOUR::Redirect>);

	void set (boost::shared_ptr<ARDOUR::Route>);
	void add (boost::shared_ptr<ARDOUR::Route>);
	void remove (boost::shared_ptr<ARDOUR::Route>);

	void clear_redirects ();
	void clear_routes ();

	bool selected (boost::shared_ptr<ARDOUR::Route>);
};

bool operator==(const RouteRedirectSelection& a, const RouteRedirectSelection& b);

#endif /* __ardour_gtk_route_redirect_selection_h__ */
