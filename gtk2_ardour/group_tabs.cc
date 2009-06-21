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
#include "route_group_dialog.h"
#include "group_tabs.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

GroupTabs::GroupTabs ()
	: _session (0),
	  _menu (0),
	  _dragging (0)
{

}

void
GroupTabs::set_session (Session* s)
{
	_session = s;
	s->RouteGroupChanged.connect (mem_fun (*this, &GroupTabs::set_dirty));
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

	Tab* prev;
	Tab* next;
	Tab* t = click_to_tab (p, &prev, &next);
	if (t == 0) {
		return false;
	}

	if (ev->button == 1) {

		_dragging = t;
		_drag_moved = false;
		_drag_last = p;

		double const h = (t->from + t->to) / 2;
		_drag_from = p < h;

		if (_drag_from) {
			_drag_limit = prev ? prev->to : 0;
		} else {
			_drag_limit = next ? next->from : extent ();
		}

	} else if (ev->button == 3) {

		if (!_menu) {
			_menu = new Menu;
			MenuList& items = _menu->items ();
			items.push_back (MenuElem (_("Edit..."), bind (mem_fun (*this, &GroupTabs::edit_group), t->group)));
			items.push_back (MenuElem (_("Remove"), bind (mem_fun (*this, &GroupTabs::remove_group), t->group)));
		}

		_menu->popup (ev->button, ev->time);

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
	
	if (p != _drag_last) {
		_drag_moved = true;
	}

	if (_drag_from) {
		double f = _dragging->from + p - _drag_last;
		if (f < _drag_limit) {
			f = _drag_limit;
		}
		_dragging->from = f;
	} else {
		double t = _dragging->to + p - _drag_last;
		if (t > _drag_limit) {
			t = _drag_limit;
		}
		_dragging->to = t;
	}

	set_dirty ();
	queue_draw ();

	_drag_last = p;
	
	return true;
}


bool
GroupTabs::on_button_release_event (GdkEventButton* ev)
{
	if (_dragging == 0) {
		return false;
	}

	if (!_drag_moved) {
		_dragging->group->set_active (!_dragging->group->is_active (), this);
		_dragging = 0;
	} else {
		_dragging = 0;
		reflect_tabs (_tabs);
		set_dirty ();
		queue_draw ();
	}

	return true;
}


void
GroupTabs::edit_group (RouteGroup* g)
{
	RouteGroupDialog d (g, Gtk::Stock::APPLY);
	d.do_run ();
}

void
GroupTabs::remove_group (RouteGroup *g)
{
	_session->remove_route_group (*g);
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


GroupTabs::Tab *
GroupTabs::click_to_tab (double c, Tab** prev, Tab** next)
{
	list<Tab>::iterator i = _tabs.begin ();
	while (i != _tabs.end() && (c < i->from || c > i->to)) {
		*prev = &(*i);
		++i;
	}

	if (i == _tabs.end()) {
		*next = 0;
		return 0;
	}

	list<Tab>::iterator j = i;
	++j;
	if (j == _tabs.end()) {
		*next = 0;
	} else {
		*next = &(*j);
	}

	return &(*i);
}
