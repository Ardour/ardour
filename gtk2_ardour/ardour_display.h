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

#ifndef __gtk2_ardour_ardour_display_h__
#define __gtk2_ardour_ardour_display_h__

#include <list>
#include <stdint.h>

#include <gtkmm/action.h>
#include <gtkmm/menu.h>
#include <gtkmm/menuitem.h>


#include "ardour_button.h"

class ArdourDisplay : public ArdourButton
{
  public:

	ArdourDisplay (Element e = default_elements);
	virtual ~ArdourDisplay ();

	boost::shared_ptr<PBD::Controllable> get_controllable() { return binding_proxy.get_controllable(); }
 	void set_controllable (boost::shared_ptr<PBD::Controllable> c);

	bool on_button_press_event (GdkEventButton*);  //mousedown will pop up our preset menu
//	bool on_button_press_event (GdkEventButton*);
//	bool on_button_release_event (GdkEventButton*);
	bool on_scroll_event (GdkEventScroll* ev);
//	bool on_motion_notify_event (GdkEventMotion *ev) ;

	void add_controllable_preset (const char*, float);
	void handle_controllable_preset (float p);

	void controllable_changed ();
	PBD::ScopedConnection watch_connection;

  private:
	Gtk::Menu      _menu;

	bool _hovering;
	bool _grabbed;
	float _grabbed_y;
};

#endif /* __gtk2_ardour_ardour_menu_h__ */
