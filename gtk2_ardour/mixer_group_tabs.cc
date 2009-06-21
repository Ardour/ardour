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


void
MixerGroupTabs::render (cairo_t* cr)
{
	/* background */
	
	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	int32_t curr_start = 0;
	RouteGroup* curr_group = 0;
	Gdk::Color curr_colour;

	int32_t x = 0;
	for (list<MixerStrip*>::iterator i = _mixer->strips.begin(); i != _mixer->strips.end(); ++i) {

		if ((*i)->route()->is_master() || (*i)->route()->is_control() || !(*i)->marked_for_display()) {
			continue;
		}

		RouteGroup* g = (*i)->route_group ();

		if (g != curr_group) {
			if (curr_group) {
				draw_group (cr, curr_start, x, curr_group, curr_colour);
			}

			curr_start = x;
			curr_group = g;
			curr_colour = (*i)->color ();
		}

		x += (*i)->get_width ();
	}

	if (curr_group) {
		draw_group (cr, curr_start, x, curr_group, curr_colour);
	}
}

void
MixerGroupTabs::draw_group (cairo_t* cr, int32_t x1, int32_t x2, RouteGroup* g, Gdk::Color const & colour)
{
	double const arc_radius = _height;

	if (g->is_active()) {
		cairo_set_source_rgba (cr, colour.get_red_p (), colour.get_green_p (), colour.get_blue_p (), 1);
	} else {
		cairo_set_source_rgba (cr, 1, 1, 1, 0.2);
	}
	
	cairo_arc (cr, x1 + arc_radius, _height, arc_radius, M_PI, 3 * M_PI / 2);
	cairo_line_to (cr, x2 - arc_radius, 0);
	cairo_arc (cr, x2 - arc_radius, _height, arc_radius, 3 * M_PI / 2, 2 * M_PI);
	cairo_line_to (cr, x1, _height);
	cairo_fill (cr);

	pair<string, double> const f = fit_to_pixels (cr, g->name(), x2 - x1 - arc_radius * 2);

	cairo_text_extents_t ext;
	cairo_text_extents (cr, g->name().c_str(), &ext);

	cairo_set_source_rgb (cr, 1, 1, 1);
	cairo_move_to (cr, x1 + (x2 - x1 - f.second) / 2, _height - ext.height / 2);
	cairo_save (cr);
	cairo_show_text (cr, f.first.c_str());
	cairo_restore (cr);
}

RouteGroup*
MixerGroupTabs::click_to_route_group (GdkEventButton* ev)
{
	int32_t x = 0;
	list<MixerStrip*>::iterator i = _mixer->strips.begin();
	while (x < ev->x && i != _mixer->strips.end()) {

		if (!(*i)->route()->is_master() && !(*i)->route()->is_control() && (*i)->marked_for_display()) {
			x += (*i)->get_width ();
		}
		
		if (x < ev->x) {
			++i;
		}
	}

	if (i == _mixer->strips.end()) {
		return 0;
	}
	
	return (*i)->route_group ();
}
