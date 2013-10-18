/*
    Copyright (C) 2001 Paul Davis 

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

#ifndef __gtkmm2ext_grouped_buttons_h__
#define __gtkmm2ext_grouped_buttons_h__

#include <stdint.h>

#include <vector>
#include <sigc++/signal.h>

#include "gtkmm2ext/visibility.h"

namespace Gtk {
	class ToggleButton;
};

class LIBGTKMM2EXT_API GroupedButtons : public sigc::trackable
{
  public:
	GroupedButtons (uint32_t nbuttons, uint32_t first_active);
	GroupedButtons (std::vector<Gtk::ToggleButton *>&);
	
	Gtk::ToggleButton& button (uint32_t which) {
		return *buttons[which];
	}

  private:
	std::vector<Gtk::ToggleButton *> buttons;
	uint32_t current_active;
	void one_clicked (uint32_t which);
};

#endif /* __gtkmm2ext_grouped_buttons_h__ */
