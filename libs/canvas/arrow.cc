/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 David Robillard <d@drobilla.net>
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

/** @file  canvas/arrow.cc
 *  @brief Implementation of the Arrow canvas object.
 */

#include "pbd/compose.h"

#include "canvas/arrow.h"
#include "canvas/debug.h"
#include "canvas/polygon.h"
#include "canvas/line.h"

using namespace ArdourCanvas;

/** Construct an Arrow.
 *  @param parent Parent canvas group.
 */
Arrow::Arrow (Canvas* c)
	: Container (c)
{
	setup ();
}

Arrow::Arrow (Item* parent)
	: Container (parent)
{
	setup ();
}

void
Arrow::setup ()
{
	/* set up default arrow heads at each end */
	for (int i = 0; i < 2; ++i) {
		_heads[i].polygon = new Polygon (this);
		_heads[i].outward = true;
		_heads[i].width = 4;
		_heads[i].height = 4;
		setup_polygon (i);
		CANVAS_DEBUG_NAME (_heads[i].polygon, string_compose ("arrow head %1", i));
	}

	_line = new Line (this);
	CANVAS_DEBUG_NAME (_line, "arrow line");
}

void
Arrow::compute_bounding_box () const
{
	/* Compute our bounding box manually rather than using the default
	   container algorithm, since having the bounding box with origin other
	   than zero causes strange problems for mysterious reasons. */

	const double outline_pad = 0.5 + (_line->outline_width() / 2.0);
	const double head_width  = std::max(_heads[0].width, _heads[1].width);

	_bounding_box = Rect(0,
	                     0,
	                     _line->x1() + (head_width / 2.0) + outline_pad,
	                     _line->y1());

	bb_clean ();
}

/** Set whether to show an arrow head at one end or other
 *  of the line.
 *  @param which 0 or 1 to specify the arrow head to set up.
 *  @param true if this arrow head should be shown.
 */
void
Arrow::set_show_head (int which, bool show)
{
	assert (which == 0 || which == 1);

	begin_change ();

	if (!show) {
		delete _heads[which].polygon;
		_heads[which].polygon = 0;
	} else {
		setup_polygon (which);
	}

	_bounding_box_dirty = true;
	end_change ();
}

/** Set whether a given arrow head points into the line or
 *  away from it.
 *  @param which 0 or 1 to specify the arrow head to set up.
 *  @param true if this arrow head should point out from the line,
 *  otherwise false to point in.
 */
void
Arrow::set_head_outward (int which, bool outward)
{
	assert (which == 0 || which == 1);

	begin_change ();

	_heads[which].outward = outward;

	setup_polygon (which);
	_bounding_box_dirty = true;
	end_change ();
}

/** Set the height of a given arrow head.
 *  @param which 0 or 1 to specify the arrow head to set up.
 *  @param height Height of the arrow head in pixels.
 */
void
Arrow::set_head_height (int which, Distance height)
{
	assert (which == 0 || which == 1);

	begin_change ();

	_heads[which].height = height;

	setup_polygon (which);
	_bounding_box_dirty = true;
	end_change ();
}

/** Set the width of a given arrow head.
 *  @param which 0 or 1 to specify the arrow head to set up.
 *  @param width Width of the arrow head in pixels.
 */
void
Arrow::set_head_width (int which, Distance width)
{
	assert (which == 0 || which == 1);

	begin_change ();

	_heads[which].width = width;

	setup_polygon (which);
	_bounding_box_dirty = true;
	end_change ();
}

/** Set the width of our line, and the outline of our arrow(s).
 *  @param width New width in pixels.
 */
void
Arrow::set_outline_width (Distance width)
{
	_line->set_outline_width (width);
	if (_heads[0].polygon) {
		_heads[0].polygon->set_outline_width (width);
	}
	if (_heads[1].polygon) {
		_heads[1].polygon->set_outline_width (width);
	}
	_bounding_box_dirty = true;
}

/** Set the x position of our line.
 *  @param x New x position in pixels (in our coordinate system).
 */
void
Arrow::set_x (Coord x)
{
	_line->set_x0 (x);
	_line->set_x1 (x);
	for (int i = 0; i < 2; ++i) {
		if (_heads[i].polygon) {
			_heads[i].polygon->set_x_position (x - _heads[i].width / 2);
		}
	}
	_bounding_box_dirty = true;
}

/** Set the y position of end 0 of our line.
 *  @param y0 New y0 position in pixels (in our coordinate system).
 */
void
Arrow::set_y0 (Coord y0)
{
	_line->set_y0 (y0);
	if (_heads[0].polygon) {
		_heads[0].polygon->set_y_position (y0);
	}
	_bounding_box_dirty = true;
}

/** Set the y position of end 1 of our line.
 *  @param y1 New y1 position in pixels (in our coordinate system).
 */
void
Arrow::set_y1 (Coord y1)
{
	_line->set_y1 (y1);
	if (_heads[1].polygon) {
		_heads[1].polygon->set_y_position (y1 - _heads[1].height);
	}
	_bounding_box_dirty = true;
}

/** @return x position of our line in pixels (in our coordinate system) */
Coord
Arrow::x () const
{
	return _line->x0 ();
}

/** @return y position of end 1 of our line in pixels (in our coordinate system) */
Coord
Arrow::y1 () const
{
	return _line->y1 ();
}

/** Set up the polygon used to represent a particular arrow head.
 *  @param which 0 or 1 to specify the arrow head to set up.
 */
void
Arrow::setup_polygon (int which)
{
	assert (which == 0 || which == 1);

	Points points;

	if ((which == 0 && _heads[which].outward) || (which == 1 && !_heads[which].outward)) {
		/* this is an arrow head pointing towards -ve y */
		points.push_back (Duple (_heads[which].width / 2, 0));
		points.push_back (Duple (_heads[which].width, _heads[which].height));
		points.push_back (Duple (0, _heads[which].height));
	} else {
		/* this is an arrow head pointing towards +ve y */
		points.push_back (Duple (0, 0));
		points.push_back (Duple (_heads[which].width, 0));
		points.push_back (Duple (_heads[which].width / 2, _heads[which].height));
		points.push_back (Duple (0, 0));
	}

	_heads[which].polygon->set (points);
}

/** Set the color of our line and arrow heads.
 *  @param color New color.
 */
void
Arrow::set_color (Gtkmm2ext::Color color)
{
	_line->set_outline_color (color);
	for (int i = 0; i < 2; ++i) {
		if (_heads[i].polygon) {
			_heads[i].polygon->set_outline_color (color);
			_heads[i].polygon->set_fill_color (color);
		}
	}
}

bool
Arrow::covers (Duple const & point) const
{
	if (_heads[0].polygon && _heads[0].polygon->covers (point)) {
		return true;
	}
	if (_line && _line->covers (point)) {
		return true;
	}

	if (_heads[1].polygon && _heads[1].polygon->covers (point)) {
		return true;
	}

	return false;
}
