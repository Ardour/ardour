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

#include <gtkmm/menu.h>
#include <gtkmm/stock.h>
#include "gtkmm2ext/utils.h"
#include "ardour/session.h"
#include "ardour/route_group.h"
#include "route_group_menu.h"
#include "route_group_dialog.h"
#include "i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace PBD;

RouteGroupMenu::RouteGroupMenu (Session* s, PropertyList* plist)
	: SessionHandlePtr (s)
	, _menu (0)
	, _default_properties (plist)
	, _inhibit_group_selected (false)
	, _selected_route_group (0)
{

}

RouteGroupMenu::~RouteGroupMenu()
{
	delete _menu;
	delete _default_properties;
}

/** @param curr Current route group to mark as selected, or 0 for no group */
void
RouteGroupMenu::build (RouteGroup* curr)
{
	using namespace Menu_Helpers;

	_selected_route_group = curr;

	_inhibit_group_selected = true;

	delete _menu;
	
	/* Note: don't use manage() here, otherwise if our _menu object is attached as a submenu
	   and its parent is then destroyed, our _menu object will be deleted and we'll have no
	   way of knowing about it.  Without manage(), when the above happens our _menu's gobject
	   will be destroyed and its value set to 0, so we know.
	*/
	_menu = new Menu;

	MenuList& items = _menu->items ();
	
	items.push_back (MenuElem (_("New group..."), sigc::mem_fun (*this, &RouteGroupMenu::new_group)));
	items.push_back (SeparatorElem ());

	RadioMenuItem::Group group;
	items.push_back (RadioMenuElem (group, _("No group"), sigc::bind (sigc::mem_fun (*this, &RouteGroupMenu::set_group), (RouteGroup *) 0)));

	if (curr == 0) {
		static_cast<RadioMenuItem*> (&items.back())->set_active ();
	}

	if (_session) {
		_session->foreach_route_group (sigc::bind (sigc::mem_fun (*this, &RouteGroupMenu::add_item), curr, &group));
	}

	_inhibit_group_selected = false;
}

void
RouteGroupMenu::add_item (RouteGroup* rg, RouteGroup* curr, RadioMenuItem::Group* group)
{
	using namespace Menu_Helpers;

	MenuList& items = _menu->items ();

	items.push_back (RadioMenuElem (*group, rg->name(), sigc::bind (sigc::mem_fun(*this, &RouteGroupMenu::set_group), rg)));

	if (rg == curr) {
		static_cast<RadioMenuItem*> (&items.back())->set_active ();
	}
}

/** Called when a group is selected from the menu.
 *  @param Group, or 0 for none.
 */
void
RouteGroupMenu::set_group (RouteGroup* g)
{
	if (g == _selected_route_group) {
		/* cut off the signal_toggled that GTK emits for an option that is being un-selected
		   when a new option is being selected instead
		*/
		return;
	}
	
	if (!_inhibit_group_selected) {
		GroupSelected (g);
	}

	_selected_route_group = g;
}

void
RouteGroupMenu::new_group ()
{
	if (!_session) {
		return;
	}

	RouteGroup* g = new RouteGroup (*_session, "");
	g->apply_changes (*_default_properties);

	RouteGroupDialog d (g, true);

	if (d.do_run ()) {
		delete g;
	} else {
		_session->add_route_group (g);
		set_group (g);
	}
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
