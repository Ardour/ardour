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

#include <cairomm/region.h>
#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/i18n.h"
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
#include "ardour/monitor_control.h"
#include "ardour/session.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/solo_safe_control.h"
#include "ardour/tempo.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"

#include "canvas/line.h"
#include "canvas/rectangle.h"
#include "canvas/text.h"

#include "canvas.h"
#include "knob.h"
#include "menu.h"
#include "push2.h"
#include "track_mix.h"
#include "utils.h"

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace ArdourCanvas;

TrackMixLayout::TrackMixLayout (Push2& p, Session& s)
	: Push2Layout (p, s)
{
	Pango::FontDescription fd ("Sans 10");

	bg = new Rectangle (this);
	bg->set (Rect (0, 0, display_width(), display_height()));
	bg->set_fill_color (p2.get_color (Push2::DarkBackground));

	upper_line = new Line (this);
	upper_line->set (Duple (0, 22.5), Duple (display_width(), 22.5));
	upper_line->set_outline_color (p2.get_color (Push2::LightBackground));

	for (int n = 0; n < 8; ++n) {
		Text* t;

		if (n < 4) {
			t = new Text (this);
			t->set_font_description (fd);
			t->set_color (p2.get_color (Push2::ParameterName));
			t->set_position ( Duple (10 + (n*Push2Canvas::inter_button_spacing()), 2));
			upper_text.push_back (t);
		}

		t = new Text (this);
		t->set_font_description (fd);
		t->set_color (p2.get_color (Push2::ParameterName));
		t->set_position (Duple (10 + (n*Push2Canvas::inter_button_spacing()), 140));

		lower_text.push_back (t);

		switch (n) {
		case 0:
			upper_text[n]->set (_("Track Volume"));
			lower_text[n]->set (_("Mute"));
			break;
		case 1:
			upper_text[n]->set (_("Track Pan"));
			lower_text[n]->set (_("Solo"));
			break;
		case 2:
			upper_text[n]->set (_("Track Width"));
			lower_text[n]->set (_("Rec-enable"));
			break;
		case 3:
			upper_text[n]->set (_("Track Trim"));
			lower_text[n]->set (_("In"));
			break;
		case 4:
			lower_text[n]->set (_("Disk"));
			break;
		case 5:
			lower_text[n]->set (_("Solo Iso"));
			break;
		case 6:
			lower_text[n]->set (_("Solo Lock"));
			break;
		case 7:
			lower_text[n]->set (_(""));
			break;
		}

		knobs[n] = new Push2Knob (p2, this);
		knobs[n]->set_position (Duple (60 + (Push2Canvas::inter_button_spacing()*n), 95));
		knobs[n]->set_radius (25);
	}

	name_text = new Text (this);
	name_text->set_font_description (fd);
	name_text->set_position (Duple (10 + (4*Push2Canvas::inter_button_spacing()), 2));

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
TrackMixLayout::show ()
{
	selection_changed ();

	Push2::ButtonID lower_buttons[] = { Push2::Lower1, Push2::Lower2, Push2::Lower3, Push2::Lower4,
	                                    Push2::Lower5, Push2::Lower6, Push2::Lower7, Push2::Lower8 };

	for (size_t n = 0; n < sizeof (lower_buttons) / sizeof (lower_buttons[0]); ++n) {
		Push2::Button* b = p2.button_by_id (lower_buttons[n]);
		b->set_color (Push2::LED::DarkGray);
		b->set_state (Push2::LED::OneShot24th);
		p2.write (b->state_msg());
	}

	Container::show ();
}

void
TrackMixLayout::hide ()
{
	set_stripable (boost::shared_ptr<Stripable>());
}

void
TrackMixLayout::render (ArdourCanvas::Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Container::render (area, context);
}

void
TrackMixLayout::button_upper (uint32_t n)
{
}

void
TrackMixLayout::button_lower (uint32_t n)
{
	if (!stripable) {
		return;
	}

	MonitorChoice mc;

	switch (n) {
	case 0:
		stripable->mute_control()->set_value (!stripable->mute_control()->get_value(), PBD::Controllable::UseGroup);
		break;
	case 1:
		stripable->solo_control()->set_value (!stripable->solo_control()->get_value(), PBD::Controllable::UseGroup);
		break;
	case 2:
		stripable->rec_enable_control()->set_value (!stripable->rec_enable_control()->get_value(), PBD::Controllable::UseGroup);
		break;
	case 3:
		mc = stripable->monitoring_control()->monitoring_choice();
		switch (mc) {
		case MonitorInput:
			stripable->monitoring_control()->set_value (MonitorAuto, PBD::Controllable::UseGroup);
			break;
		default:
			stripable->monitoring_control()->set_value (MonitorInput, PBD::Controllable::UseGroup);
			break;
		}
		break;
	case 4:
		mc = stripable->monitoring_control()->monitoring_choice();
		switch (mc) {
		case MonitorDisk:
			stripable->monitoring_control()->set_value (MonitorAuto, PBD::Controllable::UseGroup);
			break;
		default:
			stripable->monitoring_control()->set_value (MonitorDisk, PBD::Controllable::UseGroup);
			break;
		}
		break;
	case 5:
		stripable->solo_isolate_control()->set_value (!stripable->solo_isolate_control()->get_value(), PBD::Controllable::UseGroup);
		break;
	case 6:
		stripable->solo_safe_control()->set_value (!stripable->solo_safe_control()->get_value(), PBD::Controllable::UseGroup);
		break;
	case 7:
		/* nothing here */
		break;
	}
}

void
TrackMixLayout::button_left ()
{
	p2.access_action ("Editor/select-prev-route");
}

void
TrackMixLayout::button_right ()
{
	p2.access_action ("Editor/select-next-route");
}

void
TrackMixLayout::simple_control_change (boost::shared_ptr<AutomationControl> ac, Push2::ButtonID bid)
{
	if (!ac) {
		return;
	}

	Push2::Button* b = p2.button_by_id (bid);

	if (!bid) {
		return;
	}


	if (ac->get_value()) {
		b->set_color (selection_color);
	} else {
		b->set_color (Push2::LED::DarkGray);
	}
	b->set_state (Push2::LED::OneShot24th);
	p2.write (b->state_msg());
}

void
TrackMixLayout::solo_change ()
{
	if (!stripable) {
		return;
	}

	simple_control_change (stripable->solo_control(), Push2::Lower2);
}

void
TrackMixLayout::mute_change ()
{
	if (!stripable) {
		return;
	}

	simple_control_change (stripable->mute_control(), Push2::Lower1);
}

void
TrackMixLayout::rec_enable_change ()
{
	if (!stripable) {
		return;
	}

	simple_control_change (stripable->rec_enable_control(), Push2::Lower3);
}

void
TrackMixLayout::solo_iso_change ()
{
	if (!stripable) {
		return;
	}

	simple_control_change (stripable->solo_isolate_control(), Push2::Lower6);
}
void
TrackMixLayout::solo_safe_change ()
{
	if (!stripable) {
		return;
	}

	simple_control_change (stripable->solo_safe_control(), Push2::Lower7);
}

void
TrackMixLayout::monitoring_change ()
{
	if (!stripable) {
		return;
	}

	if (!stripable->monitoring_control()) {
		return;
	}

	Push2::Button* b1 = p2.button_by_id (Push2::Lower4);
	Push2::Button* b2 = p2.button_by_id (Push2::Lower5);
	uint8_t b1_color;
	uint8_t b2_color;

	MonitorChoice mc = stripable->monitoring_control()->monitoring_choice ();

	switch (mc) {
	case MonitorAuto:
		b1_color = Push2::LED::DarkGray;
		b2_color = Push2::LED::DarkGray;
		break;
	case MonitorInput:
		b1_color = selection_color;
		b2_color = Push2::LED::DarkGray;
		break;
	case MonitorDisk:
		b1_color = Push2::LED::DarkGray;
		b2_color = selection_color;
		break;
	case MonitorCue:
		b1_color = selection_color;
		b2_color = selection_color;
		break;
	}

	b1->set_color (b1_color);
	b1->set_state (Push2::LED::OneShot24th);
	p2.write (b1->state_msg());

	b2->set_color (b2_color);
	b2->set_state (Push2::LED::OneShot24th);
	p2.write (b2->state_msg());
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

		stripable->solo_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::solo_change, this), &p2);
		stripable->mute_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::mute_change, this), &p2);
		stripable->solo_isolate_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::solo_iso_change, this), &p2);
		stripable->solo_safe_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::solo_safe_change, this), &p2);

		if (stripable->rec_enable_control()) {
			stripable->rec_enable_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::rec_enable_change, this), &p2);
		}

		if (stripable->monitoring_control()) {
			stripable->monitoring_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::monitoring_change, this), &p2);
		}

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
		solo_change ();
		mute_change ();
		rec_enable_change ();
		solo_iso_change ();
		solo_safe_change ();
		monitoring_change ();
	}
}

void
TrackMixLayout::drop_stripable ()
{
	stripable_connections.drop_connections ();
	stripable.reset ();
}

void
TrackMixLayout::name_changed ()
{
	if (stripable) {
		/* poor-man's right justification */
		char buf[96];
		snprintf (buf, sizeof (buf), "%*s", (int) sizeof (buf) - 1, stripable->name().c_str());
		name_text->set (buf);
	}
}

void
TrackMixLayout::color_changed ()
{
	if (!parent()) {
		return;
	}

	Color rgba = stripable->presentation_info().color();
	selection_color = p2.get_color_index (rgba);

	name_text->set_color (rgba);

	for (int n = 0; n < 8; ++n) {
		knobs[n]->set_text_color (rgba);
		knobs[n]->set_arc_start_color (rgba);
		knobs[n]->set_arc_end_color (rgba);
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
