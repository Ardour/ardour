/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#include <gtkmm/stock.h>

#include "ardour/session.h"
#include "ardour/route_group.h"
#include "ardour/route.h"
#include "ardour/vca_manager.h"
#include "ardour/vca.h"

#include "gtkmm2ext/doi.h"

#include "gui_thread.h"
#include "route_group_dialog.h"
#include "group_tabs.h"
#include "keyboard.h"
#include "pbd/i18n.h"
#include "ardour_ui.h"
#include "rgb_macros.h"
#include "ui_config.h"
#include "utils.h"

using namespace std;
using namespace Gtk;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using Gtkmm2ext::Keyboard;

list<Gdk::Color> GroupTabs::_used_colors;

GroupTabs::GroupTabs ()
	: _menu (0)
	, _dragging (0)
	, _dragging_new_tab (0)
{
	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK|Gdk::POINTER_MOTION_MASK);
	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &GroupTabs::queue_draw));
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

		_session->route_group_removed.connect (_session_connections, invalidator (*this), boost::bind (&GroupTabs::set_dirty, this, (cairo_rectangle_t*)0), gui_context());
	}
}


/** Handle a size request.
 *  @param req GTK requisition
 */
void
GroupTabs::on_size_request (Gtk::Requisition *req)
{
	req->width = std::max (16.f, rintf (16.f * UIConfiguration::instance().get_ui_scale()));
	req->height = std::max (16.f, rintf (16.f * UIConfiguration::instance().get_ui_scale()));
}

