/*
 * Copyright (C) 2007-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2009-2010 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __gtk_ardour_velocity_region_view_h__
#define __gtk_ardour_velocity_region_view_h__

#include "canvas/poly_line.h"

#include "ghostregion.h"

namespace ArdourCanvas {
class Lollipop;
}

class VelocityGhostRegion : public MidiGhostRegion
{
public:
	VelocityGhostRegion (MidiRegionView&, TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos);
	~VelocityGhostRegion ();

	void update_contents_height();
	void add_note(NoteBase*);
	void update_note (GhostEvent* note);
	void update_hit (GhostEvent* hit);
	void remove_note (NoteBase*);
	void note_selected (NoteBase*);

	void set_colors ();
	void drag_lolli (ArdourCanvas::Lollipop* l, GdkEventMotion* ev);

	int y_position_to_velocity (double y) const;

	void set_selected (bool);

private:
	bool dragging;
	ArdourCanvas::PolyLine* dragging_line;
	int last_drag_x;
	bool drag_did_change;
	bool selected;

	bool base_event (GdkEvent*);
	bool lollevent (GdkEvent*, MidiGhostRegion::GhostEvent*);
	void set_size_and_position (MidiGhostRegion::GhostEvent&);
	void lollis_close_to_x (int x, double distance, std::vector<NoteBase*>& events);
	void lollis_between (int x0, int x1, std::vector<NoteBase*>& events);
	void desensitize_lollis ();
	void sensitize_lollis ();
};

#endif /* __gtk_ardour_velocity_region_view_h__ */
