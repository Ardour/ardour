/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
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

#include "temporal/beats.h"

#include "evoral/Note.h"

#include "canvas/polygon.h"
#include "canvas/debug.h"

#include "hit.h"

using namespace ARDOUR;
using namespace ArdourCanvas;

Hit::Hit (MidiView& region, Item* parent, double size, const std::shared_ptr<NoteType> note, bool with_events)
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

Points
Hit::points(Distance height)
{
	/* draw a diamond */

	Points p;

	const double half_height = height/2.0;
	p.push_back (Duple (-half_height, 0)); // left, middle
	p.push_back (Duple (0, -half_height)); // top
	p.push_back (Duple (+half_height, 0)); // right, middle
	p.push_back (Duple (0, +half_height)); // bottom

	return p;
}

void
Hit::set_height (Distance height)
{
	_polygon->set (points(height));
}

Duple
Hit::position ()
{
	return _polygon->position ();
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
	return _polygon->position().x + _polygon->get()[0].x;
}

Coord
Hit::x1 () const
{
	/* right vertex */
	return _polygon->position().x + _polygon->get()[2].x;
}

Coord
Hit::y0 () const
{
	/* top vertex */
	return _polygon->position().y + _polygon->get()[1].y;
}

Coord
Hit::y1 () const
{
	/* bottom vertex */
	return _polygon->position().y + _polygon->get()[3].y;
}

void
Hit::set_ignore_events (bool ignore)
{
	_polygon->set_ignore_events (ignore);
}

double
Hit::visual_velocity() const
{
	/* We don't display velocity in any explicit way */
	return 0.0;
}
