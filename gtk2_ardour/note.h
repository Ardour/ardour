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

#ifndef __gtk_ardour_note_h__
#define __gtk_ardour_note_h__

#include <iostream>
#include "note_base.h"
#include "midi_util.h"

namespace ArdourCanvas {
	class Group;
}

class Note : public NoteBase
{
public:
	typedef Evoral::Note<Evoral::MusicalTime> NoteType;

	Note (MidiRegionView&                   region,
	      ArdourCanvas::Group*              group,
	      const boost::shared_ptr<NoteType> note = boost::shared_ptr<NoteType>(),
	      bool with_events = true);

	~Note ();

	ArdourCanvas::Coord x0 () const;
	ArdourCanvas::Coord y0 () const;
	ArdourCanvas::Coord x1 () const;
	ArdourCanvas::Coord y1 () const;

	void set_x0 (ArdourCanvas::Coord);
	void set_y0 (ArdourCanvas::Coord);
	void set_x1 (ArdourCanvas::Coord);
	void set_y1 (ArdourCanvas::Coord);

	void set_outline_what (int);

	void set_outline_color (uint32_t);
	void set_fill_color (uint32_t);

	void show ();
	void hide ();

	void set_ignore_events (bool);

	void move_event (double dx, double dy);

private:
	ArdourCanvas::Rectangle* _rectangle;
};

#endif /* __gtk_ardour_note_h__ */
