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
	for (auto const & ev : events) {
		ArdourCanvas::Lollipop* l = dynamic_cast<ArdourCanvas::Lollipop*> (ev.second->item);
		l->set_length (ev.second->event->note()->velocity() / 127.0 * base_rect->y1 ());
	}
}

void
VelocityGhostRegion::add_note (NoteBase* n)
{
	ArdourCanvas::Lollipop* l = new ArdourCanvas::Lollipop (_note_group);
	GhostEvent* event = new GhostEvent (n, _note_group, l);
	events.insert (std::make_pair (n->note(), event));

	event->item->set_fill_color (UIConfiguration::instance().color_mod(n->base_color(), "ghost track midi fill"));
	event->item->set_outline_color (_outline);

	MidiStreamView* mv = midi_view();

	if (mv) {

		if (!n->item()->visible()) {
			event->item->hide();
		} else {
			l->set (ArdourCanvas::Duple (n->x0(), base_rect->y1()), n->note()->velocity() / 127.0 * base_rect->y1(), 10);
		}
	}
}

void
VelocityGhostRegion::update_note (GhostEvent* n)
{
	ArdourCanvas::Lollipop* l = dynamic_cast<ArdourCanvas::Lollipop*> (n->item);
	l->set (ArdourCanvas::Duple (n->event->x0(), base_rect->y1()), n->event->note()->velocity() / 127.0 * base_rect->y1(), 10);
}

void
VelocityGhostRegion::update_hit (GhostEvent* n)
{
}

void
VelocityGhostRegion::remove_note (NoteBase*)
{
}


