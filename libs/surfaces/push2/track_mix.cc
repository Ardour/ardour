/*
  Copyright (C) 2016 Paul Davis

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/enumwriter.h"

#include "midi++/parser.h"
#include "timecode/time.h"
#include "timecode/bbt_time.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/session.h"
#include "ardour/tempo.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"

#include "knob.h"
#include "menu.h"
#include "push2.h"
#include "track_mix.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;

TrackMixLayout::TrackMixLayout (Push2& p, Session& s, Cairo::RefPtr<Cairo::Context> context)
	: Push2Layout (p, s)
	, _dirty (true)
{
	Pango::FontDescription fd2 ("Sans 10");

	for (int n = 0; n < 8; ++n) {
		upper_layout[n] = Pango::Layout::create (context);
		upper_layout[n]->set_font_description (fd2);

		lower_layout[n] = Pango::Layout::create (context);
		lower_layout[n]->set_font_description (fd2);

		switch (n) {
		case 0:
			upper_layout[n]->set_text (_("TRACK VOLUME"));
			lower_layout[n]->set_text (_("MUTE"));
			break;
		case 1:
			upper_layout[n]->set_text (_("TRACK PAN"));
			lower_layout[n]->set_text (_("SOLO"));
			break;
		case 2:
			upper_layout[n]->set_text (_("TRACK WIDTH"));
			lower_layout[n]->set_text (_("REC-ENABLE"));
			break;
		case 3:
			upper_layout[n]->set_text (_("TRACK TRIM"));
			lower_layout[n]->set_text (_("IN"));
			break;
		case 4:
			upper_layout[n]->set_text (_(""));
			lower_layout[n]->set_text (_("DISK"));
			break;
		case 5:
			upper_layout[n]->set_text (_(""));
			lower_layout[n]->set_text (_("SOLO ISO"));
			break;
		case 6:
			upper_layout[n]->set_text (_(""));
			lower_layout[n]->set_text (_("SOLO LOCK"));
			break;
		case 7:
			upper_layout[n]->set_text (_(""));
			lower_layout[n]->set_text (_(""));
			break;
		}

		knobs[n] = new Push2Knob (p2, context);
		knobs[n]->set_position (60 + (120*n), 95);
		knobs[n]->set_radius (25);
	}

	ControlProtocol::StripableSelectionChanged.connect (selection_connection, invalidator (*this), boost::bind (&TrackMixLayout::selection_changed, this), &p2);
}

TrackMixLayout::~TrackMixLayout ()
{
	for (int n = 0; n < 8; ++n) {
		delete knobs[n];
	}
}

void
TrackMixLayout::selection_changed ()
{
	boost::shared_ptr<Stripable> s = ControlProtocol::first_selected_stripable();
	if (s) {
		set_stripable (s);
	}
}
void
TrackMixLayout::on_show ()
{
	selection_changed ();
}

bool
TrackMixLayout::redraw (Cairo::RefPtr<Cairo::Context> context, bool force) const
{
	bool children_dirty = false;

	for (int n = 0; n < 8; ++n) {
		if (knobs[n]->dirty()) {
			children_dirty = true;
			break;
		}
	}

	if (!children_dirty) {
		return false;
	}

	set_source_rgb (context, p2.get_color (Push2::DarkBackground));
	context->rectangle (0, 0, p2.cols, p2.rows);
	context->fill ();

	for (int n = 0; n < 8; ++n) {

		if (!upper_layout[n]->get_text().empty()) {

			/* Draw highlight box */

			uint32_t color = p2.get_color (Push2::ParameterName);
			set_source_rgb (context, color);

			context->move_to (10 + (n*120), 2);
			upper_layout[n]->update_from_cairo_context (context);
			upper_layout[n]->show_in_cairo_context (context);
		}

		if (!lower_layout[n]->get_text().empty()) {
			context->move_to (10 + (n*120), 140);
			lower_layout[n]->update_from_cairo_context (context);
			lower_layout[n]->show_in_cairo_context (context);
		}
	}

	context->move_to (0, 22.5);
	context->line_to (p2.cols, 22.5);
	context->set_line_width (1.0);
	context->stroke ();

	for (int n = 0; n < 8; ++n) {
		knobs[n]->redraw (context, force);
	}

	return true;
}

void
TrackMixLayout::button_upper (uint32_t n)
{
}

void
TrackMixLayout::button_lower (uint32_t n)
{
}

void
TrackMixLayout::set_stripable (boost::shared_ptr<Stripable> s)
{
	stripable_connections.drop_connections ();

	stripable = s;

	if (stripable) {

		stripable->DropReferences.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::drop_stripable, this), &p2);

		stripable->PropertyChanged.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::stripable_property_change, this, _1), &p2);
		stripable->presentation_info().PropertyChanged.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::stripable_property_change, this, _1), &p2);

		knobs[0]->set_controllable (stripable->gain_control());
		knobs[1]->set_controllable (stripable->pan_azimuth_control());
		knobs[1]->add_flag (Push2Knob::ArcToZero);
		knobs[2]->set_controllable (stripable->pan_width_control());
		knobs[3]->set_controllable (stripable->trim_control());
		knobs[3]->add_flag (Push2Knob::ArcToZero);
		knobs[4]->set_controllable (boost::shared_ptr<AutomationControl>());
		knobs[5]->set_controllable (boost::shared_ptr<AutomationControl>());
		knobs[6]->set_controllable (boost::shared_ptr<AutomationControl>());
		knobs[7]->set_controllable (boost::shared_ptr<AutomationControl>());

		name_changed ();
		color_changed ();
	}

	_dirty = true;
}

void
TrackMixLayout::drop_stripable ()
{
	stripable_connections.drop_connections ();
	stripable.reset ();
	_dirty = true;
}

void
TrackMixLayout::name_changed ()
{
	_dirty = true;
}

void
TrackMixLayout::color_changed ()
{
	uint32_t rgb = stripable->presentation_info().color();

	for (int n = 0; n < 8; ++n) {
		knobs[n]->set_text_color (rgb);
		knobs[n]->set_arc_start_color (rgb);
		knobs[n]->set_arc_end_color (rgb);
	}
}

void
TrackMixLayout::stripable_property_change (PropertyChange const& what_changed)
{
	if (what_changed.contains (Properties::color)) {
		color_changed ();
	}
	if (what_changed.contains (Properties::name)) {
		name_changed ();
	}
}

void
TrackMixLayout::strip_vpot (int n, int delta)
{
	boost::shared_ptr<Controllable> ac = knobs[n]->controllable();

	if (ac) {
		ac->set_value (ac->get_value() + ((2.0/64.0) * delta), PBD::Controllable::UseGroup);
	}
}

void
TrackMixLayout::strip_vpot_touch (int n, bool touching)
{
	boost::shared_ptr<AutomationControl> ac = knobs[n]->controllable();
	if (ac) {
		if (touching) {
			ac->start_touch (session.audible_frame());
		} else {
			ac->stop_touch (true, session.audible_frame());
		}
	}
}
