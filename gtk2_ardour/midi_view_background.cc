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

#include "canvas/line_set.h"

#include "midi_view_background.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace std;

MidiViewBackground::MidiViewBackground (ArdourCanvas::Item* parent)
	: note_range_adjustment (0.0f, 0.0f, 0.0f)
	, _range_dirty (false)
	, _range_sum_cache (-1.0)
	, _lowest_note (UIConfiguration::instance().get_default_lower_midi_note())
	, _highest_note (UIConfiguration::instance().get_default_upper_midi_note())
	, _data_note_min (60)
	, _data_note_max (71)
	, _note_lines (new ArdourCanvas::LineSet (parent, ArdourCanvas::LineSet::Horizontal))
	, _note_mode (ARDOUR::Sustained)
	, _color_mode (ARDOUR::MeterColors)
{
	_note_lines->lower_to_bottom();

	// color_handler ();

	UIConfiguration::instance().ColorsChanged.connect(sigc::mem_fun(*this, &MidiViewBackground::color_handler));
	UIConfiguration::instance().ParameterChanged.connect(sigc::mem_fun(*this, &MidiViewBackground::parameter_changed));

	note_range_adjustment.set_page_size(_highest_note - _lowest_note);
	note_range_adjustment.set_value(_lowest_note);

	note_range_adjustment.signal_value_changed().connect(sigc::mem_fun(*this, &MidiViewBackground::note_range_adjustment_changed));
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
	draw_note_lines ();
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

	//cerr << "note range adjustment changed: " << lowest << " " << highest << endl;
	//cerr << "  val=" << v_zoom_adjustment.get_value() << " page=" << v_zoom_adjustment.get_page_size() << " sum=" << v_zoom_adjustment.get_value() + v_zoom_adjustment.get_page_size() << endl;

	_lowest_note = lowest;
	_highest_note = highest;
	apply_note_range (lowest, highest, true);
}

uint8_t
MidiViewBackground::y_to_note (double y) const
{
	int const n = ((contents_height() - y) / contents_height() * (double)contents_note_range())
		+ lowest_note();

	if (n < 0) {
		return 0;
	} else if (n > 127) {
		return 127;
	}

	/* min due to rounding and/or off-by-one errors */
	return min ((uint8_t) n, highest_note());
}


void
MidiViewBackground::update_note_range(uint8_t note_num)
{
	_data_note_min = min(_data_note_min, note_num);
	_data_note_max = max(_data_note_max, note_num);
}

void
MidiViewBackground::update_contents_height ()
{
	ViewBackground::update_contents_height ();

	_note_lines->set_extent (ArdourCanvas::COORD_MAX);
	apply_note_range (lowest_note(), highest_note(), true);
}

void
MidiViewBackground::draw_note_lines()
{
	if (updates_suspended()) {
		return;
	}

	double y;
	double prev_y = 0.;
	Gtkmm2ext::Color black = UIConfiguration::instance().color_mod ("piano roll black", "piano roll black");
	Gtkmm2ext::Color white = UIConfiguration::instance().color_mod ("piano roll white", "piano roll white");
	Gtkmm2ext::Color outline = UIConfiguration::instance().color ("piano roll black outline");
	Gtkmm2ext::Color color;

	ArdourCanvas::LineSet::ResetRAII lr (*_note_lines);

	if (contents_height() < 140 || note_height() < 3) {
		/* context is too small for note lines, or there are too many */
		return;
	}

	/* do this is order of highest ... lowest since that matches the
	 * coordinate system in which y=0 is at the top
	 */

	for (int i = highest_note() + 1; i >= lowest_note(); --i) {

		y = floor (note_to_y (i));

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
			/* this is the line actually corresponding to the division between B & C and E & F */
			_note_lines->add_coord (y, 1.0, outline);
			/* fallthrough */
		default:
			color = white;
			break;
		}

		double h = y - prev_y;
		double middle = y + (h/2.0);

		if (!fmod (h, 2.) && !fmod (middle, 1.)) {
			middle += 0.5;
		}

		if (middle >= 0 && h > 1.0) {
			_note_lines->add_coord (middle, h, color);
		}

		prev_y = y;
	}
}

void
MidiViewBackground::set_note_range(VisibleNoteRange r)
{
	if (r == FullRange) {
		_lowest_note = 0;
		_highest_note = 127;
	} else {
		_lowest_note = _data_note_min;
		_highest_note = _data_note_max;
	}

	apply_note_range(_lowest_note, _highest_note, true);
}

void
MidiViewBackground::apply_note_range(uint8_t lowest, uint8_t highest, bool to_children)
{
	_highest_note = highest;
	_lowest_note = lowest;

	float uiscale = UIConfiguration::instance().get_ui_scale();
	uiscale = expf (uiscale) / expf (1.f);

	const int mnh = UIConfiguration::instance().get_max_note_height();
	int const max_note_height = std::max<int> (mnh, mnh * uiscale);
	int const range = _highest_note - _lowest_note;

	int const available_note_range = floor (contents_height() / max_note_height);
	int additional_notes = available_note_range - range;

	/* distribute additional notes to higher and lower ranges, clamp at 0 and 127 */
	for (int i = 0; i < additional_notes; i++){

		if (i % 2 && _highest_note < 127){
			_highest_note++;
		}
		else if (i % 2) {
			_lowest_note--;
		}
		else if (_lowest_note > 0){
			_lowest_note--;
		}
		else {
			_highest_note++;
		}
	}

	note_range_adjustment.set_page_size (_highest_note - _lowest_note);
	note_range_adjustment.set_value (_lowest_note);

	draw_note_lines();

	if (to_children) {
		apply_note_range_to_children ();
	}

	NoteRangeChanged(); /* EMIT SIGNAL*/
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
