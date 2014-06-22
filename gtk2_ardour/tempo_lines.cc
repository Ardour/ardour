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
#include "ardour_ui.h"
#include "public_editor.h"

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
TempoLines::draw (const ARDOUR::TempoMap::BBTPointList::const_iterator& begin, 
		  const ARDOUR::TempoMap::BBTPointList::const_iterator& end)
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

	for (i = begin; i != end; ++i) {

		if ((*i).is_bar()) {
			color = ARDOUR_UI::config()->get_canvasvar_MeasureLineBar();
		} else {
			if (beat_density > 0.3) {
				continue; /* only draw beat lines if the gaps between beats are large. */
			}
			color = ARDOUR_UI::config()->get_canvasvar_MeasureLineBeat();
		}

		ArdourCanvas::Coord xpos = PublicEditor::instance().sample_to_pixel_unrounded ((*i).frame);

		lines.add (xpos, 1.0, color);
	}
}

