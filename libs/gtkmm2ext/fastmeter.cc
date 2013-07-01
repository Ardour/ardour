/*
    Copyright (C) 2003-2006 Paul Davis

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
#include <cstring>

#include <gdkmm/rectangle.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/utils.h>

#define UINT_TO_RGB(u,r,g,b) { (*(r)) = ((u)>>16)&0xff; (*(g)) = ((u)>>8)&0xff; (*(b)) = (u)&0xff; }
#define UINT_TO_RGBA(u,r,g,b,a) { UINT_TO_RGB(((u)>>8),r,g,b); (*(a)) = (u)&0xff; }
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;

int FastMeter::min_pattern_metric_size = 10;
int FastMeter::max_pattern_metric_size = 1024;

FastMeter::PatternMap FastMeter::v_pattern_cache;
FastMeter::PatternMap FastMeter::h_pattern_cache;

FastMeter::FastMeter (long hold, unsigned long dimen, Orientation o, int len, int clr0, int clr1, int clr2, int clr3)
{
	orientation = o;
	hold_cnt = hold;
	resized = true;
	hold_state = 0;
	current_peak = 0;
	current_level = 0;
	last_peak_rect.width = 0;
	last_peak_rect.height = 0;
	_clr0 = clr0;
	_clr1 = clr1;
	_clr2 = clr2;
	_clr3 = clr3;

	_bgc0 = 0x000000ff;
	_bgc1 = 0x333333ff;
	_bgc2 = 0x333333ff;
	_bgc3 = 0x444444ff;

	set_events (BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK);

	pixrect.x = 1;
	pixrect.y = 0;

	if (orientation == Vertical) {
		if (!len) {
			len = 250;
		}
		fgpattern = request_vertical_meter(dimen, len, clr0, clr1, clr2, clr3);
		bgpattern = request_vertical_meter(dimen, len, _bgc0, _bgc1, _bgc2, _bgc3);
		pixheight = len;
		pixwidth = dimen;
	} else {
		if (!len) {
			len = 186; // interesting size, eh?
		}
		fgpattern = request_horizontal_meter(len, dimen, clr0, clr1, clr2, clr3);
		bgpattern = request_horizontal_meter(len, dimen, _bgc0, _bgc1, _bgc2, _bgc3);
		pixheight = dimen;
		pixwidth = len;
	}

	if (orientation == Vertical) {
		pixrect.width = pixwidth;
		pixrect.height = pixheight;
	} else {
		pixrect.width = pixwidth;
		pixrect.height = min (pixheight, (gint) dimen);
	}

	request_width = pixrect.width + 2;
	request_height= pixrect.height;
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::generate_meter_pattern (
		int width, int height, int clr0, int clr1, int clr2, int clr3)
{
	guint8 r0,g0,b0,r1,g1,b1,r2,g2,b2,r3,g3,b3,a;

	/*
	  The knee is the hard transition point (e.g. at 0dB where the colors
	  change dramatically to make clipping apparent). Thus there are two
	  gradients in the pattern, the "normal range" and the "clip range", which
	  are separated at the knee point.

	  clr0: color at bottom of normal range gradient
	  clr1: color at top of normal range gradient
	  clr2: color at bottom of clip range gradient
	  clr3: color at top of clip range gradient
	*/

	UINT_TO_RGBA (clr0, &r0, &g0, &b0, &a);
	UINT_TO_RGBA (clr1, &r1, &g1, &b1, &a);
	UINT_TO_RGBA (clr2, &r2, &g2, &b2, &a);
	UINT_TO_RGBA (clr3, &r3, &g3, &b3, &a);

	// fake log calculation copied from log_meter.h
	// actual calculation:
	// log_meter(0.0f) =
	//  def = (0.0f + 20.0f) * 2.5f + 50f
	//  return def / 115.0f

	const int knee = (int)floor((float)height * 100.0f / 115.0f);
	cairo_pattern_t* pat = cairo_pattern_create_linear (0.0, 0.0, width, height);

	/*
	  Cairo coordinate space goes downwards as y value goes up, so invert
	  knee-based positions by using (1.0 - y)
	*/

	// Clip range top
	cairo_pattern_add_color_stop_rgb (pat, 0.0,
	                                  r3/255.0, g3/255.0, b3/255.0);

	// Clip range bottom
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r2/255.0, g2/255.0, b2/255.0);

	// Normal range top (double-stop at knee)
	cairo_pattern_add_color_stop_rgb (pat, 1.0 - (knee/(double)height),
	                                  r1/255.0, g1/255.0, b1/255.0);

	// Normal range bottom
	cairo_pattern_add_color_stop_rgb (pat, 1.0,
	                                  r0/255.0, g0/255.0, b0/255.0); // top

	Cairo::RefPtr<Cairo::Pattern> p (new Cairo::Pattern (pat, false));

	return p;
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::request_vertical_meter(
		int width, int height, int clr0, int clr1, int clr2, int clr3)
{
	if (height < min_pattern_metric_size)
		height = min_pattern_metric_size;
	if (height > max_pattern_metric_size)
		height = max_pattern_metric_size;

	const PatternMapKey key (width, height, clr0, clr1, clr2, clr3);
	PatternMap::iterator i;
	if ((i = v_pattern_cache.find (key)) != v_pattern_cache.end()) {
		return i->second;
	}

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_pattern (
		width, height, clr0, clr1, clr2, clr3);
	v_pattern_cache[key] = p;

	return p;
}

