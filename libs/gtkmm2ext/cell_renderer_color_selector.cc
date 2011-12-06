/*
    Copyright (C) 2011 Paul Davis
 
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

    $Id: cell_renderer_toggle_pixbuf.cc $
*/

#include <iostream>
#include <gtkmm.h>

#include "gtkmm2ext/cell_renderer_color_selector.h"
#include "gtkmm2ext/utils.h"

using namespace std;
using namespace Gtk;
using namespace Gdk;
using namespace Glib;
using namespace Gtkmm2ext;


CellRendererColorSelector::CellRendererColorSelector()
	: Glib::ObjectBase (typeid(CellRendererColorSelector) )
	, Gtk::CellRenderer()
	, _property_color (*this, "color")
{
	property_mode() = Gtk::CELL_RENDERER_MODE_ACTIVATABLE;
	property_sensitive() = false;
	property_xpad() = 2;
	property_ypad() = 2;

	Gdk::Color c;

	c.set_red (0);
	c.set_green (0);
	c.set_blue (0);
	
	property_color() = c;
}

CellRendererColorSelector::~CellRendererColorSelector ()
{
}

Glib::PropertyProxy<Gdk::Color> 
CellRendererColorSelector::property_color()
{
	return _property_color.get_proxy();
} 

void 
CellRendererColorSelector::render_vfunc (const Glib::RefPtr<Gdk::Drawable>& window, Gtk::Widget& /*widget*/, const Gdk::Rectangle& /*background_area*/, const Gdk::Rectangle& cell_area, const Gdk::Rectangle& expose_area, Gtk::CellRendererState /*flags*/)
{
	cairo_t* cr = gdk_cairo_create (window->gobj());
	double r, g, b;
	Gdk::Color c = _property_color.get_value();

	cairo_rectangle (cr, expose_area.get_x(), expose_area.get_y(), expose_area.get_width(), expose_area.get_height());
	cairo_clip (cr);

	r = c.get_red_p();
	g = c.get_green_p();
	b = c.get_blue_p();

	cairo_rectangle_t drawing_rect;

	drawing_rect.x = cell_area.get_x() + property_xpad();
	drawing_rect.y = cell_area.get_y() + property_ypad();
	drawing_rect.width = cell_area.get_width() - (2 * property_xpad());
	drawing_rect.height = cell_area.get_height() - (2 * property_ypad());

	Gtkmm2ext::rounded_rectangle (cr, drawing_rect.x, drawing_rect.y, drawing_rect.width, drawing_rect.height, 5);
	cairo_set_source_rgb (cr, r, g, b);
	cairo_fill (cr);

	cairo_destroy (cr);
}

