/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#include "recorder_group_tabs.h"
#include "recorder_ui.h"
#include "track_record_axis.h"
#include "ui_config.h"

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

using namespace ARDOUR;

RecorderGroupTabs::RecorderGroupTabs (RecorderUI* parent)
	: _recorder (parent)
{
}

double
RecorderGroupTabs::primary_coordinate (double, double y) const
{
	return y;
}

double
RecorderGroupTabs::extent () const
{
	return get_height ();
}

std::list<GroupTabs::Tab>
RecorderGroupTabs::compute_tabs () const
{
	std::list<Tab> tabs;

	Tab tab;
	tab.from  = 0;
	tab.group = 0;
	int32_t y = 0;

	std::list<TrackRecordAxis*> recorders = _recorder->visible_recorders ();
	for (std::list<TrackRecordAxis*>::const_iterator i = recorders.begin (); i != recorders.end (); ++i) {
		if ((*i)->route ()->presentation_info ().hidden ()) { // marked_for_display ()
			continue;
		}

		RouteGroup* g = (*i)->route_group ();

		if (g != tab.group) {
			if (tab.group) {
				tab.to = y;
				tabs.push_back (tab);
			}

			tab.from  = y;
			tab.group = g;
			if (g) {
				tab.color = group_color (g);
			}
		}

		y += (*i)->get_height ();
	}

	if (tab.group) {
		tab.to = y;
		tabs.push_back (tab);
	}

	return tabs;
}

RouteList
RecorderGroupTabs::routes_for_tab (Tab const* t) const
{
	RouteList routes;
	int32_t   y = 0;

	std::list<TrackRecordAxis*> recorders = _recorder->visible_recorders ();
	for (std::list<TrackRecordAxis*>::const_iterator i = recorders.begin (); i != recorders.end (); ++i) {
		if (y >= t->to) {
			/* tab finishes before this track starts */
			break;
		}

		double const h = y + (*i)->get_height () / 2;
		if (t->from < h && t->to > h) {
			routes.push_back ((*i)->route ());
		}
		y += (*i)->get_height ();
	}

	return routes;
}

void
RecorderGroupTabs::draw_tab (cairo_t* cr, Tab const& tab)
{
	double const arc_radius = get_width ();
	double       r, g, b, a;

	if (tab.group && tab.group->is_active ()) {
		Gtkmm2ext::color_to_rgba (tab.color, r, g, b, a);
	} else {
		Gtkmm2ext::color_to_rgba (UIConfiguration::instance ().color ("inactive group tab"), r, g, b, a);
	}

	a = 1.0;

	cairo_set_source_rgba (cr, r, g, b, a);
	cairo_move_to (cr, 0, tab.from + arc_radius);
	cairo_arc (cr, get_width (), tab.from + arc_radius, arc_radius, M_PI, 3 * M_PI / 2);
	cairo_line_to (cr, get_width (), tab.to);
	cairo_arc (cr, get_width (), tab.to - arc_radius, arc_radius, M_PI / 2, M_PI);
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

		cairo_move_to (cr, (get_width () - text_height) * .5, (text_width + tab.to + tab.from) * .5);

		Gtkmm2ext::Color c = Gtkmm2ext::contrasting_text_color (Gtkmm2ext::rgba_to_color (r, g, b, a));
		Gtkmm2ext::color_to_rgba (c, r, g, b, a);
		cairo_set_source_rgb (cr, r, g, b);

		cairo_save (cr);
		cairo_rotate (cr, M_PI * -.5);
		pango_cairo_show_layout (cr, layout->gobj ());
		cairo_restore (cr);
	}
}

RouteList
RecorderGroupTabs::selected_routes () const
{
	RouteList rl;
	return rl;
}
