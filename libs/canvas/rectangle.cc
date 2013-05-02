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
Rectangle::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	/* Cairo goes a little (!) wrong when asked to fill/stroke rectangles that
	 * extend way beyond the surface boundaries. To avoid this issue,
	 * clamp what we are drawing using the absolute end of the visible
	 * canvas, converting to item-space coordinates, of course.
	 */

	Rect plot = _rect;
	Rect visible = _canvas->visible_area();
	Duple visible_end = canvas_to_item (Duple (visible.x1, visible.y1));

	plot.x1 = min (plot.x1, visible_end.x);
	plot.y1 = min (plot.y1, visible_end.y);	

	if (_fill) {
		setup_fill_context (context);
		cerr << "Fill rect: " << plot << endl;
		context->rectangle (plot.x0, plot.y0, plot.width(), plot.height());
		
		if (!_outline) {
			context->fill ();
		} else {
			
			/* special/common case: outline the entire rectangle is
			 * requested, so just use the same path for the fill
			 * and stroke.
			 */

			if (_outline_what == What (LEFT|RIGHT|BOTTOM|TOP)) {
				context->fill_preserve();
				setup_outline_context (context);
				context->stroke ();
			} else {
				context->fill ();
			}
		}
	} 
	
	if (_outline) {
		
		setup_outline_context (context);

		if (_outline_what == What (LEFT|RIGHT|BOTTOM|TOP)) {

			/* if we filled and use full outline, we are already
			 * done. otherwise, draw the frame here.
			 */

			if (!_fill) { 
				context->rectangle (plot.x0, plot.y0, plot.width(), plot.height());
				context->stroke ();
			}
			
		} else {
			
			if (_outline_what & LEFT) {
				context->move_to (plot.x0, plot.y0);
				context->line_to (plot.x0, plot.y1);
			}
			
			if (_outline_what & BOTTOM) {
				context->move_to (plot.x0, plot.y1);
				context->line_to (plot.x1, plot.y1);
			}
			
			if (_outline_what & RIGHT) {
				context->move_to (plot.x1, plot.y0);
				context->line_to (plot.x1, plot.y1);
			}
			
			if (_outline_what & TOP) {
				context->move_to (plot.x0, plot.y0);
				context->line_to (plot.x1, plot.y0);
			}
			
			context->stroke ();
		}
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

