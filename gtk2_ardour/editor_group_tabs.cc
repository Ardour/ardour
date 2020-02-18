/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#include "gtkmm2ext/utils.h"

#include "ardour/route_group.h"

#include "gtkmm2ext/colors.h"

#include "editor.h"
#include "editor_group_tabs.h"
#include "editor_route_groups.h"
#include "editor_routes.h"
#include "rgb_macros.h"
#include "route_time_axis.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;

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
EditorGroupTabs::draw_tab (cairo_t* cr, Tab const & tab)
{
	double const arc_radius = get_width();
	double r, g, b, a;

	if (tab.group && tab.group->is_active()) {
		Gtkmm2ext::color_to_rgba (tab.color, r, g, b, a);
	} else {
		Gtkmm2ext::color_to_rgba (UIConfiguration::instance().color ("inactive group tab"), r, g, b, a);
	}

	a = 1.0;

	cairo_set_source_rgba (cr, r, g, b, a);
	cairo_move_to (cr, 0, tab.from + arc_radius);
	cairo_arc (cr, get_width(), tab.from + arc_radius, arc_radius, M_PI, 3 * M_PI / 2);
	cairo_line_to (cr, get_width(), tab.to);
	cairo_arc (cr, get_width(), tab.to - arc_radius, arc_radius, M_PI / 2, M_PI);
	cairo_line_to (cr, 0, tab.from + arc_radius);
	cairo_fill (cr);

	if (tab.group && (tab.to - tab.from) > arc_radius) {
		int text_width, text_height;

		Glib::RefPtr<Pango::Layout> layout;
		layout = Pango::Layout::create (get_pango_context ());
		layout->set_ellipsize (Pango::ELLIPSIZE_MIDDLE);

		layout->set_text (tab.group->name ());
		layout->set_width ((tab.to - tab.from - arc_radius) * PANGO_SCALE);
		layout->get_pixel_size (text_width, text_height);

		cairo_move_to (cr, (get_width() - text_height) * .5, (text_width + tab.to + tab.from) * .5);

		Gtkmm2ext::Color c = Gtkmm2ext::contrasting_text_color (Gtkmm2ext::rgba_to_color (r, g, b, a));
		Gtkmm2ext::color_to_rgba (c, r, g, b, a);
		cairo_set_source_rgb (cr, r, g, b);

		cairo_save (cr);
		cairo_rotate (cr, M_PI * -.5);
		pango_cairo_show_layout (cr, layout->gobj ());
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

