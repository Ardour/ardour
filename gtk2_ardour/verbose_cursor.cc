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

#include "canvas/debug.h"
#include "canvas/scroll_group.h"
#include "canvas/tracking_text.h"

#include "ardour_ui.h"
#include "audio_clock.h"
#include "editor.h"
#include "editor_drag.h"
#include "global_signals.h"
#include "main_clock.h"
#include "verbose_cursor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;

VerboseCursor::VerboseCursor (Editor* editor)
	: _editor (editor)
{
	_canvas_item = new ArdourCanvas::TrackingText (_editor->get_noscroll_group());
	CANVAS_DEBUG_NAME (_canvas_item, "verbose canvas cursor");
	_canvas_item->set_font_description (Pango::FontDescription (ARDOUR_UI::config()->get_canvasvar_NormalBoldFont()));
	color_handler ();

	ARDOUR_UI_UTILS::ColorsChanged.connect (sigc::mem_fun (*this, &VerboseCursor::color_handler));
}

void
VerboseCursor::color_handler ()
{
	_canvas_item->set_color (ARDOUR_UI::config()->get_canvasvar_VerboseCanvasCursor());
}

ArdourCanvas::Item *
VerboseCursor::canvas_item () const
{
	return _canvas_item;
}

/** Set the contents of the cursor.
 */
void
VerboseCursor::set (string const & text)
{
	_canvas_item->set (text);
}

void
VerboseCursor::show ()
{
	_canvas_item->show_and_track (true, true);
	_canvas_item->parent()->raise_to_top ();
}

void
VerboseCursor::hide ()
{
	_canvas_item->hide ();
	_canvas_item->parent()->lower_to_bottom ();
	/* reset back to a sensible default for the next time we display the VC */
	_canvas_item->set_offset (ArdourCanvas::Duple (10, 10));
}

void
VerboseCursor::set_offset (ArdourCanvas::Duple const & d)
{
	_canvas_item->set_offset (d);
}

void
VerboseCursor::set_time (framepos_t frame)
{
	_canvas_item->set (format_time (frame));
        
}

string
VerboseCursor::format_time (framepos_t frame)
{
	char buf[128];
	Timecode::Time timecode;
	Timecode::BBT_Time bbt;

	if (_editor->_session == 0) {
		return string();
	}

	/* Take clock mode from the primary clock */

	AudioClock::Mode m = ARDOUR_UI::instance()->primary_clock->mode();

	switch (m) {
	case AudioClock::BBT:
		_editor->_session->bbt_time (frame, bbt);
		snprintf (buf, sizeof (buf), "%02" PRIu32 "|%02" PRIu32 "|%02" PRIu32, bbt.bars, bbt.beats, bbt.ticks);
		break;

	case AudioClock::Timecode:
		_editor->_session->timecode_time (frame, timecode);
		snprintf (buf, sizeof (buf), "%s", Timecode::timecode_format_time (timecode).c_str());
		break;

	case AudioClock::MinSec:
		AudioClock::print_minsec (frame, buf, sizeof (buf), _editor->_session->frame_rate());
		break;

	default:
		snprintf (buf, sizeof(buf), "%" PRIi64, frame);
		break;
	}

        return buf;
}

void
VerboseCursor::set_duration (framepos_t start, framepos_t end)
{
        _canvas_item->set (format_duration (start, end));
}

string
VerboseCursor::format_duration (framepos_t start, framepos_t end)
{
	char buf[128];
	Timecode::Time timecode;
	Timecode::BBT_Time sbbt;
	Timecode::BBT_Time ebbt;
	Meter meter_at_start (_editor->_session->tempo_map().meter_at(start));

	if (_editor->_session == 0) {
		return string();
	}

	AudioClock::Mode m = ARDOUR_UI::instance()->primary_clock->mode ();

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
		snprintf (buf, sizeof (buf), "%s", Timecode::timecode_format_time (timecode).c_str());
		break;

	case AudioClock::MinSec:
		AudioClock::print_minsec (end - start, buf, sizeof (buf), _editor->_session->frame_rate());
		break;

	default:
		snprintf (buf, sizeof(buf), "%" PRIi64, end - start);
		break;
	}

        return buf;
}

bool
VerboseCursor::visible () const
{
	return _canvas_item->visible();
}
