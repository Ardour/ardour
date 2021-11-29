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

#include "midi_region_trimmer_box.h"

#include "pbd/i18n.h"

using namespace Gtk;
using namespace ARDOUR;
using namespace ArdourWidgets;
using std::min;
using std::max;

/* ------------ */

MidiTrimmer::MidiTrimmer (ArdourCanvas::Item* parent)
	: Rectangle (parent)
{
//	set_homogenous (true);
//	set_row_spacing (4);

	set_fill_color (UIConfiguration::instance().color (X_("theme:darkest")));
	set_fill (true);

	const double scale = UIConfiguration::instance().get_ui_scale();
	const double width = 600. * scale;
	const double height = 210. * scale;

//	name = string_compose ("trigger %1", _trigger.index());

	Event.connect (sigc::mem_fun (*this, &MidiTrimmer::event_handler));

	ArdourCanvas::Rect r (0, 0, width, height);
	set (r);
	set_outline_all ();
	
//	selection_connection = PublicEditor::instance().get_selection().TriggersChanged.connect (sigc::mem_fun (*this, &TriggerBoxUI::selection_changed));
}

MidiTrimmer::~MidiTrimmer ()
{
}

void
MidiTrimmer::render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> cr) const
{
//	ArdourCanvas::Rect self (item_to_window (_rect, NO_ROUND));
//	boost::optional<ArdourCanvas::Rect> i = self.intersection (area);
//	if (!i) {
//		return;
//	}
	cr->set_identity_matrix();
	cr->translate (area.x0, area.y0-0.5);  //should be self

	float height = area.height();  //should be self
	float width = area.width();

	//black border...this should be in draw_bg
	Gtkmm2ext::set_source_rgba (cr, Gtkmm2ext::rgba_to_color (0,0,0,1));
	cr->set_line_width(1);
	cr->rectangle(0, 0, width, height);
	cr->fill ();
}

bool
MidiTrimmer::event_handler (GdkEvent* ev)
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

/* ------------ */

TrimmerBoxWidget::TrimmerBoxWidget ()
{
	trimmer = new MidiTrimmer (root());
	set_background_color (UIConfiguration::instance().color (X_("theme:bg")));
}

void
TrimmerBoxWidget::size_request (double& w, double& h) const
{
	trimmer->size_request (w, h);
	w=600;
	h=210;
}

void
TrimmerBoxWidget::on_map ()
{
	GtkCanvas::on_map ();
}

void
TrimmerBoxWidget::on_unmap ()
{
	GtkCanvas::on_unmap ();
}

/* ====================================================== */

MidiRegionTrimmerBox::MidiRegionTrimmerBox () : SessionHandlePtr()
{
	_header_label.set_text(_("MIDI Region Trimmer:"));
	_header_label.set_alignment(0.0, 0.5);
	pack_start(_header_label, false, false, 6);

	trimmer_widget = manage (new TrimmerBoxWidget());
	trimmer_widget->set_size_request(600,120);

	pack_start(*trimmer_widget, true, true);
	trimmer_widget->show();
}

MidiRegionTrimmerBox::~MidiRegionTrimmerBox ()
{
}

void
MidiRegionTrimmerBox::set_session (Session* s)
{
	SessionHandlePtr::set_session (s);
}

void
MidiRegionTrimmerBox::set_region (boost::shared_ptr<Region> r)
{
	set_session(&r->session());

	state_connection.disconnect();

	_region = r;

	PBD::PropertyChange interesting_stuff;
	region_changed(interesting_stuff);

	_region->PropertyChanged.connect (state_connection, invalidator (*this), boost::bind (&MidiRegionTrimmerBox::region_changed, this, _1), gui_context());
}

void
MidiRegionTrimmerBox::region_changed (const PBD::PropertyChange& what_changed)
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



