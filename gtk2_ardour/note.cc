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

#include "canvas/note.h"
#include "canvas/debug.h"

#include "note.h"
#include "public_editor.h"

using namespace ARDOUR;
using ArdourCanvas::Coord;
using ArdourCanvas::Duple;

Note::Note (
	MidiRegionView& region, ArdourCanvas::Item* parent, const boost::shared_ptr<NoteType> note, bool with_events)
	: NoteBase (region, with_events, note)
	, _note (new ArdourCanvas::Note (parent))
{
	CANVAS_DEBUG_NAME (_note, "note");
	set_item (_note);
}

Note::~Note ()
{
	delete _note;
}

void
Note::move_event (double dx, double dy)
{
	_note->set (_note->get().translate (Duple (dx, dy)));
}

Coord
Note::x0 () const
{
	return _note->x0 ();
}

Coord
Note::x1 () const
{
	return _note->x1 ();
}

Coord
Note::y0 () const
{
	return _note->y0 ();
}

Coord
Note::y1 () const
{
	return _note->y1 ();
}

void
Note::set_outline_color (uint32_t color)
{
	_note->set_outline_color (color);
}

void
Note::set_fill_color (uint32_t color)
{
	_note->set_fill_color (color);
}

void
Note::show ()
{
	_note->show ();
}

void
Note::hide ()
{
	_note->hide ();
}

void
Note::set (ArdourCanvas::Rect rect)
{
	_note->set (rect);
}

void
Note::set_x0 (Coord x0)
{
	_note->set_x0 (x0);
}

void
Note::set_y0 (Coord y0)
{
	_note->set_y0 (y0);
}

void
Note::set_x1 (Coord x1)
{
	_note->set_x1 (x1);
}

void
Note::set_y1 (Coord y1)
{
	_note->set_y1 (y1);
}

void
Note::set_outline_what (ArdourCanvas::Rectangle::What what)
{
	_note->set_outline_what (what);
}

void
Note::set_outline_all ()
{
	_note->set_outline_all ();
}

void
Note::set_ignore_events (bool ignore)
{
	_note->set_ignore_events (ignore);
}

void
Note::set_velocity (double fract)
{
	_note->set_velocity (fract);
}

