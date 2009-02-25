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
#include <glibmm/ustring.h>

#include "canvas.h"

namespace Gtk {
	class Window;
	class ComboBoxText;
	class Paned;
}

static inline double 
gain_to_slider_position (ARDOUR::gain_t g)
{
	if (g == 0) return 0;
	return pow((6.0*log(g)/log(2.0)+192.0)/198.0, 8.0);

}

static inline ARDOUR::gain_t 
slider_position_to_gain (double pos)
{
	/* XXX Marcus writes: this doesn't seem right to me. but i don't have a better answer ... */
	if (pos == 0.0) return 0;
	return pow (2.0,(sqrt(sqrt(sqrt(pos)))*198.0-192.0)/6.0);
}

Glib::ustring fit_to_pixels (const Glib::ustring&, int pixel_width, Pango::FontDescription& font, int& actual_width, bool with_ellipses = false);
int pixel_width (const Glib::ustring& str, Pango::FontDescription& font);

gint   just_hide_it (GdkEventAny*, Gtk::Window*);
void   allow_keyboard_focus (bool);

unsigned char* xpm2rgb  (const char** xpm, uint32_t& w, uint32_t& h);
unsigned char* xpm2rgba (const char** xpm, uint32_t& w, uint32_t& h);

ArdourCanvas::Points* get_canvas_points (std::string who, uint32_t npoints);

Pango::FontDescription* get_font_for_style (std::string widgetname);

uint32_t rgba_from_style (std::string, uint32_t, uint32_t, uint32_t, uint32_t, std::string = "fg", int = Gtk::STATE_NORMAL, bool = true);

Gdk::Color color_from_style (std::string widget_style_name, int state, std::string attr);
Glib::RefPtr<Gdk::GC> gc_from_style (std::string widget_style_name, int state, std::string attr);


void decorate (Gtk::Window& w, Gdk::WMDecoration d);

bool canvas_item_visible (ArdourCanvas::Item* item);

void set_color (Gdk::Color&, int);

bool key_press_focus_accelerator_handler (Gtk::Window& window, GdkEventKey* ev);
bool possibly_translate_keyval_to_make_legal_accelerator (uint32_t& keyval);

Glib::RefPtr<Gdk::Pixbuf> get_xpm (std::string);
Glib::RefPtr<Gdk::Pixbuf> get_icon (const char*);
static std::map<std::string, Glib::RefPtr<Gdk::Pixbuf> > xpm_map;
const char* const *get_xpm_data (std::string path);
std::string longest (std::vector<std::string>&);
bool key_is_legal_for_numeric_entry (guint keyval);
void reset_dpi ();
void set_pango_fontsize ();

#endif /* __ardour_gtk_utils_h__ */
