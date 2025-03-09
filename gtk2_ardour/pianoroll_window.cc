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

#include "gtkmm2ext/doi.h"

#include "ardour_ui.h"
#include "pianoroll.h"
#include "pianoroll_window.h"
#include "region_editor.h"

using namespace ARDOUR;

PianorollWindow::PianorollWindow (std::string const & name, Session& s)
	: ArdourWindow (string_compose ("%1 - %2", PROGRAM_NAME, name))
	, pianoroll (new Pianoroll (name))
{
	pianoroll->set_session (&s);
	pianoroll->viewport().set_size_request (600, 120);

	add (hpacker);
	hpacker.show ();
}

PianorollWindow::~PianorollWindow ()
{
	delete pianoroll;
}

void
PianorollWindow::set (std::shared_ptr<MidiTrack> track, std::shared_ptr<MidiRegion> region)
{
	pianoroll->set_track (track);
	pianoroll->set_region (region);

	region_editor = new RegionEditor (pianoroll->session(), region);
	hpacker.pack_start (*region_editor, false, false);
	hpacker.pack_start (pianoroll->contents(), true, true);

	region_editor->show ();
	pianoroll->contents().show ();
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