bool
GroupTabs::on_button_press_event (GdkEventButton* ev)
{
	using namespace Menu_Helpers;

	if (!get_sensitive ()) {
		return true;
	}

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

		if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier) && g) {
			remove_group (g);
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier) && g) {
			edit_group (g);
		} else {
			Menu* m = get_menu (g, true);
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

	gdk_event_request_motions(ev);

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
				run_new_group_dialog (&routes, false);
			} else {
				boost::shared_ptr<RouteList> r = _session->get_routes ();
				/* First add new ones, then remove old ones.
				 * We cannot allow the group to become temporarily empty, because
				 * Session::route_removed_from_route_group() will delete empty groups.
				 */
				for (RouteList::const_iterator i = routes.begin(); i != routes.end(); ++i) {
					/* RouteGroup::add () ignores routes already present in the set */
					_dragging->group->add (*i);
				}
				for (RouteList::const_iterator i = r->begin(); i != r->end(); ++i) {

					bool const was_in_tab = find (
						_initial_dragging_routes.begin(), _initial_dragging_routes.end(), *i
						) != _initial_dragging_routes.end ();

					bool const now_in_tab = find (routes.begin(), routes.end(), *i) != routes.end();

					if (was_in_tab && !now_in_tab) {
						_dragging->group->remove (*i);
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
GroupTabs::render (Cairo::RefPtr<Cairo::Context> const& ctx, cairo_rectangle_t*)
{
	cairo_t* cr = ctx->cobj();
	Gdk::Color c;

	if (!get_sensitive ()) {
		c = get_style()->get_base (Gtk::STATE_INSENSITIVE);
	} else {
		c = get_style()->get_base (Gtk::STATE_NORMAL);
	}

	/* background */

	cairo_set_source_rgb (cr, c.get_red_p(), c.get_green_p(), c.get_blue_p());
	cairo_rectangle (cr, 0, 0, get_width(), get_height());
	cairo_fill (cr);

	if (!get_sensitive ()) {
		return;
	}

	if (_dragging == 0) {
		_tabs = compute_tabs ();
	}

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

void
GroupTabs::add_new_from_items (Menu_Helpers::MenuList& items)
{
	using namespace Menu_Helpers;
	Menu *new_from;

	new_from = manage (new Menu);
	{
		MenuList& f = new_from->items ();
		f.push_back (MenuElem (_("Selection..."), sigc::bind (sigc::mem_fun (*this, &GroupTabs::new_from_selection), false)));
		f.push_back (MenuElem (_("Record Enabled..."), sigc::bind (sigc::mem_fun (*this, &GroupTabs::new_from_rec_enabled), false)));
		f.push_back (MenuElem (_("Soloed..."), sigc::bind (sigc::mem_fun (*this, &GroupTabs::new_from_soloed), false)));
	}
	items.push_back (MenuElem (_("Create New Group From..."), *new_from));

	new_from = manage (new Menu);
	{
		MenuList& f = new_from->items ();
		f.push_back (MenuElem (_("Selection..."), sigc::bind (sigc::mem_fun (*this, &GroupTabs::new_from_selection), true)));
		f.push_back (MenuElem (_("Record Enabled..."), sigc::bind (sigc::mem_fun (*this, &GroupTabs::new_from_rec_enabled), true)));
		f.push_back (MenuElem (_("Soloed..."), sigc::bind (sigc::mem_fun (*this, &GroupTabs::new_from_soloed), true)));
	}
	items.push_back (MenuElem (_("Create New Group with Master From..."), *new_from));
}

Gtk::Menu*
GroupTabs::get_menu (RouteGroup* g, bool in_tab_area)
{
	using namespace Menu_Helpers;

	delete _menu;

	_menu = new Menu;
	_menu->set_name ("ArdourContextMenu");

	MenuList& items = _menu->items();
	Menu* vca_menu;

	const VCAList vcas = _session->vca_manager().vcas ();

	if (!in_tab_area) {
		/* context menu is not for a group tab, show the "create new
		   from" items here
		*/
		add_new_from_items (items);
	}

	if (g) {
		items.push_back (SeparatorElem());
		items.push_back (MenuElem (_("Edit Group..."), sigc::bind (sigc::mem_fun (*this, &GroupTabs::edit_group), g)));
		items.push_back (MenuElem (_("Collect Group"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::collect), g)));
		items.push_back (MenuElem (_("Remove Group"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::remove_group), g)));

		items.push_back (SeparatorElem());

		if (g->has_control_master()) {
			items.push_back (MenuElem (_("Drop Group from VCA..."), sigc::bind (sigc::mem_fun (*this, &GroupTabs::unassign_group_to_master), g->group_master_number(), g)));
		} else {
			vca_menu = manage (new Menu);
			MenuList& f (vca_menu->items());
			f.push_back (MenuElem ("New", sigc::bind (sigc::mem_fun (*this, &GroupTabs::assign_group_to_master), 0, g, true)));

			for (VCAList::const_iterator v = vcas.begin(); v != vcas.end(); ++v) {
				f.push_back (MenuElem ((*v)->name().empty() ? string_compose ("VCA %1", (*v)->number()) : (*v)->name(), sigc::bind (sigc::mem_fun (*this, &GroupTabs::assign_group_to_master), (*v)->number(), g, true)));
			}
			items.push_back (MenuElem (_("Assign Group to VCA..."), *vca_menu));
		}

		items.push_back (SeparatorElem());

		bool can_subgroup = true;
		boost::shared_ptr<RouteList> rl = g->route_list();
		for (RouteList::const_iterator i = rl->begin(); i != rl->end(); ++i) {
#ifdef MIXBUS
			if ((*i)->mixbus ()) {
				can_subgroup = false;
				break;
			}
#endif
			if ((*i)->output()->n_ports().n_midi() != 0) {
				can_subgroup = false;
				break;
			}
		}

		if (g->has_subgroup ()) {
			items.push_back (MenuElem (_("Remove Subgroup Bus"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::un_subgroup), g)));
		} else if (can_subgroup) {
			items.push_back (MenuElem (_("Add New Subgroup Bus"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::subgroup), g, false, PreFader)));
		}

		if (can_subgroup) {
			items.push_back (MenuElem (_("Add New Aux Bus (pre-fader)"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::subgroup), g, true, PreFader)));
			items.push_back (MenuElem (_("Add New Aux Bus (post-fader)"), sigc::bind (sigc::mem_fun (*this, &GroupTabs::subgroup), g, true, PostFader)));
		}

		if (can_subgroup || g->has_subgroup ()) {
			items.push_back (SeparatorElem());
		}
	}

	add_menu_items (_menu, g);

	if (in_tab_area) {
		/* context menu is for a group tab, show the "create new
		   from" items here
		*/
		add_new_from_items (items);
	}

	items.push_back (SeparatorElem());

	vca_menu = manage (new Menu);
	{
		MenuList& f (vca_menu->items());
		f.push_back (MenuElem ("New", sigc::bind (sigc::mem_fun (*this, &GroupTabs::assign_selection_to_master), 0)));
		for (VCAList::const_iterator v = vcas.begin(); v != vcas.end(); ++v) {
			f.push_back (MenuElem ((*v)->name().empty() ? string_compose ("VCA %1", (*v)->number()) : (*v)->name(), sigc::bind (sigc::mem_fun (*this, &GroupTabs::assign_selection_to_master), (*v)->number())));
		}
	}

	items.push_back (MenuElem (_("Assign Selection to VCA..."), *vca_menu));

	vca_menu = manage (new Menu);
	{
		MenuList& f (vca_menu->items());
		f.push_back (MenuElem ("New", sigc::bind (sigc::mem_fun (*this, &GroupTabs::assign_recenabled_to_master), 0)));
		for (VCAList::const_iterator v = vcas.begin(); v != vcas.end(); ++v) {
			f.push_back (MenuElem ((*v)->name().empty() ? string_compose ("VCA %1", (*v)->number()) : (*v)->name(), sigc::bind (sigc::mem_fun (*this, &GroupTabs::assign_recenabled_to_master), (*v)->number())));
		}

	}
	items.push_back (MenuElem (_("Assign Record Enabled to VCA..."), *vca_menu));

	vca_menu = manage (new Menu);
	{
		MenuList& f (vca_menu->items());
		f.push_back (MenuElem ("New", sigc::bind (sigc::mem_fun (*this, &GroupTabs::assign_soloed_to_master), 0)));
		for (VCAList::const_iterator v = vcas.begin(); v != vcas.end(); ++v) {
			f.push_back (MenuElem ((*v)->name().empty() ? string_compose ("VCA %1", (*v)->number()) : (*v)->name(), sigc::bind (sigc::mem_fun (*this, &GroupTabs::assign_soloed_to_master), (*v)->number())));
		}

	}
	items.push_back (MenuElem (_("Assign Soloed to VCA..."), *vca_menu));

	items.push_back (SeparatorElem());
	items.push_back (MenuElem (_("Enable All Groups"), sigc::mem_fun(*this, &GroupTabs::activate_all)));
	items.push_back (MenuElem (_("Disable All Groups"), sigc::mem_fun(*this, &GroupTabs::disable_all)));

	return _menu;
}