Cairo::RefPtr<Cairo::Pattern>
FastMeter::request_horizontal_meter(
		int width, int height, int clr0, int clr1, int clr2, int clr3)
{
	if (width < min_pattern_metric_size)
		width = min_pattern_metric_size;
	if (width > max_pattern_metric_size)
		width = max_pattern_metric_size;

	const PatternMapKey key (width, height, clr0, clr1, clr2, clr3);
	PatternMap::iterator i;
	if ((i = h_pattern_cache.find (key)) != h_pattern_cache.end()) {
		return i->second;
	}

	/* flip height/width so that we get the right pattern */

	Cairo::RefPtr<Cairo::Pattern> p = generate_meter_pattern (
		height, width, clr0, clr1, clr2, clr3);

	/* rotate to make it horizontal */

	cairo_matrix_t m;
	cairo_matrix_init_rotate (&m, -M_PI/2.0);
	cairo_pattern_set_matrix (p->cobj(), &m);

	h_pattern_cache[key] = p;

	return p;
}

FastMeter::~FastMeter ()
{
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
	if (orientation == Vertical) {

		req->height = request_height;
		req->height = max(req->height, min_pattern_metric_size);
		req->height = min(req->height, max_pattern_metric_size);

		req->width  = request_width;

	} else {

		req->width  = request_width;
		req->width  = max(req->width,  min_pattern_metric_size);
		req->width  = min(req->width,  max_pattern_metric_size);

		req->height = request_height;
	}

}

void
FastMeter::on_size_allocate (Gtk::Allocation &alloc)
{
	if (orientation == Vertical) {

		if (alloc.get_width() != request_width) {
			alloc.set_width (request_width);
		}

		int h = alloc.get_height();
		h = max (h, min_pattern_metric_size);
		h = min (h, max_pattern_metric_size);

		if (h != alloc.get_height()) {
			alloc.set_height (h);
		}

		if (pixheight != h) {
			fgpattern = request_vertical_meter (
				request_width, h, _clr0, _clr1, _clr2, _clr3);
			bgpattern = request_vertical_meter (
				request_width, h, _bgc0, _bgc1, _bgc2, _bgc3);
			pixheight = h;
			pixwidth  = request_width - 2;
		}

	} else {

		if (alloc.get_height() != request_height) {
			alloc.set_height(request_height);
		}

		int w = alloc.get_width();
		w = max (w, min_pattern_metric_size);
		w = min (w, max_pattern_metric_size);

		if (w != alloc.get_width()) {
			alloc.set_width (w);
		}

		if (pixwidth != w) {
			fgpattern = request_horizontal_meter (
				w, request_height, _clr0, _clr1, _clr2, _clr3);
			bgpattern = request_horizontal_meter (
				w, request_height, _bgc0, _bgc1, _bgc2, _bgc3);
			pixheight = request_height;
			pixwidth  = w;
		}
	}

	DrawingArea::on_size_allocate (alloc);
	resized = true;
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
	Glib::RefPtr<Gdk::Window> win = get_window ();
	gint top_of_meter;
	GdkRectangle intersection;
	GdkRectangle background;

	cairo_t* cr = gdk_cairo_create (get_window ()->gobj());

	if (resized) {
		cairo_set_source_rgb (cr, 0, 0, 0); // black
		cairo_rectangle (cr, 0, 0, pixrect.width + 2, pixheight);
		cairo_fill (cr);
	}

	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	top_of_meter = (gint) floor (pixheight * current_level);

	/* reset the height & origin of the rect that needs to show the pixbuf
	 */

	pixrect.height = top_of_meter;
	pixrect.y = pixheight - top_of_meter;

	background.x = 1;
	background.y = 0;
	background.width = pixrect.width;
	background.height = pixheight - top_of_meter;

	if (gdk_rectangle_intersect (&background, &ev->area, &intersection)) {
		cairo_set_source (cr, bgpattern->cobj());
		cairo_rectangle (cr, intersection.x, intersection.y, intersection.width, intersection.height);
		cairo_fill (cr);
	}

	if (gdk_rectangle_intersect (&pixrect, &ev->area, &intersection)) {
		// draw the part of the meter image that we need. the area we draw is bounded "in reverse" (top->bottom)
		cairo_set_source (cr, fgpattern->cobj());
		cairo_rectangle (cr, intersection.x, intersection.y, intersection.width, intersection.height);
		cairo_fill (cr);
	}

	// draw peak bar

	if (hold_state) {
		last_peak_rect.x = 1;
		last_peak_rect.width = pixwidth;
		last_peak_rect.y = pixheight - (gint) floor (pixheight * current_peak);
		last_peak_rect.height = min(3, pixheight - last_peak_rect.y);

		cairo_set_source (cr, fgpattern->cobj());
		cairo_rectangle (cr, 1, last_peak_rect.y, pixwidth, last_peak_rect.height);
		cairo_fill (cr);

	} else {
		last_peak_rect.width = 0;
		last_peak_rect.height = 0;
	}

	cairo_destroy (cr);

	return TRUE;
}

