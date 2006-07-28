/*
    Copyright (C) 2003 Paul Davis 

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

#include <ardour/curve.h>
#include <ardour/route.h>

#include "gain_automation_time_axis.h"
#include "automation_line.h"
#include "canvas.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

GainAutomationTimeAxisView::GainAutomationTimeAxisView (Session& s, boost::shared_ptr<Route> r, 
							PublicEditor& e, TimeAxisView& parent, 
							ArdourCanvas::Canvas& canvas, const string & n, ARDOUR::Curve& c)

	: AxisView (s),
	  AutomationTimeAxisView (s, r, e, parent, canvas, n, X_("gain"), ""),
	  curve (c)
	
{
}

GainAutomationTimeAxisView::~GainAutomationTimeAxisView ()
{
}

void
GainAutomationTimeAxisView::add_automation_event (ArdourCanvas::Item* item, GdkEvent* event, jack_nframes_t when, double y)
{
	double x = 0;

	canvas_display->w2i (x, y);

	/* compute vertical fractional position */

	y = 1.0 - (y / height);

	/* map using line */

	lines.front()->view_to_model_y (y);

	_session.begin_reversible_command (_("add gain automation event"));

	_session.add_undo (curve.get_memento());
	curve.add (when, y);
	_session.add_redo_no_execute (curve.get_memento());
	_session.commit_reversible_command ();
	_session.set_dirty ();
}

void
GainAutomationTimeAxisView::set_automation_state (AutoState state)
{
	if (!ignore_state_request) {
		route->set_gain_automation_state (state);
	}
}
