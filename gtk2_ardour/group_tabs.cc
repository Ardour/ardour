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

#include <gtkmm/stock.h>
#include "ardour/session.h"
#include "ardour/route_group.h"
#include "ardour/route.h"
#include "route_group_dialog.h"
#include "group_tabs.h"
#include "keyboard.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
using Gtkmm2ext::Keyboard;

GroupTabs::GroupTabs (Editor* e)
	: EditorComponent (e),
	  _dragging (0),
	  _dragging_new_tab (0)
{

}

void
GroupTabs::connect_to_session (Session* s)
{
	EditorComponent::connect_to_session (s);

	_session_connections.push_back (_session->RouteGroupChanged.connect (mem_fun (*this, &GroupTabs::set_dirty)));
}


/** Handle a size request.
 *  @param req GTK requisition
 */
void
GroupTabs::on_size_request (Gtk::Requisition *req)
{
	/* Use a dummy, small width and the actual height that we want */
	req->width = 16;
	req->height = 16;
}

bool
GroupTabs::on_button_press_event (GdkEventButton* ev)
{
	using namespace Menu_Helpers;

	double const p = primary_coordinate (ev->x, ev->y);

	list<Tab>::iterator prev;
	list<Tab>::iterator next;
	Tab* t = click_to_tab (p, &prev, &next);

	_drag_min = prev != _tabs.end() ? prev->to : 0;
	_drag_max = next != _tabs.end() ? next->from : extent ();

	if (ev->button == 1) {

		if (t == 0) {
			Tab n;
			n.from = n.to = p;
			_dragging_new_tab = true;

			if (next == _tabs.end()) {
				_tabs.push_back (n);
				t = &_tabs.back ();
			} else {
				list<Tab>::iterator j = _tabs.insert (next, n);
				t = &(*j);
			}
			
		} else {
			_dragging_new_tab = false;
		}

		_dragging = t;
		_drag_moved = false;
		_drag_first = p;

		double const h = (t->from + t->to) / 2;
		if (p < h) {
			_drag_moving = t->from;
			_drag_fixed = t->to;
			_drag_offset = p - t->from;
		} else {
			_drag_moving = t->to;
			_drag_fixed = t->from;
			_drag_offset = p - t->to;
		}

	} else if (ev->button == 3) {

		RouteGroup* g = t ? t->group : 0;
		Menu* m = get_menu (g);
		if (m) {
			m->popup (ev->button, ev->time);
		}

	}

	return true;
}


bool
GroupTabs::on_motion_notify_event (GdkEventMotion* ev)
{
	if (_dragging == 0) {
		return false;
	}

	double const p = primary_coordinate (ev->x, ev->y);

	if (p != _drag_first) {
		_drag_moved = true;
	}

	_drag_moving = p - _drag_offset;

	_dragging->from = min (_drag_moving, _drag_fixed);
	_dragging->to = max (_drag_moving, _drag_fixed);

	_dragging->from = max (_dragging->from, _drag_min);
	_dragging->to = min (_dragging->to, _drag_max);

	set_dirty ();
	queue_draw ();

	return true;
}


bool
GroupTabs::on_button_release_event (GdkEventButton* ev)
{
	if (_dragging == 0) {
		return false;
	}

	if (!_drag_moved) {

		if (_dragging->group) {
			
			if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
				
				/* edit */
				RouteGroupDialog d (_dragging->group, Gtk::Stock::APPLY);
				d.do_run ();
				
			} else {
				
				/* toggle active state */
				_dragging->group->set_active (!_dragging->group->is_active (), this);
				
			}
		}

	} else {
		/* finish drag */
		RouteList routes = routes_for_tab (_dragging);

		if (!routes.empty()) {
			if (_dragging_new_tab) {
				RouteGroup* g = new_route_group ();
				if (g) {
					for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
						(*i)->set_route_group (g, this);
					}
				}
			} else {
				boost::shared_ptr<RouteList> r = _session->get_routes ();
				for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {

					if (find (routes.begin(), routes.end(), *i) == routes.end()) {
						/* this route is not on the list of those that should be in _dragging's group */
						if ((*i)->route_group() == _dragging->group) {
							(*i)->drop_route_group (this);
						}
					} else {
						(*i)->set_route_group (_dragging->group, this);
					}
				}
			}
		}

		set_dirty ();
		queue_draw ();
	}

	_dragging = 0;

	return true;
}

void
GroupTabs::render (cairo_t* cr)
{
	if (_dragging == 0) {
		_tabs = compute_tabs ();
	}

	/* background */

	cairo_set_source_rgb (cr, 0, 0, 0);
	cairo_rectangle (cr, 0, 0, _width, _height);
	cairo_fill (cr);

	/* tabs */

	for (list<Tab>::const_iterator i = _tabs.begin(); i != _tabs.end(); ++i) {
		draw_tab (cr, *i);
	}
}


/** Convert a click position to a tab.
 *  @param c Click position.
 *  @param prev Filled in with the previous tab to the click, or 0.
 *  @param next Filled in with the next tab after the click, or 0.
 *  @return Tab under the click, or 0.
 */

GroupTabs::Tab *
GroupTabs::click_to_tab (double c, list<Tab>::iterator* prev, list<Tab>::iterator* next)
{
	*prev = *next = _tabs.end ();
	Tab* under = 0;

	list<Tab>::iterator i = _tabs.begin ();
	while (i != _tabs.end()) {

		if (i->from > c) {
			break;
		}

		if (i->to < c) {
			*prev = i;
			++i;
			continue;
		}

		if (i->from <= c && c < i->to) {
			under = &(*i);
		}

		++i;
	}

	if (i != _tabs.end()) {
		*next = i;

		if (under) {
			*next++;
		}
	}

	return under;
}

