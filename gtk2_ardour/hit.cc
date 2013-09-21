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
#include "midi_region_view.h"
#include "public_editor.h"
#include "utils.h"
#include "hit.h"

using namespace ARDOUR;
using namespace ArdourCanvas;

Hit::Hit (
	MidiRegionView&                   region,
	Group*                            group,
	double                            /*size*/,
	const boost::shared_ptr<NoteType> note,
	bool with_events) 
	: NoteBase (region, with_events, note)
{
	_polygon = new Polygon (group);
	set_item (_polygon);
}

void
Hit::move_event (double dx, double dy)
{
	_polygon->move (Duple (dx, dy));
}

Coord
Hit::x0 () const
{
	boost::optional<ArdourCanvas::Rect> bbox = _polygon->bounding_box ();
	assert (bbox);
	return bbox.get().x0;
}

Coord
Hit::x1 () const
{
	boost::optional<ArdourCanvas::Rect> bbox = _polygon->bounding_box ();
	assert (bbox);
	return bbox.get().x1;
}

Coord
Hit::y0 () const
{
	boost::optional<ArdourCanvas::Rect> bbox = _polygon->bounding_box ();
	assert (bbox);
	return bbox.get().y0;
}

Coord
Hit::y1 () const
{
	boost::optional<ArdourCanvas::Rect> bbox = _polygon->bounding_box ();
	assert (bbox);
	return bbox.get().y1;
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
Hit::set_height (Distance /*height*/)
{
	/* XXX */
}

void
Hit::set_position (Duple position)
{
	_polygon->set_position (position);
}
