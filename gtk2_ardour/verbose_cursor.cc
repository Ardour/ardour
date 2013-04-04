/*
    Copyright (C) 2000-2011 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#include <string>
#include <gtkmm/enums.h>
#include "pbd/stacktrace.h"
#include "ardour/profile.h"

#include "ardour_ui.h"
#include "audio_clock.h"
#include "editor.h"
#include "editor_drag.h"
#include "main_clock.h"
#include "utils.h"
#include "verbose_cursor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

VerboseCursor::VerboseCursor (Editor* editor)
	: _editor (editor)
	, _visible (false)
	, _xoffset (0)
	, _yoffset (0)
{
	_canvas_item = new ArdourCanvas::Text (_editor->_track_canvas->root());
	_canvas_item->set_ignore_events (true);
	_canvas_item->set_font_description (get_font_for_style (N_("VerboseCanvasCursor")));
	// CAIROCANVAS
	// _canvas_item->property_anchor() = Gtk::ANCHOR_NW;
}

ArdourCanvas::Item *
VerboseCursor::canvas_item () const
{
	return _canvas_item;
}

void
VerboseCursor::set (string const & text, double x, double y)
{
	set_text (text);
	set_position (x, y);
}

void
VerboseCursor::set_text (string const & text)
{
	_canvas_item->set (text);
}

/** @param xoffset x offset to be applied on top of any set_position() call
 *  before the next show ().
 *  @param yoffset y offset as above.
 */
void
VerboseCursor::show (double xoffset, double yoffset)
{
	_xoffset = xoffset;
	_yoffset = yoffset;

	if (_visible) {
		return;
	}

	_canvas_item->raise_to_top ();
	_canvas_item->show ();
	_visible = true;
}

void
VerboseCursor::hide ()
{
	_canvas_item->hide ();
	_visible = false;
}

double
VerboseCursor::clamp_x (double x)
{
	_editor->clamp_verbose_cursor_x (x);
	return x;
}

double
VerboseCursor::clamp_y (double y)
{
	_editor->clamp_verbose_cursor_y (y);
	return y;
}

void
VerboseCursor::set_time (framepos_t frame, double x, double y)
{
	char buf[128];
	Timecode::Time timecode;
	Timecode::BBT_Time bbt;
	int hours, mins;
	framepos_t frame_rate;
	float secs;

	if (_editor->_session == 0) {
		return;
	}

	AudioClock::Mode m;

	if (Profile->get_sae() || Profile->get_small_screen()) {
		m = ARDOUR_UI::instance()->primary_clock->mode();
	} else {
		m = ARDOUR_UI::instance()->secondary_clock->mode();
	}

	switch (m) {
	case AudioClock::BBT:
		_editor->_session->bbt_time (frame, bbt);
		snprintf (buf, sizeof (buf), "%02" PRIu32 "|%02" PRIu32 "|%02" PRIu32, bbt.bars, bbt.beats, bbt.ticks);
		break;

	case AudioClock::Timecode:
		_editor->_session->timecode_time (frame, timecode);
		snprintf (buf, sizeof (buf), "%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32, timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
		break;

	case AudioClock::MinSec:
		/* XXX this is copied from show_verbose_duration_cursor() */
		frame_rate = _editor->_session->frame_rate();
		hours = frame / (frame_rate * 3600);
		frame = frame % (frame_rate * 3600);
		mins = frame / (frame_rate * 60);
		frame = frame % (frame_rate * 60);
		secs = (float) frame / (float) frame_rate;
		snprintf (buf, sizeof (buf), "%02" PRId32 ":%02" PRId32 ":%07.4f", hours, mins, secs);
		break;

	default:
		snprintf (buf, sizeof(buf), "%" PRIi64, frame);
		break;
	}

	set (buf, x, y);
}

void
VerboseCursor::set_duration (framepos_t start, framepos_t end, double x, double y)
{
	char buf[128];
	Timecode::Time timecode;
	Timecode::BBT_Time sbbt;
	Timecode::BBT_Time ebbt;
	int hours, mins;
	framepos_t distance, frame_rate;
	float secs;
	Meter meter_at_start (_editor->_session->tempo_map().meter_at(start));

	if (_editor->_session == 0) {
		return;
	}

	AudioClock::Mode m;

	if (Profile->get_sae() || Profile->get_small_screen()) {
		m = ARDOUR_UI::instance()->primary_clock->mode ();
	} else {
		m = ARDOUR_UI::instance()->secondary_clock->mode ();
	}

	switch (m) {
	case AudioClock::BBT:
	{
		_editor->_session->bbt_time (start, sbbt);
		_editor->_session->bbt_time (end, ebbt);

		/* subtract */
		/* XXX this computation won't work well if the
		user makes a selection that spans any meter changes.
		*/

		/* use signed integers for the working values so that
		   we can underflow.
		*/

		int ticks = ebbt.ticks;
		int beats = ebbt.beats;
		int bars = ebbt.bars;

		ticks -= sbbt.ticks;
		if (ticks < 0) {
			ticks += int (Timecode::BBT_Time::ticks_per_beat);
			--beats;
		}

		beats -= sbbt.beats;
		if (beats < 0) {
			beats += int (meter_at_start.divisions_per_bar());
			--bars;
		}

		bars -= sbbt.bars;

		snprintf (buf, sizeof (buf), "%02" PRIu32 "|%02" PRIu32 "|%02" PRIu32, bars, beats, ticks);
		break;
	}

	case AudioClock::Timecode:
		_editor->_session->timecode_duration (end - start, timecode);
		snprintf (buf, sizeof (buf), "%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32, timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
		break;

	case AudioClock::MinSec:
		/* XXX this stuff should be elsewhere.. */
		distance = end - start;
		frame_rate = _editor->_session->frame_rate();
		hours = distance / (frame_rate * 3600);
		distance = distance % (frame_rate * 3600);
		mins = distance / (frame_rate * 60);
		distance = distance % (frame_rate * 60);
		secs = (float) distance / (float) frame_rate;
		snprintf (buf, sizeof (buf), "%02" PRId32 ":%02" PRId32 ":%07.4f", hours, mins, secs);
		break;

	default:
		snprintf (buf, sizeof(buf), "%" PRIi64, end - start);
		break;
	}

	set (buf, x, y);
}

void
VerboseCursor::set_color (uint32_t color)
{
	_canvas_item->set_color (color);
}

/** Set the position of the verbose cursor.  Any x/y offsets
 *  passed to the last call to show() will be applied to the
 *  coordinates passed in here.
 */
void
VerboseCursor::set_position (double x, double y)
{
	_canvas_item->set_x_position (clamp_x (x + _xoffset));
	_canvas_item->set_y_position (clamp_y (y + _yoffset));
}

bool
VerboseCursor::visible () const
{
	return _visible;
}
