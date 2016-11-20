/*
    Copyright (C) 2002-2007 Paul Davis

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

#include "pbd/compose.h"

#include "canvas/canvas.h"
#include "canvas/debug.h"

#include "tempo_lines.h"
#include "public_editor.h"
#include "rgb_macros.h"
#include "ui_config.h"

using namespace std;

TempoLines::TempoLines (ArdourCanvas::Container* group, double)
	: lines (group, ArdourCanvas::LineSet::Vertical)
{
	lines.set_extent (ArdourCanvas::COORD_MAX);
}

void
TempoLines::tempo_map_changed()
{
	lines.clear ();
}

void
TempoLines::show ()
{
	lines.show ();
}

void
TempoLines::hide ()
{
	lines.hide ();
}

void
TempoLines::draw_ticks (std::vector<ARDOUR::TempoMap::BBTPoint>& grid,
			unsigned                                              divisions,
                        framecnt_t                                            leftmost_frame,
                        framecnt_t                                            frame_rate)
{
	const uint32_t base = UIConfiguration::instance().color_mod("measure line beat", "measure line beat");

	for (unsigned l = 1; l < divisions; ++l) {
		/* find the coarsest division level this tick falls on */
		unsigned level = divisions;
		for (unsigned d = divisions; d >= 4; d /= 2) {
			if (l % (divisions / d) == 0) {
				level = d;
			}
		}
		/* draw line with alpha corresponding to coarsest level */
		const uint8_t    a = max(8, (int)rint(UINT_RGBA_A(base) / (0.8 * log2(level))));
		const uint32_t   c = UINT_RGBA_CHANGE_A(base, a);
		framepos_t f = 0;

		if (grid.begin()->c != 0.0) {
			const double beat_divisions = (l / ((double) divisions)) * (grid.begin()->tempo.note_type() / grid.begin()->meter.note_divisor());
			const double time_at_division = log (((grid.begin()->c * (beat_divisions)) /
							   grid.begin()->tempo.note_types_per_minute()) + 1) / grid.begin()->c;

			f = grid.begin()->frame + (framecnt_t) floor ((time_at_division * 60.0 * frame_rate) + 0.5);
		} else {
			const double fpb  = grid.begin()->tempo.frames_per_note_type (frame_rate)
				* (grid.begin()->tempo.note_type() / grid.begin()->meter.note_divisor());

			f = grid.begin()->frame + (l * (fpb / (double) divisions));
		}
		if (f > leftmost_frame) {
			lines.add (PublicEditor::instance().sample_to_pixel_unrounded (f), 1.0, c);
		}
	}
}

void
TempoLines::draw (std::vector<ARDOUR::TempoMap::BBTPoint>& grid,
		  unsigned                                              divisions,
                  framecnt_t                                            leftmost_frame,
                  framecnt_t                                            frame_rate)
{
	std::vector<ARDOUR::TempoMap::BBTPoint>::const_iterator i;
	double  beat_density;

	uint32_t beats = 0;
	uint32_t bars = 0;
	const uint32_t bar_color = UIConfiguration::instance().color ("measure line bar");
	const uint32_t beat_color = UIConfiguration::instance().color_mod ("measure line beat", "measure line beat");
	uint32_t color;

	bool all_bars = false;
	/* get the first bar spacing */

	i = grid.end();
	i--;
	bars = (*i).bar - (*grid.begin()).bar;

	int32_t bar_mod = 4;

	if (bars < distance (grid.begin(), grid.end()) - 1) {
		/* grid contains beats and bars */
		beats = distance (grid.begin(), grid.end()) - bars;
	} else {
		/* grid contains only bars */
		beats = distance (grid.begin(), grid.end());

		if (i != grid.begin()) {
			const int32_t last_bar = (*i).bar;
			i--;
			bar_mod = (last_bar - (*i).bar) * 4;
		}

		all_bars = true;
	}

	double canvas_width_used = 1.0;
	if (leftmost_frame < grid.front().frame) {
		const framecnt_t frame_distance = max ((framecnt_t) 1, grid.back().frame - grid.front().frame);
		canvas_width_used = 1.0 - ((grid.front().frame - leftmost_frame) / (double) (frame_distance + grid.front().frame));
	}

	beat_density = (beats * 10.0f) / (lines.canvas()->width() * canvas_width_used);

	if (beat_density > 2.0f) {
		/* if the lines are too close together, they become useless */
		lines.clear ();
		return;
	}

	/* constrain divisions to a log2 factor to cap line density */
	while (divisions > 3 && beat_density * divisions > 0.4) {
		divisions /= 2;
	}

	lines.clear ();
	if (beat_density <= 0.12 && grid.begin() != grid.end() && grid.begin()->frame > 0 && !all_bars) {
		/* draw subdivisions of the beat before the first visible beat line XX this shouldn't happen now */
		std::vector<ARDOUR::TempoMap::BBTPoint> vec;
		vec.push_back (*i);
		draw_ticks (vec, divisions, leftmost_frame, frame_rate);
	}

	for (i = grid.begin(); i != grid.end(); ++i) {

		if ((*i).is_bar()) {
			/* keep all_bar beat density down */
			if (all_bars && beat_density > 0.3 && ((*i).bar % bar_mod) != 1) {
				continue;
			}

			color = bar_color;
		} else {
			if (beat_density > 0.3) {
				continue; /* only draw beat lines if the gaps between beats are large. */
			}
			color = beat_color;
		}

		ArdourCanvas::Coord xpos = PublicEditor::instance().sample_to_pixel_unrounded ((*i).frame);

		lines.add (xpos, 1.0, color);

		if (beat_density <= 0.12 && !all_bars) {
			/* draw subdivisions of this beat */
			std::vector<ARDOUR::TempoMap::BBTPoint> vec;
			vec.push_back (*i);
			draw_ticks (vec, divisions, leftmost_frame, frame_rate);
		}
	}
}

