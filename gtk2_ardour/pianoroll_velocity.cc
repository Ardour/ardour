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

#include "canvas/container.h"
#include "canvas/debug.h"

#include "editing_context.h"
#include "pianoroll_velocity.h"
#include "note_base.h"
#include "ui_config.h"

PianorollVelocityDisplay::PianorollVelocityDisplay (EditingContext& ec, MidiViewBackground& background, MidiView& mv, ArdourCanvas::Rectangle& base_rect, Gtkmm2ext::Color oc)
	: VelocityDisplay (ec, background, mv, base_rect, *(_note_group = new ArdourCanvas::Container (&base_rect)), events, oc)
{
	CANVAS_DEBUG_NAME (_note_group, "cue velocity lolli container");
}

void
PianorollVelocityDisplay::set_height (double h)
{
	redisplay ();
}

void
PianorollVelocityDisplay::set_colors ()
{
	base.set_fill_color (UIConfiguration::instance().color_mod ("ghost track base", "ghost track midi fill"));

	for (auto & gev : events) {
		gev.second->item->set_fill_color (gev.second->event->base_color());
	}
}

void
PianorollVelocityDisplay::remove_note (NoteBase* nb)
{
	GhostEvent::EventList::iterator f = events.find (nb->note());
	if (f == events.end()) {
		return;
	}

	delete f->second;
	events.erase (f);

	_optimization_iterator = events.end ();
}

bool
PianorollVelocityDisplay::base_event (GdkEvent* ev)
{
	if (!_sensitive) {
		return false;
	}
	return editing_context.canvas_velocity_base_event (ev, &base);
}

bool
PianorollVelocityDisplay::lollevent (GdkEvent* ev, GhostEvent* gev)
{
	return editing_context.canvas_velocity_event (ev, gev->item);
}

