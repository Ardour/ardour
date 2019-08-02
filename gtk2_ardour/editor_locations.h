/*
 * Copyright (C) 2009 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2010-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_editor_locations_h__
#define __gtk_ardour_editor_locations_h__

#include "pbd/xml++.h"

#include <gtkmm/scrolledwindow.h>
#include "ardour/session_handle.h"
#include "editor_component.h"

class LocationUI;

namespace Gtk {
	class Widget;
}

class EditorLocations : public EditorComponent, public ARDOUR::SessionHandlePtr
{
public:
	EditorLocations (Editor *);

	void set_session (ARDOUR::Session *);

	Gtk::Widget& widget ();
	XMLNode & get_state () const;
	int set_state (const XMLNode&);

private:
	Gtk::ScrolledWindow _scroller;
	LocationUI* _locations;
};


#endif /* __gtk_ardour_editor_locations_h__ */
