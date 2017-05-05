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
#include "pbd/i18n.h"

#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/session_handle.h"

#include "axis_provider.h"
#include "gui_thread.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "route_processor_selection.h"
#include "route_ui.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

RouteProcessorSelection::RouteProcessorSelection (SessionHandlePtr& s, AxisViewProvider& ap)
	: shp (s), avp (ap)
{
}

RouteProcessorSelection&
RouteProcessorSelection::operator= (const RouteProcessorSelection& other)
{
	if (&other != this) {
		(*((ProcessorSelection*) this)) = (*((ProcessorSelection const *) &other));
		axes = other.axes;
	}
	return *this;
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
RouteProcessorSelection::add (AxisView* r)
{
	if (!shp.session()) {
		return;
	}

	if (axes.insert (r).second) {

		shp.session()->selection().add (r->stripable(), boost::shared_ptr<AutomationControl>());

		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);

		if (ms) {
			ms->CatchDeletion.connect (*this, invalidator (*this), boost::bind (&RouteProcessorSelection::remove, this, _1), gui_context());
		}
	}
}

void
RouteProcessorSelection::remove (AxisView* r)
{
	if (!shp.session()) {
		return;
	}
	ENSURE_GUI_THREAD (*this, &RouteProcessorSelection::remove, r);
	shp.session()->selection().remove (r->stripable(), boost::shared_ptr<AutomationControl>());
}

void
RouteProcessorSelection::set (AxisView* r)
{
	if (!shp.session()) {
		return;
	}
	shp.session()->selection().set (r->stripable(), boost::shared_ptr<AutomationControl>());
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
