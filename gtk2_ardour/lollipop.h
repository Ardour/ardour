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

#ifndef __gtk_ardour_lollipop_h__
#define __gtk_ardour_lollipop_h__

#include <iostream>

#include "canvas/rectangle.h"

#include "note_base.h"
#include "midi_util.h"

namespace ArdourCanvas {
	class Container;
	class Lollipop;
}

class Lollipop : public NoteBase
{
public:
	typedef Evoral::Note<Temporal::Beats> NoteType;

	Lollipop (MidiRegionView&                   region,
	          ArdourCanvas::Item*               parent,
	          const boost::shared_ptr<NoteType> note = boost::shared_ptr<NoteType>(),
	          bool with_events = true);

	~Lollipop ();

	void set (ArdourCanvas::Duple const &, ArdourCanvas::Coord, ArdourCanvas::Coord);
	void set_x (ArdourCanvas::Coord);
	void set_len (ArdourCanvas::Coord);

	void set_outline_what (ArdourCanvas::Rectangle::What);
	void set_outline_all ();

	void set_outline_color (uint32_t);
	void set_fill_color (uint32_t);

	void show ();
	void hide ();

	void set_ignore_events (bool);

	void move_event (double dx, double dy);

private:
	ArdourCanvas::Lollipop* _lollipop;
};

#endif /* __gtk_ardour_lollipop_h__ */
