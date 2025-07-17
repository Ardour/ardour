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

#include "canvas/poly_line.h"

#include "ghostregion.h"
#include "velocity_display.h"

namespace ArdourCanvas {
	class Lollipop;
}

class GhostEvent;

class VelocityGhostRegion : public MidiGhostRegion, public VelocityDisplay
{
  public:
	VelocityGhostRegion (MidiRegionView&, TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos);
	~VelocityGhostRegion ();

	void remove_note (NoteBase*);
	void add_note (NoteBase*);
	void note_selected (NoteBase*);
	void update_note (GhostEvent*);
	void update_contents_height ();
	void update_hit (GhostEvent*);

	ArdourCanvas::Rectangle& base_item();

	void set_colors ();
  private:
	bool base_event (GdkEvent*);
	bool lollevent (GdkEvent*, GhostEvent*);
};

