/*
    Copyright (C) 20002-2013 Paul Davis

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

#include <algorithm>
#include <string>

#include "ardour_ui.h"
#include "audio_clock.h"
#include "big_clock_window.h"
#include "public_editor.h"
#include "utils.h"

#include "i18n.h"

using std::min;
using std::string;

BigClockWindow::BigClockWindow (AudioClock& c) 
	: ArdourWindow (_("Big Clock"))
	, clock (c)
	, resize_in_progress (false)
	, original_height (0)
	, original_width (0)
	, original_font_size (0)
{
	ARDOUR_UI::Clock.connect (sigc::mem_fun (clock, &AudioClock::set));

	clock.set_corner_radius (0.0);
	clock.mode_changed.connect (sigc::mem_fun (*this, &BigClockWindow::reset_aspect_ratio));

	set_keep_above (true);
	set_border_width (0);
	add (clock);
	clock.show_all ();
}

void
BigClockWindow::on_unmap ()
{
	ArdourWindow::on_unmap ();

	PublicEditor::instance().reset_focus ();
}

bool
BigClockWindow::on_key_press_event (GdkEventKey* ev)
{
	return relay_key_press (ev, this);
}

void
BigClockWindow::on_realize ()
{
	int x, y, w, d, h;

	ArdourWindow::on_realize ();

	get_window()->set_decorations (Gdk::DECOR_BORDER|Gdk::DECOR_RESIZEH);
	get_window()->get_geometry (x, y, w, h, d);

	reset_aspect_ratio ();

	original_height = h;
	original_width = w;

	Pango::FontDescription fd (clock.get_style()->get_font());
	original_font_size = fd.get_size ();

	if (!fd.get_size_is_absolute ()) {
		original_font_size /= PANGO_SCALE;
	}
}

void
BigClockWindow::on_size_allocate (Gtk::Allocation& alloc)
{
	ArdourWindow::on_size_allocate (alloc);

	if (!resize_in_progress) {
		Glib::signal_idle().connect (sigc::bind (sigc::mem_fun (*this, &BigClockWindow::text_resizer), 0, 0));
		resize_in_progress = true;
	}
}

void
BigClockWindow::reset_aspect_ratio ()
{
	Gtk::Requisition req;

	clock.size_request (req);

	float aspect = req.width/(float)req.height;
	Gdk::Geometry geom;

	geom.min_aspect = aspect;
	geom.max_aspect = aspect;
	
	set_geometry_hints (clock, geom, Gdk::HINT_ASPECT);
}

bool
BigClockWindow::text_resizer (int, int)
{
	resize_in_progress = false;

	Glib::RefPtr<Gdk::Window> win = get_window();
	Pango::FontDescription fd (clock.get_style()->get_font());
	int current_size = fd.get_size ();
	int x, y, w, h, d;

	if (!fd.get_size_is_absolute ()) {
		current_size /= PANGO_SCALE;
	}

	win->get_geometry (x, y, w, h, d);

	double scale  = min (((double) w / (double) original_width),
	                     ((double) h / (double) original_height));

	int size = (int) lrintf (original_font_size * scale);

	if (size != current_size) {

		string family = fd.get_family();
		char buf[family.length()+16];
		snprintf (buf, family.length()+16, "%s %d", family.c_str(), size);

		try {
			Pango::FontDescription fd (buf);
			Glib::RefPtr<Gtk::RcStyle> rcstyle = clock.get_modifier_style ();
			rcstyle->set_font (fd);
			clock.modify_style (rcstyle);
		}

		catch (...) {
			/* oh well, do nothing */
		}
	}

	return false;
}

