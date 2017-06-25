/*
    Copyright (C) 2009-2013 Paul Davis
    Author: Johannes Mueller

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

#ifndef ardour_shuttlepro_button_config_widget_h
#define ardour_shuttlepro_button_config_widget_h

#include <gtkmm/box.h>
#include <gtkmm/radiobutton.h>

#include "pbd/signals.h"

#include "shuttlepro.h"
#include "jump_distance_widget.h"

namespace ArdourSurface
{
class ButtonConfigWidget : public Gtk::HBox
{
public:
	ButtonConfigWidget ();
	~ButtonConfigWidget () {};

private:
	Gtk::RadioButton _choice_jump;
	Gtk::RadioButton _choice_action;

	void update_choice ();

	JumpDistanceWidget _jump_distance;
};
}

#endif /* ardour_shuttlepro_button_config_widget_h */
