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
#include "ardour/session.h"
#include "mixer_group_tabs.h"
#include "mixer_strip.h"
#include "mixer_ui.h"
#include "utils.h"

using namespace std;
using namespace ARDOUR;

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
	for (list<MixerStrip*>::iterator i = _mixer->strips.begin(); i != _mixer->strips.end(); ++i) {

		if ((*i)->route()->is_master() || (*i)->route()->is_control() || !(*i)->marked_for_display()) {
			continue;
		}

		RouteGroup* g = (*i)->route_group ();

		if (g != tab.group) {
			if (tab.group) {
				tab.to = x;
				tabs.push_back (tab);
			}

			tab.from = x;
			tab.group = g;
			tab.colour = (*i)->color ();
		}

		x += (*i)->get_width ();
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
	double const arc_radius = _height;

	if (tab.group->is_active()) {
		cairo_set_source_rgba (cr, tab.colour.get_red_p (), tab.colour.get_green_p (), tab.colour.get_blue_p (), 1);
	} else {
		cairo_set_source_rgba (cr, 1, 1, 1, 0.2);
	}
	
	cairo_arc (cr, tab.from + arc_radius, _height, arc_radius, M_PI, 3 * M_PI / 2);
	cairo_line_to (cr, tab.to - arc_radius, 0);
	cairo_arc (cr, tab.to - arc_radius, _height, arc_radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to (cr, tab.from, _height);
	cairo_fill (cr);

	pair<string, double> const f = fit_to_pixels (cr, tab.group->name(), tab.to - tab.from - arc_radius * 2);

	cairo_text_extents_t ext;
	cairo_text_extents (cr, tab.group->name().c_str(), &ext);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_move_to (cr, tab.from + (tab.to - tab.from - f.second) / 2, _height - ext.height / 2);
	cairo_save (cr);
	cairo_show_text (cr, f.first.c_str());
	cairo_restore (cr);
}

double
MixerGroupTabs::primary_coordinate (double x, double) const
{
	return x;
}

void
MixerGroupTabs::reflect_tabs (list<Tab> const & tabs)
{
	list<Tab>::const_iterator j = tabs.begin ();
	
	int32_t x = 0;
	for (list<MixerStrip*>::iterator i = _mixer->strips.begin(); i != _mixer->strips.end(); ++i) {

		if ((*i)->route()->is_master() || (*i)->route()->is_control() || !(*i)->marked_for_display()) {
			continue;
		}
		
		if (j == tabs.end()) {
			
			/* already run out of tabs, so no edit group */
			(*i)->route()->set_route_group (0, this);
			
		} else {
			
			if (x >= j->to) {
				/* this tab finishes before this track starts, so onto the next tab */
				++j;
			}
			
			double const h = x + (*i)->get_width() / 2;
			
			if (j->from < h && j->to > h) {
				(*i)->route()->set_route_group (j->group, this);
			} else {
				(*i)->route()->set_route_group (0, this);
			}
			
		}

		x += (*i)->get_width ();
	}
}

