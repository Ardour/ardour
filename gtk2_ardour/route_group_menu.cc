/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2011 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <gtkmm/menu.h>
#include <gtkmm/stock.h>

#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/doi.h"

#include "ardour/session.h"
#include "ardour/route_group.h"
#include "ardour/route.h"
#include "route_group_menu.h"
#include "route_group_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

RouteGroupMenu::RouteGroupMenu (Session* s, PropertyList* plist)
	: SessionHandlePtr (s)
	, _menu (0)
	, _default_properties (plist)
	, _inhibit_group_selected (false)
{

}

RouteGroupMenu::~RouteGroupMenu()
{
	delete _menu;
	delete _default_properties;
}

/** @param s Routes to operate on */
void
RouteGroupMenu::build (WeakRouteList const & s)
{
	assert (!s.empty ());

	using namespace Menu_Helpers;

	_subject = s;

	/* FInd all the route groups that our subjects are members of */
	std::set<RouteGroup*> groups;
	for (WeakRouteList::const_iterator i = _subject.begin (); i != _subject.end(); ++i) {
		boost::shared_ptr<Route> r = i->lock ();
		if (r) {
			groups.insert (r->route_group ());
		}
	}

	_inhibit_group_selected = true;

	delete _menu;

	/* Note: don't use manage() here, otherwise if our _menu object is attached as a submenu
	   and its parent is then destroyed, our _menu object will be deleted and we'll have no
	   way of knowing about it.  Without manage(), when the above happens our _menu's gobject
	   will be destroyed and its value set to 0, so we know.
	*/
	_menu = new Menu;

	MenuList& items = _menu->items ();

	items.push_back (MenuElem (_("New Group..."), sigc::mem_fun (*this, &RouteGroupMenu::new_group)));
	if (groups.size() == 1 && *groups.begin() != 0) {
		items.push_back (MenuElem (_("Edit Group..."), sigc::bind (sigc::mem_fun (*this, &RouteGroupMenu::edit_group), *groups.begin ())));
	}
	items.push_back (SeparatorElem ());

	RadioMenuItem::Group group;
	items.push_back (RadioMenuElem (group, _("No Group")));
	RadioMenuItem* i = static_cast<RadioMenuItem *> (&items.back ());
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteGroupMenu::set_group), i, (RouteGroup *) 0));

	if (groups.size() == 1 && *groups.begin() == 0) {
		i->set_active ();
	} else if (groups.size() > 1) {
		i->set_inconsistent ();
	}

	if (_session) {
		_session->foreach_route_group (sigc::bind (sigc::mem_fun (*this, &RouteGroupMenu::add_item), groups, &group));
	}

	_inhibit_group_selected = false;
}

/** @param rg Route group to add.
 *  @param groups Active route groups (may included 0 for `no group')
 *  @param group Radio item group to add radio items to.
 */
void
RouteGroupMenu::add_item (RouteGroup* rg, std::set<RouteGroup*> const & groups, RadioMenuItem::Group* group)
{
	using namespace Menu_Helpers;

	MenuList& items = _menu->items ();

	items.push_back (RadioMenuElem (*group, rg->name()));
	RadioMenuItem* i = static_cast<RadioMenuItem*> (&items.back ());
	i->signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &RouteGroupMenu::set_group), i, rg));

	if (groups.size() == 1 && *groups.begin() == rg) {
		/* there's only one active group, and it's this one */
		i->set_active ();
	} else if (groups.size() > 1) {
		/* there are >1 active groups */
		i->set_inconsistent ();
	}
}

/** Called when a group is selected from the menu.
 *  @param Group, or 0 for none.
 */
void
RouteGroupMenu::set_group (Gtk::RadioMenuItem* e, RouteGroup* g)
{
	if (_inhibit_group_selected) {
		return;
	}
	if (e && !e->get_active()) {
		return;
	}

	for (WeakRouteList::const_iterator i = _subject.begin(); i != _subject.end(); ++i) {
		boost::shared_ptr<Route> r = i->lock ();
		if (!r || r->route_group () == g) {
			/* lock of weak_ptr failed, or the group for this route is already right */
			continue;
		}

		if (g) {
			g->add (r);
		} else {
			if (r->route_group ()) {
				r->route_group()->remove (r);
			}
		}
	}
}

void
RouteGroupMenu::new_group ()
{
	if (!_session) {
		return;
	}

	RouteGroup* g = new RouteGroup (*_session, "");
	RouteGroupDialog* d = new RouteGroupDialog (g, true);

	d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &RouteGroupMenu::new_group_dialog_finished), d));
	d->present ();
}

void
RouteGroupMenu::new_group_dialog_finished (int r, RouteGroupDialog* d)
{
	if (r == RESPONSE_OK) {
		_session->add_route_group (d->group());
		set_group (0, d->group());
	} else {
		delete d->group ();
	}

	delete_when_idle (d);
}

void
RouteGroupMenu::edit_group (ARDOUR::RouteGroup* g)
{
	RouteGroupDialog* d = new RouteGroupDialog (g, false);
	d->signal_response().connect (sigc::hide (sigc::bind (sigc::ptr_fun (&delete_when_idle<RouteGroupDialog>), d)));
	d->present ();
}

Gtk::Menu *
RouteGroupMenu::menu ()
{
	/* Our menu's gobject can be 0 if it was attached as a submenu whose
	   parent was subsequently deleted.
	*/
	assert (_menu && _menu->gobj());
	return _menu;
}

void
RouteGroupMenu::detach ()
{
	if (_menu && _menu->gobj ()) {
		Gtkmm2ext::detach_menu (*_menu);
	}
}
