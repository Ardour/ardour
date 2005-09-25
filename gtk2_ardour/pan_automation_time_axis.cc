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
#include <ardour/panner.h>

#include <gtkmmext/popup.h>

#include "pan_automation_time_axis.h"
#include "automation_line.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace Gtk;

PanAutomationTimeAxisView::PanAutomationTimeAxisView (Session& s, Route& r, PublicEditor& e, TimeAxisView& parent, Widget* p, std::string n)

	: AxisView (s),
	  AutomationTimeAxisView (s, r, e, parent, p, n, X_("pan"), "")
{
}

PanAutomationTimeAxisView::~PanAutomationTimeAxisView ()
{
}

void
PanAutomationTimeAxisView::add_automation_event (GtkCanvasItem* item, GdkEvent* event, jack_nframes_t when, double y)
{
	if (lines.empty()) {
		/* no data, possibly caused by no outputs/inputs */
		return;
	}

	if (lines.size() > 1) {

		Gtkmmext::PopUp* msg = new Gtkmmext::PopUp (GTK_WIN_POS_MOUSE, 5000, true);
		
		msg->set_text (_("You can't graphically edit panning of more than stream"));
		msg->touch ();
		
		return;
	}

	double x = 0;

	gtk_canvas_item_w2i (canvas_display, &x, &y);

	/* compute vertical fractional position */

	y = 1.0 - (y / height);

	/* map using line */

	lines.front()->view_to_model_y (y);

	AutomationList& alist (lines.front()->the_list());

	_session.begin_reversible_command (_("add pan automation event"));
	_session.add_undo (alist.get_memento());
	alist.add (when, y);
	_session.add_undo (alist.get_memento());
	_session.commit_reversible_command ();
	_session.set_dirty ();
}

void
PanAutomationTimeAxisView::set_automation_state (AutoState state)
{
	if (!ignore_state_request) {
		route.panner().set_automation_state (state);
	}
}
