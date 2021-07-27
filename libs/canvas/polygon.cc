/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include "canvas/polygon.h"

using namespace ArdourCanvas;

Polygon::Polygon (Canvas* c)
	: PolyItem (c)
	, multiple (0)
	, constant (0)
	, cached_size (0)
{
}

Polygon::Polygon (Item* parent)
	: PolyItem (parent)
	, multiple (0)
	, constant (0)
	, cached_size (0)
{
}

Polygon::~Polygon ()
{
	delete [] multiple;
	delete [] constant;
}

void
Polygon::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Points::size_type npoints = _points.size();
	if (npoints < 2) {
		return;
	}

	if (_outline || _fill) {
		const double pixel_adjust = (_outline_width == 1.0 ? 0.5 : 0.0);

		for (Points::size_type i = 0; i < npoints; i++) {
			Duple c = item_to_window (Duple (_points[i].x, _points[i].y));
			if (i == 0) {
				context->move_to (c.x + pixel_adjust, c.y + pixel_adjust);
			} else {
				context->line_to (c.x + pixel_adjust, c.y + pixel_adjust);
			}
		}
		context->close_path ();
	}

	if (_outline) {
		setup_outline_context (context);
		if (_fill) {
			context->stroke_preserve ();
		} else {
			context->stroke ();
		}
	}

	if (_fill) {
		setup_fill_context (context);
		context->fill ();
	}
}

void
Polygon::cache_shape_computation () const
{
	Points::size_type npoints = _points.size();

	if (npoints == 0) {
		return;
	}

	Points::size_type i;
	Points::size_type j = npoints -1;

	if (cached_size < npoints) {
		cached_size = npoints;
		delete [] multiple;
		multiple = new float[cached_size];
		delete [] constant;
		constant = new float[cached_size];
	}

	for (i = 0; i < npoints; i++) {
		if (_points[j].y == _points[i].y) {
			constant[i] = _points[i].x;
			multiple[i] = 0;
		} else {
			constant[i] = _points[i].x-(_points[i].y*_points[j].x)/(_points[j].y-_points[i].y)+(_points[i].y*_points[i].x)/(_points[j].y-_points[i].y);
			multiple[i] = (_points[j].x-_points[i].x)/(_points[j].y-_points[i].y);
		}

		j = i;
	}
}

bool
Polygon::covers (Duple const & point) const
{
	Duple p = window_to_item (point);

	Points::size_type npoints = _points.size();

	if (npoints == 0) {
		return false;
	}

	Points::size_type i;
	Points::size_type j = npoints -1;
	bool oddNodes = false;

	if (_bounding_box_dirty) {
		(void) bounding_box ();
	}

	for (i = 0; i < npoints; i++) {
		if (((_points[i].y < p.y && _points[j].y >= p.y) || (_points[j].y < p.y && _points[i].y >= p.y))) {
			oddNodes ^= (p.y * multiple[i] + constant[i] < p.x);
		}
		j = i;
	}

	return oddNodes;
}

void
Polygon::compute_bounding_box () const
{
	PolyItem::compute_bounding_box ();
	cache_shape_computation ();
}

