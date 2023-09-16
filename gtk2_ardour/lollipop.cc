/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
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

#include "evoral/Note.h"

#include "canvas/lollipop.h"
#include "canvas/debug.h"

#include "lollipop.h"
#include "public_editor.h"

using namespace ARDOUR;
using ArdourCanvas::Coord;
using ArdourCanvas::Duple;

Lollipop::Lollipop (
	MidiRegionView& region, ArdourCanvas::Item* parent, const boost::shared_ptr<NoteType> note, bool with_events)
	: NoteBase (region, with_events, note)
	, _lollipop (new ArdourCanvas::Lollipop (parent))
{
	CANVAS_DEBUG_NAME (_lollipop, "note");
	set_item (_lollipop);
}

Lollipop::~Lollipop ()
{
	delete _lollipop;
}

void
Lollipop::move_event (double dx, double dy)
{
	_lollipop->set (Duple (_lollipop->x(), _lollipop->y0()).translate (Duple (dx, dy)), _lollipop->length(), _lollipop->radius());
}

void
Lollipop::set_outline_color (uint32_t color)
{
	_lollipop->set_outline_color (color);
}

void
Lollipop::set_fill_color (uint32_t color)
{
	_lollipop->set_fill_color (color);
}

void
Lollipop::show ()
{
	_lollipop->show ();
}

void
Lollipop::hide ()
{
	_lollipop->hide ();
}

void
Lollipop::set (ArdourCanvas::Duple const & d, ArdourCanvas::Coord len, ArdourCanvas::Coord radius)
{
	_lollipop->set (d, len, radius);
}

void
Lollipop::set_x (Coord x)
{
	_lollipop->set_x (x);
}

void
Lollipop::set_len (Coord l)
{
	_lollipop->set_length (l);
}

void
Lollipop::set_outline_what (ArdourCanvas::Rectangle::What what)
{
	// _lollipop->set_outline_what (what);
}

void
Lollipop::set_outline_all ()
{
	// _lollipop->set_outline_all ();
}

void
Lollipop::set_ignore_events (bool ignore)
{
	_lollipop->set_ignore_events (ignore);
}
