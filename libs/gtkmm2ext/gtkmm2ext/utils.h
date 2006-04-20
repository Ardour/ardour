/*
    Copyright (C) 1999 Paul Barton-Davis 

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

#ifndef __gtkmm2ext_utils_h__
#define __gtkmm2ext_utils_h__

#include <vector>
#include <string>

#include <gtkmm/treeview.h>
#include <gdkmm/window.h> /* for WMDecoration */

namespace Gtk {
	class ComboBoxText;
	class Widget;
	class Window;
	class Paned;
}

namespace Gtkmm2ext {
	void init ();

	void get_ink_pixel_size (Glib::RefPtr<Pango::Layout>, int& width, int& height);

	void set_size_request_to_display_given_text (Gtk::Widget &w,
						     const gchar *text,
						     gint hpadding,
						     gint vpadding);

	void set_popdown_strings (Gtk::ComboBoxText&, const std::vector<std::string>&);
	
	template<class T> void deferred_delete (void *ptr) {
		delete static_cast<T *> (ptr);
	}

	GdkWindow* get_paned_handle (Gtk::Paned& paned);
	void set_decoration (Gtk::Window* win, Gdk::WMDecoration decor);
	void set_treeview_header_as_default_label(Gtk::TreeViewColumn *c);
	Glib::RefPtr<Gdk::Drawable> get_bogus_drawable();
};

#endif /*  __gtkmm2ext_utils_h__ */
