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

#ifndef __gtk_ardour_hit_h__
#define __gtk_ardour_hit_h__

#include <iostream>
#include "note_base.h"

namespace ArdourCanvas {
	class Polygon;
}

class Hit : public NoteBase
{
public:
	typedef Evoral::Note<double> NoteType;

	Hit (
		MidiRegionView&                   region,
		ArdourCanvas::Group*              group,
		double                            size,
		const boost::shared_ptr<NoteType> note = boost::shared_ptr<NoteType>(),
		bool with_events = true);

	void show ();
	void hide ();

	ArdourCanvas::Coord x0 () const;
	ArdourCanvas::Coord y0 () const;
	ArdourCanvas::Coord x1 () const;
	ArdourCanvas::Coord y1 () const;

	void set_position (ArdourCanvas::Duple);

	void set_height (ArdourCanvas::Coord);

	void set_outline_color (uint32_t);
	void set_fill_color (uint32_t);

	void move_event (double, double);

private:
	ArdourCanvas::Polygon* _polygon;
};

#endif /* __gtk_ardour_hit_h__ */
