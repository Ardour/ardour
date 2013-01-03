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

#include "gui_thread.h"
#include "route_group_dialog.h"
#include "group_tabs.h"
#include "keyboard.h"
#include "i18n.h"
#include "ardour_ui.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
using Gtkmm2ext::Keyboard;

list<Gdk::Color> GroupTabs::_used_colors;

GroupTabs::GroupTabs ()
	: _menu (0)
	, _dragging (0)
	, _dragging_new_tab (0)
{

}

GroupTabs::~GroupTabs ()
{
	delete _menu;
}

void
GroupTabs::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);

	if (_session) {
		_session->RouteGroupPropertyChanged.connect (
			_session_connections, invalidator (*this), boost::bind (&GroupTabs::route_group_property_changed, this, _1), gui_context()
			);
		_session->RouteAddedToRouteGroup.connect (
			_session_connections, invalidator (*this), boost::bind (&GroupTabs::route_added_to_route_group, this, _1, _2), gui_context()
			);
		_session->RouteRemovedFromRouteGroup.connect (
			_session_connections, invalidator (*this), boost::bind (&GroupTabs::route_removed_from_route_group, this, _1, _2), gui_context()
			);
		
		_session->route_group_removed.connect (_session_connections, invalidator (*this), boost::bind (&GroupTabs::set_dirty, this), gui_context());
	}
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
			_initial_dragging_routes = routes_for_tab (t);
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
		
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier) && g) {
			/* edit */
			RouteGroupDialog d (g, false);
			d.do_run ();
		} else {
			Menu* m = get_menu (g);
			if (m) {
				m->popup (ev->button, ev->time);
			}
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
GroupTabs::on_button_release_event (GdkEventButton*)
{
	if (_dragging == 0) {
		return false;
	}
	
	if (!_drag_moved) {
		
		if (_dragging->group) {
			/* toggle active state */
			_dragging->group->set_active (!_dragging->group->is_active (), this);
		}
		
	} else {
		/* finish drag */
		RouteList routes = routes_for_tab (_dragging);
		
		if (!routes.empty()) {
			if (_dragging_new_tab) {
				RouteGroup* g = create_and_add_group ();
				if (g) {
					for (RouteList::iterator i = routes.begin(); i != routes.end(); ++i) {
						g->add (*i);
					}
				}
			} else {
				boost::shared_ptr<RouteList> r = _session->get_routes ();
				for (RouteList::iterator i = r->begin(); i != r->end(); ++i) {
					
					bool const was_in_tab = find (
						_initial_dragging_routes.begin(), _initial_dragging_routes.end(), *i
						) != _initial_dragging_routes.end ();
					
					bool const now_in_tab = find (routes.begin(), routes.end(), *i) != routes.end();
					
					if (was_in_tab && !now_in_tab) {
						_dragging->group->remove (*i);
					} else if (!was_in_tab && now_in_tab) {
						_dragging->group->add (*i);
					}
				}
			}
		}
		
		set_dirty ();
		queue_draw ();
	}
	
	_dragging = 0;
	_initial_dragging_routes.clear ();

	return true;
}

void
GroupTabs::render (cairo_t* cr)
{
	if (_dragging == 0) {
		_tabs = compute_tabs ();
	}

	/* background */

	Gdk::Color c = get_style()->get_base (Gtk::STATE_NORMAL);

	cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
	cairo_rectangle (cr, 0, 0, get_width(), get_height());
	cairo_fill (cr);
	
	/* tabs */

	for (list<Tab>::const_iterator i = _tabs.begin(); i != _tabs.end(); ++i) {
		draw_tab (cr, *i);
	}
}


/** Convert a click position to a tab.
 *  @param c Click position.
 *  @param prev Filled in with the previous tab to the click, or _tabs.end().
 *  @param next Filled in with the next tab after the click, or _tabs.end().
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
			*next = i;
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

	return under;
}

Gtk::Menu*
GroupTabs::get_menu (RouteGroup* g)
{
	using namespace Menu_Helpers;

	delete _menu;

	Menu* new_from = new Menu;
	MenuList& f = new_from->items ();
	f.push_back (MenuElem (_("Selection..."), sigc::mem_fun (*this, &GroupTabs::new_from_selection)));
	f.push_back (MenuElem (_("Record Enabled..."), sigc::mem_fun (*this, &GroupTabs::new_from_rec_enabled)));
	f.push_back (MenuElem (_("Soloed..."), sigc::mem_fun (*this, &GroupTabs::new_from_soloed)));

	_menu = new Menu;
	_menu->set_name ("ArdourContextMenu");
	MenuList& items = _menu->items();

	items.push_back (MenuElem (_("Create New Group ..."), hide_return (sigc::mem_fun(*this, &GroupTabs::create_and_add_group))));
	items.push_back (MenuElem (_("Create New Group From"), *new_from));

	if (g) {
		items.push_back (MenuElem (_("Edit Group..."), sigc::bind (sigc::mem_fun (*this, &GroupTabs::edit_group), g)));
		items.push_back (MenuElem (_("Collect Group"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::collect), g)));
		items.push_back (MenuElem (_("Remove Group"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::remove_group), g)));
		items.push_back (SeparatorElem());
		if (g->has_subgroup()) {
			items.push_back (MenuElem (_("Remove Subgroup Bus"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::un_subgroup), g)));
		} else {
			items.push_back (MenuElem (_("Add New Subgroup Bus"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::subgroup), g, false, PreFader)));
		}
		items.push_back (MenuElem (_("Add New Aux Bus (pre-fader)"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::subgroup), g, true, PreFader)));
		items.push_back (MenuElem (_("Add New Aux Bus (post-fader)"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::subgroup), g, true, PostFader)));
	}

	add_menu_items (_menu, g);

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Enable All Groups"), sigc::mem_fun(*this, &GroupTabs::activate_all)));
	items.push_back (MenuElem (_("Disable All Groups"), sigc::mem_fun(*this, &GroupTabs::disable_all)));

	return _menu;

}

void
GroupTabs::new_from_selection ()
{
	RouteList rl = selected_routes ();
	if (rl.empty()) {
		return;
	}

	run_new_group_dialog (rl);
}

void
GroupTabs::new_from_rec_enabled ()
{
	boost::shared_ptr<RouteList> rl = _session->get_routes ();

	RouteList rec_enabled;

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if ((*i)->record_enabled()) {
			rec_enabled.push_back (*i);
		}
	}

	if (rec_enabled.empty()) {
		return;
	}

	run_new_group_dialog (rec_enabled);
}

void
GroupTabs::new_from_soloed ()
{
	boost::shared_ptr<RouteList> rl = _session->get_routes ();

	RouteList soloed;

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		if (!(*i)->is_master() && (*i)->soloed()) {
			soloed.push_back (*i);
		}
	}

	if (soloed.empty()) {
		return;
	}

	run_new_group_dialog (soloed);
}

void
GroupTabs::run_new_group_dialog (RouteList const & rl)
{
	RouteGroup* g = new RouteGroup (*_session, "");
	g->apply_changes (default_properties ());

	RouteGroupDialog d (g, true);

	if (d.do_run ()) {
		delete g;
	} else {
		_session->add_route_group (g);
		for (RouteList::const_iterator i = rl.begin(); i != rl.end(); ++i) {
			g->add (*i);
		}
	}
}

RouteGroup *
GroupTabs::create_and_add_group () const
{
	RouteGroup* g = new RouteGroup (*_session, "");

	g->apply_changes (default_properties ());

	RouteGroupDialog d (g, true);

	if (d.do_run ()) {
		delete g;
		return 0;
	}

	_session->add_route_group (g);
	return g;
}

void
GroupTabs::edit_group (RouteGroup* g)
{
	RouteGroupDialog d (g, false);
	d.do_run ();
}

void
GroupTabs::subgroup (RouteGroup* g, bool aux, Placement placement)
{
	g->make_subgroup (aux, placement);
}

void
GroupTabs::un_subgroup (RouteGroup* g)
{
	g->destroy_subgroup ();
}

struct CollectSorter {
	CollectSorter (RouteSortOrderKey key) : _key (key) {}

	bool operator () (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		return a->order_key (_key) < b->order_key (_key);
	}

        RouteSortOrderKey _key;
};

struct OrderSorter {
	OrderSorter (RouteSortOrderKey key) : _key (key) {}
	
	bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
		return a->order_key (_key) < b->order_key (_key);
	}

	RouteSortOrderKey _key;
};

/** Collect all members of a RouteGroup so that they are together in the Editor or Mixer.
 *  @param g Group to collect.
 */
void
GroupTabs::collect (RouteGroup* g)
{
	boost::shared_ptr<RouteList> group_routes = g->route_list ();
	group_routes->sort (CollectSorter (order_key ()));
	int const N = group_routes->size ();

	RouteList::iterator i = group_routes->begin ();
	boost::shared_ptr<RouteList> routes = _session->get_routes ();
	routes->sort (OrderSorter (order_key ()));
	RouteList::const_iterator j = routes->begin ();

	int diff = 0;
	int coll = -1;
	while (i != group_routes->end() && j != routes->end()) {

		int const k = (*j)->order_key (order_key ());

		if (*i == *j) {

			if (coll == -1) {
				coll = k;
				diff = N - 1;
			} else {
				--diff;
			}

			(*j)->set_order_key (order_key (), coll);

			++coll;
			++i;

		} else {

			(*j)->set_order_key (order_key (), k + diff);

		}

		++j;
	}

	sync_order_keys ();
}

void
GroupTabs::activate_all ()
{
	_session->foreach_route_group (
		sigc::bind (sigc::mem_fun (*this, &GroupTabs::set_activation), true)
		);
}

void
GroupTabs::disable_all ()
{
	_session->foreach_route_group (
		sigc::bind (sigc::mem_fun (*this, &GroupTabs::set_activation), false)
		);
}

void
GroupTabs::set_activation (RouteGroup* g, bool a)
{
	g->set_active (a, this);
}

void
GroupTabs::remove_group (RouteGroup* g)
{
	_session->remove_route_group (*g);
}

/** Set the color of the tab of a route group */
void
GroupTabs::set_group_color (RouteGroup* group, Gdk::Color color)
{
	assert (group);

	/* Hack to disallow black route groups; force a dark grey instead */
	if (color.get_red() == 0 && color.get_green() == 0 && color.get_blue() == 0) {
		color.set_grey_p (0.1);
	}
	
	GUIObjectState& gui_state = *ARDOUR_UI::instance()->gui_object_state;

	char buf[64];
	snprintf (buf, sizeof (buf), "%d:%d:%d", color.get_red(), color.get_green(), color.get_blue());
	gui_state.set_property (group_gui_id (group), "color", buf);
	
	/* the group color change notification */
	
	PBD::PropertyChange change;
	change.add (Properties::color);
	group->PropertyChanged (change);

	/* This is a bit of a hack, but this might change
	   our route's effective color, so emit gui_changed
	   for our routes.
	*/

	emit_gui_changed_for_members (group);
}

/** @return the ID string to use for the GUI state of a route group */
string
GroupTabs::group_gui_id (RouteGroup* group)
{
	assert (group);

	char buf[64];
	snprintf (buf, sizeof (buf), "route_group %s", group->id().to_s().c_str ());

	return buf;
}

/** @return the color to use for a route group tab */
Gdk::Color
GroupTabs::group_color (RouteGroup* group)
{
	assert (group);
	
	GUIObjectState& gui_state = *ARDOUR_UI::instance()->gui_object_state;

	string const gui_id = group_gui_id (group);

	bool empty;
	string const color = gui_state.get_string (gui_id, "color", &empty);
	if (empty) {
		/* no color has yet been set, so use a random one */
		Gdk::Color const color = unique_random_color (_used_colors);
		set_group_color (group, color);
		return color;
	}

	Gdk::Color c;

	int r, g, b;

	sscanf (color.c_str(), "%d:%d:%d", &r, &g, &b);

	c.set_red (r);
	c.set_green (g);
	c.set_blue (b);
	
	return c;
}

void
GroupTabs::route_group_property_changed (RouteGroup* rg)
{
	/* This is a bit of a hack, but this might change
	   our route's effective color, so emit gui_changed
	   for our routes.
	*/

	emit_gui_changed_for_members (rg);
	
	set_dirty ();
}

void
GroupTabs::route_added_to_route_group (RouteGroup*, boost::weak_ptr<Route> w)
{
	/* Similarly-spirited hack as in route_group_property_changed */

	boost::shared_ptr<Route> r = w.lock ();
	if (!r) {
		return;
	}

	r->gui_changed (X_("color"), 0);

	set_dirty ();
}

void
GroupTabs::route_removed_from_route_group (RouteGroup*, boost::weak_ptr<Route> w)
{
	/* Similarly-spirited hack as in route_group_property_changed */

	boost::shared_ptr<Route> r = w.lock ();
	if (!r) {
		return;
	}

	r->gui_changed (X_("color"), 0);

	set_dirty ();
}

void
GroupTabs::emit_gui_changed_for_members (RouteGroup* rg)
{
	for (RouteList::iterator i = rg->route_list()->begin(); i != rg->route_list()->end(); ++i) {
		(*i)->gui_changed (X_("color"), 0);
	}
}
