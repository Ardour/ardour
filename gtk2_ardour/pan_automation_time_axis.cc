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

#include <ardour/curve.h>
#include <ardour/route.h>
#include <ardour/panner.h>

#include <gtkmm2ext/popup.h>
#include <pbd/memento_command.h>

#include "pan_automation_time_axis.h"
#include "automation_line.h"
#include "canvas_impl.h"
#include "route_ui.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

PanAutomationTimeAxisView::PanAutomationTimeAxisView (Session& s, boost::shared_ptr<Route> r, PublicEditor& e, 
						      TimeAxisView& parent, Canvas& canvas, std::string n)

	: AxisView (s),
	  AutomationTimeAxisView (s, r, e, parent, canvas, n, X_("pan"), "")
{
	multiline_selector.set_name ("PanAutomationLineSelector");
	
	controls_table.attach (multiline_selector, 1, 5, 1, 2, Gtk::EXPAND, Gtk::EXPAND);
}

PanAutomationTimeAxisView::~PanAutomationTimeAxisView ()
{
}

void
PanAutomationTimeAxisView::add_automation_event (ArdourCanvas::Item* item, GdkEvent* event, nframes_t when, double y)
{
	if (lines.empty()) {
		/* no data, possibly caused by no outputs/inputs */
		Gtkmm2ext::PopUp* msg = new Gtkmm2ext::PopUp (Gtk::WIN_POS_MOUSE, 5000, true);

		msg->set_text (_("Pan automation track has no lines, unable to add point\n(is track pannable?)"));
		msg->touch ();

		return;
	}

	int line_index = 0;

	if (lines.size() > 1) {
		line_index = multiline_selector.get_active_row_number();

		if (line_index < 0 || line_index >= (int)lines.size()) {
			Gtkmm2ext::PopUp* msg = new Gtkmm2ext::PopUp (Gtk::WIN_POS_MOUSE, 5000, true);
		
			msg->set_text (_("You need to select which line to edit"));
			msg->touch ();

			return;
		}
	}

	double x = 0;

	canvas_display->w2i (x, y);

	/* compute vertical fractional position */

	y = 1.0 - (y / height);

	/* map using line */

	lines.front().first->view_to_model_y (y);

	boost::shared_ptr<AutomationList> alist (lines[line_index].first->the_list());

	_session.begin_reversible_command (_("add pan automation event"));
	XMLNode &before = alist->get_state();
	alist->add (when, y);
	XMLNode &after = alist->get_state();
	_session.add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	_session.commit_reversible_command ();
	_session.set_dirty ();
}

void
PanAutomationTimeAxisView::clear_lines ()
{
	AutomationTimeAxisView::clear_lines();
	multiline_selector.clear();
}

void
PanAutomationTimeAxisView::add_line (AutomationLine& line)
{
	char buf[32];
	snprintf(buf,32,"Line %zu",lines.size()+1);
	multiline_selector.append_text(buf);

	if (lines.empty()) {
		multiline_selector.set_active(0);
	}

	if (lines.size() + 1 > 1 && (height_style != Small && height_style != Smaller)) {
		multiline_selector.show();
	} else {
		multiline_selector.hide();

	}

	AutomationTimeAxisView::add_line(line);
}

void
PanAutomationTimeAxisView::set_height (TimeAxisView::TrackHeight th)
{
	AutomationTimeAxisView::set_height(th);

	switch (th) {
		case Largest:
		case Large:
		case Larger:
		case Normal:
			if (lines.size() > 1) {
				multiline_selector.show();
				break;
			} 
		default:
			multiline_selector.hide();
	}
}
