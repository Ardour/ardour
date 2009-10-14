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
#include "route_time_axis.h"
#include "utils.h"
#include "editor_route_groups.h"

using namespace std;
using namespace ARDOUR;

EditorGroupTabs::EditorGroupTabs (Editor* e)
	: GroupTabs (e)
{

}

list<GroupTabs::Tab>
EditorGroupTabs::compute_tabs () const
{
	list<Tab> tabs;

	Tab tab;
	tab.from = 0;
	tab.group = 0;

	int32_t y = 0;
	for (Editor::TrackViewList::iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {

		if ((*i)->marked_for_display() == false) {
			continue;
		}

		RouteGroup* g = (*i)->route_group ();

		if (g != tab.group) {
			if (tab.group) {
				tab.to = y;
				tab.last_ui_size = (*i)->effective_height ();
				tabs.push_back (tab);
			}

			tab.from = y;
			tab.group = g;
			tab.colour = (*i)->color ();
			tab.first_ui_size = (*i)->effective_height ();
		}

		y += (*i)->effective_height ();
	}

	if (tab.group) {
		tab.to = y;
		tabs.push_back (tab);
	}

	return tabs;
}

void
EditorGroupTabs::draw_tab (cairo_t* cr, Tab const & tab) const
{
	double const arc_radius = _width;

	if (tab.group->is_active()) {
		cairo_set_source_rgba (cr, tab.colour.get_red_p (), tab.colour.get_green_p (), tab.colour.get_blue_p (), 1);
	} else {
		cairo_set_source_rgba (cr, 1, 1, 1, 0.2);
	}

	cairo_move_to (cr, 0, tab.from + arc_radius);
	cairo_arc (cr, _width, tab.from + arc_radius, arc_radius, M_PI, 3 * M_PI / 2);
	cairo_line_to (cr, _width, tab.to);
	cairo_arc (cr, _width, tab.to - arc_radius, arc_radius, M_PI / 2, M_PI);
	cairo_line_to (cr, 0, tab.from + arc_radius);
	cairo_fill (cr);

	pair<string, double> const f = fit_to_pixels (cr, tab.group->name(), tab.to - tab.from - arc_radius * 2);

	cairo_text_extents_t ext;
	cairo_text_extents (cr, tab.group->name().c_str(), &ext);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_move_to (cr, _width - ext.height / 2, tab.from + (f.second + tab.to - tab.from) / 2);
	cairo_save (cr);
	cairo_rotate (cr, - M_PI / 2);
	cairo_show_text (cr, f.first.c_str());
	cairo_restore (cr);
}

double
EditorGroupTabs::primary_coordinate (double, double y) const
{
	return y;
}

void
EditorGroupTabs::reflect_tabs (list<Tab> const & tabs)
{
	list<Tab>::const_iterator j = tabs.begin ();

	int32_t y = 0;
	for (Editor::TrackViewList::iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {

		if ((*i)->marked_for_display() == false) {
			continue;
		}

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
		if (rtv) {

			if (j == tabs.end()) {

				/* already run out of tabs, so no edit group */
				rtv->route()->set_route_group (0, this);

			} else {

				if (y >= j->to) {
					/* this tab finishes before this track starts, so onto the next tab */
					++j;
				}

				double const h = y + (*i)->effective_height() / 2;

				if (j->from < h && j->to > h) {
					rtv->route()->set_route_group (j->group, this);
				} else {
					rtv->route()->set_route_group (0, this);
				}

			}
		}

		y += (*i)->effective_height ();
	}
}


Gtk::Menu*
EditorGroupTabs::get_menu (RouteGroup *g)
{
	return _editor->_route_groups->menu (g);
}
