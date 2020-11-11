/*
 * Copyright (C) 2016-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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

#include <cairomm/region.h>
#include <pangomm/layout.h>

#include "pbd/compose.h"
#include "pbd/convert.h"
#include "pbd/debug.h"
#include "pbd/failed_constructor.h"
#include "pbd/file_utils.h"
#include "pbd/search_path.h"
#include "pbd/enumwriter.h"

#include "midi++/parser.h"

#include "temporal/time.h"
#include "temporal/bbt_time.h"

#include "ardour/async_midi_port.h"
#include "ardour/audioengine.h"
#include "ardour/debug.h"
#include "ardour/dsp_filter.h"
#include "ardour/filesystem_paths.h"
#include "ardour/midiport_manager.h"
#include "ardour/midi_track.h"
#include "ardour/midi_port.h"
#include "ardour/monitor_control.h"
#include "ardour/meter.h"
#include "ardour/session.h"
#include "ardour/solo_isolate_control.h"
#include "ardour/solo_safe_control.h"
#include "ardour/tempo.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"

#include "canvas/box.h"
#include "canvas/line.h"
#include "canvas/meter.h"
#include "canvas/rectangle.h"
#include "canvas/text.h"
#include "canvas/types.h"

#include "canvas.h"
#include "knob.h"
#include "level_meter.h"
#include "menu.h"
#include "push2.h"
#include "track_mix.h"
#include "utils.h"

#include "pbd/i18n.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace ARDOUR;
using namespace std;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace ArdourCanvas;

TrackMixLayout::TrackMixLayout (Push2& p, Session & s, std::string const & name)
	: Push2Layout (p, s, name)
{
	Pango::FontDescription fd ("Sans 10");

	bg = new ArdourCanvas::Rectangle (this);
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

	meter = new LevelMeter (p2, this, 300, ArdourCanvas::Meter::Horizontal);
	meter->set_position (Duple (10 + (4 * Push2Canvas::inter_button_spacing()), 30));

	Pango::FontDescription fd2 ("Sans 18");
	bbt_text = new Text (this);
	bbt_text->set_font_description (fd2);
	bbt_text->set_color (p2.get_color (Push2::LightBackground));
	bbt_text->set_position (Duple (10 + (4 * Push2Canvas::inter_button_spacing()), 60));

	minsec_text = new Text (this);
	minsec_text->set_font_description (fd2);
	minsec_text->set_color (p2.get_color (Push2::LightBackground));
	minsec_text->set_position (Duple (10 + (4 * Push2Canvas::inter_button_spacing()), 90));
}

TrackMixLayout::~TrackMixLayout ()
{
	for (int n = 0; n < 8; ++n) {
		delete knobs[n];
	}
}

void
TrackMixLayout::show ()
{
	Push2::ButtonID lower_buttons[] = { Push2::Lower1, Push2::Lower2, Push2::Lower3, Push2::Lower4,
	                                    Push2::Lower5, Push2::Lower6, Push2::Lower7, Push2::Lower8 };

	for (size_t n = 0; n < sizeof (lower_buttons) / sizeof (lower_buttons[0]); ++n) {
		boost::shared_ptr<Push2::Button> b = p2.button_by_id (lower_buttons[n]);
		b->set_color (Push2::LED::DarkGray);
		b->set_state (Push2::LED::OneShot24th);
		p2.write (b->state_msg());
	}

	show_state ();

	Container::show ();
}

void
TrackMixLayout::hide ()
{

}

void
TrackMixLayout::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
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
		if (stripable->mute_control()) {
			stripable->mute_control()->set_value (!stripable->mute_control()->get_value(), PBD::Controllable::UseGroup);
		}
		break;
	case 1:
		if (stripable->solo_control()) {
			session.set_control (stripable->solo_control(), !stripable->solo_control()->self_soloed(), PBD::Controllable::UseGroup);
		}
		break;
	case 2:
		if (stripable->rec_enable_control()) {
			stripable->rec_enable_control()->set_value (!stripable->rec_enable_control()->get_value(), PBD::Controllable::UseGroup);
		}
		break;
	case 3:
		if (stripable->monitor_control()) {
			mc = stripable->monitoring_control()->monitoring_choice();
			switch (mc) {
			case MonitorInput:
				stripable->monitoring_control()->set_value (MonitorAuto, PBD::Controllable::UseGroup);
				break;
			default:
				stripable->monitoring_control()->set_value (MonitorInput, PBD::Controllable::UseGroup);
				break;
			}
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
		if (stripable->solo_isolate_control()) {
			stripable->solo_isolate_control()->set_value (!stripable->solo_isolate_control()->get_value(), PBD::Controllable::UseGroup);
		}
		break;
	case 6:
		if (stripable->solo_safe_control()) {
			stripable->solo_safe_control()->set_value (!stripable->solo_safe_control()->get_value(), PBD::Controllable::UseGroup);
		}
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
	if (!ac || !parent()) {
		return;
	}

	boost::shared_ptr<Push2::Button> b = p2.button_by_id (bid);

	if (!b) {
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
TrackMixLayout::solo_mute_change ()
{
	if (!stripable) {
		return;
	}

	boost::shared_ptr<Push2::Button> b = p2.button_by_id (Push2::Lower2);

	if (b) {
		boost::shared_ptr<SoloControl> sc = stripable->solo_control();

		if (sc) {
			if (sc->soloed_by_self_or_masters()) {
				b->set_color (selection_color);
				b->set_state (Push2::LED::OneShot24th);
			} else if (sc->soloed_by_others_upstream() || sc->soloed_by_others_downstream()) {
				b->set_color (selection_color);
				b->set_state (Push2::LED::Blinking8th);
			} else {
				b->set_color (Push2::LED::DarkGray);
				b->set_state (Push2::LED::OneShot24th);
			}
		} else {
			b->set_color (Push2::LED::DarkGray);
			b->set_state (Push2::LED::OneShot24th);
		}

		p2.write (b->state_msg());
	}

	b = p2.button_by_id (Push2::Lower1);

	if (b) {
		boost::shared_ptr<MuteControl> mc = stripable->mute_control();

		if (mc) {
			if (mc->muted_by_self_or_masters()) {
				b->set_color (selection_color);
				b->set_state (Push2::LED::OneShot24th);
			} else if (mc->muted_by_others_soloing()) {
				b->set_color (selection_color);
				b->set_state (Push2::LED::Blinking8th);
			} else {
				b->set_color (Push2::LED::DarkGray);
				b->set_state (Push2::LED::OneShot24th);
			}

		} else {
			b->set_color (Push2::LED::DarkGray);
			b->set_state (Push2::LED::OneShot24th);
		}

		p2.write (b->state_msg());
	}

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

	boost::shared_ptr<Push2::Button> b1 = p2.button_by_id (Push2::Lower4);
	boost::shared_ptr<Push2::Button> b2 = p2.button_by_id (Push2::Lower5);
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
TrackMixLayout::show_state ()
{
	if (!parent()) {
		return;
	}

	if (stripable) {
		name_changed ();
		color_changed ();
		solo_mute_change ();
		rec_enable_change ();
		solo_iso_change ();
		solo_safe_change ();
		monitoring_change ();

		meter->set_meter (stripable->peak_meter ().get());
	} else {
		meter->set_meter (0);
	}
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

		stripable->solo_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::solo_mute_change, this), &p2);
		stripable->mute_control()->Changed.connect (stripable_connections, invalidator (*this), boost::bind (&TrackMixLayout::solo_mute_change, this), &p2);
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
	}

	show_state ();
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

		name_text->set (stripable->name());

		/* right justify */

		Duple pos;
		pos.y = name_text->position().y;
		pos.x = display_width() - 10 - name_text->width();

		name_text->set_position (pos);
	}
}

