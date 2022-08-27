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
#include "ardour/triggerbox.h"

#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/rgb_macros.h"

#include "canvas/box.h"
#include "canvas/line.h"
#include "canvas/meter.h"
#include "canvas/rectangle.h"
#include "canvas/text.h"
#include "canvas/types.h"

#include "canvas.h"
#include "cues.h"
#include "knob.h"
#include "level_meter.h"
#include "push2.h"
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

CueLayout::CueLayout (Push2& p, Session & s, std::string const & name)
	: Push2Layout (p, s, name)
	, track_base (0)
	, scene_base (0)
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

		t = new Text (this);
		t->set_font_description (fd);
		t->set_color (_p2.get_color (Push2::ParameterName));
		t->set_position ( Duple (10 + (n*Push2Canvas::inter_button_spacing()), 2));
		_upper_text.push_back (t);

		t = new Text (this);
		t->set_font_description (fd);
		t->set_color (_p2.get_color (Push2::ParameterName));
		t->set_position (Duple (10 + (n*Push2Canvas::inter_button_spacing()), 140));

		_lower_text.push_back (t);

		_knobs[n] = new Push2Knob (_p2, this);
		_knobs[n]->set_position (Duple (60 + (Push2Canvas::inter_button_spacing()*n), 95));
		_knobs[n]->set_radius (25);
	}
}


CueLayout::~CueLayout ()
{
	for (int n = 0; n < 8; ++n) {
		delete _knobs[n];
	}
}

void
CueLayout::show ()
{
	Push2::ButtonID lower_buttons[] = {
		Push2::Lower1, Push2::Lower2, Push2::Lower3, Push2::Lower4,
		Push2::Lower5, Push2::Lower6, Push2::Lower7, Push2::Lower8
	};

	for (auto & lb : lower_buttons) {
		boost::shared_ptr<Push2::Button> b = _p2.button_by_id (lb);
		b->set_color (Push2::LED::DarkGray);
		b->set_state (Push2::LED::OneShot24th);
		_p2.write (b->state_msg());
	}

	Push2::ButtonID scene_buttons[] = {
		Push2::Fwd32ndT, Push2::Fwd32nd, Push2::Fwd16th, Push2::Fwd16thT,
		Push2::Fwd8thT, Push2::Fwd8th, Push2::Fwd4trT, Push2::Fwd4tr
	};

	for (auto & sb : scene_buttons) {
		boost::shared_ptr<Push2::Button> b = _p2.button_by_id (sb);
		b->set_color (Push2::LED::Green);
		b->set_state (Push2::LED::NoTransition);
		_p2.write (b->state_msg());
	}

	show_state ();

	Container::show ();
}

void
CueLayout::hide ()
{
	Push2::ButtonID scene_buttons[] = {
		Push2::Fwd32ndT, Push2::Fwd32nd, Push2::Fwd16th, Push2::Fwd16thT,
		Push2::Fwd8thT, Push2::Fwd8th, Push2::Fwd4trT, Push2::Fwd4tr
	};

	for (auto & sb : scene_buttons) {
		boost::shared_ptr<Push2::Button> b = _p2.button_by_id (sb);
		b->set_color (Push2::LED::Black);
		b->set_state (Push2::LED::NoTransition);
		_p2.write (b->state_msg());
	}
}

void
CueLayout::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Container::render (area, context);
}

void
CueLayout::button_upper (uint32_t n)
{
}

void
CueLayout::button_lower (uint32_t n)
{
	if (_p2.stop_down()) {
		_p2.unbang (n + track_base);
	}
}

void
CueLayout::button_left ()
{
	if (track_base > 0) {
		track_base--;
		show_state ();
	}
}

void
CueLayout::button_page_left ()
{
	if (track_base > 8) {
		track_base -= 8; /* XXX get back to zero when appropriate */
		show_state ();
	}
}

void
CueLayout::button_right ()
{
	track_base++;
	show_state ();
}

void
CueLayout::button_page_right ()
{
	track_base += 8; /* XXX limit to number of tracks */
	show_state ();
}

void
CueLayout::button_up ()
{
	if (scene_base > 0) {
		scene_base--;
		show_state ();
	}
}

void
CueLayout::button_octave_up ()
{
	if (scene_base > 8) {
		scene_base -= 8;
		show_state ();
	}
}

void
CueLayout::button_down ()
{
	scene_base++;
	show_state ();
}

void
CueLayout::button_octave_down ()
{
	scene_base++;
	show_state ();
}

void
CueLayout::show_state ()
{
	if (!parent()) {
		return;
	}

	for (auto & t : _upper_text) {
	}

	for (auto & t : _lower_text) {
	}
}

void
CueLayout::strip_vpot (int n, int delta)
{
}

void
CueLayout::strip_vpot_touch (int n, bool touching)
{
}

void
CueLayout::button_rhs (int row)
{
	std::cerr << "Scene " << row + scene_base << std::endl;
	_p2.get_session().cue_bang (row + scene_base);
}

void
CueLayout::button_stop_press ()
{
	if (_p2.modifier_state() == Push2::ModShift) {
		_p2.get_session().stop_all_triggers (false); /* quantized global stop */
	}
}

void
CueLayout::pad_press (int x, int y)
{
	_p2.bang (x + track_base, y + scene_base);
}