bool
FastMeter::horizontal_expose (GdkEventExpose* ev)
{
	Glib::RefPtr<Gdk::Window> win = get_window ();
	gint right_of_meter;
	GdkRectangle intersection;
	GdkRectangle background;

	cairo_t* cr = gdk_cairo_create (get_window ()->gobj());
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	right_of_meter = (gint) floor (pixwidth * current_level);
	pixrect.width = right_of_meter;

	background.x = 0;
	background.y = 0;
	background.width  = pixwidth - right_of_meter;
	background.height = pixrect.height;

	if (gdk_rectangle_intersect (&background, &ev->area, &intersection)) {
		cairo_set_source_rgb (cr, 0, 0, 0); // black
		cairo_rectangle (cr, intersection.x + right_of_meter, intersection.y, intersection.width, intersection.height);
		cairo_fill (cr);
	}

	if (gdk_rectangle_intersect (&pixrect, &ev->area, &intersection)) {
		// draw the part of the meter image that we need. the area we draw is bounded "in reverse" (top->bottom)
		cairo_matrix_t m;
		cairo_matrix_init_translate (&m, -intersection.x, -intersection.y);
		cairo_pattern_set_matrix (fgpattern->cobj(), &m);
		cairo_set_source (cr, fgpattern->cobj());
		cairo_rectangle (cr, intersection.x, intersection.y, pixrect.width, intersection.height);
		cairo_fill (cr);
	}

	// draw peak bar
	// XXX: peaks don't work properly
	/*
	if (hold_state && intersection.height > 0) {
		gint x = (gint) floor(pixwidth * current_peak);

		get_window()->draw_pixbuf (get_style()->get_fg_gc(get_state()), pixbuf,
					   x, intersection.y,
					   x, intersection.y,
					   3, intersection.height,
					   Gdk::RGB_DITHER_NONE, 0, 0);
	}
	*/

	cairo_destroy (cr);

	return true;
}

void
FastMeter::set (float lvl)
{
	float old_level = current_level;
	float old_peak = current_peak;

	current_level = lvl;

	if (lvl > current_peak) {
		current_peak = lvl;
		hold_state = hold_cnt;
	}

	if (hold_state > 0) {
		if (--hold_state == 0) {
			current_peak = lvl;
		}
	}

	if (current_level == old_level && current_peak == old_peak && hold_state == 0) {
		return;
	}


	Glib::RefPtr<Gdk::Window> win;

	if ((win = get_window()) == 0) {
		queue_draw ();
		return;
	}

	if (orientation == Vertical) {
		queue_vertical_redraw (win, old_level);
	} else {
		queue_horizontal_redraw (win, old_level);
	}
}

void
FastMeter::queue_vertical_redraw (const Glib::RefPtr<Gdk::Window>& win, float old_level)
{
	GdkRectangle rect;

	gint new_top = (gint) floor (pixheight * current_level);

	rect.x = 1;
	rect.width = pixwidth;
	rect.height = new_top;
	rect.y = pixheight - new_top;

	if (current_level > old_level) {
		/* colored/pixbuf got larger, just draw the new section */
		/* rect.y stays where it is because of X coordinates */
		/* height of invalidated area is between new.y (smaller) and old.y
		   (larger).
		   X coordinates just make my brain hurt.
		*/
		rect.height = pixrect.y - rect.y;
	} else {
		/* it got smaller, compute the difference */
		/* rect.y becomes old.y (the smaller value) */
		rect.y = pixrect.y;
		/* rect.height is the old.y (smaller) minus the new.y (larger)
		*/
		rect.height = pixrect.height - rect.height;
	}

	GdkRegion* region = 0;
	bool queue = false;

	if (rect.height != 0) {

		/* ok, first region to draw ... */

		region = gdk_region_rectangle (&rect);
		queue = true;
	}

	/* redraw the last place where the last peak hold bar was;
	   the next expose will draw the new one whether its part of
	   expose region or not.
	*/

	if (last_peak_rect.width * last_peak_rect.height != 0) {
		if (!queue) {
			region = gdk_region_new ();
			queue = true;
		}
		gdk_region_union_with_rect (region, &last_peak_rect);
	}

	if (queue) {
		gdk_window_invalidate_region (win->gobj(), region, true);
	}
	if (region) {
		gdk_region_destroy(region);
		region = 0;
	}
}

void
FastMeter::queue_horizontal_redraw (const Glib::RefPtr<Gdk::Window>& /*win*/, float /*old_level*/)
{
	/* XXX OPTIMIZE (when we have some horizontal meters) */
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
