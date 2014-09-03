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

class WavesDropdown : public WavesIconButton
{
  public:

	WavesDropdown (const std::string& title = "");
	virtual ~WavesDropdown ();
	Gtk::Menu& get_menu () { return _menu; }

	bool on_mouse_pressed (GdkEventButton*);

	void AddMenuElem (const std::string& item, void* coockie);

	sigc::signal2<void, WavesDropdown*, void*> signal_menu_item_clicked;

  private:
	Gtk::Menu _menu;
	void _on_menu_item (void* cookie);
	void _on_popup_menu_position (int& x, int& y, bool& push_in);
};

#endif /* __gtk2_ardour_waves_dropdown_h__ */
