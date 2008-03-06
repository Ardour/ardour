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
#include <gdkmm/rectangle.h>
#include <gtkmm2ext/fastmeter.h>
#include <gtkmm2ext/utils.h>
#include <gtkmm/style.h>
#include <string.h>

#define UINT_TO_RGB(u,r,g,b) { (*(r)) = ((u)>>16)&0xff; (*(g)) = ((u)>>8)&0xff; (*(b)) = (u)&0xff; }
#define UINT_TO_RGBA(u,r,g,b,a) { UINT_TO_RGB(((u)>>8),r,g,b); (*(a)) = (u)&0xff; }
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;


int FastMeter::min_v_pixbuf_size = 10;
int FastMeter::max_v_pixbuf_size = 1024;
Glib::RefPtr<Gdk::Pixbuf>* FastMeter::v_pixbuf_cache = 0;

int FastMeter::min_h_pixbuf_size = 10;
int FastMeter::max_h_pixbuf_size = 1024;
Glib::RefPtr<Gdk::Pixbuf>* FastMeter::h_pixbuf_cache = 0;

int FastMeter::rgb0 = 0;
int FastMeter::rgb1 = 0;
int FastMeter::rgb2 = 0;
int FastMeter::rgb3 = 0;

FastMeter::FastMeter (long hold, unsigned long dimen, Orientation o, int len, int clr0, int clr1, int clr2, int clr3)
{
	orientation = o;
	hold_cnt = hold;
	hold_state = 0;
	current_peak = 0;
	current_level = 0;
	last_peak_rect.width = 0;
	last_peak_rect.height = 0;
	rgb0 = clr0;
	rgb1 = clr1;
	rgb2 = clr2;
	rgb3 = clr3;

	set_events (BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK);

	pixrect.x = 0;
	pixrect.y = 0;

	if (orientation == Vertical) {
		if (!len)
			len = 250;
		pixbuf = request_vertical_meter(dimen, len);
	} else {
		if (!len)
			len = 186;
		pixbuf = request_horizontal_meter(len, dimen);
	}

	pixheight = pixbuf->get_height();
	pixwidth  = pixbuf->get_width();

	if (orientation == Vertical) {
		pixrect.width = min (pixwidth, (gint) dimen);
		pixrect.height = pixheight;
	} else {
		pixrect.width = pixwidth;
		pixrect.height = min (pixheight, (gint) dimen);
	}

	request_width = pixrect.width;
	request_height= pixrect.height;
}

Glib::RefPtr<Gdk::Pixbuf> FastMeter::request_vertical_meter(int width, int height)
{
	if (height < min_v_pixbuf_size)
		height = min_v_pixbuf_size;
	if (height > max_v_pixbuf_size)
		height = max_v_pixbuf_size;
	
	//int index = height - 1;

	//if (v_pixbuf_cache == 0) {
	//	v_pixbuf_cache = (Glib::RefPtr<Gdk::Pixbuf>*) malloc(sizeof(Glib::RefPtr<Gdk::Pixbuf>) * max_v_pixbuf_size);
	//	memset(v_pixbuf_cache,0,sizeof(Glib::RefPtr<Gdk::Pixbuf>) * max_v_pixbuf_size);
	//}
	Glib::RefPtr<Gdk::Pixbuf> ret;// = v_pixbuf_cache[index];
	//if (ret)
	//	return ret;

	guint8* data;

	data = (guint8*) malloc(width*height * 3);

	guint8 r,g,b,r0,g0,b0,r1,g1,b1,r2,g2,b2,r3,g3,b3,a;

	UINT_TO_RGBA (rgb0, &r0, &g0, &b0, &a);
	UINT_TO_RGBA (rgb1, &r1, &g1, &b1, &a);
	UINT_TO_RGBA (rgb2, &r2, &g2, &b2, &a);
	UINT_TO_RGBA (rgb3, &r3, &g3, &b3, &a);
	// fake log calculation copied from log_meter.h
	// actual calculation:
	// log_meter(0.0f) =
	//  def = (0.0f + 20.0f) * 2.5f + 50f
	//  return def / 115.0f
	int knee = (int)floor((float)height * 100.0f / 115.0f);
	int y;

	for (y = 0; y < knee/2; y++) {

		r = (guint8)floor((float)abs(r1 - r0) * (float)y / (float)(knee/2));
		(r0 >= r1) ? r = r0 - r : r += r0;
			
		g = (guint8)floor((float)abs(g1 - g0) * (float)y / (float)(knee/2));
		(g0 >= g1) ? g = g0 - g : g += g0;

		b = (guint8)floor((float)abs(b1 - b0) * (float)y / (float)(knee/2));
		(b0 >= b1) ? b = b0 - b : b += b0;

		for (int x = 0; x < width; x++) {
			data[ (x+(height-y-1)*width) * 3 + 0 ] = r;
			data[ (x+(height-y-1)*width) * 3 + 1 ] = g;
			data[ (x+(height-y-1)*width) * 3 + 2 ] = b;
		}
	}

	int offset = knee - y;
	for (int i=0; i < offset; i++,y++) {

		r = (guint8)floor((float)abs(r2 - r1) * (float)i / (float)offset);
		(r1 >= r2) ? r = r1 - r : r += r1;

		g = (guint8)floor((float)abs(g2 - g1) * (float)i / (float)offset);
		(g1 >= g2) ? g = g1 - g : g += g1;

		b = (guint8)floor((float)abs(b2 - b1) * (float)i / (float)offset);
		(b1 >= b2) ? b = b1 - b : b += b1;

		for (int x = 0; x < width; x++) {
			data[ (x+(height-y-1)*width) * 3 + 0 ] = r;
			data[ (x+(height-y-1)*width) * 3 + 1 ] = g;
			data[ (x+(height-y-1)*width) * 3 + 2 ] = b;
		}
	}

	for (; y < height; y++) {
		for (int x = 0; x < width; x++) {
			data[ (x+(height-y-1)*width) * 3 + 0 ] = r3;
			data[ (x+(height-y-1)*width) * 3 + 1 ] = g3;
			data[ (x+(height-y-1)*width) * 3 + 2 ] = b3;
		}
	}
	
	ret = Pixbuf::create_from_data(data, COLORSPACE_RGB, false, 8, width, height, width * 3);
	//v_pixbuf_cache[index] = ret;

	return ret;
}