void
GroupTabs::assign_group_to_master (uint32_t which, RouteGroup* group, bool rename_master) const
{
	if (!_session || !group) {
		return;
	}

	boost::shared_ptr<VCA> master;

	if (which == 0) {
		if (_session->vca_manager().create_vca (1).empty ()) {
			/* error */
			return;
		}

		/* Get most recently created VCA... */
		which = _session->vca_manager().vcas().back()->number();
	}

	master = _session->vca_manager().vca_by_number (which);

	if (!master) {
		/* should never happen; if it does, basically something deeply
		   odd happened, no reason to tell user because there's no
		   sensible explanation.
		*/
		return;
	}

	group->assign_master (master);

	if (rename_master){
		master->set_name (group->name());
	}
}

void
GroupTabs::unassign_group_to_master (uint32_t which, RouteGroup* group) const
{
	if (!_session || !group) {
		return;
	}

	boost::shared_ptr<VCA> master = _session->vca_manager().vca_by_number (which);

	if (!master) {
		/* should never happen; if it does, basically something deeply
		   odd happened, no reason to tell user because there's no
		   sensible explanation.
		*/
		return;
	}

	group->unassign_master (master);
}

void
GroupTabs::assign_some_to_master (uint32_t which, RouteList rl, std::string vcaname)
{
	if (!_session) {
		return;
	}

	boost::shared_ptr<VCA> master;
	bool set_name = false;

	if (which == 0) {
		if (_session->vca_manager().create_vca (1).empty ()) {
			/* error */
			return;
		}
		set_name = true;

		/* Get most recently created VCA... */
		which = _session->vca_manager().vcas().back()->number();
	}

	master = _session->vca_manager().vca_by_number (which);

	if (!master) {
		/* should never happen; if it does, basically something deeply
		   odd happened, no reason to tell user because there's no
		   sensible explanation.
		*/
		return;
	}


	if (rl.empty()) {
		return;
	}

	for (RouteList::iterator r = rl.begin(); r != rl.end(); ++r) {
		(*r)->assign (master);
	}
	if (set_name && !vcaname.empty()) {
		master->set_name (vcaname);
	}
}

