/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2009 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2005-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006 Doug McLain <doug@nostar.net>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __ardour_gtk_utils_h__
#define __ardour_gtk_utils_h__

#include <string>
#include <cmath>
#include <vector>
#include <map>

#include "ardour/types.h"

#include <gdkmm/types.h>
#include <gtkmm/menushell.h>

#include "canvas/types.h"

namespace PBD {
	class Controllable;
	class Controllable;
	class ScopedConnectionList;
}

namespace Gtk {
	class Window;
	class ComboBoxText;
	class Adjustment;
}

namespace ArdourCanvas {
	class Item;
}

namespace ARDOUR {
	class Auditioner;
	class Trigger;
}

namespace ARDOUR_UI_UTILS {

gint   just_hide_it (GdkEventAny*, Gtk::Window*);
void add_item_with_sensitivity (Gtk::Menu_Helpers::MenuList &, Gtk::Menu_Helpers::MenuElem, bool);

bool engine_is_running ();

unsigned char* xpm2rgb  (const char** xpm, uint32_t& w, uint32_t& h);
unsigned char* xpm2rgba (const char** xpm, uint32_t& w, uint32_t& h);

ArdourCanvas::Points* get_canvas_points (std::string who, uint32_t npoints);

Pango::FontDescription sanitized_font (std::string const&);
Pango::FontDescription get_font_for_style (std::string widgetname);

void decorate (Gtk::Window& w, Gdk::WMDecoration d);

Gdk::Color gdk_color_from_rgb (uint32_t);
Gdk::Color gdk_color_from_rgba (uint32_t);
uint32_t gdk_color_to_rgba (Gdk::Color const&);

void set_color_from_rgb (Gdk::Color&, uint32_t);
void set_color_from_rgba (Gdk::Color&, uint32_t);

bool relay_key_press (GdkEventKey* ev, Gtk::Window* win);
bool key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev);
bool emulate_key_event (unsigned int);

Glib::RefPtr<Gdk::Pixbuf> get_xpm (std::string);
std::vector<std::string> get_icon_sets ();
void get_color_themes (std::map<std::string,std::string>&);
std::string get_icon_path (const char*, std::string icon_set = std::string(), bool is_image = true);
Glib::RefPtr<Gdk::Pixbuf> get_icon (const char*, std::string icon_set = std::string());
static std::map<std::string, Glib::RefPtr<Gdk::Pixbuf> > xpm_map;
const char* const *get_xpm_data (std::string path);
std::string longest (std::vector<std::string>&);
bool key_is_legal_for_numeric_entry (guint keyval);

void resize_window_to_proportion_of_monitor (Gtk::Window*, int, int);

std::string escape_underscores (std::string const &);

Gdk::Color unique_random_color (std::list<Gdk::Color> &);

std::string rate_as_string (float r);
std::string samples_as_time_string (ARDOUR::samplecnt_t s, float r, bool show_samples = false);

std::string midi_channels_as_string (std::set<uint8_t> const&);

bool windows_overlap (Gtk::Window *a, Gtk::Window *b);

bool overwrite_file_dialog (Gtk::Window& parent, std::string title, std::string text);
bool running_from_source_tree ();

void inhibit_screensaver (bool);

void copy_patch_changes (boost::shared_ptr<ARDOUR::Auditioner>, boost::shared_ptr<ARDOUR::Trigger>);

bool convert_drop_to_paths (std::vector<std::string>&, const Gtk::SelectionData&);

} // namespace
#endif /* __ardour_gtk_utils_h__ */
