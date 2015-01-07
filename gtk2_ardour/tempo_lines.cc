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

#include "ardour_ui.h"
#include "public_editor.h"
#include "rgb_macros.h"
#include "tempo_lines.h"

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

static uint8_t
tick_alpha(uint8_t base_alpha, unsigned divisions)
{
	if (divisions == 32) {
		return std::max(0, base_alpha / 5);
	} else if (divisions == 16) {
		return std::max(0, base_alpha / 4);
	} else if (divisions == 8) {
		return std::max(0, base_alpha / 3);
	} else if (divisions == 4) {
		return std::max(0, base_alpha / 2);
	}
	return 0;
}

void
TempoLines::draw_ticks (const ARDOUR::TempoMap::BBTPointList::const_iterator& b,
                        double                                                beat_density,
                        framecnt_t                                            leftmost_frame,
                        framecnt_t                                            frame_rate)
{
	const double   fpb       = b->tempo->frames_per_beat(frame_rate);
	const uint32_t base      = ARDOUR_UI::config()->color_mod("measure line beat", "measure line beat");
	unsigned       divisions = 4;
	if (beat_density < 0.01) {
		divisions = 32;
	} else if (beat_density < 0.025) {
		divisions = 16;
	} else if (beat_density < 0.05) {
		divisions = 8;
	} else if (beat_density < 0.1) {
		divisions = 4;
	}

	for (unsigned l = 1; l < divisions; ++l) {
		/* find the coarsest division level this tick falls on */
		unsigned level = divisions;
		for (unsigned d = divisions; d >= 4; d /= 2) {
			if (l % (divisions / d) == 0) {
				level = d;
			}
		}

		/* draw line with alpha corresponding to coarsest level */
		const uint32_t   c = UINT_RGBA_CHANGE_A(base, tick_alpha(UINT_RGBA_A(base), level));
		const framepos_t f = b->frame + (l * (fpb / (double)divisions));
		if (f > leftmost_frame) {
			lines.add (PublicEditor::instance().sample_to_pixel_unrounded (f), 1.0, c);
		}
	}
}

void
TempoLines::draw (const ARDOUR::TempoMap::BBTPointList::const_iterator& begin,
                  const ARDOUR::TempoMap::BBTPointList::const_iterator& end,
                  framecnt_t                                            leftmost_frame,
                  framecnt_t                                            frame_rate)
{
	ARDOUR::TempoMap::BBTPointList::const_iterator i;
	double  beat_density;

	uint32_t beats = 0;
	uint32_t bars = 0;
	uint32_t color;

	/* get the first bar spacing */

	i = end;
	i--;
	bars = (*i).bar - (*begin).bar; 
	beats = distance (begin, end) - bars;

	beat_density = (beats * 10.0f) / lines.canvas()->width();

	if (beat_density > 4.0f) {
		/* if the lines are too close together, they become useless */
		lines.clear ();
		return;
	}

	lines.clear ();

	if (beat_density < 0.1 && begin != end && begin->frame > 0) {
		/* draw subdivisions of the beat before the first visible beat line */
		ARDOUR::TempoMap::BBTPointList::const_iterator prev = begin;
		--prev;
		draw_ticks(prev, beat_density, leftmost_frame, frame_rate);
	}

	for (i = begin; i != end; ++i) {

		if ((*i).is_bar()) {
			color = ARDOUR_UI::config()->color ("measure line bar");
		} else {
			if (beat_density > 0.3) {
				continue; /* only draw beat lines if the gaps between beats are large. */
			}
			color = ARDOUR_UI::config()->color_mod ("measure line beat", "measure line beat");
		}

		ArdourCanvas::Coord xpos = PublicEditor::instance().sample_to_pixel_unrounded ((*i).frame);

		lines.add (xpos, 1.0, color);

		if (beat_density < 0.1) {
			/* draw subdivisions of this beat */
			draw_ticks(i, beat_density, leftmost_frame, frame_rate);
		}
	}
}

