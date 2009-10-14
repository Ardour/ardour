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
#include "keyboard.h"
#include "i18n.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;

GroupTabs::GroupTabs (Editor* e)
	: EditorComponent (e),
	  _dragging (0)
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

	Tab* prev;
	Tab* next;
	Tab* t = click_to_tab (p, &prev, &next);

	if (ev->button == 1 && t) {

		_dragging = t;
		_drag_moved = false;
		_drag_last = p;

		double const h = (t->from + t->to) / 2;
		_drag_from = p < h;

		if (_drag_from) {
			/* limit is the end of the previous tab */
			_drag_limit = prev ? prev->to : 0;
		} else {
			/* limit is the start of the next tab */
			_drag_limit = next ? next->from : extent ();
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

	if (p != _drag_last) {
		_drag_moved = true;
	}

	if (_drag_from) {

		double f = _dragging->from + p - _drag_last;

		if (f < _drag_limit) {
			/* limit drag in the `too big' direction */
			f = _drag_limit;
		}

		double const t = _dragging->to - _dragging->last_ui_size;
		if (f > t) {
			/* limit drag in the `too small' direction */
			f = t;
		}

		_dragging->from = f;

	} else {

		double t = _dragging->to + p - _drag_last;

		if (t > _drag_limit) {
			/* limit drag in the `too big' direction */
			t = _drag_limit;
		}

		double const f = _dragging->from + _dragging->first_ui_size;
		if (t < f) {
			/* limit drag in the `too small' direction */
			t = f;
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

		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {

			/* edit */
			RouteGroupDialog d (_dragging->group, Gtk::Stock::APPLY);
			d.do_run ();

		} else {

			/* toggle active state */
			_dragging->group->set_active (!_dragging->group->is_active (), this);
			_dragging = 0;

		}

	} else {
		/* finish drag */
		_dragging = 0;
		reflect_tabs (_tabs);
		set_dirty ();
		queue_draw ();
	}

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
GroupTabs::click_to_tab (double c, Tab** prev, Tab** next)
{
	*prev = 0;

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

