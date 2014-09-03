/*
	Copyright (C) 2014 Waves Audio Ltd.

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

#include "waves_dropdown.h"

WavesDropdown::WavesDropdown (const std::string& title)
  : WavesIconButton (title)
{
	signal_button_press_event().connect (sigc::mem_fun(*this, &WavesDropdown::on_mouse_pressed));
}

WavesDropdown::~WavesDropdown ()
{
}

bool
WavesDropdown::on_mouse_pressed (GdkEventButton*)
{
	_menu.popup (sigc::mem_fun(this, &WavesDropdown::_on_popup_menu_position), 1, gtk_get_current_event_time());
	return true;
}

void
WavesDropdown::AddMenuElem (const std::string& item, void* cookie)
{
	Gtk::Menu_Helpers::MenuList& items = _menu.items ();
	
	items.push_back (Gtk::Menu_Helpers::MenuElem (item, sigc::bind (sigc::mem_fun(*this, &WavesDropdown::_on_menu_item), cookie)));
}

void
WavesDropdown::_on_menu_item (void* cookie)
{
	signal_menu_item_clicked (this, cookie);
}

void
WavesDropdown::_on_popup_menu_position (int& x, int& y, bool& push_in)
{
    Gtk::Container *toplevel = get_toplevel ();
    if (toplevel) {
    	translate_coordinates (*toplevel, 0, 0, x, y);
    	Gtk::Allocation a = toplevel->get_allocation ();
    	x = a.get_x ();
    	y = a.get_y ();
        
        a = get_allocation ();
        y += a.get_height ();
        
        int xo;
    	int yo;
    	get_window ()->get_origin (xo, yo);
    	x += xo;
    	y += yo;
    }

    /*
    if (get_window ()) {
        Gtk::Allocation a = get_allocation ();

        int xo;
        int yo;
        
        get_window ()->get_origin (xo, yo);
        
        x = xo;
        y = yo + a.get_height ();
    }
     */
}