Glib::RefPtr<Gdk::Pixbuf> FastMeter::request_horizontal_meter(int width, int height)
{
	if (width < min_h_pixbuf_size)
		width = min_h_pixbuf_size;
	if (width > max_h_pixbuf_size)
		width = max_h_pixbuf_size;
	
	int index = width - 1;
	
	if (h_pixbuf_cache == 0) {
		h_pixbuf_cache = (Glib::RefPtr<Gdk::Pixbuf>*) malloc(sizeof(Glib::RefPtr<Gdk::Pixbuf>) * max_h_pixbuf_size);
		memset(h_pixbuf_cache,0,sizeof(Glib::RefPtr<Gdk::Pixbuf>) * max_h_pixbuf_size);
	}
	Glib::RefPtr<Gdk::Pixbuf> ret = h_pixbuf_cache[index];
	if (ret)
		return ret;

	guint8* data;

	data = (guint8*) malloc(width*height * 3);
	
	guint8 r,g,b;
	r=0;
	g=255;
	b=0;

	// fake log calculation copied from log_meter.h
	// actual calculation:
	// log_meter(0.0f) =
	//  def = (0.0f + 20.0f) * 2.5f + 50f
	//  return def / 115.0f
	int knee = (int)floor((float)width * 100.0f / 115.0f);
	
	int x;
	
	for (x = 0; x < knee / 2; x++) {

		r = (guint8)floor(255.0 * (float)x/(float)(knee / 2));
		
		for (int y = 0; y < height; y++) {
			data[ (x+(height-y-1)*width) * 3 + 0 ] = r;
			data[ (x+(height-y-1)*width) * 3 + 1 ] = g;
			data[ (x+(height-y-1)*width) * 3 + 2 ] = b;
		}
	}
	
	for (; x < knee; x++) {

		g = 255 - (guint8)floor(170.0 * (float)(x - knee/ 2)/(float)(knee / 2));
		
		for (int y = 0; y < height; y++) {
			data[ (x+(height-y-1)*width) * 3 + 0 ] = r;
			data[ (x+(height-y-1)*width) * 3 + 1 ] = g;
			data[ (x+(height-y-1)*width) * 3 + 2 ] = b;
		}
	}

	r=255;
	g=0;
	b=0;
	for (; x < width; x++) {
		for (int y = 0; y < height; y++) {
			data[ (x+(height-y-1)*width) * 3 + 0 ] = r;
			data[ (x+(height-y-1)*width) * 3 + 1 ] = g;
			data[ (x+(height-y-1)*width) * 3 + 2 ] = b;
		}
	}
	
	ret = Pixbuf::create_from_data(data, COLORSPACE_RGB, false, 8, width, height, width * 3);
	h_pixbuf_cache[index] = ret;

	return ret;
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
		req->height = max(req->height, min_v_pixbuf_size);
		req->height = min(req->height, max_v_pixbuf_size);

		req->width  = request_width;

	} else {

		req->width  = request_width;
		req->width  = max(req->width,  min_h_pixbuf_size);
		req->width  = min(req->width,  max_h_pixbuf_size);

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
		h = max(h, min_v_pixbuf_size);
		h = min(h, max_v_pixbuf_size);

		if ( h != alloc.get_height())
			alloc.set_height(h);

		if (pixheight != h) {
			pixbuf = request_vertical_meter(request_width, h);
		}

	} else {

		if (alloc.get_height() != request_height) {
			alloc.set_height(request_height);
		}

		int w = alloc.get_width();
		w = max(w, min_h_pixbuf_size);
		w = min(w, max_h_pixbuf_size);

		if ( w != alloc.get_width())
			alloc.set_width(w);

		if (pixwidth != w) {
			pixbuf = request_horizontal_meter(w, request_height);
		}
	}

	pixheight = pixbuf->get_height();
	pixwidth  = pixbuf->get_width();

	DrawingArea::on_size_allocate(alloc);
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
	gint top_of_meter;
	GdkRectangle intersection;
	GdkRectangle background;

	top_of_meter = (gint) floor (pixheight * current_level);
	
	/* reset the height & origin of the rect that needs to show the pixbuf
	 */

	pixrect.height = top_of_meter;
	pixrect.y = pixheight - top_of_meter;

	background.x = 0;
	background.y = 0;
	background.width = pixrect.width;
	background.height = pixheight - top_of_meter;

	if (gdk_rectangle_intersect (&background, &ev->area, &intersection)) {
		get_window()->draw_rectangle (get_style()->get_black_gc(), true, 
					      intersection.x, intersection.y,
					      intersection.width, intersection.height);
	}

	if (gdk_rectangle_intersect (&pixrect, &ev->area, &intersection)) {
		// draw the part of the meter image that we need. the area we draw is bounded "in reverse" (top->bottom)
		get_window()->draw_pixbuf(get_style()->get_fg_gc(get_state()), pixbuf, 
					  intersection.x, intersection.y,
					  intersection.x, intersection.y,
					  intersection.width, intersection.height,
					  Gdk::RGB_DITHER_NONE, 0, 0);
	}

	// draw peak bar 

	if (hold_state) {
		last_peak_rect.x = 0;
		last_peak_rect.width = pixwidth;
		last_peak_rect.y = pixheight - (gint) floor (pixheight * current_peak);
		last_peak_rect.height = min(3, pixheight - last_peak_rect.y);

		get_window()->draw_pixbuf (get_style()->get_fg_gc(get_state()), pixbuf,
					   0, last_peak_rect.y,
					   0, last_peak_rect.y,
					   pixwidth, last_peak_rect.height,
					   Gdk::RGB_DITHER_NONE, 0, 0);
	} else {
		last_peak_rect.width = 0;
		last_peak_rect.height = 0;
	}

	return TRUE;
}

