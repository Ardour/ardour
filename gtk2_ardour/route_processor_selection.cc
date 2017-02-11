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

#include "gui_thread.h"
#include "mixer_strip.h"
#include "route_processor_selection.h"
#include "route_ui.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;


RouteProcessorSelection::RouteProcessorSelection()
{
}

RouteProcessorSelection&
RouteProcessorSelection::operator= (const RouteProcessorSelection& other)
{
	if (&other != this) {
		processors = other.processors;
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
RouteProcessorSelection::clear_processors ()
{
	processors.clear ();
	ProcessorsChanged ();
}

void
RouteProcessorSelection::clear_routes ()
{
	PresentationInfo::ChangeSuspender cs;

	for (AxisViewSelection::iterator i = axes.begin(); i != axes.end(); ++i) {
		(*i)->set_selected (false);
	}
	axes.clear ();
	drop_connections ();
}

void
RouteProcessorSelection::add (XMLNode* node)
{
	// XXX check for duplicate
	processors.add (node);
	ProcessorsChanged();
}

void
RouteProcessorSelection::set (XMLNode* node)
{
	clear_processors ();
	processors.set (node);
	ProcessorsChanged ();
}

void
RouteProcessorSelection::add (AxisView* r)
{
	if (axes.insert (r).second) {

		r->set_selected (true);

		MixerStrip* ms = dynamic_cast<MixerStrip*> (r);

		if (ms) {
			ms->CatchDeletion.connect (*this, invalidator (*this), boost::bind (&RouteProcessorSelection::remove, this, _1), gui_context());
		}
	}
}

void
RouteProcessorSelection::remove (AxisView* r)
{
	ENSURE_GUI_THREAD (*this, &RouteProcessorSelection::remove, r);
	PresentationInfo::ChangeSuspender cs;

	AxisViewSelection::iterator i;
	if ((i = find (axes.begin(), axes.end(), r)) != axes.end()) {
		(*i)->set_selected (false);
		axes.erase (i);
	}
}

void
RouteProcessorSelection::set (AxisView* r)
{
	clear_routes ();
	add (r);
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
