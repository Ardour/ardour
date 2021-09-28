/*
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_gtk_route_group_menu_h__
#define __ardour_gtk_route_group_menu_h__

#include "ardour/route_group.h"
#include "ardour/session_handle.h"

class RouteGroupDialog;

class RouteGroupMenu : public ARDOUR::SessionHandlePtr
{
public:
	RouteGroupMenu (ARDOUR::Session*, PBD::PropertyList*);
	~RouteGroupMenu();

	Gtk::Menu* menu ();
	void build (ARDOUR::WeakRouteList const &);
	void detach ();

  private:
	void add_item (ARDOUR::RouteGroup *, std::set<ARDOUR::RouteGroup*> const &, Gtk::RadioMenuItem::Group*);
	void new_group ();
	void edit_group (ARDOUR::RouteGroup *);
	void set_group (Gtk::RadioMenuItem*, ARDOUR::RouteGroup *);
	void new_group_dialog_finished (int, RouteGroupDialog*);

	Gtk::Menu* _menu;

	PBD::PropertyList* _default_properties;
	bool _inhibit_group_selected;
	ARDOUR::WeakRouteList _subject;
};

#endif /* __ardour_gtk_route_group_menu_h__ */
