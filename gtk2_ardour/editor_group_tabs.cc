/*
    Copyright (C) 2009 Paul Davis 

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

#include "ardour/route_group.h"
#include "editor_group_tabs.h"
#include "editor.h"
#include "time_axis_view.h"
#include "utils.h"

using namespace std;
using namespace ARDOUR;

EditorGroupTabs::EditorGroupTabs (Editor* e)
	: _editor (e)
{
	
}

void
EditorGroupTabs::set_session (Session* s)
{
	s->RouteEditGroupChanged.connect (mem_fun (*this, &EditorGroupTabs::set_dirty));
}


/** Handle a size request.
 *  @param req GTK requisition
 */
void
EditorGroupTabs::on_size_request (Gtk::Requisition *req)
{
	/* Use a dummy, small height and the actual width that we want */
	req->width = 16;
	req->height = 16;
}


void
EditorGroupTabs::render (cairo_t* cr)
{
	/* background */
	
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	int32_t curr_start = 0;
	RouteGroup* curr_group = 0;
	Gdk::Color curr_colour;

	int32_t y = 0;
	for (Editor::TrackViewList::iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		RouteGroup* g = (*i)->edit_group ();

		if (g != curr_group) {
			if (curr_group) {
				draw_group (cr, curr_start, y, curr_group, curr_colour);
			}

			curr_start = y;
			curr_group = g;
			curr_colour = (*i)->color ();
		}

		y += (*i)->effective_height ();
	}

	if (curr_group) {
		draw_group (cr, curr_start, y, curr_group, curr_colour);
	}
}

void
EditorGroupTabs::draw_group (cairo_t* cr, int32_t y1, int32_t y2, RouteGroup* g, Gdk::Color const & colour)
{
	double const arc_radius = _width;

	if (g->is_active()) {
		cairo_set_source_rgba (cr, colour.get_red_p (), colour.get_green_p (), colour.get_blue_p (), 1);
	} else {
		cairo_set_source_rgba (cr, 1, 1, 1, 0.2);
	}
	
	cairo_move_to (cr, 0, y1 + arc_radius);
	cairo_arc (cr, _width, y1 + arc_radius, arc_radius, M_PI, 3 * M_PI / 2);
	cairo_line_to (cr, _width, y2);
	cairo_arc (cr, _width, y2 - arc_radius, arc_radius, M_PI / 2, M_PI);
	cairo_line_to (cr, 0, y1 + arc_radius);
	cairo_fill (cr);

	pair<string, double> const f = fit_to_pixels (cr, g->name(), y2 - y1 - arc_radius * 2);

	cairo_text_extents_t ext;
	cairo_text_extents (cr, g->name().c_str(), &ext);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_move_to (cr, _width - ext.height / 2, y1 + (f.second + y2 - y1) / 2);
	cairo_save (cr);
	cairo_rotate (cr, - M_PI / 2);
	cairo_show_text (cr, f.first.c_str());
	cairo_restore (cr);
}

bool
EditorGroupTabs::on_button_press_event (GdkEventButton* ev)
{
	int32_t y = 0;
	cout << y << "\n";
	Editor::TrackViewList::iterator i = _editor->track_views.begin();
	while (y < ev->y && i != _editor->track_views.end()) {
		y += (*i)->effective_height ();
		if (y < ev->y) {
			cout << "skip past " << (*i)->name() << "\n";
			++i;
		}
	}

	if (i == _editor->track_views.end()) {
		return false;
	}
	
	RouteGroup* g = (*i)->edit_group ();
	if (g) {
		g->set_active (!g->is_active (), this);
	}

	return true;
}
