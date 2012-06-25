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

#include <boost/foreach.hpp>

#include "gtkmm2ext/utils.h"

#include "ardour/route_group.h"
#include "mixer_group_tabs.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "utils.h"
#include "i18n.h"
#include "route_group_dialog.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
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

		MixerStrip* s = (*i)[_mixer->track_columns.strip];

		if (s->route()->is_master() || s->route()->is_monitor() || !s->marked_for_display()) {
			continue;
		}

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

		x += s->get_width ();
	}

	if (tab.group) {
		tab.to = x;
		tabs.push_back (tab);
	}

	return tabs;
}

void
MixerGroupTabs::draw_tab (cairo_t* cr, Tab const & tab) const
{
	double const arc_radius = get_height();

	if (tab.group && tab.group->is_active()) {
		cairo_set_source_rgba (cr, tab.color.get_red_p (), tab.color.get_green_p (), tab.color.get_blue_p (), 1);
	} else {
		cairo_set_source_rgba (cr, 1, 1, 1, 0.2);
	}

	cairo_arc (cr, tab.from + arc_radius, get_height(), arc_radius, M_PI, 3 * M_PI / 2);
	cairo_line_to (cr, tab.to - arc_radius, 0);
	cairo_arc (cr, tab.to - arc_radius, get_height(), arc_radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to (cr, tab.from, get_height());
	cairo_fill (cr);

	if (tab.group) {
		pair<string, double> const f = Gtkmm2ext::fit_to_pixels (cr, tab.group->name(), tab.to - tab.from - arc_radius * 2);

		cairo_text_extents_t ext;
		cairo_text_extents (cr, tab.group->name().c_str(), &ext);

		cairo_set_source_rgb (cr, 1, 1, 1);
		cairo_move_to (cr, tab.from + (tab.to - tab.from - f.second) / 2, get_height() - ext.height / 2);
		cairo_save (cr);
		cairo_show_text (cr, f.first.c_str());
		cairo_restore (cr);
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

		MixerStrip* s = (*i)[_mixer->track_columns.strip];

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

PropertyList
MixerGroupTabs::default_properties () const
{
	PropertyList plist;

	plist.add (Properties::active, true);
	plist.add (Properties::mute, true);
	plist.add (Properties::solo, true);
	plist.add (Properties::gain, true);
	plist.add (Properties::recenable, true);

	return plist;
}

RouteSortOrderKey
MixerGroupTabs::order_key () const
{
	return MixerSort;
}

RouteList
MixerGroupTabs::selected_routes () const
{
	RouteList rl;
	BOOST_FOREACH (RouteUI* r, _mixer->selection().routes) {
		boost::shared_ptr<Route> rp = r->route();
		if (rp) {
			rl.push_back (rp);
		}
	}
	return rl;
}

void
MixerGroupTabs::sync_order_keys ()
{
	_mixer->sync_order_keys (UndefinedSort);
}
