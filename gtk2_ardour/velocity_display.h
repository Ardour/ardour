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

#pragma once

#include "canvas/rectangle.h"
#include "canvas/poly_line.h"

#include "gtkmm2ext/colors.h"

#include "ghost_event.h"

namespace ArdourCanvas {
	class Container;
	class Lollipop;
	class Rectangle;
}

class EditingContext;
class MidiViewBackground;
class MidiView;
class NoteBase;

class VelocityDisplay
{
  public:
	VelocityDisplay (EditingContext&, MidiViewBackground&, MidiView&, ArdourCanvas::Rectangle& base_rect, ArdourCanvas::Container&, GhostEvent::EventList& el, Gtkmm2ext::Color oc);
	virtual ~VelocityDisplay ();

	void hide ();
	void show ();

	void redisplay();
	void add_note(NoteBase*);

	void update_note (NoteBase*);

	void update_ghost_event (GhostEvent*);
	void color_ghost_event (GhostEvent*);
	void update_note (GhostEvent* gev) { update_ghost_event (gev); }
	void update_hit (GhostEvent* gev)  { update_ghost_event (gev); }

	virtual void remove_note (NoteBase*) = 0;
	void note_selected (NoteBase*);
	void clear ();

	void set_colors ();
	void drag_lolli (ArdourCanvas::Lollipop* l, GdkEventMotion* ev);

	int y_position_to_velocity (double y) const;

	void set_sensitive (bool yn);
	bool sensitive () const;

	void set_selected (bool);

	bool line_draw_motion (ArdourCanvas::Duple const & d, ArdourCanvas::Rectangle const & r, double last_x);
	bool line_extended (ArdourCanvas::Duple const & from, ArdourCanvas::Duple const & to, ArdourCanvas::Rectangle const & r, double last_x);

	void start_line_drag ();
	void end_line_drag (bool did_change);

	ArdourCanvas::Rectangle& base_item() { return base; }
	MidiView& midi_view() const { return view; }

  protected:
	virtual bool lollevent (GdkEvent*, GhostEvent*) = 0;

	EditingContext& editing_context;
	MidiViewBackground& bg;
	MidiView& view;
	ArdourCanvas::Rectangle& base;
	ArdourCanvas::Container* lolli_container;
	GhostEvent::EventList& events;
	Gtkmm2ext::Color _outline;
	bool dragging;
	ArdourCanvas::PolyLine* dragging_line;
	int last_drag_x;
	bool drag_did_change;
	bool selected;
	GhostEvent::EventList::iterator _optimization_iterator;
	bool _sensitive;

	virtual bool base_event (GdkEvent*) = 0;
	void set_size_and_position (GhostEvent&);
	void lollis_close_to_x (int x, double distance, std::vector<GhostEvent*>& events);
	void lollis_between (int x0, int x1, std::vector<GhostEvent*>& events);
	void desensitize_lollis ();
	void sensitize_lollis ();
};

