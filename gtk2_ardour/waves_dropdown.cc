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

char* WavesDropdown::menu_item_data_key = "waves_dropdown_item_cookie";

WavesDropdown::WavesDropdown (const std::string& title)
  : WavesIconButton (title)
  , _current_item_number (-1)
  , _maxmenuheight (-1)
{
	signal_button_press_event().connect (sigc::mem_fun(*this, &WavesDropdown::_on_mouse_pressed));
	_menu.signal_hide ().connect (sigc::bind (sigc::mem_fun (*this, &CairoWidget::set_active), false));
}

WavesDropdown::~WavesDropdown ()
{
}

void
WavesDropdown::clear_items ()
{
    _menu.items().clear ();
}

void*
WavesDropdown::get_item_associated_data(int item_number)
{
    Gtk::Menu_Helpers::MenuList& items = _menu.items ();
    if (item_number >= items.size() ) {
        return NULL;
    }
    
    Gtk::Menu_Helpers::MenuList::iterator i = items.begin();
    std::advance (i, item_number);
    
    return (*i).get_data (menu_item_data_key);
}

Gtk::MenuItem*
WavesDropdown::get_item (int item_number)
{
    Gtk::Menu_Helpers::MenuList& items = _menu.items ();
    if (item_number >= items.size() ) {
        return NULL;
    }
    
    Gtk::Menu_Helpers::MenuList::iterator i = items.begin();
    std::advance (i, item_number);
    
    return &(*i);
}

Gtk::MenuItem*
WavesDropdown::get_item (const std::string& text_label)
{
    Gtk::Menu_Helpers::MenuList& items = _menu.items ();
    Gtk::Menu_Helpers::MenuList::iterator i = items.begin();
    for (; i != items.end(); ++i) {
        if (i->get_label() == text_label) {
            return &(*i);
        }
    }
    
    return NULL;
}


void
WavesDropdown::set_current_item (int current_item_number)
{
	Gtk::Menu_Helpers::MenuList& items = _menu.items ();

	if ((current_item_number < 0) || (current_item_number >= items.size ())) {
		return;
	}

	if (current_item_number == _current_item_number) {
		return;
	}
    
    Gtk::Menu_Helpers::MenuList::iterator i = items.begin();
    std::advance (i, current_item_number);
    
    Gtk::RadioMenuItem* radio_menu_item = dynamic_cast <Gtk::RadioMenuItem*> (&(*i));
	if (radio_menu_item) {
        
        Gtk::Menu_Helpers::MenuList::iterator prev_current_item = items.begin();
        std::advance (prev_current_item, _current_item_number);
        Gtk::RadioMenuItem* radio_menu_item = dynamic_cast <Gtk::RadioMenuItem*> (&(*prev_current_item));
        if (radio_menu_item) {
            radio_menu_item->set_active (false);
        }
		
        radio_menu_item->set_active (true);
	}
    
    Gtk::CheckMenuItem* check_menu_item = dynamic_cast <Gtk::CheckMenuItem*> (&(*i));
    if (check_menu_item) {
        check_menu_item->set_active (true);
    }
    
	_on_menu_item (current_item_number, (*i).get_data (menu_item_data_key));
}

Gtk::MenuItem&
WavesDropdown::add_menu_item (const std::string& item, void* cookie)
{
	Gtk::Menu_Helpers::MenuList& items = _menu.items ();
	
	items.push_back (Gtk::Menu_Helpers::MenuElem (item, sigc::bind (sigc::mem_fun(*this, &WavesDropdown::_on_menu_item), items.size (), cookie)));
    
	Gtk::MenuItem& menuitem = _menu.items ().back ();
	ensure_style();
	Widget* child = menuitem.get_child ();
	if (child) {
		child->set_style (get_style());
	}

	menuitem.set_data (menu_item_data_key, cookie);

    return menuitem;
}

Gtk::RadioMenuItem&
WavesDropdown::add_radio_menu_item (const std::string& item, void* cookie)
{
	Gtk::Menu_Helpers::MenuList& items = _menu.items ();
	
    if (items.empty()) {
            Gtk::RadioMenuItem::Group group;
            items.push_back (Gtk::Menu_Helpers::RadioMenuElem (group, item, sigc::bind (sigc::mem_fun(*this, &WavesDropdown::_on_menu_item), items.size (), cookie)));
    } else {
            Gtk::RadioMenuItem* first = dynamic_cast <Gtk::RadioMenuItem*> (&_menu.items ().front ());
            Gtk::RadioMenuItem::Group group = first->get_group();
            items.push_back (Gtk::Menu_Helpers::RadioMenuElem (group, item, sigc::bind (sigc::mem_fun(*this, &WavesDropdown::_on_menu_item), items.size (), cookie)));
    }
    
	Gtk::RadioMenuItem& menuitem = *dynamic_cast <Gtk::RadioMenuItem*> (&_menu.items ().back ());
	ensure_style();
	Widget* child = menuitem.get_child ();
	if (child) {
		child->set_style (get_style());
	}

	menuitem.set_data (menu_item_data_key, cookie);

    return menuitem;
}

Gtk::CheckMenuItem&
WavesDropdown::add_check_menu_item (const std::string& item, void* cookie)
{
	Gtk::Menu_Helpers::MenuList& items = _menu.items ();
	
	items.push_back (Gtk::Menu_Helpers::CheckMenuElem (item, sigc::bind (sigc::mem_fun(*this, &WavesDropdown::_on_menu_item), items.size (), cookie)));
    
	Gtk::CheckMenuItem& menuitem = *dynamic_cast <Gtk::CheckMenuItem*> (&_menu.items ().back ());
	ensure_style();
	Widget* child = menuitem.get_child ();
	if (child) {
		child->set_style (get_style());
	}

	menuitem.set_data (menu_item_data_key, cookie);

    return menuitem;
}

void
WavesDropdown::_on_menu_item (int item_number, void* cookie)
{
	_current_item_number = item_number;

    Gtk::Menu_Helpers::MenuList& items = _menu.items ();
    Gtk::Menu_Helpers::MenuList::iterator i = items.begin();
    std::advance (i, _current_item_number);
    set_text ((*i).get_label());
	selected_item_changed (this, item_number);
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
}

bool
WavesDropdown::_on_mouse_pressed (GdkEventButton*)
{
	_hovering = _pushed = false;
	if (!_menu.items ().empty ()) {
		_menu.popup (sigc::mem_fun(this, &WavesDropdown::_on_popup_menu_position), 1, gtk_get_current_event_time());
		Gtk::Allocation a = _menu.get_allocation ();
		if ((_maxmenuheight > -1) && _maxmenuheight < a.get_height ()) {
			_menu.set_size_request (-1, _maxmenuheight);
		}
	} else {
		set_active (false);
	}
	return true;
}
