/*
 * Copyright (C) 2011-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2011-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2011-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#include <string>
#include <gtkmm/enums.h>

#include "ardour/profile.h"

#include "canvas/debug.h"
#include "canvas/scroll_group.h"
#include "canvas/tracking_text.h"

#include "audio_clock.h"
#include "editor.h"
#include "editor_drag.h"
#include "main_clock.h"
#include "verbose_cursor.h"
#include "ardour_ui.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace Temporal;

VerboseCursor::VerboseCursor (Editor* editor)
	: _editor (editor)
{
	_canvas_item = new ArdourCanvas::TrackingText (_editor->get_noscroll_group());
	CANVAS_DEBUG_NAME (_canvas_item, "verbose canvas cursor");
	_canvas_item->set_font_description (Pango::FontDescription (UIConfiguration::instance().get_LargerBoldFont()));
	color_handler ();

	UIConfiguration::instance().ColorsChanged.connect (sigc::mem_fun (*this, &VerboseCursor::color_handler));
}

void
VerboseCursor::color_handler ()
{
	_canvas_item->set_color (UIConfiguration::instance().color_mod ("verbose canvas cursor", "verbose canvas cursor"));
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
VerboseCursor::set_time (samplepos_t sample)
{
	char buf[128];
	Timecode::Time timecode;
	Temporal::BBT_Time bbt;

	if (_editor->_session == 0) {
		return;
	}

	/* Take clock mode from the primary clock */

	AudioClock::Mode m = ARDOUR_UI::instance()->primary_clock->mode();

	switch (m) {
	case AudioClock::BBT:
		_editor->_session->bbt_time (timepos_t (sample), bbt);
		snprintf (buf, sizeof (buf), "%02" PRIu32 "|%02" PRIu32 "|%02" PRIu32, bbt.bars, bbt.beats, bbt.ticks);
		break;

	case AudioClock::Timecode:
		_editor->_session->timecode_time (sample, timecode);
		snprintf (buf, sizeof (buf), "%s", Timecode::timecode_format_time (timecode).c_str());
		break;

	case AudioClock::MinSec:
		AudioClock::print_minsec (sample, buf, sizeof (buf), _editor->_session->sample_rate());
		break;

	case AudioClock::Seconds:
		snprintf (buf, sizeof(buf), "%.1f", sample / (float)_editor->_session->sample_rate());
		break;

	default:
		snprintf (buf, sizeof(buf), "%" PRIi64, sample);
		break;
	}

	_canvas_item->set (buf);
}

void
VerboseCursor::set_duration (samplepos_t start, samplepos_t end)
{
	char buf[128];
	Timecode::Time timecode;
	Temporal::BBT_Time sbbt;
	Temporal::BBT_Time ebbt;
	Meter& meter_at_start (_editor->_session->tempo_map().metric_at (start).meter());

	if (_editor->_session == 0) {
		return;
	}

	AudioClock::Mode m = ARDOUR_UI::instance()->primary_clock->mode ();

	switch (m) {
	case AudioClock::BBT:
	{
		_editor->_session->bbt_time (timepos_t (start), sbbt);
		_editor->_session->bbt_time (timepos_t (end), ebbt);

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
			ticks += int (Temporal::ticks_per_beat);
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
		AudioClock::print_minsec (end - start, buf, sizeof (buf), _editor->_session->sample_rate());
		break;

	case AudioClock::Seconds:
		snprintf (buf, sizeof(buf), "%.1f", (end - start) / (float)_editor->_session->sample_rate());
		break;

	default:
		snprintf (buf, sizeof(buf), "%" PRIi64, end - start);
		break;
	}

	_canvas_item->set (buf);
}

bool
VerboseCursor::visible () const
{
	return _canvas_item->visible();
}