void
TrackMixLayout::color_changed ()
{
	if (!parent()) {
		return;
	}

	Gtkmm2ext::Color rgba = stripable->presentation_info().color();
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
		const timepos_t now (session.audible_sample());
		if (touching) {
			ac->start_touch (now);
		} else {
			ac->stop_touch (now);
		}
	}
}

void
TrackMixLayout::update_meters ()
{
	if (!stripable) {
		return;
	}

	meter->update_meters ();
}

void
TrackMixLayout::update_clocks ()
{
	samplepos_t pos = session.audible_sample();
	bool negative = false;

	if (pos < 0) {
		pos = -pos;
		negative = true;
	}

	char buf[16];
	Temporal::BBT_Time BBT = session.tempo_map().bbt_at (pos);

#define BBT_BAR_CHAR "|"

	if (negative) {
		snprintf (buf, sizeof (buf), "-%03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
			  BBT.bars, BBT.beats, BBT.ticks);
	} else {
		snprintf (buf, sizeof (buf), " %03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
			  BBT.bars, BBT.beats, BBT.ticks);
	}

	bbt_text->set (buf);

	samplecnt_t left;
	int hrs;
	int mins;
	int secs;
	int millisecs;

	const double sample_rate = session.sample_rate ();

	left = pos;
	hrs = (int) floor (left / (sample_rate * 60.0f * 60.0f));
	left -= (samplecnt_t) floor (hrs * sample_rate * 60.0f * 60.0f);
	mins = (int) floor (left / (sample_rate * 60.0f));
	left -= (samplecnt_t) floor (mins * sample_rate * 60.0f);
	secs = (int) floor (left / (float) sample_rate);
	left -= (samplecnt_t) floor ((double)(secs * sample_rate));
	millisecs = floor (left * 1000.0 / (float) sample_rate);

	if (negative) {
		snprintf (buf, sizeof (buf), "-%02" PRId32 ":%02" PRId32 ":%02" PRId32 ".%03" PRId32, hrs, mins, secs, millisecs);
	} else {
		snprintf (buf, sizeof (buf), " %02" PRId32 ":%02" PRId32 ":%02" PRId32 ".%03" PRId32, hrs, mins, secs, millisecs);
	}

	minsec_text->set (buf);
}
