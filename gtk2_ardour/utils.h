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
#include <libgnomecanvasmm/line.h>
#include <gdkmm/types.h>
#include <gtkmm/menushell.h>

#include "canvas.h"

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

extern sigc::signal<void>  DPIReset;

gint   just_hide_it (GdkEventAny*, Gtk::Window*);
void add_item_with_sensitivity (Gtk::Menu_Helpers::MenuList &, Gtk::Menu_Helpers::MenuElem, bool);

unsigned char* xpm2rgb  (const char** xpm, uint32_t& w, uint32_t& h);
unsigned char* xpm2rgba (const char** xpm, uint32_t& w, uint32_t& h);

ArdourCanvas::Points* get_canvas_points (std::string who, uint32_t npoints);

Pango::FontDescription get_font_for_style (std::string widgetname);

uint32_t rgba_from_style (std::string, uint32_t, uint32_t, uint32_t, uint32_t, std::string = "fg", int = Gtk::STATE_NORMAL, bool = true);

void decorate (Gtk::Window& w, Gdk::WMDecoration d);

bool canvas_item_visible (ArdourCanvas::Item* item);

void set_color (Gdk::Color&, int);

bool relay_key_press (GdkEventKey* ev, Gtk::Window* win);
bool forward_key_press (GdkEventKey* ev);
bool key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev);

Glib::RefPtr<Gdk::Pixbuf> get_xpm (std::string);
std::string get_icon_path (const char*);
Glib::RefPtr<Gdk::Pixbuf> get_icon (const char*);
static std::map<std::string, Glib::RefPtr<Gdk::Pixbuf> > xpm_map;
const char* const *get_xpm_data (std::string path);
std::string longest (std::vector<std::string>&);
bool key_is_legal_for_numeric_entry (guint keyval);
void reset_dpi ();
void set_pango_fontsize ();

void resize_window_to_proportion_of_monitor (Gtk::Window*, int, int);

std::string escape_underscores (std::string const &);
std::string escape_angled_brackets (std::string const &);

Gdk::Color unique_random_color (std::list<Gdk::Color> &);

#endif /* __ardour_gtk_utils_h__ */
