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

#ifndef __gtk2_ardour_waves_dropdown_h__
#define __gtk2_ardour_waves_dropdown_h__

#include "waves_icon_button.h"
#include "canvas/xml_ui.h"

using namespace ArdourCanvas::XMLUI;

class WavesDropdown : public WavesIconButton
{
  public:
    
    WavesDropdown (const std::string& title = "");
    virtual ~WavesDropdown ();
    Gtk::Menu& get_menu () { return _menu; }
    void clear_items ();
    
	int get_current_item () const { return _current_item_number; }
	void set_current_item (int current_item_number);
    
    void* get_item_data_pv (size_t);
	int get_item_data_i (size_t item) { return (char*)get_item_data_pv(item) - (char*)0; }
	unsigned int get_item_data_u (size_t item) { return (char*)get_item_data_pv(item) - (char*)0; }
    Gtk::MenuItem* get_item (int);
    Gtk::MenuItem* get_item (const std::string&);
    
    Gtk::MenuItem& add_menu_item (const std::string& item, void* cookie = 0,  DestroyNotify cookie_cleaner = 0, bool provide_style = true);
    Gtk::RadioMenuItem& add_radio_menu_item (const std::string& item, void* cookie = 0,  DestroyNotify cookie_cleaner = 0, bool provide_style = true);
    Gtk::CheckMenuItem& add_check_menu_item (const std::string& item, void* cookie = 0,  DestroyNotify cookie_cleaner = 0, bool provide_style = true);
	void set_maxmenuheight (int maxmenuheight) { _maxmenuheight = ((maxmenuheight < 0) ? -1 : maxmenuheight); }
	int get_maxmenuheight () const { return _maxmenuheight; }

    sigc::signal2<void, WavesDropdown*, size_t> selected_item_changed;

  private:
    static char* menu_item_data_key;
    Gtk::Menu _menu;
	int _current_item_number;
    void _on_menu_item (int item_number, void* cookie);
    void _on_popup_menu_position (int& x, int& y, bool& push_in);
    bool _on_mouse_pressed (GdkEventButton*);
	int _maxmenuheight;
};

#endif /* __gtk2_ardour_waves_dropdown_h__ */
