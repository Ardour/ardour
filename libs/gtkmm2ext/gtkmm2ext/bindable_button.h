/*
    Copyright (C) 2004 Paul Davis

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

    $Id$
*/

#ifndef __bindable_button_h__
#define __bindable_button_h__

#include <string>

#include <gtkmm2ext/stateful_button.h>
#include "binding_proxy.h"

namespace PBD {
	class Controllable;
}

class BindableToggleButton : public Gtk::ToggleButton
{
   public:
	BindableToggleButton (PBD::Controllable& c) : binding_proxy (c) {}
	explicit BindableToggleButton (PBD::Controllable& c, const std::string &label) : Gtk::ToggleButton (label), binding_proxy (c) {}
	virtual ~BindableToggleButton() {}
	
	bool on_button_press_event (GdkEventButton *ev) {
		return binding_proxy.button_press_handler (ev);
	}

  private:
	BindingProxy binding_proxy;
};

#endif
