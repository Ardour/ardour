/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <boost/foreach.hpp>

#include "gtkmm2ext/utils.h"

#include "ardour/route_group.h"

#include "gtkmm2ext/colors.h"

#include "mixer_group_tabs.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "rgb_macros.h"
#include "route_group_dialog.h"
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;

MixerGroupTabs::MixerGroupTabs (Mixer_UI* m)
	: _mixer (m)
{

}


list<GroupTabs::Tab>
MixerGroupTabs::compute_tabs () const
{
	list<Tab> tabs;

	Tab tab;
	tab.from = 0;
	tab.group = 0;

	int32_t x = 0;
	TreeModel::Children rows = _mixer->track_model->children ();
	for (TreeModel::Children::iterator i = rows.begin(); i != rows.end(); ++i) {

		AxisView* av = (*i)[_mixer->stripable_columns.strip];
		MixerStrip* s = dynamic_cast<MixerStrip*> (av);

		if (!s) {
			continue;
		}

		if (s->route()->is_master() || s->route()->is_monitor() || !s->marked_for_display()) {
			continue;
		}
#ifdef MIXBUS
		if (s->route()->mixbus()) {
			continue;
		}
#endif

		RouteGroup* g = s->route_group ();

		if (g != tab.group) {
			if (tab.group) {
				tab.to = x;
				tabs.push_back (tab);
			}

			tab.from = x;
			tab.group = g;

			if (g) {
				tab.color = group_color (g);
			}
		}

		int ww = 0, wh = 0;
		s->get_size_request (ww, wh); // widget may not be realized, get_width() is invalid.
		x += ww;
	}

	if (tab.group) {
		tab.to = x;
		tabs.push_back (tab);
	}

	return tabs;
}

void
MixerGroupTabs::draw_tab (cairo_t* cr, Tab const & tab)
{
	double const arc_radius = get_height();
	double r, g, b, a;

	if (tab.group && tab.group->is_active()) {
		Gtkmm2ext::color_to_rgba (tab.color, r, g, b, a);
	} else {
		Gtkmm2ext::color_to_rgba (UIConfiguration::instance().color ("inactive group tab"), r, g, b, a);
	}

	a = 1.0;

	cairo_set_source_rgba (cr, r, g, b, a);
	cairo_arc (cr, tab.from + arc_radius, get_height(), arc_radius, M_PI, 3 * M_PI / 2);
	cairo_line_to (cr, tab.to - arc_radius, 0);
	cairo_arc (cr, tab.to - arc_radius, get_height(), arc_radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to (cr, tab.from, get_height());
	cairo_fill (cr);

	if (tab.group && (tab.to - tab.from) > arc_radius) {
		int text_width, text_height;

		Glib::RefPtr<Pango::Layout> layout;
		layout = Pango::Layout::create (get_pango_context ());
		layout->set_ellipsize (Pango::ELLIPSIZE_MIDDLE);

		layout->set_text (tab.group->name ());
		layout->set_width ((tab.to - tab.from - arc_radius) * PANGO_SCALE);
		layout->get_pixel_size (text_width, text_height);

		cairo_move_to (cr, tab.from + (tab.to - tab.from - text_width) * .5, (get_height () - text_height) * .5);

		Gtkmm2ext::Color c = Gtkmm2ext::contrasting_text_color (Gtkmm2ext::rgba_to_color (r, g, b, a));
		Gtkmm2ext::color_to_rgba (c, r, g, b, a);
		cairo_set_source_rgb (cr, r, g, b);

		pango_cairo_show_layout (cr, layout->gobj ());
	}
}

double
MixerGroupTabs::primary_coordinate (double x, double) const
{
	return x;
}

RouteList
MixerGroupTabs::routes_for_tab (Tab const * t) const
{
	RouteList routes;
	int32_t x = 0;

	TreeModel::Children rows = _mixer->track_model->children ();
	for (TreeModel::Children::iterator i = rows.begin(); i != rows.end(); ++i) {

		AxisView* av = (*i)[_mixer->stripable_columns.strip];
		MixerStrip* s = dynamic_cast<MixerStrip*> (av);

		if (!s) {
			continue;
		}

		if (s->route()->is_master() || s->route()->is_monitor() || !s->marked_for_display()) {
			continue;
		}

		if (x >= t->to) {
			/* tab finishes before this track starts */
			break;
		}

		double const h = x + s->get_width() / 2;

		if (t->from < h && t->to > h) {
			routes.push_back (s->route ());
		}

		x += s->get_width ();
	}

	return routes;
}

RouteList
MixerGroupTabs::selected_routes () const
{
	RouteList rl;
	BOOST_FOREACH (AxisView* r, _mixer->selection().axes) {
		boost::shared_ptr<Route> rp = boost::dynamic_pointer_cast<Route> (r->stripable());
		if (rp) {
			rl.push_back (rp);
		}
	}
	return rl;
}

