/*
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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
#include <algorithm>

#include "gtkmm2ext/actions.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/utils.h"

#include "ardour/location.h"
#include "ardour/profile.h"
#include "ardour/session.h"

#include "widgets/ardour_button.h"

#include "audio_clock.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor.h"
#include "midi_cue_editor.h"
#include "region_view.h"
#include "ui_config.h"

#include "midi_clip_editor.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourCanvas;
using std::max;
using std::min;


MidiClipEditorBox::MidiClipEditorBox ()
{
	_header_label.set_text (_("MIDI Region Trimmer:"));
	_header_label.set_alignment (0.0, 0.5);
	pack_start (_header_label, false, false, 6);

	editor = new MidiCueEditor ();
	editor->viewport().set_size_request (600, 120);

	pack_start (editor->viewport(), true, true);
	editor->viewport().show ();
}

MidiClipEditorBox::~MidiClipEditorBox ()
{
	delete editor;
}

void
MidiClipEditorBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
	editor->set_session (s);
}

void
MidiClipEditorBox::set_region (std::shared_ptr<Region> r, TriggerReference /*notused*/)
{
	set_session (&r->session ());

	state_connection.disconnect ();

	_region = r;

	PBD::PropertyChange interesting_stuff;
	region_changed (interesting_stuff);

	_region->PropertyChanged.connect (state_connection, invalidator (*this), boost::bind (&MidiClipEditorBox::region_changed, this, _1), gui_context ());
}

void
MidiClipEditorBox::region_changed (const PBD::PropertyChange& what_changed)
{
}
