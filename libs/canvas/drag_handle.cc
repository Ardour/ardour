/*
    Copyright (C) 2014 Paul Davis

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
#include <cairomm/context.h>
#include "pbd/stacktrace.h"
#include "pbd/compose.h"

#include "canvas/drag_handle.h"

using namespace ArdourCanvas;

DragHandle::DragHandle (Group* g, Rect const & r, bool left_side)
	: Item (g)
	, Rectangle (g, r)
	, _left_side (left_side)
{
}

void
DragHandle::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rectangle::render (area, context);

	Duple circle_center (_left_side ? x0() : x1(), (y1() - y0())/2.0);
	Duple window_circle_center = item_to_window (circle_center);
	
	context->set_source_rgba (1.0, 0.0, 0.0, 1.0);

	if (_left_side) {
		context->arc (window_circle_center.x, window_circle_center.y, 7.0, -M_PI/2.0, +M_PI/2.0);
	} else {
		context->arc_negative (window_circle_center.x, window_circle_center.y, 7.0, -M_PI/2.0, +M_PI/2.0);
	}

	context->fill ();
}
