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

#include "velocity_ghost_region.h"
#include "editing.h"
#include "editor.h"
#include "editor_drag.h"
#include "gui_thread.h"
#include "midi_automation_line.h"
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
VelocityGhostRegion::add_note(NoteBase*)
{
}

void
VelocityGhostRegion::update_note (GhostEvent* note)
{
}

void
VelocityGhostRegion::update_hit (GhostEvent* hit)
{
}

void
VelocityGhostRegion::remove_note (NoteBase*)
{
}


