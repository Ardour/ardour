/*
    Copyright (C) 2002 Paul Davis

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

#ifndef __ardour_gtk_utils_h__
#define __ardour_gtk_utils_h__

#include <string>
#include <cmath>
#include <vector>

#include "ardour/types.h"

#include <gdkmm/types.h>
#include <gtkmm/menushell.h>

#include "canvas/types.h"

namespace PBD {
        class Controllable;
        class ScopedConnectionList;
}

namespace Gtk {
	class Window;
	class ComboBoxText;
	class Paned;
        class Adjustment;
}

namespace ArdourCanvas {
	class Item;
}

namespace ARDOUR_UI_UTILS {

gint   just_hide_it (GdkEventAny*, Gtk::Window*);
void add_item_with_sensitivity (Gtk::Menu_Helpers::MenuList &, Gtk::Menu_Helpers::MenuElem, bool);

unsigned char* xpm2rgb  (const char** xpm, uint32_t& w, uint32_t& h);
unsigned char* xpm2rgba (const char** xpm, uint32_t& w, uint32_t& h);

ArdourCanvas::Points* get_canvas_points (std::string who, uint32_t npoints);

Pango::FontDescription sanitized_font (std::string const&);
Pango::FontDescription get_font_for_style (std::string widgetname);

void decorate (Gtk::Window& w, Gdk::WMDecoration d);

void set_color_from_rgb (Gdk::Color&, uint32_t);
void set_color_from_rgba (Gdk::Color&, uint32_t);
uint32_t gdk_color_to_rgba (Gdk::Color const&);
uint32_t contrasting_text_color (uint32_t c);

bool relay_key_press (GdkEventKey* ev, Gtk::Window* win = 0);
bool key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev);
bool emulate_key_event (unsigned int);

Glib::RefPtr<Gdk::Pixbuf> get_xpm (std::string);
std::vector<std::string> get_icon_sets ();
std::string get_icon_path (const char*, std::string icon_set = std::string(), bool is_image = true);
Glib::RefPtr<Gdk::Pixbuf> get_icon (const char*, std::string icon_set = std::string());
static std::map<std::string, Glib::RefPtr<Gdk::Pixbuf> > xpm_map;
const char* const *get_xpm_data (std::string path);
std::string longest (std::vector<std::string>&);
bool key_is_legal_for_numeric_entry (guint keyval);

void resize_window_to_proportion_of_monitor (Gtk::Window*, int, int);

std::string escape_underscores (std::string const &);
std::string escape_angled_brackets (std::string const &);

Gdk::Color unique_random_color (std::list<Gdk::Color> &);

std::string rate_as_string (float r);

bool windows_overlap (Gtk::Window *a, Gtk::Window *b);

bool overwrite_file_dialog (Gtk::Window& parent, std::string title, std::string text);
std::string show_gdk_event_state (int state);

} // namespace
#endif /* __ardour_gtk_utils_h__ */
