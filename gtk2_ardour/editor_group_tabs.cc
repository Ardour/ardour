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

#include "gtkmm2ext/utils.h"

#include "ardour/route_group.h"
#include "editor_group_tabs.h"
#include "editor.h"
#include "route_time_axis.h"
#include "utils.h"
#include "editor_route_groups.h"
#include "editor_routes.h"
#include "i18n.h"

using namespace std;
using namespace ARDOUR;

EditorGroupTabs::EditorGroupTabs (Editor* e)
	: EditorComponent (e)
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
	for (TrackViewList::iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {

		if ((*i)->marked_for_display() == false) {
			continue;
		}

		RouteGroup* g = (*i)->route_group ();

		if (g != tab.group) {
			if (tab.group) {
				tab.to = y;
				tabs.push_back (tab);
			}

			tab.from = y;
			tab.group = g;
			if (g) {
				tab.color = group_color (g);
			}
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
	double const arc_radius = get_width();

	if (tab.group && tab.group->is_active()) {
		cairo_set_source_rgba (cr, tab.color.get_red_p (), tab.color.get_green_p (), tab.color.get_blue_p (), 1);
	} else {
		cairo_set_source_rgba (cr, 1, 1, 1, 0.2);
	}

	cairo_move_to (cr, 0, tab.from + arc_radius);
	cairo_arc (cr, get_width(), tab.from + arc_radius, arc_radius, M_PI, 3 * M_PI / 2);
	cairo_line_to (cr, get_width(), tab.to);
	cairo_arc (cr, get_width(), tab.to - arc_radius, arc_radius, M_PI / 2, M_PI);
	cairo_line_to (cr, 0, tab.from + arc_radius);
	cairo_fill (cr);

	if (tab.group) {
		pair<string, double> const f = Gtkmm2ext::fit_to_pixels (cr, tab.group->name(), tab.to - tab.from - arc_radius * 2);

		cairo_text_extents_t ext;
		cairo_text_extents (cr, tab.group->name().c_str(), &ext);

		cairo_set_source_rgb (cr, 1, 1, 1);
		cairo_move_to (cr, get_width() - ext.height / 2, tab.from + (f.second + tab.to - tab.from) / 2);
		cairo_save (cr);
		cairo_rotate (cr, - M_PI / 2);
		cairo_show_text (cr, f.first.c_str());
		cairo_restore (cr);
	}
}

double
EditorGroupTabs::primary_coordinate (double, double y) const
{
	return y;
}

RouteList
EditorGroupTabs::routes_for_tab (Tab const * t) const
{
	RouteList routes;
	int32_t y = 0;

	for (TrackViewList::iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {

		if ((*i)->marked_for_display() == false) {
			continue;
		}

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
		if (rtv) {

			if (y >= t->to) {
				/* tab finishes before this track starts */
				break;
			}

			double const h = y + (*i)->effective_height() / 2;

			if (t->from < h && t->to > h) {
				routes.push_back (rtv->route ());
			}
		}

		y += (*i)->effective_height ();
	}

	return routes;
}


void
EditorGroupTabs::add_menu_items (Gtk::Menu* m, RouteGroup* g)
{
	using namespace Gtk::Menu_Helpers;

	if (g) {
		MenuList& items = m->items ();
		items.push_back (MenuElem (_("Fit to Window"), sigc::bind (sigc::mem_fun (*_editor, &Editor::fit_route_group), g)));
	}
}

PBD::PropertyList
EditorGroupTabs::default_properties () const
{
	PBD::PropertyList plist;

	plist.add (Properties::active, true);
	plist.add (Properties::mute, true);
	plist.add (Properties::solo, true);
	plist.add (Properties::recenable, true);

	return plist;
}

RouteSortOrderKey
EditorGroupTabs::order_key () const
{
	return EditorSort;
}

RouteList
EditorGroupTabs::selected_routes () const
{
	RouteList rl;

	for (TrackSelection::iterator i = _editor->get_selection().tracks.begin(); i != _editor->get_selection().tracks.end(); ++i) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
		if (rtv) {
			rl.push_back (rtv->route());
		}
	}

	return rl;
}

void
EditorGroupTabs::sync_order_keys ()
{
	_editor->_routes->sync_order_keys_from_treeview ();
}
