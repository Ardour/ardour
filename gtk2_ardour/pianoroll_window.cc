/*
 * Copyright (C) 2025 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/compose.h"

#include "ardour/midi_region.h"
#include "ardour/midi_track.h"

#include "gtkmm2ext/doi.h"

#include "ardour_ui.h"
#include "chord_box.h"
#include "pianoroll.h"
#include "pianoroll_window.h"
#include "region_editor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;

PianorollWindow::PianorollWindow (std::string const & name, Session& s)
	: ArdourWindow (string_compose ("%1 - %2", PROGRAM_NAME, name))
	, pianoroll (new Pianoroll (name, true))
	, region_editor (nullptr)
{
	pianoroll->set_session (&s);
	pianoroll->get_canvas_viewport()->set_size_request (1270, 700);

	hpacker.pack_start (pianoroll->contents(), true, true);
	pianoroll->contents().show ();

	add (hpacker);
	hpacker.show ();
}

PianorollWindow::~PianorollWindow ()
{
	delete pianoroll;
	delete region_editor;
}

void
PianorollWindow::set_show_source (bool yn)
{
	pianoroll->set_show_source (yn);
}

void
PianorollWindow::add_region (std::shared_ptr<MidiTrack> track, std::shared_ptr<MidiRegion> region)
{
	pianoroll->add_region (region, track);
}

void
PianorollWindow::set_region (std::shared_ptr<MidiTrack> track, std::shared_ptr<MidiRegion> region)
{
	pianoroll->add_region (region, track);
	pianoroll->set_region (region);

	set_title (string_compose (_("%1 Pianoroll: %2"), PROGRAM_NAME, region->name()));
}

void
PianorollWindow::replace_region (std::shared_ptr<MidiTrack> track, std::shared_ptr<MidiRegion> region)
{
	pianoroll->replace_region (region, track);
}

void
PianorollWindow::remove_regions ()
{
	pianoroll->remove_regions ();
}

bool
PianorollWindow::on_key_press_event (GdkEventKey* ev)
{
	return ARDOUR_UI::instance()->key_event_handler (ev, this);
}

bool
PianorollWindow::on_delete_event (GdkEventAny*)
{
	delete_when_idle (this);
	return true;
}