RouteList
GroupTabs::get_rec_enabled ()
{
	RouteList rec_enabled;

	if (!_session) {
		return rec_enabled;
	}

	boost::shared_ptr<RouteList> rl = _session->get_routes ();

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		boost::shared_ptr<Track> trk (boost::dynamic_pointer_cast<Track> (*i));
		if (trk && trk->rec_enable_control()->get_value()) {
			rec_enabled.push_back (*i);
		}
	}

	return rec_enabled;
}


RouteList
GroupTabs::get_soloed ()
{
	RouteList rl = _session->get_routelist ();
	RouteList soloed;

	for (RouteList::iterator i = rl.begin(); i != rl.end(); ++i) {
		if (!(*i)->is_master() && (*i)->soloed()) {
			soloed.push_back (*i);
		}
	}

	return soloed;
}

void
GroupTabs::assign_selection_to_master (uint32_t which)
{
	assign_some_to_master (which, selected_routes (), _("Selection"));
}

void
GroupTabs::assign_recenabled_to_master (uint32_t which)
{
	assign_some_to_master (which, get_rec_enabled());
}

void
GroupTabs::assign_soloed_to_master (uint32_t which)
{
	assign_some_to_master (which, get_soloed());
}

void
GroupTabs::new_from_selection (bool with_master)
{
	RouteList rl (selected_routes());
	run_new_group_dialog (&rl, with_master);
}

void
GroupTabs::new_from_rec_enabled (bool with_master)
{
	RouteList rl (get_rec_enabled());
	run_new_group_dialog (&rl, with_master);
}

void
GroupTabs::new_from_soloed (bool with_master)
{
	RouteList rl (get_soloed());
	run_new_group_dialog (&rl, with_master);
}

void
GroupTabs::run_new_group_dialog (RouteList const * rl, bool with_master)
{
	if (rl && rl->empty()) {
		return;
	}

	RouteGroup* g = new RouteGroup (*_session, "");
	RouteGroupDialog* d = new RouteGroupDialog (g, true);

	d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &GroupTabs::new_group_dialog_finished), d, rl ? new RouteList (*rl): 0, with_master));
	d->present ();
}

void
GroupTabs::new_group_dialog_finished (int r, RouteGroupDialog* d, RouteList const * rl, bool with_master) const
{
	if (r == RESPONSE_OK) {

		if (!d->name_check()) {
			return;
		}

		_session->add_route_group (d->group());

		if (rl) {
			for (RouteList::const_iterator i = rl->begin(); i != rl->end(); ++i) {
				d->group()->add (*i);
			}

			if (with_master) {
				assign_group_to_master (0, d->group(), true); /* zero => new master */
			}
		}
	} else {
		delete d->group ();
	}

	delete rl;
	delete_when_idle (d);
}

