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

using namespace Gtk;
using namespace ARDOUR;

GroupTabs::GroupTabs ()
	: _session (0),
	  _menu (0)
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
	
	RouteGroup* g = click_to_route_group (ev);
	
	if (ev->button == 1 && g) {
		
		g->set_active (!g->is_active (), this);
		
	} else if (ev->button == 3 && g) {

		if (!_menu) {
			_menu = new Menu;
			MenuList& items = _menu->items ();
			items.push_back (MenuElem (_("Edit..."), bind (mem_fun (*this, &GroupTabs::edit_group), g)));
			items.push_back (MenuElem (_("Remove"), bind (mem_fun (*this, &GroupTabs::remove_group), g)));
		}

		_menu->popup (ev->button, ev->time);

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
