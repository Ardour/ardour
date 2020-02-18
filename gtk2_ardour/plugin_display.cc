/*
 * Copyright (C) 2017 Johannes Mueller <github@johannes-mueller.org>
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

#include <gtkmm/container.h>

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ui_config.h"

#include "plugin_display.h"



PluginDisplay::PluginDisplay (boost::shared_ptr<ARDOUR::Plugin> p, uint32_t max_height)
	: _plug (p)
	, _surf (0)
	, _max_height (max_height)
	, _cur_height (1)
	, _scroll (false)
{
	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
	_plug->DropReferences.connect (_death_connection, invalidator (*this), boost::bind (&PluginDisplay::plugin_going_away, this), gui_context());
	_plug->QueueDraw.connect (_qdraw_connection, invalidator (*this),
			boost::bind (&Gtk::Widget::queue_draw, this), gui_context ());
}

PluginDisplay::~PluginDisplay ()
{
	if (_surf) {
		cairo_surface_destroy (_surf);
	}
}

bool
PluginDisplay::on_button_press_event (GdkEventButton *ev)
{
	return false;
}

bool
PluginDisplay::on_button_release_event (GdkEventButton *ev)
{
	return false;
}

void
PluginDisplay::on_size_request (Gtk::Requisition* req)
{
	req->width = 300;
	req->height = _cur_height;
}


void
PluginDisplay::update_height_alloc (uint32_t height)
{
	uint32_t shm = std::min (_max_height, height);

	if (shm != _cur_height) {
		_cur_height = shm;
		queue_resize ();
	}
}

uint32_t
PluginDisplay::render_inline (cairo_t* cr, uint32_t width)
{
	ARDOUR::Plugin::Display_Image_Surface* dis = _plug->render_inline_display (width, _max_height);
	if (!dis) {
		return 0;
	}

	/* allocate a local image-surface,
	 * We cannot re-use the data via cairo_image_surface_create_for_data(),
	 * since pixman keeps a reference to it.
	 * we'd need to hand over the data and ha cairo_surface_destroy to free it.
	 * it might be possible to work around via cairo_surface_set_user_data().
	 */
	if (!_surf
			|| dis->width !=  cairo_image_surface_get_width (_surf)
			|| dis->height !=  cairo_image_surface_get_height (_surf)
		 ) {
		if (_surf) {
			cairo_surface_destroy (_surf);
		}
		_surf = cairo_image_surface_create (CAIRO_FORMAT_ARGB32, dis->width, dis->height);
	}

	if (cairo_image_surface_get_stride (_surf) == dis->stride) {
		memcpy (cairo_image_surface_get_data (_surf), dis->data, dis->stride * dis->height);
	} else {
		unsigned char *src = dis->data;
		unsigned char *dst = cairo_image_surface_get_data (_surf);
		const int dst_stride =  cairo_image_surface_get_stride (_surf);
		for (int y = 0; y < dis->height; ++y) {
			memcpy (dst, src, dis->width * 4 /*ARGB32*/);
			src += dis->stride;
			dst += dst_stride;
		}
	}

	cairo_surface_flush(_surf);
	cairo_surface_mark_dirty(_surf);
	const double xc = floor ((width - dis->width) * .5);
	cairo_set_source_surface(cr, _surf, xc, 0);
	cairo_paint (cr);

	return dis->height;
}

bool
PluginDisplay::on_expose_event (GdkEventExpose* ev)
{
	Gtk::Allocation a = get_allocation();
	double const width = a.get_width();
	double const height = a.get_height();

	cairo_t* cr = gdk_cairo_create (get_window()->gobj());
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);

	Gdk::Color const bg = get_style()->get_bg (Gtk::STATE_NORMAL);
	cairo_set_source_rgb (cr, bg.get_red_p (), bg.get_green_p (), bg.get_blue_p ());
	cairo_rectangle (cr, 0, 0, width, height);
	cairo_fill (cr);

	cairo_save (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_SOURCE);
	display_frame(cr, width, height);
	cairo_clip (cr);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);

	uint32_t ht = render_inline (cr, width);
	cairo_restore (cr);

	if (ht == 0) {
		hide ();
		if (_cur_height != 1) {
			_cur_height = 1;
			queue_resize ();
		}
		cairo_destroy (cr);
		return true;
	} else {
		update_height_alloc (ht);
	}

	bool failed = false;
	std::string name = get_name();
	Gtkmm2ext::Color fill_color = UIConfiguration::instance().color (string_compose ("%1: fill active", name), &failed);

	display_frame(cr, width, height);
	cairo_set_operator (cr, CAIRO_OPERATOR_OVER);
	cairo_set_line_width(cr, 1.0);
	if (failed) {
		cairo_set_source_rgba (cr, .75, .75, .75, 1.0);
	} else {
		Gtkmm2ext::set_source_rgb_a (cr, fill_color, 1.0);
	}
	cairo_stroke (cr);

	cairo_destroy(cr);
	return true;
}

void
PluginDisplay::display_frame (cairo_t* cr, double w, double h)
{
	cairo_rectangle (cr, 0.0, 0.0, w, h);
}
