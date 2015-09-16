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

#include "tooltips.h"

#include "gtkmm2ext/gtk_ui.h"

namespace ARDOUR_UI_UTILS {

void
set_tooltip (Gtk::Widget& w, const std::string& text) {
	Gtkmm2ext::UI::instance()->set_tip(w, text);
}

void
set_tooltip (Gtk::Widget* w, const std::string& text) {
	Gtkmm2ext::UI::instance()->set_tip(*w, text);
}

void
set_tooltip (Gtk::Widget* w, const std::string& text, const std::string& help_text) {
	Gtkmm2ext::UI::instance()->set_tip(w, text.c_str(), help_text.c_str());
}

} // ARDOUR_UI_UTILS
