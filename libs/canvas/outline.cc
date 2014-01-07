/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include <cairomm/context.h>

#include "pbd/compose.h"
#include "pbd/convert.h"

#include "ardour/utils.h"
#include "canvas/outline.h"
#include "canvas/utils.h"
#include "canvas/debug.h"

using namespace ArdourCanvas;

Outline::Outline (Group* parent)
	: Item (parent)
	, _outline_color (0x000000ff)
	, _outline_width (1.0)
	, _outline (true)
{

}

void
Outline::set_outline_color (Color color)
{
	if (color != _outline_color) {
		begin_visual_change ();
		_outline_color = color;
		end_visual_change ();
	}
}

void
Outline::set_outline_width (Distance width)
{
	if (width != _outline_width) {
		begin_change ();
		_outline_width = width;
		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Outline::set_outline (bool outline)
{
	if (outline != _outline) {
		begin_change ();
		_outline = outline;
		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Outline::setup_outline_context (Cairo::RefPtr<Cairo::Context> context) const
{
	set_source_rgba (context, _outline_color);
	context->set_line_width (_outline_width);
}

