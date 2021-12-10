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

#include <algorithm>
#include "pbd/compose.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/actions.h"

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
#include "region_view.h"
#include "ui_config.h"

#include "audio_clip_editor.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourCanvas;
using std::min;
using std::max;

/* ------------ */

AudioClipEditor::AudioClipEditor ()
{
//	set_homogenous (true);
//	set_row_spacing (4);

	set_background_color (UIConfiguration::instance().color (X_("theme:darkest")));

	const double scale = UIConfiguration::instance().get_ui_scale();
	const double width = 600. * scale;
	const double height = 210. * scale;

//	name = string_compose ("trigger %1", _trigger.index());

	frame = new Rectangle (this);

	ArdourCanvas::Rect r (0, 0, width, height);
	frame->set (r);
	frame->set_outline_all ();

	frame->Event.connect (sigc::mem_fun (*this, &AudioClipEditor::event_handler));

//	selection_connection = PublicEditor::instance().get_selection().TriggersChanged.connect (sigc::mem_fun (*this, &TriggerBoxUI::selection_changed));
}

AudioClipEditor::~AudioClipEditor ()
{
}

bool
AudioClipEditor::event_handler (GdkEvent* ev)
{
	switch (ev->type) {
	case GDK_BUTTON_PRESS:
//		PublicEditor::instance().get_selection().set (this);
		break;
	case GDK_ENTER_NOTIFY:
//		redraw ();
		break;
	case GDK_LEAVE_NOTIFY:
//		redraw ();
		break;
	default:
		break;
	}

	return false;
}

/* ====================================================== */

AudioClipEditorBox::AudioClipEditorBox ()
{
	_header_label.set_text(_("AUDIO Region Trimmer:"));
	_header_label.set_alignment(0.0, 0.5);
	pack_start(_header_label, false, false, 6);

	editor = manage (new AudioClipEditor);
	editor->set_size_request(600,120);

	pack_start(*editor, true, true);
	editor->show();
}

AudioClipEditorBox::~AudioClipEditorBox ()
{
}

void
AudioClipEditorBox::set_region (boost::shared_ptr<Region> r)
{
	set_session(&r->session());

	state_connection.disconnect();

	_region = r;

	PBD::PropertyChange interesting_stuff;
	region_changed(interesting_stuff);

	_region->PropertyChanged.connect (state_connection, invalidator (*this), boost::bind (&AudioClipEditorBox::region_changed, this, _1), gui_context());
}

void
AudioClipEditorBox::region_changed (const PBD::PropertyChange& what_changed)
{
//ToDo:  refactor the region_editor.cc  to cover this basic stuff
//	if (what_changed.contains (ARDOUR::Properties::name)) {
//		name_changed ();
//	}

//	PBD::PropertyChange interesting_stuff;
//	interesting_stuff.add (ARDOUR::Properties::length);
//	interesting_stuff.add (ARDOUR::Properties::start);
//	if (what_changed.contains (interesting_stuff))
	{
	}
}
