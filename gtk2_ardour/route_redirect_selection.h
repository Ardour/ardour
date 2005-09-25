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
#include <sigc++/signal_system.h>

#include "redirect_selection.h"
#include "route_selection.h"

class RouteRedirectSelection : public SigC::Object 
{
  public:
	RedirectSelection    redirects;
	RouteSelection       routes;

	RouteRedirectSelection() {}

	RouteRedirectSelection& operator= (const RouteRedirectSelection& other);

	SigC::Signal0<void> RedirectsChanged;
	SigC::Signal0<void> RoutesChanged;

	void clear ();
	bool empty();

	void set (ARDOUR::Redirect*);
	void set (const std::vector<ARDOUR::Redirect*>&);
	void add (ARDOUR::Redirect*);
	void add (const std::vector<ARDOUR::Redirect*>&);
	void remove (ARDOUR::Redirect*);

	void set (ARDOUR::Route*);
	void add (ARDOUR::Route*);
	void remove (ARDOUR::Route*);

	void clear_redirects ();
	void clear_routes ();

	bool selected (ARDOUR::Route*);
};

bool operator==(const RouteRedirectSelection& a, const RouteRedirectSelection& b);

#endif /* __ardour_gtk_route_redirect_selection_h__ */
