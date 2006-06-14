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

using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace std;


int FastMeter::min_v_pixbuf_size = 50;
int FastMeter::max_v_pixbuf_size = 1024;
Glib::RefPtr<Gdk::Pixbuf>* FastMeter::v_pixbuf_cache = 0;

int FastMeter::min_h_pixbuf_size = 50;
int FastMeter::max_h_pixbuf_size = 1024;
Glib::RefPtr<Gdk::Pixbuf>* FastMeter::h_pixbuf_cache = 0;


FastMeter::FastMeter (long hold, unsigned long dimen, Orientation o)
{
	orientation = o;
	hold_cnt = hold;
	hold_state = 0;
	current_peak = 0;
	current_level = 0;
	current_user_level = -100.0f;
	
	set_events (BUTTON_PRESS_MASK|BUTTON_RELEASE_MASK);

	pixrect.x = 0;
	pixrect.y = 0;


	if (orientation == Vertical) {
		pixbuf = request_vertical_meter(250);
	} else {
		pixbuf = request_horizontal_meter(186);
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

Glib::RefPtr<Gdk::Pixbuf> FastMeter::request_vertical_meter(int length)
{
	if (length < min_v_pixbuf_size)
		length = min_v_pixbuf_size;
	if (length > max_v_pixbuf_size)
		length = max_v_pixbuf_size;
	
	int index = length - 1;

	if (v_pixbuf_cache == 0) {
		v_pixbuf_cache = (Glib::RefPtr<Gdk::Pixbuf>*) malloc(sizeof(Glib::RefPtr<Gdk::Pixbuf>) * max_v_pixbuf_size);
		memset(v_pixbuf_cache,0,sizeof(Glib::RefPtr<Gdk::Pixbuf>) * max_v_pixbuf_size);
	}
	Glib::RefPtr<Gdk::Pixbuf> ret = v_pixbuf_cache[index];
	if (ret)
		return ret;

	guint8* data;
	int width = 5;
	int height = length;

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
	int knee = (int)floor((float)height * 100.0f / 115.0f);
	
	int y;
	
	for (y = 0; y < knee / 2; y++) {

		r = (guint8)floor(255.0 * (float)y/(float)(knee / 2));
		
		for (int x = 0; x < width; x++) {
			data[ (x+(height-y-1)*width) * 3 + 0 ] = r;
			data[ (x+(height-y-1)*width) * 3 + 1 ] = g;
			data[ (x+(height-y-1)*width) * 3 + 2 ] = b;
		}
	}
	
	for (; y < knee; y++) {

		g = 255 - (guint8)floor(170.0 * (float)(y - knee/ 2)/(float)(knee / 2));
		
		for (int x = 0; x < width; x++) {
			data[ (x+(height-y-1)*width) * 3 + 0 ] = r;
			data[ (x+(height-y-1)*width) * 3 + 1 ] = g;
			data[ (x+(height-y-1)*width) * 3 + 2 ] = b;
		}
	}

	r=255;
	g=0;
	b=0;
	for (; y < height; y++) {
		for (int x = 0; x < width; x++) {
			data[ (x+(height-y-1)*width) * 3 + 0 ] = r;
			data[ (x+(height-y-1)*width) * 3 + 1 ] = g;
			data[ (x+(height-y-1)*width) * 3 + 2 ] = b;
		}
	}
	
	ret = Pixbuf::create_from_data(data, COLORSPACE_RGB, false, 8, width, height, width * 3);
	v_pixbuf_cache[index] = ret;

	return ret;
}

Glib::RefPtr<Gdk::Pixbuf> FastMeter::request_horizontal_meter(int length)
{
	if (length < min_h_pixbuf_size)
		length = min_h_pixbuf_size;
	if (length > max_h_pixbuf_size)
		length = max_h_pixbuf_size;
	
	int index = length - 1;

	if (h_pixbuf_cache == 0) {
		h_pixbuf_cache = (Glib::RefPtr<Gdk::Pixbuf>*) malloc(sizeof(Glib::RefPtr<Gdk::Pixbuf>) * max_h_pixbuf_size);
		memset(h_pixbuf_cache,0,sizeof(Glib::RefPtr<Gdk::Pixbuf>) * max_h_pixbuf_size);
	}
	Glib::RefPtr<Gdk::Pixbuf> ret = h_pixbuf_cache[index];
	if (ret)
		return ret;

	guint8* data;
	int width = length;
	int height = 5;

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

		req->width  = 5;
	} else {
		req->width  = request_width;

		req->width  = max(req->width,  min_h_pixbuf_size);
		req->width  = min(req->width,  max_h_pixbuf_size);

		req->height = 5;
	}

}

void
FastMeter::on_size_allocate (Gtk::Allocation &alloc)
{
	if (orientation == Vertical) {
		if (alloc.get_width() != 5) {
			alloc.set_width(5);
		}

		int h = alloc.get_height();
		h = max(h, min_v_pixbuf_size);
		h = min(h, max_v_pixbuf_size);

		if ( h != alloc.get_height())
			alloc.set_height(h);

		if (pixheight != h) {
			pixbuf = request_vertical_meter(h);
		}
	} else {
		if (alloc.get_height() != 5) {
			alloc.set_height(5);
		}

		int w = alloc.get_width();
		w = max(w, min_h_pixbuf_size);
		w = min(w, max_h_pixbuf_size);

		if ( w != alloc.get_width())
			alloc.set_width(w);

		if (pixwidth != w) {
			pixbuf = request_horizontal_meter(w);
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
	pixrect.height = top_of_meter;

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
					  intersection.x, pixheight - top_of_meter,
					  intersection.x, pixheight - top_of_meter,
					  intersection.width, intersection.height,
					  Gdk::RGB_DITHER_NONE, 0, 0);
	}

	// draw peak bar 
	if (hold_state && intersection.width > 0) {
		gint y = pixheight - (gint) floor (pixheight * current_peak);

		get_window()->draw_pixbuf (get_style()->get_fg_gc(get_state()), pixbuf,
					   intersection.x, y,
					   intersection.x, y,
					   intersection.width, 3,
					   Gdk::RGB_DITHER_NONE, 0, 0);
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
					  intersection.width, intersection.height,
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
