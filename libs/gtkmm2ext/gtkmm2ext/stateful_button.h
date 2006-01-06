/*
    Copyright (C) 2005 Paul Davis

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

#ifndef __pbd_gtkmm_abutton_h__
#define __pbd_gtkmm_abutton_h__

#include <vector>

#include <gtkmm/togglebutton.h>

namespace Gtkmm2ext {

class StatefulButton : public Gtk::Button
{
   public:
	StatefulButton();
	explicit StatefulButton(const std::string &label);
	virtual ~StatefulButton() {}

	void set_colors (const std::vector<Gdk::Color>& colors);
	void set_state (int);
	int  get_state () { return current_state; }
	void set_active (bool yn) {
		set_state (yn ? 1 : 0);
	}
	

  protected:
	std::vector<Gdk::Color> colors;
	int current_state;
	Gdk::Color saved_bg;
	bool have_saved_bg;

	void on_realize ();
};

};

#endif
