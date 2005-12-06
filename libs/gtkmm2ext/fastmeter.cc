/*
    Copyright (C) 2003 Paul Davis 

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

#include <iostream>
#include <cmath>
#include <algorithm>
#include <gdkmm/rectangle.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm/style.h>

using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

Glib::RefPtr<Gdk::Pixmap> FastMeter::v_pixmap;
Glib::RefPtr<Gdk::Bitmap> FastMeter::v_mask;
gint       FastMeter::v_pixheight = 0;
gint       FastMeter::v_pixwidth = 0;

Glib::RefPtr<Gdk::Pixmap> FastMeter::h_pixmap;
Glib::RefPtr<Gdk::Bitmap> FastMeter::h_mask;
gint       FastMeter::h_pixheight = 0;
gint       FastMeter::h_pixwidth = 0;

FastMeter::FastMeter (long hold, unsigned long dimen, Orientation o)
{
	orientation = o;
	hold_cnt = hold;
	hold_state = 0;
	current_peak = 0;
	current_level = 0;
	current_user_level = -100.0f;
	
	set_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);

	pixrect.set_x(0);
	pixrect.set_y(0);

	if (orientation == Vertical) {
		pixrect.set_width(min (v_pixwidth, (gint) dimen));
		pixrect.set_height(v_pixheight);
	} else {
		pixrect.set_width(h_pixwidth);
		pixrect.set_height(min (h_pixheight, (gint) dimen));
	}

	request_width = pixrect.get_width();
	request_height= pixrect.get_height();
}

FastMeter::~FastMeter ()
{
}

void
FastMeter::set_vertical_xpm (const char **xpm)
{
	if (v_pixmap == 0) {
		gint w, h;

		v_pixmap = Gdk::Pixmap::create_from_xpm(Gdk::Colormap::get_system(), v_mask, xpm);
		v_pixmap->get_size(w, h);
		
		v_pixheight = h;
		v_pixwidth = w;
	}
}

void
FastMeter::set_horizontal_xpm (const char **xpm)
{
	if (h_pixmap == 0) {
		gint w, h;
		
		h_pixmap = Gdk::Pixmap::create_from_xpm(Gdk::Colormap::get_system(), h_mask, xpm);
		h_pixmap->get_size(w, h);
		
		h_pixheight = h;
		h_pixwidth = w;
	}
}

void
FastMeter::set_hold_count (long val)
{
	if (val < 1) {
		val = 1;
	}
	
	hold_cnt = val;
	hold_state = 0;
	current_peak = 0;
	
	queue_draw ();
}

void
FastMeter::on_size_request (GtkRequisition* req)
{
	req->width = request_width;
	req->height = request_height;
}

bool
FastMeter::on_expose_event (GdkEventExpose* ev)
{
	if (orientation == Vertical) {
		return vertical_expose (ev);
	} else {
		return horizontal_expose (ev);
	}
}

bool
FastMeter::vertical_expose (GdkEventExpose* ev)
{
	Gdk::Rectangle intersect;
	gint top_of_meter;
	bool blit = false;
	bool intersecting = false;

	top_of_meter = (gint) floor (v_pixheight * current_level);
	pixrect.set_height(top_of_meter);

        intersect = pixrect.intersect(Glib::wrap(&ev->area), intersecting);
        if (intersecting) {
		/* draw the part of the meter image that we need. the area we draw is bounded "in reverse" (top->bottom)
		 */

		Glib::RefPtr<Gdk::Window> win(get_window());
		win->draw_drawable(get_style()->get_fg_gc(get_state()), v_pixmap, 
				   intersect.get_x(), v_pixheight - top_of_meter,
				   intersect.get_x(), v_pixheight - top_of_meter,
				   intersect.get_width(), intersect.get_height());
		
		blit = true;
	}

	/* draw peak bar */
		
	if (hold_state) {
		Glib::RefPtr<Gdk::Window> win(get_window());
		win->draw_drawable(get_style()->get_fg_gc(get_state()), v_pixmap,
				   intersect.get_x(), v_pixheight - (gint) floor (v_pixheight * current_peak),
				   intersect.get_x(), v_pixheight - (gint) floor (v_pixheight * current_peak),
				   intersect.get_width(), 3);
	}

	return true;
}

bool
FastMeter::horizontal_expose (GdkEventExpose* ev)
{
	Gdk::Rectangle intersect;
	bool intersecting = false;
	gint right_of_meter;

	right_of_meter = (gint) floor (h_pixwidth * current_level);
	pixrect.set_width(right_of_meter);

	intersect = pixrect.intersect(Glib::wrap(&ev->area), intersecting);
	if (intersecting) {
		/* draw the part of the meter image that we need. 
		 */

		Glib::RefPtr<Gdk::Window> win(get_window());
		win->draw_drawable(get_style()->get_fg_gc(get_state()), h_pixmap,
				   intersect.get_x(), intersect.get_y(),
				   intersect.get_x(), intersect.get_y(),
				   intersect.get_width(), intersect.get_height());
	}

	/* draw peak bar */
		
	if (hold_state) {
		Glib::RefPtr<Gdk::Window> win(get_window());
		win->draw_drawable(get_style()->get_fg_gc(get_state()), h_pixmap,
			      right_of_meter, intersect.get_y(),
			      right_of_meter, intersect.get_y(),
			      3, intersect.get_height());
	}

	return true;
}

void
FastMeter::set (float lvl, float usrlvl)
{
	current_level = lvl;
	current_user_level = usrlvl;
	
	if (lvl > current_peak) {
		current_peak = lvl;
		hold_state = hold_cnt;
	}
	
	if (hold_state > 0) {
		if (--hold_state == 0) {
			current_peak = lvl;
		}
	}

	queue_draw ();
}

void
FastMeter::clear ()
{
	current_level = 0;
	current_peak = 0;
	hold_state = 0;
	queue_draw ();
}
