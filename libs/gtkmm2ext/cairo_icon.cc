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
#include <iostream>

#include "gtkmm2ext/cairo_icon.h"

using namespace Gtkmm2ext;

CairoIcon::CairoIcon (ArdourIcon::Icon t, uint32_t foreground_color)
	: icon_type (t)
	, fg (foreground_color)
{
	set_draw_background (false);
	set_widget_prelight (false);
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
	int width = get_width();
	int height = get_height ();

	ArdourIcon::render (cr, icon_type, width, height, Off, fg);
}
