/*
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2007 Doug McLain <doug@nostar.net>
 * Copyright (C) 2008-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include "canvas/debug.h"
#include "canvas/rect_set.h"

#include "midi_view_background.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;

MidiViewBackground::MidiViewBackground (ArdourCanvas::Item* parent, EditingContext& ec)
	: note_range_adjustment (0.0f, 0.0f, 0.0f)
	, _editing_context (ec)
	, _range_dirty (false)
	, _range_sum_cache (-1.0)
	, _lowest_note (UIConfiguration::instance().get_default_lower_midi_note())
	, _highest_note (UIConfiguration::instance().get_default_upper_midi_note())
	, _data_note_min (127)
	, _data_note_max (0)
	, _note_lines (new ArdourCanvas::RectSet (parent))
	, _note_mode (ARDOUR::Sustained)
	, _color_mode (ARDOUR::MeterColors)
	, _visibility_note_range (ContentsRange)
	, note_range_set (false)
{
	CANVAS_DEBUG_NAME (_note_lines, "MVB note lines");
	_note_lines->lower_to_bottom();

	// color_handler ();

	UIConfiguration::instance().ColorsChanged.connect(sigc::mem_fun(*this, &MidiViewBackground::color_handler));
	UIConfiguration::instance().ParameterChanged.connect(sigc::mem_fun(*this, &MidiViewBackground::parameter_changed));

	note_range_adjustment.set_page_size(_highest_note - _lowest_note);
	note_range_adjustment.set_value(_lowest_note);
	note_range_adjustment.set_lower(0);
	note_range_adjustment.set_upper(127);

	note_range_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &MidiViewBackground::note_range_adjustment_changed));
}

MidiViewBackground::~MidiViewBackground()
{
}

void
MidiViewBackground::parameter_changed (std::string const & param)
{
	if (param == X_("max-note-height")) {
		apply_note_range (_lowest_note, _highest_note, true);
	}
}

void
MidiViewBackground::color_handler ()
{
	setup_note_lines ();
}

uint8_t
MidiViewBackground::y_to_note (int y) const
{
	int const n = highest_note() - (floor ((double) y / note_height()));

	if (n < 0) {
		return 0;
	} else if (n > 127) {
		return 127;
	}

	/* min due to rounding and/or off-by-one errors */
	return min ((uint8_t) n, highest_note());
}



void
MidiViewBackground::update_contents_height ()
{
	ViewBackground::update_contents_height ();

	setup_note_lines ();
	apply_note_range (lowest_note(), highest_note(), true);
}

void
MidiViewBackground::get_note_positions (std::vector<int>& numbers, std::vector<int>& pos, std::vector<int>& heights) const
{
	for (auto const & r : _note_lines->rects()) {
		numbers.push_back (r.index);
		pos.push_back (r.y0);
		heights.push_back (r.height());
	}
}

void
MidiViewBackground::setup_note_lines()
{
	if (updates_suspended()) {
		return;
	}

	Gtkmm2ext::Color black = UIConfiguration::instance().color_mod ("piano roll black", "piano roll black");
	Gtkmm2ext::Color white = UIConfiguration::instance().color_mod ("piano roll white", "piano roll white");
	Gtkmm2ext::Color divider = UIConfiguration::instance().color ("piano roll black outline");
	Gtkmm2ext::Color color;

	ArdourCanvas::RectSet::ResetRAII lr (*_note_lines);

	if (contents_height() < 10 || note_height() < 2) {
		/* context is too small for note lines, or there are too many */
		return;
	}

	/* do this is order of highest ... lowest since that matches the
	 * coordinate system in which y=0 is at the top
	 */

	int h = note_height();
	double y = 0;

	for (int i = highest_note(); i >= lowest_note(); --i) {

		if (i > 127) {
			continue;
		}

		/* add a thicker line/bar which covers the entire vertical height of this note. */

		switch (i % 12) {
		case 1:
		case 3:
		case 6:
		case 8:
		case 10:
			color = black;
			break;
		case 4:
		case 11:
			/* this is the line corresponding to the division between B & C and E & F */
			_note_lines->add_rect (i, ArdourCanvas::Rect (0., y, ArdourCanvas::COORD_MAX, y + 1.), divider);
			/* fallthrough */
		default:
			color = white;
			break;
		}

		/* There's no clipping region trivially available for the note
		 * lines, so make sure the last line doesn't draw "too tall"
		 */

		if (y + h > contents_height()) {
			h = contents_height() - y;
		}

		_note_lines->add_rect (i, ArdourCanvas::Rect (0., y, ArdourCanvas::COORD_MAX, y + h), color);

		y += h;

		if (y >= contents_height()) {
			break;
		}
	}
}

void
MidiViewBackground::set_note_visibility_range_style (VisibleNoteRange r)
{
	if (r == ContentsRange) {
		if (apply_note_range (_data_note_min, _data_note_max, true)) {
			_visibility_note_range = ContentsRange;
		}
	} else {
		if (apply_note_range (0, 127, true)) {
			_visibility_note_range = FullRange;
		}
	}
}

