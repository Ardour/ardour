/*
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
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
	typedef Evoral::Note<Temporal::Beats> NoteType;

	Hit (MidiView&                         region,
	     ArdourCanvas::Item*               parent,
	     double                            size,
	     const std::shared_ptr<NoteType> note        = std::shared_ptr<NoteType>(),
	     bool                              with_events = true);

	~Hit();

	void show ();
	void hide ();

	ArdourCanvas::Coord x0 () const;
	ArdourCanvas::Coord y0 () const;
	ArdourCanvas::Coord x1 () const;
	ArdourCanvas::Coord y1 () const;

	ArdourCanvas::Duple position ();

	void set_position (ArdourCanvas::Duple);

	void set_height (ArdourCanvas::Coord);

	void set_outline_color (uint32_t);
	void set_fill_color (uint32_t);

	void set_ignore_events (bool);

	void move_event (double, double);

	/* no trimming of percussive hits */
	bool big_enough_to_trim() const { return false; }

	static ArdourCanvas::Points points(ArdourCanvas::Distance height);

	double visual_velocity() const;

private:
	ArdourCanvas::Polygon* _polygon;
};

#endif /* __gtk_ardour_hit_h__ */
