/*
    Copyright (C) 2007 Paul Davis
    Author: Dave Robillard
    Author: Hans Baier

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
#include "canvas/rectangle.h"
#include "note.h"
#include "midi_region_view.h"
#include "public_editor.h"

using namespace ARDOUR;
using namespace ArdourCanvas;

Note::Note (
	MidiRegionView&                   region,
	Group*                            group,
	const boost::shared_ptr<NoteType> note,
	bool with_events
	)
	: NoteBase (region, with_events, note)
{
	_rectangle = new ArdourCanvas::Rectangle (group);
#ifdef CANVAS_DEBUG
	_rectangle->name = "note";
#endif
	set_item (_rectangle);
}

Note::~Note ()
{
	delete _rectangle;
}

void
Note::move_event (double dx, double dy)
{
	_rectangle->move (Duple (dx, dy));

	/* XXX */
	// if (_text) {
	// 	_text->hide();
	// 	_text->property_x() = _text->property_x() + dx;
	// 	_text->property_y() = _text->property_y() + dy;
	// 	_text->show();
	// }
}

Coord
Note::x0 () const
{
	return _rectangle->x0 ();
}

Coord
Note::x1 () const
{
	return _rectangle->x1 ();
}

Coord
Note::y0 () const
{
	return _rectangle->y0 ();
}

Coord
Note::y1 () const
{
	return _rectangle->y1 ();
}

void
Note::set_outline_color (uint32_t color)
{
	_rectangle->set_outline_color (color);
}

void
Note::set_fill_color (uint32_t color)
{
	_rectangle->set_fill_color (color);
}

void
Note::show ()
{
	_rectangle->show ();
}

void
Note::hide ()
{
	_rectangle->hide ();
}

void
Note::set_x0 (Coord x0)
{
	_rectangle->set_x0 (x0);
}

void
Note::set_y0 (Coord y0)
{
	_rectangle->set_y0 (y0);
}

void
Note::set_x1 (Coord x1)
{
	_rectangle->set_x1 (x1);
}

void
Note::set_y1 (Coord y1)
{
	_rectangle->set_y1 (y1);
}

void
Note::set_outline_what (int what)
{
	_rectangle->set_outline_what (what);
}

void
Note::set_ignore_events (bool ignore)
{
	_rectangle->set_ignore_events (ignore);
}
