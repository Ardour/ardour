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

Rectangle::Rectangle (Canvas* c)
	: Item (c)
	, _outline_what ((What) (LEFT | RIGHT | TOP | BOTTOM))
{
}

Rectangle::Rectangle (Canvas* c, Rect const & rect)
	: Item (c)
	, _rect (rect)
	, _outline_what ((What) (LEFT | RIGHT | TOP | BOTTOM))
{
}

Rectangle::Rectangle (Group* g)
	: Item (g)
	, _outline_what ((What) (LEFT | RIGHT | TOP | BOTTOM))
{
}

Rectangle::Rectangle (Group* g, Rect const & rect)
	: Item (g)
	, _rect (rect)
	, _outline_what ((What) (LEFT | RIGHT | TOP | BOTTOM))
{
}

void
Rectangle::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	/* In general, a Rectangle will have a _position of (0,0) within its
	   parent, and its extent is actually defined by _rect. But in the
	   unusual case that _position is set to something other than (0,0),
	   we should take that into account when rendering.
	*/
	Rect self = item_to_window (_rect.translate (_position));
	boost::optional<Rect> r = self.intersection (area);

	if (!r) {
		return;
	}

	Rect draw = r.get ();

	if (_fill && !_transparent) {
		if (_stops.empty()) {
			setup_fill_context (context);
		} else {
			setup_gradient_context (context, self, Duple (draw.x0, draw.y0));
		}

		context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
		context->fill ();
	} 
	
	if (_outline) {

		setup_outline_context (context);
		
		/* see the cairo FAQ on single pixel lines to see why we do
		 * the 0.5 pixel additions.
		 */

		if (_outline_what == What (LEFT|RIGHT|BOTTOM|TOP)) {
			
			context->rectangle (self.x0 + 0.5, self.y0 + 0.5, self.width() - 1.0, self.height() - 1.0);

		} else {

			if (_outline_what & LEFT) {
				/* vertical line: move x-coordinate by 0.5 pixels */
				context->move_to (self.x0 + 0.5, self.y0);
				context->line_to (self.x0 + 0.5, self.y1);
			}
			
			if (_outline_what & TOP) {
				/* horizontal line: move y-coordinate by 0.5 pixels */
				context->move_to (self.x0, self.y0 + 0.5);
				context->line_to (self.x1, self.y0 + 0.5);
			}

			if (_outline_what & BOTTOM) {
				/* horizontal line: move y-coordinate by 0.5 pixels */
				context->move_to (self.x0, self.y1 - 0.5);
				context->line_to (self.x1, self.y1 - 0.5);
			}
			
			if (_outline_what & RIGHT) {
				/* vertical line: move x-coordinate by 0.5 pixels */
				context->move_to (self.x1 + 0.5, self.y0);
				context->line_to (self.x1 + 0.5, self.y1);
			}
			
		}
		
		context->stroke ();
	}
}

void
Rectangle::compute_bounding_box () const
{
	if (!_rect.empty()) {
		Rect r = _rect.fix ();
		/* take into acount the 0.5 addition to the bounding
		   box for the right and bottom edges, see ::render() above
		*/

		r.x1 += 1.0; // XXX this makes no sense but is necessary
		r.y1 += 0.5;

		_bounding_box = r;
	}

	_bounding_box_dirty = false;
}

void
Rectangle::set (Rect const & r)
{
	/* We don't update the bounding box here; it's just
	   as cheap to do it when asked.
	*/

	if (r != _rect) {
		
		begin_change ();
		
		_rect = r;
		
		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Rectangle::set_x0 (Coord x0)
{
	if (x0 != _rect.x0) {
		begin_change ();
		
		_rect.x0 = x0;
		
		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Rectangle::set_y0 (Coord y0)
{
	if (y0 != _rect.y0) {
		begin_change ();
		
		_rect.y0 = y0;
		
		_bounding_box_dirty = true;
		end_change();
	}
}

void
Rectangle::set_x1 (Coord x1)
{
	if (x1 != _rect.x1) {
		begin_change ();
		
		_rect.x1 = x1;
		
		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Rectangle::set_y1 (Coord y1)
{
	if (y1 != _rect.y1) {
		begin_change ();
		
		_rect.y1 = y1;
		
		_bounding_box_dirty = true;
		end_change ();
	}
}

void
Rectangle::set_outline_what (What what)
{
	if (what != _outline_what) {
		begin_visual_change ();
		_outline_what = what;
		end_visual_change ();
	}
}

