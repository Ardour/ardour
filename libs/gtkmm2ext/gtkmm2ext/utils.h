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

*/

#ifndef __gtkmm2ext_utils_h__
#define __gtkmm2ext_utils_h__

#include <vector>
#include <string>
#include <stdint.h>

#include <cairomm/refptr.h>

#include <gtkmm/container.h>
#include <gtkmm/treeview.h>
#include <gdkmm/window.h> /* for WMDecoration */
#include <gdkmm/pixbuf.h>
#include <pangomm/fontdescription.h>

namespace Cairo {
        class Context;
}

namespace Gtk {
	class ComboBoxText;
	class Widget;
	class Window;
	class Paned;
	class Menu;
}

namespace Gtkmm2ext {
	void init ();

	std::string fit_to_pixels (const std::string&, int pixel_width, Pango::FontDescription& font, int& actual_width, bool with_ellipses = false);
	std::pair<std::string, double> fit_to_pixels (cairo_t *, std::string, double);
	int pixel_width (const std::string& str, Pango::FontDescription& font);

	void get_ink_pixel_size (Glib::RefPtr<Pango::Layout>, 
				 int& width, int& height);

	void set_size_request_to_display_given_text (Gtk::Widget &w,
						     const gchar *text,
						     gint hpadding,
						     gint vpadding);

	void set_size_request_to_display_given_text (Gtk::Widget &w,
						     const std::vector<std::string>&,
						     gint hpadding,
						     gint vpadding);

	Glib::RefPtr<Gdk::Pixbuf> pixbuf_from_string (const std::string& name, 
	                                              const Pango::FontDescription& font, 
	                                              int clip_width, 
	                                              int clip_height, 
	                                              Gdk::Color fg);

	void set_popdown_strings (Gtk::ComboBoxText&, 
	                          const std::vector<std::string>&);

	template<class T> void deferred_delete (void *ptr) {
		delete static_cast<T *> (ptr);
	}

	GdkWindow* get_paned_handle (Gtk::Paned& paned);
	void set_decoration (Gtk::Window* win, Gdk::WMDecoration decor);
	void set_treeview_header_as_default_label(Gtk::TreeViewColumn *c);
	Glib::RefPtr<Gdk::Drawable> get_bogus_drawable();
	void detach_menu (Gtk::Menu&);

	Glib::RefPtr<Gdk::Window> window_to_draw_on (Gtk::Widget& w, Gtk::Widget** parent);

        bool possibly_translate_keyval_to_make_legal_accelerator (uint32_t& keyval);
        uint32_t possibly_translate_legal_accelerator_to_real_key (uint32_t keyval);

        int physical_screen_height (Glib::RefPtr<Gdk::Window>);
        int physical_screen_width (Glib::RefPtr<Gdk::Window>);

        void container_clear (Gtk::Container&);
        void rounded_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r=10);
        void rounded_top_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r=10);
        void rounded_top_left_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r=10);
        void rounded_top_right_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r=10);
	void rounded_top_half_rectangle (Cairo::RefPtr<Cairo::Context>, double x, double y, double w, double h, double r=10);
	void rounded_bottom_half_rectangle (Cairo::RefPtr<Cairo::Context>, double x, double y, double w, double h, double r=10);

        void rounded_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
        void rounded_top_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
        void rounded_top_left_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
        void rounded_top_right_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);

	void rounded_top_half_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
	void rounded_bottom_half_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);

	Gtk::Label* left_aligned_label (std::string const &);
};

#endif /*  __gtkmm2ext_utils_h__ */
