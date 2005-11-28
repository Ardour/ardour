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

    $Id$
*/

#ifndef __ardour_gtk_utils_h__
#define __ardour_gtk_utils_h__

#include <string>
#include <vector>
#include <cmath>
#include <ardour/types.h>
#include <libgnomecanvasmm/line.h>
#include <gdkmm/types.h>
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

std::string short_version (std::string, std::string::size_type target_length);
std::string fit_to_pixels (std::string, int pixel_width, std::string font);

int    atoi (const std::string&);
double atof (const std::string&);
void   strip_whitespace_edges (std::string& str);
void   url_decode (std::string&);
gint   just_hide_it (GdkEventAny*, Gtk::Window*);
void   allow_keyboard_focus (bool);

unsigned char* xpm2rgb  (const char** xpm, uint32_t& w, uint32_t& h);
unsigned char* xpm2rgba (const char** xpm, uint32_t& w, uint32_t& h);

ArdourCanvas::Points* get_canvas_points (std::string who, uint32_t npoints);

int channel_combo_get_channel_count (Gtk::ComboBoxText& combo);
Pango::FontDescription get_font_for_style (std::string widgetname);

gint pane_handler (GdkEventButton*, Gtk::Paned*);
uint32_t rgba_from_style (std::string style, uint32_t, uint32_t, uint32_t, uint32_t);

void decorate (Gtk::Window& w, Gdk::WMDecoration d);

bool canvas_item_visible (ArdourCanvas::Item* item);

#endif /* __ardour_gtk_utils_h__ */
