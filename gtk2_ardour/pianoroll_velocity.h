/*
 * Copyright (C) 2024 Paul Davis <paul@linuxaudiosystems.com>
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

#include "ghost_event.h"
#include "velocity_display.h"

class PianorollVelocityDisplay : public VelocityDisplay
{
  public:
	PianorollVelocityDisplay (EditingContext&, MidiViewBackground&, MidiView&, ArdourCanvas::Rectangle& base_rect, Gtkmm2ext::Color oc);

	void remove_note (NoteBase*);
	void set_colors ();
	void set_height (double);

  private:
	ArdourCanvas::Container* _note_group;
	GhostEvent::EventList events;
	GhostEvent::EventList::iterator _optimization_iterator;

	bool base_event (GdkEvent*);
	bool lollevent (GdkEvent*, GhostEvent*);
};
