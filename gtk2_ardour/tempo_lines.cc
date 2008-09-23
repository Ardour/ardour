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

#include <libgnomecanvasmm/canvas.h>
#include <libgnomecanvasmm/group.h>
#include "tempo_lines.h"
#include "ardour_ui.h"

using namespace std;

#define MAX_CACHED_LINES 512
	
TempoLines::TempoLines(ArdourCanvas::Canvas& canvas, ArdourCanvas::Group* group)
	: _canvas(canvas)
	, _group(group)
	, _clean_left(DBL_MAX)
	, _clean_right(0.0)
{
}

void
TempoLines::tempo_map_changed()
{
	_clean_left = DBL_MAX;
	_clean_right = 0.0;

	// TODO: Dirty/slow, but 'needed' for zoom :(
	for (Lines::iterator i = _lines.begin(); i != _lines.end(); ) {
		Lines::iterator next = i;
		++next;
      	i->second->property_x1() = DBL_MAX;
      	i->second->property_x2() = DBL_MAX;
		_lines.erase(i);
		_lines.insert(make_pair(DBL_MAX, i->second));
		i = next;
	}
}

void
TempoLines::show ()
{
	for (Lines::iterator i = _lines.begin(); i != _lines.end(); ++i) {
      	i->second->show();
	}
}

void
TempoLines::hide ()
{
	for (Lines::iterator i = _lines.begin(); i != _lines.end(); ++i) {
      	i->second->hide();
	}
}

void
TempoLines::draw (ARDOUR::TempoMap::BBTPointList& points, double frames_per_unit)
{
	ARDOUR::TempoMap::BBTPointList::iterator i;
	ArdourCanvas::SimpleLine *line;
	gdouble xpos;
	double who_cares;
	double x1, x2, y1, y2, beat_density;

	uint32_t beats = 0;
	uint32_t bars = 0;
	uint32_t color;
	
	const size_t needed = points.size();

	_canvas.get_scroll_region (x1, y1, x2, who_cares);
	_canvas.root()->get_bounds(who_cares, who_cares, who_cares, y2);

	/* get the first bar spacing */

	i = points.end();
	i--;
	bars = (*i).bar - (*points.begin()).bar;
	beats = points.size() - bars;

	beat_density = (beats * 10.0f) / _canvas.get_width ();

	if (beat_density > 4.0f) {
		/* if the lines are too close together, they become useless */
		return;
	}

	xpos = rint(((nframes64_t)(*i).frame) / (double)frames_per_unit);
	const double needed_right = xpos;

	i = points.begin();
	
	xpos = rint(((nframes64_t)(*i).frame) / (double)frames_per_unit);
	const double needed_left = xpos;

	Lines::iterator left = _lines.lower_bound(xpos); // first line >= xpos

	bool exhausted = (left == _lines.end());
	Lines::iterator li = left;
	
	// Tempo map hasn't changed and we're entirely within a clean
	// range, don't need to do anything.  Yay.
	if (needed_left >= _clean_left && needed_right <= _clean_right) {
		//cout << "LINE CACHE PERFECT HIT!" << endl;
		return;
	}

	//cout << "LINE CACHE MISS :/" << endl;

	bool inserted_last_time = false;
	bool invalidated = false;
	
	for (i = points.begin(); i != points.end(); ++i) {

		switch ((*i).type) {
		case ARDOUR::TempoMap::Bar:
			break;

		case ARDOUR::TempoMap::Beat:
			
			if ((*i).beat == 1) {
				color = ARDOUR_UI::config()->canvasvar_MeasureLineBar.get();
			} else {
				color = ARDOUR_UI::config()->canvasvar_MeasureLineBeat.get();

				if (beat_density > 2.0) {
					/* only draw beat lines if the gaps between beats are large. */
					break;
				}
			}
			
			xpos = rint(((nframes64_t)(*i).frame) / (double)frames_per_unit);

			if (inserted_last_time) {
				li = _lines.lower_bound(xpos); // first line >= xpos
			}
			
			if (!exhausted) {
				line = li->second;
				exhausted = ((++li) == _lines.end());
				inserted_last_time = false;
			} else if (_lines.size() < needed || _lines.size() < MAX_CACHED_LINES) {
				line = new ArdourCanvas::SimpleLine (*_group);
				line->property_x1() = xpos;
				line->property_x2() = xpos;
				line->property_y2() = y2;
				line->property_color_rgba() = color;
				_lines.insert(make_pair(xpos, line));
				inserted_last_time = true;
			} else {
				assert(li != _lines.begin());
				line = _lines.begin()->second; // steal leftmost line
				_lines.erase(_lines.begin());
				_lines.insert(make_pair(xpos, line));
				inserted_last_time = true;
				invalidated = true;
			}

			/* At this point, line's key is correct, but actual pos may not be */
			if (line->property_x1() != xpos) {
				// Turn this on to see the case where this isn't quite ideal yet
				// (scrolling left, lots of lines are moved left when there is
				// likely to be a huge chunk with equivalent coords)
				//cout << "MOVE " << line->property_x1() << " -> " << xpos << endl;
				double x1 = line->property_x1();
				bool was_clean = x1 >= _clean_left && x1 <= _clean_right;
				invalidated = invalidated || was_clean;
				line->property_x1() = xpos;
				line->property_x2() = xpos;
				line->property_y2() = y2;
				line->property_color_rgba() = color;
			}

			break;
		}
	}
	
	if (invalidated) { // We messed things up, visible range is all we know is valid
		_clean_left  = needed_left;
		_clean_right = needed_right;
	} else { // Extend range to what we've 'fixed'
		_clean_left  = min(_clean_left, needed_left);
		_clean_right = max(_clean_right, needed_right);
	}
}

