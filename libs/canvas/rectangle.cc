/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
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

#include <iostream>
#include <cairomm/context.h>
#include "pbd/compose.h"

#include "canvas/canvas.h"
#include "canvas/rectangle.h"
#include "canvas/debug.h"

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

Rectangle::Rectangle (Item* parent)
	: Item (parent)
	, _outline_what ((What) (LEFT | RIGHT | TOP | BOTTOM))
{
}

Rectangle::Rectangle (Item* parent, Rect const & rect)
	: Item (parent)
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

	Rect self (item_to_window (_rect.translate (_position), false));
	const Rect draw = self.intersection (area);

	if (!draw) {
		return;
	}

	if (_fill && !_transparent) {
		if (_stops.empty()) {
			setup_fill_context (context);
		} else {
			setup_gradient_context (context, self, Duple (draw.x0, draw.y0));
		}

		context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
		context->fill ();
	}

	if (_outline && _outline_width && _outline_what) {

		setup_outline_context (context);

		/* the goal here is that if the border is 1 pixel
		 * thick, it will precisely align with the corner
		 * coordinates of the rectangle. So if the rectangle
		 * has a left edge at 0 and a right edge at 10, then
		 * the left edge must span 0..1, the right edge
		 * must span 10..11 because the first and final pixels
		 * to be colored are actually "at" 0.5 and 10.5 (midway
		 * between the integer coordinates).
		 *
		 * See the Cairo FAQ on single pixel lines for more
		 * detail.
		 */

		if (fmod (_outline_width, 2.0)  != 0.0) {
			const double shift = _outline_width * 0.5;
			self = self.translate (Duple (shift, shift));
		}

		if (_outline_what == What (LEFT|RIGHT|BOTTOM|TOP)) {

			context->rectangle (self.x0, self.y0, self.width(), self.height());

		} else {

			if (_outline_what & LEFT) {
				context->move_to (self.x0, self.y0);
				context->line_to (self.x0, self.y1);
			}

			if (_outline_what & TOP) {
				context->move_to (self.x0, self.y0);
				context->line_to (self.x1, self.y0);
			}

			if (_outline_what & BOTTOM) {
				context->move_to (self.x0, self.y1);
				context->line_to (self.x1, self.y1);
			}

			if (_outline_what & RIGHT) {
				context->move_to (self.x1, self.y0);
				context->line_to (self.x1, self.y1);
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

		/* if the outline is 1 pixel, then the actual
		   bounding box is 0.5 pixels outside the stated
		   corners of the rectangle.

		   if the outline is 2 pixels, then the actual
		   bounding box is 1.0 pixels outside the stated
		   corners of the rectangle (so that the middle
		   of the 2 pixel wide border passes through
		   the corners, alternatively described as 1 row
		   of pixels outside of the corners, and 1 row
		   inside).

		   if the outline is 3 pixels, then the actual
		   bounding box is 1.5 outside the stated corners
		   of the rectangle (so that the middle row of
		   pixels of the border passes through the corners).

		   if the outline is 4 pixels, then the actual bounding
		   box is 2.0 pixels outside the stated corners
		   of the rectangle, so that the border consists
		   of 2 pixels outside the corners and 2 pixels inside.

		   hence ... the bounding box is width * 0.5 larger
		   than the rectangle itself.
		*/

		_bounding_box = r.expand (1.0 + _outline_width * 0.5);
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

double
Rectangle::vertical_fraction (double y) const
{
        /* y is in canvas coordinates */

        Duple i (canvas_to_item (Duple (0, y)));
        Rect r = bounding_box();
        if (!r) {
                return 0; /* not really correct, but what else can we do? */
        }

        Rect bbox (r);

        if (i.y < bbox.y0 || i.y >= bbox.y1) {
                return 0;
        }

        /* convert to fit Cairo origin model (origin at upper left)
         */

        return 1.0 - ((i.y - bbox.y0) / bbox.height());
}
