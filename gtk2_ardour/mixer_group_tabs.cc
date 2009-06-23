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
#include "i18n.h"
#include "route_group_dialog.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

MixerGroupTabs::MixerGroupTabs (Mixer_UI* m)
	: _mixer (m),
	  _menu (0)
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

		if (s->route()->is_master() || s->route()->is_control() || !s->marked_for_display()) {
			continue;
		}

		RouteGroup* g = s->route_group ();

		if (g != tab.group) {
			if (tab.group) {
				tab.to = x;
				tab.last_ui_size = s->get_width ();
				tabs.push_back (tab);
			}

			tab.from = x;
			tab.group = g;
			tab.colour = s->color ();
			tab.first_ui_size = s->get_width ();
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
	TreeModel::Children rows = _mixer->track_model->children ();
	for (TreeModel::Children::iterator i = rows.begin(); i != rows.end(); ++i) {

		MixerStrip* s = (*i)[_mixer->track_columns.strip];
		
		if (s->route()->is_master() || s->route()->is_control() || !s->marked_for_display()) {
			continue;
		}
		
		if (j == tabs.end()) {
			
			/* already run out of tabs, so no edit group */
			s->route()->set_route_group (0, this);
			
		} else {
			
			if (x >= j->to) {
				/* this tab finishes before this track starts, so onto the next tab */
				++j;
			}
			
			double const h = x + s->get_width() / 2;
			
			if (j->from < h && j->to > h) {
				s->route()->set_route_group (j->group, this);
			} else {
				s->route()->set_route_group (0, this);
			}
			
		}

		x += s->get_width ();
	}
}

Gtk::Menu*
MixerGroupTabs::get_menu (RouteGroup* g)
{
	if (g == 0) {
		return 0;
	}
	
	using namespace Menu_Helpers;
	
	delete _menu;
	_menu = new Menu;
	
	MenuList& items = _menu->items ();
	items.push_back (MenuElem (_("Edit..."), bind (mem_fun (*this, &MixerGroupTabs::edit_group), g)));
	items.push_back (MenuElem (_("Remove"), bind (mem_fun (*this, &MixerGroupTabs::remove_group), g)));

	return _menu;
}

void
MixerGroupTabs::edit_group (RouteGroup* g)
{
	RouteGroupDialog d (g, Gtk::Stock::APPLY);
	d.do_run ();
}

void
MixerGroupTabs::remove_group (RouteGroup *g)
{
	_session->remove_route_group (*g);
}
