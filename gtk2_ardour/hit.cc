/*
    Copyright (C) 2007 Paul Davis
    Author: Dave Robillard

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

#include "evoral/Note.hpp"

#include "canvas/polygon.h"
#include "canvas/debug.h"

#include "midi_region_view.h"
#include "public_editor.h"
#include "hit.h"

using namespace ARDOUR;
using namespace ArdourCanvas;

Hit::Hit (MidiRegionView& region, Item* parent, double size, const boost::shared_ptr<NoteType> note, bool with_events) 
	: NoteBase (region, with_events, note)
{
	_polygon = new ArdourCanvas::Polygon (parent);
	CANVAS_DEBUG_NAME (_polygon, "note");
	set_item (_polygon);
	set_height (size);
}

Hit::~Hit ()
{
	delete _polygon;
}

void
Hit::move_event (double dx, double dy)
{
	Points points = _polygon->get();
	Points moved;
	for (Points::iterator p = points.begin(); p != points.end(); ++p) {
		moved.push_back ((*p).translate (ArdourCanvas::Duple (dx, dy)));
	}
	_polygon->set (moved);
}

void
Hit::set_outline_color (uint32_t color)
{
	_polygon->set_outline_color (color);
}

void
Hit::set_fill_color (uint32_t color)
{
	_polygon->set_fill_color (color);
}

void
Hit::show ()
{
	_polygon->show ();
}

void
Hit::hide ()
{
	_polygon->hide ();
}

void
Hit::set_height (Distance height)
{
	/* draw a diamond */

	Points p;

	const double half_height = height/2.0;
	p.push_back (Duple (-half_height, 0)); // left, middle
	p.push_back (Duple (0, -half_height)); // top
	p.push_back (Duple (+half_height, 0)); // right, middle
	p.push_back (Duple (0, +half_height)); // bottom

	_polygon->set (p);
}

void
Hit::set_position (Duple position)
{
	_polygon->set_position (position);
}

Coord
Hit::x0 () const
{
	/* left vertex */
	return _polygon->get()[0].x;
}

Coord
Hit::x1 () const
{
	/* right vertex */
	return _polygon->get()[2].x;
}

Coord
Hit::y0 () const
{
	/* top vertex */
	return _polygon->get()[1].y;
}

Coord
Hit::y1 () const
{
	/* bottom vertex */
	return _polygon->get()[3].y;
}
