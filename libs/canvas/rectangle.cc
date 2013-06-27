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

#include <iostream>
#include <cairomm/context.h>
#include "pbd/stacktrace.h"
#include "pbd/compose.h"

#include "canvas/canvas.h"
#include "canvas/rectangle.h"
#include "canvas/debug.h"
#include "canvas/utils.h"

using namespace std;
using namespace ArdourCanvas;

Rectangle::Rectangle (Group* parent)
	: Item (parent)
	, Outline (parent)
	, Fill (parent)
	, _outline_what ((What) (LEFT | RIGHT | TOP | BOTTOM))
{

}

Rectangle::Rectangle (Group* parent, Rect const & rect)
	: Item (parent)
	, Outline (parent)
	, Fill (parent)
	, _rect (rect)
	, _outline_what ((What) (LEFT | RIGHT | TOP | BOTTOM))
{
	
}

void
Rectangle::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rect self = item_to_window (_rect);
	boost::optional<Rect> d = self.intersection (area);

	if (!d) {
		return;
	}
	
	Rect draw = d.get();
	static const double boundary = 0.5;

	draw.x0 = max (self.x0, max (0.0, draw.x0 - boundary));
	draw.x1 = min (self.x1, min (2000.0, draw.x1 + boundary));

	draw.y0 = max (self.y0, max (0.0, draw.y0 - boundary));
	draw.y1 = min (self.y1, min (2000.0, draw.y1 + boundary));

	Rect fill_rect = draw;
	Rect stroke_rect = fill_rect.expand (0.5);

	if (_fill) {
		if (_stops.empty()) {
			setup_fill_context (context);
		} else {
			setup_gradient_context (context, self, Duple (draw.x0, draw.y0));
		}
		context->rectangle (fill_rect.x0, fill_rect.y0, fill_rect.width(), fill_rect.height());
		context->fill ();
	}
	
	if (_outline) {

		setup_outline_context (context);

		if (_outline_what & LEFT) {
			context->move_to (stroke_rect.x0, stroke_rect.y0);
			context->line_to (stroke_rect.x0, stroke_rect.y1);
		}
		
		if (_outline_what & BOTTOM) {
			context->move_to (stroke_rect.x0, stroke_rect.y1);
			context->line_to (stroke_rect.x1, stroke_rect.y1);
		}
		
		if (_outline_what & RIGHT) {
			context->move_to (stroke_rect.x1, stroke_rect.y0);
			context->line_to (stroke_rect.x1, stroke_rect.y1);
		}
		
		if (_outline_what & TOP) {
			context->move_to (stroke_rect.x0, stroke_rect.y0);
			context->line_to (stroke_rect.x1, stroke_rect.y0);
		}
		
		context->stroke ();
	}
}

void
Rectangle::compute_bounding_box () const
{
	Rect r = _rect.fix ();
	_bounding_box = boost::optional<Rect> (r.expand (_outline_width / 2));
	
	_bounding_box_dirty = false;
}

void
Rectangle::set (Rect const & r)
{
	/* We don't update the bounding box here; it's just
	   as cheap to do it when asked.
	*/
	
	begin_change ();
	
	_rect = r;
	
	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: rectangle change (set)\n");
}

void
Rectangle::set_x0 (Coord x0)
{
	begin_change ();

	_rect.x0 = x0;

	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: rectangle change (x0)\n");
}

void
Rectangle::set_y0 (Coord y0)
{
	begin_change ();
	
	_rect.y0 = y0;

	_bounding_box_dirty = true;
	end_change();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: rectangle change (y0)\n");
}

void
Rectangle::set_x1 (Coord x1)
{
	begin_change ();
	
	_rect.x1 = x1;

	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: rectangle change (x1)\n");
}

void
Rectangle::set_y1 (Coord y1)
{
	begin_change ();

	_rect.y1 = y1;

	_bounding_box_dirty = true;
	end_change ();

	DEBUG_TRACE (PBD::DEBUG::CanvasItemsDirtied, "canvas item dirty: rectangle change (y1)\n");
}

void
Rectangle::set_outline_what (What what)
{
	begin_change ();
	
	_outline_what = what;

	end_change ();
}

void
Rectangle::set_outline_what (int what)
{
	set_outline_what ((What) what);
}

