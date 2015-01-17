/*
    Copyright (C) 2014 Paul Davis

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

#ifndef __gtk2_ardour_ardour_dropdown_h__
#define __gtk2_ardour_ardour_dropdown_h__

#include <list>
#include <stdint.h>

#include <gtkmm/action.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>


#include "ardour_button.h"

class ArdourDropdown : public ArdourButton
{
  public:

	ArdourDropdown (Element e = default_elements);
	virtual ~ArdourDropdown ();

	bool on_mouse_pressed (GdkEventButton*);

	void AddMenuElem (Gtk::Menu_Helpers::Element e);

  private:
	Gtk::Menu      _menu;
};

#endif /* __gtk2_ardour_ardour_menu_h__ */
