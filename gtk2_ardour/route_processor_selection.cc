/*
 * Copyright (C) 2007-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2018 Robin Gareus <robin@gareus.org>
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

#include <algorithm>
#include <sigc++/bind.h>

#include "pbd/error.h"

#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/session_handle.h"

#include "axis_provider.h"
#include "gui_thread.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "route_processor_selection.h"
#include "route_ui.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

RouteProcessorSelection::RouteProcessorSelection (SessionHandlePtr& s, AxisViewProvider& ap)
	: shp (s), avp (ap)
{
}

bool
operator== (const RouteProcessorSelection& a, const RouteProcessorSelection& b)
{
	// XXX MUST TEST PROCESSORS SOMEHOW
	return a.axes == b.axes;
}

void
RouteProcessorSelection::clear ()
{
	clear_processors ();
	clear_routes ();
}

void
RouteProcessorSelection::clear_routes ()
{
	if (shp.session()) {
		PresentationInfo::ChangeSuspender cs;
		shp.session()->selection().clear_stripables ();
	}
}

std::list<AxisView*>
RouteProcessorSelection::add_grouped_tracks (AxisView* r) const
{
	std::list<AxisView*> rv;

	boost::shared_ptr<Route> route = boost::dynamic_pointer_cast<Route>(r->stripable());
	if (route) {
		ARDOUR::RouteGroup* rg = route->route_group ();
		if (rg && rg->is_active() && rg->is_select ()) {

			boost::shared_ptr<RouteList> rl = rg->route_list ();
			for (RouteList::const_iterator i = rl->begin(); i != rl->end(); ++i) {
				AxisView* av = avp.axis_view_by_stripable (*i);
				rv.push_back (av);
			}
		}
	}
	return rv;
}

void
RouteProcessorSelection::presentation_info_changed (PropertyChange const & what_changed)
{
	Session* s = shp.session();

	if (!s) {
		/* too early ... session handle provider doesn't know about the
		   session yet.
		*/
		return;
	}

	PropertyChange pc;
	pc.add (Properties::selected);

	CoreSelection::StripableAutomationControls sc;
	s->selection().get_stripables (sc);

	for (AxisViewSelection::iterator a = axes.begin(); a != axes.end(); ++a) {
		(*a)->set_selected (false);
	}

	axes.clear ();

	for (CoreSelection::StripableAutomationControls::const_iterator i = sc.begin(); i != sc.end(); ++i) {
		AxisView* av = avp.axis_view_by_stripable ((*i).stripable);
		if (av) {
			axes.insert (av);
			av->set_selected (true);
		}
	}
}

void
RouteProcessorSelection::add (AxisView* r, bool with_groups)
{
	if (!shp.session()) {
		return;
	}

	std::list<AxisView*> avl;
	if (with_groups) {
		avl= add_grouped_tracks (r);
	}
	avl.push_back (r);

	PresentationInfo::ChangeSuspender cs;
	for (std::list<AxisView*>::const_iterator i = avl.begin (); i != avl.end (); ++i) {
		if (axes.insert (*i).second) {
			shp.session()->selection().add ((*i)->stripable(), boost::shared_ptr<AutomationControl>());
			MixerStrip* ms = dynamic_cast<MixerStrip*> (*i);
			if (ms) {
				ms->CatchDeletion.connect (*this, invalidator (*this), boost::bind (&RouteProcessorSelection::remove, this, _1, false), gui_context());
			}
		}
	}
}

void
RouteProcessorSelection::remove (AxisView* r, bool with_groups)
{
	if (!shp.session()) {
		return;
	}
	ENSURE_GUI_THREAD (*this, &RouteProcessorSelection::remove, r);

	std::list<AxisView*> avl;
	if (with_groups) {
		avl= add_grouped_tracks (r);
	}
	avl.push_back (r);

	PresentationInfo::ChangeSuspender cs;
	for (std::list<AxisView*>::const_iterator i = avl.begin (); i != avl.end (); ++i) {
		shp.session()->selection().remove ((*i)->stripable(), boost::shared_ptr<AutomationControl>());
	}
}

void
RouteProcessorSelection::set (AxisView* r)
{
	if (!shp.session()) {
		return;
	}
	PresentationInfo::ChangeSuspender cs;
	shp.session()->selection().clear_stripables ();
	add (r, true);
}

bool
RouteProcessorSelection::selected (AxisView* r)
{
	return find (axes.begin(), axes.end(), r) != axes.end();
}

bool
RouteProcessorSelection::empty ()
{
	return processors.empty () && axes.empty ();
}
