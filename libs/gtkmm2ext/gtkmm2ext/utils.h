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

#include <cairomm/cairomm.h>
#include <pangomm/fontdescription.h>

#include <gtkmm/container.h>
#include <gtkmm/treeview.h>
#include <gdkmm/window.h> /* for WMDecoration */
#include <gdkmm/pixbuf.h>

#include "gtkmm2ext/visibility.h"

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
	LIBGTKMM2EXT_API void init (const char*);

	LIBGTKMM2EXT_API std::string fit_to_pixels (const std::string&, int pixel_width, Pango::FontDescription& font, int& actual_width, bool with_ellipses = false);
	LIBGTKMM2EXT_API std::pair<std::string, double> fit_to_pixels (cairo_t *, std::string, double);
	LIBGTKMM2EXT_API int pixel_width (const std::string& str, Pango::FontDescription& font);

	LIBGTKMM2EXT_API void get_ink_pixel_size (Glib::RefPtr<Pango::Layout>, 
						  int& width, int& height);
	

	LIBGTKMM2EXT_API void get_pixel_size (Glib::RefPtr<Pango::Layout>, 
					      int& width, int& height);
	
	LIBGTKMM2EXT_API void set_size_request_to_display_given_text (Gtk::Widget &w,
								      const gchar *text,
								      gint hpadding,
								      gint vpadding);
	
	LIBGTKMM2EXT_API void set_size_request_to_display_given_text (Gtk::Widget &w,
								      const std::vector<std::string>&,
								      gint hpadding,
								      gint vpadding);

	LIBGTKMM2EXT_API Glib::RefPtr<Gdk::Pixbuf> pixbuf_from_string (const std::string& name, 
								       const Pango::FontDescription& font, 
								       int clip_width, 
								       int clip_height, 
								       Gdk::Color fg);

	LIBGTKMM2EXT_API void set_popdown_strings (Gtk::ComboBoxText&, 
	                          const std::vector<std::string>&);
	
	template<class T> LIBGTKMM2EXT_API void deferred_delete (void *ptr) {
		delete static_cast<T *> (ptr);
	}

	LIBGTKMM2EXT_API GdkWindow* get_paned_handle (Gtk::Paned& paned);
	LIBGTKMM2EXT_API void set_decoration (Gtk::Window* win, Gdk::WMDecoration decor);
	LIBGTKMM2EXT_API void set_treeview_header_as_default_label(Gtk::TreeViewColumn *c);
	LIBGTKMM2EXT_API Glib::RefPtr<Gdk::Drawable> get_bogus_drawable();
	LIBGTKMM2EXT_API void detach_menu (Gtk::Menu&);

	LIBGTKMM2EXT_API Glib::RefPtr<Gdk::Window> window_to_draw_on (Gtk::Widget& w, Gtk::Widget** parent);

        LIBGTKMM2EXT_API bool possibly_translate_keyval_to_make_legal_accelerator (uint32_t& keyval);
        LIBGTKMM2EXT_API uint32_t possibly_translate_legal_accelerator_to_real_key (uint32_t keyval);

        LIBGTKMM2EXT_API int physical_screen_height (Glib::RefPtr<Gdk::Window>);
        LIBGTKMM2EXT_API int physical_screen_width (Glib::RefPtr<Gdk::Window>);

        LIBGTKMM2EXT_API void container_clear (Gtk::Container&);

	/* C++ API for rounded rectangles */
	
        LIBGTKMM2EXT_API void rounded_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r=10);
        LIBGTKMM2EXT_API void rounded_top_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r=10);
        LIBGTKMM2EXT_API void rounded_top_left_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r=10);
        LIBGTKMM2EXT_API void rounded_top_right_rectangle (Cairo::RefPtr<Cairo::Context> context, double x, double y, double w, double h, double r=10);
	LIBGTKMM2EXT_API void rounded_top_half_rectangle (Cairo::RefPtr<Cairo::Context>, double x, double y, double w, double h, double r=10);
	LIBGTKMM2EXT_API void rounded_bottom_half_rectangle (Cairo::RefPtr<Cairo::Context>, double x, double y, double w, double h, double r=10);
	LIBGTKMM2EXT_API void rounded_right_half_rectangle (Cairo::RefPtr<Cairo::Context>, double x, double y, double w, double h, double r=10);

	/* C API for rounded rectangles */

        LIBGTKMM2EXT_API void rounded_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
        LIBGTKMM2EXT_API void rounded_top_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
        LIBGTKMM2EXT_API void rounded_top_left_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
        LIBGTKMM2EXT_API void rounded_top_right_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
	LIBGTKMM2EXT_API void rounded_top_half_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
	LIBGTKMM2EXT_API void rounded_bottom_half_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);
	LIBGTKMM2EXT_API void rounded_right_half_rectangle (cairo_t*, double x, double y, double w, double h, double r=10);

	LIBGTKMM2EXT_API Gtk::Label* left_aligned_label (std::string const &);

	LIBGTKMM2EXT_API void set_no_tooltip_whatsoever (Gtk::Widget &);
	LIBGTKMM2EXT_API void enable_tooltips ();
	LIBGTKMM2EXT_API void disable_tooltips ();

	LIBGTKMM2EXT_API void convert_bgra_to_rgba (guint8 const *, guint8 * dst, int, int);
};

#endif /*  __gtkmm2ext_utils_h__ */
