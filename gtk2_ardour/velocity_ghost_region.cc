/*
 * Copyright (C) 2022 Paul Davis <paul@linuxaudiosystems.com>
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

#include <utility>

#include "pbd/memento_command.h"

#include "ardour/automation_control.h"
#include "ardour/event_type_map.h"
#include "ardour/midi_automation_list_binder.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/session.h"

#include "gtkmm2ext/keyboard.h"

#include "canvas/lollipop.h"

#include "velocity_ghost_region.h"
#include "editing.h"
#include "editor.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "midi_automation_line.h"
#include "note_base.h"
#include "public_editor.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace Temporal;

static double const lollipop_radius = 8.0;

VelocityGhostRegion::VelocityGhostRegion (MidiRegionView& mrv, TimeAxisView& tv, TimeAxisView& source_tv, double initial_unit_pos)
	: MidiGhostRegion (mrv, tv, source_tv, initial_unit_pos)
{
}

VelocityGhostRegion::~VelocityGhostRegion ()
{
}

void
VelocityGhostRegion::update_contents_height ()
{
	for (auto const & i : events) {
		set_size_and_position (*i.second);
	}
}

bool
VelocityGhostRegion::lollevent (GdkEvent* ev, MidiGhostRegion::GhostEvent* gev)
{
	return trackview.editor().canvas_velocity_event (ev, gev->item);
}

void
VelocityGhostRegion::add_note (NoteBase* nb)
{
	ArdourCanvas::Lollipop* l = new ArdourCanvas::Lollipop (_note_group);

	GhostEvent* event = new GhostEvent (nb, _note_group, l);
	events.insert (std::make_pair (nb->note(), event));
	l->Event.connect (sigc::bind (sigc::mem_fun (*this, &VelocityGhostRegion::lollevent), event));
	l->raise_to_top ();

	event->item->set_fill_color (UIConfiguration::instance().color_mod(nb->base_color(), "ghost track midi fill"));
	event->item->set_outline_color (_outline);

	MidiStreamView* mv = midi_view();

	if (mv) {
		if (!nb->item()->visible()) {
			l->hide();
		} else {
			set_size_and_position (*event);
		}
	}
}

void
VelocityGhostRegion::set_size_and_position (GhostEvent& ev)
{
	ArdourCanvas::Lollipop* l = dynamic_cast<ArdourCanvas::Lollipop*> (ev.item);
	const double available_height = base_rect->y1() - (2.0 * lollipop_radius);
	const double actual_height = (ev.event->note()->velocity() / 127.0) * available_height;
	l->set (ArdourCanvas::Duple (ev.event->x0() - 1.0, base_rect->y1() - actual_height), actual_height, lollipop_radius);
}

void
VelocityGhostRegion::update_note (GhostEvent* ev)
{
	set_size_and_position (*ev);
}

void
VelocityGhostRegion::update_hit (GhostEvent* ev)
{
	set_size_and_position (*ev);
}

void
VelocityGhostRegion::remove_note (NoteBase*)
{
}

void
VelocityGhostRegion::set_colors ()
{
	base_rect->set_fill_color (Gtkmm2ext::Color (0xff000085));
}
