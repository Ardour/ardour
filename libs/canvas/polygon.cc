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

#include "canvas/polygon.h"

using namespace ArdourCanvas;

Polygon::Polygon (Group* parent)
	: Item (parent)
	, PolyItem (parent)
	, Fill (parent)
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
	if (_outline) {
		setup_outline_context (context);
		render_path (area, context);
		
		if (!_points.empty ()) {
			/* close path */
			Duple p = item_to_window (Duple (_points.front().x, _points.front().y));
			context->move_to (p.x, p.y);
		}

		context->stroke_preserve ();
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
	Duple p = canvas_to_item (point);

	Points::size_type npoints = _points.size();

	if (npoints == 0) {
		return false;
	}

	Points::size_type i;
	Points::size_type j = npoints -1;
	bool oddNodes = false;
	
	if (_bounding_box_dirty) {
		compute_bounding_box ();
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

