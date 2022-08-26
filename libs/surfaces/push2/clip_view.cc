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
#include "push2.h"
#include "clip_view.h"
#include "utils.h"

#include "pbd/i18n.h"

#ifdef __APPLE__
#define Rect ArdourCanvas::Rect
#endif

using namespace ARDOUR;
using namespace PBD;
using namespace Glib;
using namespace ArdourSurface;
using namespace ArdourCanvas;

ClipViewLayout::ClipViewLayout (Push2& p, Session & s, std::string const & name)
	: Push2Layout (p, s, name)
{
	Pango::FontDescription fd ("Sans 10");

	_bg = new ArdourCanvas::Rectangle (this);
	_bg->set (Rect (0, 0, display_width(), display_height()));
	_bg->set_fill_color (_p2.get_color (Push2::DarkBackground));

	_upper_line = new Line (this);
	_upper_line->set (Duple (0, 22.5), Duple (display_width(), 22.5));
	_upper_line->set_outline_color (_p2.get_color (Push2::LightBackground));

	for (int n = 0; n < 8; ++n) {
		Text* t;

		if (n < 4) {
			t = new Text (this);
			t->set_font_description (fd);
			t->set_color (_p2.get_color (Push2::ParameterName));
			t->set_position ( Duple (10 + (n*Push2Canvas::inter_button_spacing()), 2));
			_upper_text.push_back (t);
		}

		t = new Text (this);
		t->set_font_description (fd);
		t->set_color (_p2.get_color (Push2::ParameterName));
		t->set_position (Duple (10 + (n*Push2Canvas::inter_button_spacing()), 140));

		_lower_text.push_back (t);

		switch (n) {
		case 0:
			_upper_text[n]->set (_("Track Volume"));
			_lower_text[n]->set (_("Mute"));
			break;
		case 1:
			_upper_text[n]->set (_("Track Pan"));
			_lower_text[n]->set (_("Solo"));
			break;
		case 2:
			_upper_text[n]->set (_("Track Width"));
			_lower_text[n]->set (_("Rec-enable"));
			break;
		case 3:
			_upper_text[n]->set (_("Track Trim"));
			_lower_text[n]->set (_("In"));
			break;
		case 4:
			_lower_text[n]->set (_("Disk"));
			break;
		case 5:
			_lower_text[n]->set (_("Solo Iso"));
			break;
		case 6:
			_lower_text[n]->set (_("Solo Lock"));
			break;
		case 7:
			_lower_text[n]->set (_(""));
			break;
		}

		_knobs[n] = new Push2Knob (_p2, this);
		_knobs[n]->set_position (Duple (60 + (Push2Canvas::inter_button_spacing()*n), 95));
		_knobs[n]->set_radius (25);
	}

	_name_text = new Text (this);
	_name_text->set_font_description (fd);
	_name_text->set_position (Duple (10 + (4*Push2Canvas::inter_button_spacing()), 2));

	_meter = new LevelMeter (_p2, this, 300, ArdourCanvas::Meter::Horizontal);
	_meter->set_position (Duple (10 + (4 * Push2Canvas::inter_button_spacing()), 30));

	Pango::FontDescription fd2 ("Sans 18");
	_bbt_text = new Text (this);
	_bbt_text->set_font_description (fd2);
	_bbt_text->set_color (_p2.get_color (Push2::LightBackground));
	_bbt_text->set_position (Duple (10 + (4 * Push2Canvas::inter_button_spacing()), 60));

	_minsec_text = new Text (this);
	_minsec_text->set_font_description (fd2);
	_minsec_text->set_color (_p2.get_color (Push2::LightBackground));
	_minsec_text->set_position (Duple (10 + (4 * Push2Canvas::inter_button_spacing()), 90));
}

ClipViewLayout::~ClipViewLayout ()
{
	for (int n = 0; n < 8; ++n) {
		delete _knobs[n];
	}
}

void
ClipViewLayout::show ()
{
	Push2::ButtonID lower_buttons[] = { Push2::Lower1, Push2::Lower2, Push2::Lower3, Push2::Lower4,
	                                    Push2::Lower5, Push2::Lower6, Push2::Lower7, Push2::Lower8 };

	for (size_t n = 0; n < sizeof (lower_buttons) / sizeof (lower_buttons[0]); ++n) {
		boost::shared_ptr<Push2::Button> b = _p2.button_by_id (lower_buttons[n]);
		b->set_color (Push2::LED::DarkGray);
		b->set_state (Push2::LED::OneShot24th);
		_p2.write (b->state_msg());
	}

	show_state ();

	Container::show ();
}

void
ClipViewLayout::hide ()
{

}

void
ClipViewLayout::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Container::render (area, context);
}

void
ClipViewLayout::button_upper (uint32_t n)
{
}

void
ClipViewLayout::button_lower (uint32_t n)
{
}

void
ClipViewLayout::button_left ()
{
}

void
ClipViewLayout::button_right ()
{
}

void
ClipViewLayout::show_state ()
{
	if (!parent()) {
		return;
	}
}

void
ClipViewLayout::strip_vpot (int n, int delta)
{
}

void
ClipViewLayout::strip_vpot_touch (int n, bool touching)
{
}

void
ClipViewLayout::update_meters ()
{
}

void
ClipViewLayout::update_clocks ()
{
	samplepos_t pos = _session.audible_sample();
	bool negative = false;

	if (pos < 0) {
		pos = -pos;
		negative = true;
	}

	char buf[16];
	Temporal::BBT_Time BBT = Temporal::TempoMap::fetch()->bbt_at (timepos_t (pos));

#define BBT_BAR_CHAR "|"

	if (negative) {
		snprintf (buf, sizeof (buf), "-%03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
			  BBT.bars, BBT.beats, BBT.ticks);
	} else {
		snprintf (buf, sizeof (buf), " %03" PRIu32 BBT_BAR_CHAR "%02" PRIu32 BBT_BAR_CHAR "%04" PRIu32,
			  BBT.bars, BBT.beats, BBT.ticks);
	}

	_bbt_text->set (buf);

	samplecnt_t left;
	int hrs;
	int mins;
	int secs;
	int millisecs;

	const double sample_rate = _session.sample_rate ();

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

	_minsec_text->set (buf);
}