void
MidiViewBackground::maybe_extend_note_range (uint8_t note_num)
{

	bool changed = false;

	if (_visibility_note_range == FullRange) {
		return;
	}

	if (note_range_set) {

		if (_lowest_note > _data_note_min) {
			changed = true;
		}

		if (_highest_note < _data_note_max) {
			changed = true;
		}
	} else {
		changed = true;
	}

	if (changed) {
		apply_note_range (_data_note_min, _data_note_max, true);
	}
}

void
MidiViewBackground::maybe_apply_note_range (uint8_t lowest, uint8_t highest, bool to_children, RangeCanMove can_move)
{
	if (note_range_set && _lowest_note <= lowest && _highest_note >= highest) {
		/* already large enough */
		return;
	}

	apply_note_range (lowest, highest, to_children, can_move);
}

bool
MidiViewBackground::apply_note_range (uint8_t lowest, uint8_t highest, bool to_children, RangeCanMove can_move)
{
	if (contents_height() == 0) {
		return false;
	}

	bool changed = false;
	uint8_t ol = _lowest_note;
	uint8_t oh = _highest_note;

	/* Enforce a 1 octave minimum */

	if (highest - lowest < 12) {
		int8_t mid = lowest + ((highest - lowest) / 2);
		lowest = std::max (mid - 6, 0);
		highest = lowest + 12;
	}

	if (_highest_note != highest) {
		_highest_note = highest;
		changed = true;
	}

	if (_lowest_note != lowest) {
		changed = true;
		_lowest_note = lowest;
	}

	if (note_range_set && !changed) {
		return false;
	}

	float uiscale = UIConfiguration::instance().get_ui_scale();

	int range = _highest_note - _lowest_note;
	int apparent_note_height = (int) ceil ((double) contents_height() / range);
	int nh = std::min ((int) (UIConfiguration::instance().get_max_note_height() * uiscale), apparent_note_height);
	int additional_notes = 0;

	if (nh < 3) {
		/* range does not fit, so center on the data range */
		nh = 3;
		range = _data_note_max - _data_note_min;
		int center = _data_note_min + (range /2);
		highest = center + ((contents_height() / nh) / 2);
		lowest = center - ((contents_height() / nh) / 2);
	}

	if (note_range_set) {
		/* how many notes do we need to add or remove to adequately
		 * fill contents_height() with note lines?
		 */
		additional_notes = (int) ceil ((contents_height() - (nh * range)) / (double) nh);
	}

	/* distribute additional notes to higher and lower ranges, clamp at 0 and 127 */
	if (additional_notes > 0) {
		for (int i = 0; i < additional_notes; i++){

			if ((can_move & CanMoveTop) || (i % 2 && _highest_note < 127)) {
				_highest_note++;
			} else if ((can_move & CanMoveBottom) && (i % 2)) {
				_lowest_note--;
			} else if ((can_move & CanMoveBottom) && (_lowest_note > 0)) {
				_lowest_note--;
			} else if (can_move & CanMoveTop) {
				_highest_note++;
			}
		}
	} else if (additional_notes < 0) {
		for (int i = 0; i < -additional_notes; i++){

			if ((can_move & CanMoveTop) && (i % 2 && _highest_note < 127)) {
				_highest_note--;
			} else if ((can_move & CanMoveBottom) && (i % 2)) {
				_lowest_note++;
			} else if ((can_move & CanMoveBottom) && (_lowest_note > 0)) {
				_lowest_note++;
			} else if ((can_move & CanMoveTop)) {
				_highest_note--;
			}
		}
	}

	note_range_adjustment.set_page_size (_highest_note - _lowest_note);
	note_range_adjustment.set_value (_lowest_note);

	setup_note_lines();

	if (to_children) {
		apply_note_range_to_children ();
	}

	note_range_set = true;

	NoteRangeChanged(); /* EMIT SIGNAL*/

	return true;
}

void
MidiViewBackground::note_range_adjustment_changed()
{
	double sum = note_range_adjustment.get_value() + note_range_adjustment.get_page_size();
	int lowest = (int) floor(note_range_adjustment.get_value());
	int highest;

	if (sum == _range_sum_cache) {
		//cerr << "cached" << endl;
		highest = (int) floor(sum);
	} else {
		//cerr << "recalc" << endl;
		highest = lowest + (int) floor(note_range_adjustment.get_page_size());
		_range_sum_cache = sum;
	}

	if (lowest == _lowest_note && highest == _highest_note) {
		return;
	}

	// cerr << "  val=" << v_zoom_adjustment.get_value() << " page=" << v_zoom_adjustment.get_page_size() << " sum=" << v_zoom_adjustment.get_value() + v_zoom_adjustment.get_page_size() << endl;

	apply_note_range (lowest, highest, true);
}

bool
MidiViewBackground::update_data_note_range (uint8_t min, uint8_t max)
{
	bool dirty = false;

	if (min < _data_note_min) {
		_data_note_min = min;
		dirty = true;
	}
	if (max > _data_note_max) {
		_data_note_max = max;
		dirty = true;
	}
	return dirty;
}

void
MidiViewBackground::set_note_mode (ARDOUR::NoteMode nm)
{
	if (_note_mode != nm) {
		_note_mode = nm;
		NoteModeChanged(); /* EMIT SIGNAL */
	}
}
