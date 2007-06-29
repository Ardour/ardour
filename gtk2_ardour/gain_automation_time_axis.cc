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

*/

#include <ardour/automation_event.h>
#include <ardour/route.h>
#include <pbd/memento_command.h>

#include "gain_automation_time_axis.h"
#include "automation_line.h"
#include "canvas.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

GainAutomationTimeAxisView::GainAutomationTimeAxisView (Session& s, boost::shared_ptr<Route> r, 
							PublicEditor& e, TimeAxisView& parent, 
							ArdourCanvas::Canvas& canvas, const string & n,
							boost::shared_ptr<ARDOUR::AutomationControl> c)

	: AxisView (s),
	  AutomationTimeAxisView (s, r, e, parent, canvas, n, X_("gain"), ""),
	  _control (c)
{
}

GainAutomationTimeAxisView::~GainAutomationTimeAxisView ()
{
}

void
GainAutomationTimeAxisView::add_automation_event (ArdourCanvas::Item* item, GdkEvent* event, nframes_t when, double y)
{
	double x = 0;

	canvas_display->w2i (x, y);

	/* compute vertical fractional position */

	y = 1.0 - (y / height);

	/* map using line */

	lines.front().first->view_to_model_y (y);

	_session.begin_reversible_command (_("add gain automation event"));
	XMLNode& before = _control->list()->get_state();
	_control->list()->add (when, y);
	XMLNode& after = _control->list()->get_state();
	_session.commit_reversible_command (new MementoCommand<ARDOUR::AutomationList>(*_control->list(), &before, &after));
	_session.set_dirty ();
}