void
GroupTabs::edit_group (RouteGroup* g)
{
	RouteGroupDialog* d = new RouteGroupDialog (g, false);
	d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &GroupTabs::edit_group_dialog_finished), d));
	d->present ();
}

void
GroupTabs::edit_group_dialog_finished (int r, RouteGroupDialog* d) const
{
	delete_when_idle (d);
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

/** Collect all members of a RouteGroup so that they are together in the Editor or Mixer.
 *  @param g Group to collect.
 */
void
GroupTabs::collect (RouteGroup* g)
{
	boost::shared_ptr<RouteList> group_routes = g->route_list ();
	group_routes->sort (Stripable::Sorter());
	int const N = group_routes->size ();

	RouteList::iterator i = group_routes->begin ();
	RouteList routes = _session->get_routelist ();
	routes.sort (Stripable::Sorter());
	RouteList::const_iterator j = routes.begin ();

	int diff = 0;
	int coll = -1;

	PresentationInfo::ChangeSuspender cs;

	while (i != group_routes->end() && j != routes.end()) {

		PresentationInfo::order_t const k = (*j)->presentation_info ().order();

		if (*i == *j) {

			if (coll == -1) {
				coll = k;
				diff = N - 1;
			} else {
				--diff;
			}

			(*j)->set_presentation_order (coll);

			++coll;
			++i;

		} else {

			(*j)->set_presentation_order (k + diff);

		}

		++j;
	}
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
	boost::shared_ptr<RouteList> rl (g->route_list ());
	_session->remove_route_group (*g);

	emit_gui_changed_for_members (rl);
}

/** Set the color of the tab of a route group */
void
GroupTabs::set_group_color (RouteGroup* group, uint32_t color)
{
	assert (group);
	PresentationInfo::ChangeSuspender cs;
	group->set_rgba (color);
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
uint32_t
GroupTabs::group_color (RouteGroup* group)
{
	assert (group);

	/* prefer libardour color, if set */
	uint32_t rgba = group->rgba ();
	if (rgba != 0) {
		return rgba;
	}

	/* backwards compatibility, load old color */

	GUIObjectState& gui_state = *ARDOUR_UI::instance()->gui_object_state;
	string const gui_id = group_gui_id (group);
	bool empty;
	string const color = gui_state.get_string (gui_id, "color", &empty);

	if (empty) {
		/* no color has yet been set, so use a random one */
		uint32_t c = gdk_color_to_rgba (unique_random_color (_used_colors));
		set_group_color (group, c);
		return c;
	}

	int r, g, b;

	/* for historical reasons, colors are stored as 16 bit values.  */

	sscanf (color.c_str(), "%d:%d:%d", &r, &g, &b);

	r /= 256;
	g /= 256;
	b /= 256;

	group->migrate_rgba (RGBA_TO_UINT (r, g, b, 255));
	gui_state.remove_node (gui_id);

	return RGBA_TO_UINT (r, g, b, 255);
}

void
GroupTabs::route_group_property_changed (RouteGroup* rg)
{
	/* This is a bit of a hack, but this might change
	   our route's effective color, so emit gui_changed
	   for our routes.
	*/

	emit_gui_changed_for_members (rg->route_list ());

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

	r->presentation_info().PropertyChanged (Properties::color);

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

	r->presentation_info().PropertyChanged (Properties::color);

	set_dirty ();
}

void
GroupTabs::emit_gui_changed_for_members (boost::shared_ptr<RouteList> rl)
{
	PresentationInfo::ChangeSuspender cs;

	for (RouteList::iterator i = rl->begin(); i != rl->end(); ++i) {
		(*i)->presentation_info().PropertyChanged (Properties::color);
	}
}
