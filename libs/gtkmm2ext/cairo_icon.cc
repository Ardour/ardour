/*
    Copyright (C) 2015 Paul Davis

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

#if !defined USE_CAIRO_IMAGE_SURFACE && !defined NDEBUG
#define OPTIONAL_CAIRO_IMAGE_SURFACE
#endif


#include "gtkmm2ext/cairo_icon.h"
#include "gtkmm2ext/gtk_ui.h"

using namespace Gtkmm2ext;

CairoIcon::CairoIcon (ArdourIcon::Icon t, uint32_t foreground_color)
	: icon_type (t)
	, fg (foreground_color)
{
	add_events (Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK);
}

CairoIcon::~CairoIcon ()
{
}

void
CairoIcon::set_fg (uint32_t color)
{
	fg = color;
	queue_draw ();
}

void
CairoIcon::render (cairo_t* cr , cairo_rectangle_t* area)
{
	const double scale = UI::instance()->ui_scale;
	int width = get_width() * scale;
	int height = get_height () * scale;

	ArdourIcon::render (cr, icon_type, width, height, Off, fg);
}

bool
CairoIcon::on_expose_event (GdkEventExpose *ev)
{
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	Cairo::RefPtr<Cairo::Context> cr;
	if (getenv("ARDOUR_IMAGE_SURFACE")) {
		if (!image_surface) {
			image_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, get_width(), get_height());
		}
		cr = Cairo::Context::create (image_surface);
	} else {
		cr = get_window()->create_cairo_context ();
	}
#elif defined USE_CAIRO_IMAGE_SURFACE

	if (!image_surface) {
		image_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, get_width(), get_height());
	}

	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (image_surface);
#else
	Cairo::RefPtr<Cairo::Context> cr = get_window()->create_cairo_context ();
#endif

	cr->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cr->clip ();

	cr->translate (ev->area.x, ev->area.y);

	cairo_rectangle_t expose_area;
	expose_area.x = ev->area.x;
	expose_area.y = ev->area.y;
	expose_area.width = ev->area.width;
	expose_area.height = ev->area.height;

	CairoIcon::render (cr->cobj(), &expose_area);

#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	if (getenv("ARDOUR_IMAGE_SURFACE")) {
#endif
#if defined USE_CAIRO_IMAGE_SURFACE || defined OPTIONAL_CAIRO_IMAGE_SURFACE
	image_surface->flush();
	/* now blit our private surface back to the GDK one */

	Cairo::RefPtr<Cairo::Context> cairo_context = get_window()->create_cairo_context ();

	cairo_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_context->clip ();
	cairo_context->set_source (image_surface, 0, 0);
	cairo_context->set_operator (Cairo::OPERATOR_SOURCE);
	cairo_context->paint ();
#endif
#ifdef OPTIONAL_CAIRO_IMAGE_SURFACE
	}
#endif

	return true;
}
