/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Robin Gareus <robin@gareus.org>

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

#include "canvas/surface_group.h"

using namespace std;
using namespace ArdourCanvas;

SurfaceGroup::SurfaceGroup (Group* parent)
	: Group (parent)
	, _surface_position (0,0)
	, _surface_geometry (0,0)
	, _surface (0)
{

}

SurfaceGroup::SurfaceGroup (Group* parent, Duple position)
	: Group (parent, position)
	, _surface_geometry (0,0)
	, _surface (0)
{

}

SurfaceGroup::~SurfaceGroup ()
{

}

void
SurfaceGroup::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	bool re_expose = false;
	boost::optional<Rect> bb = bounding_box ();

	if (bb) {
		Duple geo = Duple (bb->width (), bb->height ());
		if (geo != _surface_geometry) {
			re_expose = true;
#if 0 // DEBUG
			printf ("re allocate surface group %1.f x %.1f @ %1.f x %.1f self: %1.f x %.1f\n",
					geo.x, geo.y, bb->x0, bb->y0, _position.x, _position.y);
#endif
			if (geo.x > 32768 || geo.y > 32768) {
				// TODO allocate N surfaces, render at offset and stitch them together.
				// OR use one surfacen and re-expose child items when offset changes
				return;
			}

			_surface = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, geo.x, geo.y);
			_surface_geometry = geo;
			_surface_position = Duple (bb->x0, bb->y0);
		}
	}

	if (!_surface) return;

	Rect a (0, 0, _surface_geometry.x, _surface_geometry.y);

	if (!re_expose) {
		Duple tmp_p0 = window_to_item (Duple (area.x0, area.y0));
		Duple tmp_p1 = window_to_item (Duple (area.x1, area.y1));
		Rect wa (tmp_p0.x, tmp_p0.y, tmp_p1.x, tmp_p1.y);
		boost::optional<Rect> d = a.intersection (wa);
		assert (d);
		a = d.get ();
	}

	Duple window_space;
	window_space = item_to_window (Duple (_position.x + _surface_position.x, _position.y + _surface_position.y));
	Cairo::RefPtr<Cairo::Context> cr = Cairo::Context::create (_surface);

#if 1 // clear surface, needed if any content is semi-transparent
	cr->save ();
	cr->set_operator (Cairo::OPERATOR_CLEAR);
	cr->rectangle (a.x0, a.y0, a.x1, a.y1);
	cr->fill ();
	cr->restore ();
#endif

#if 0 // DEBUG
	printf ("EXPOSE: %1.f x %.1f -> %1.f x %.1f\n", a.x0, a.y0, a.x1, a.y1);
	Rect dbg = item_to_window (a);
	printf ("RENDER: %1.f x %.1f -> %1.f x %.1f  WS: %1.f x %.1f \n",
			dbg.x0, dbg.y0, dbg.x1, dbg.y1, window_space.x, -window_space.y);
#endif

	cr->translate (-window_space.x, -window_space.y);

	Group::render (item_to_window (a), cr);

	context->set_source (_surface, window_space.x, window_space.y);
	context->paint ();
}