bool
FastMeter::horizontal_expose (GdkEventExpose* ev)
{
	gint right_of_meter;
	GdkRectangle intersection;
	GdkRectangle background;

	right_of_meter = (gint) floor (pixwidth * current_level);
	pixrect.width = right_of_meter;

	background.x = 0;
	background.y = 0;
	background.width  = pixwidth - right_of_meter;
	background.height = pixrect.height;

	if (gdk_rectangle_intersect (&background, &ev->area, &intersection)) {
		get_window()->draw_rectangle (get_style()->get_black_gc(), true, 
					      intersection.x + right_of_meter, intersection.y,
					      intersection.width, intersection.height);
	}
	
	if (gdk_rectangle_intersect (&pixrect, &ev->area, &intersection)) {
		// draw the part of the meter image that we need. the area we draw is bounded "in reverse" (top->bottom)
		get_window()->draw_pixbuf(get_style()->get_fg_gc(get_state()), pixbuf, 
					  intersection.x, intersection.y,
					  intersection.x, intersection.y,
					  pixrect.width, intersection.height,
					  Gdk::RGB_DITHER_NONE, 0, 0);
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
	
	rect.x = 0;
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
FastMeter::queue_horizontal_redraw (const Glib::RefPtr<Gdk::Window>& win, float old_level)
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
