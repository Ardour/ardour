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

#include "canvas/line.h"
#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "tempo_lines.h"
#include "ardour_ui.h"

using namespace std;

#define MAX_CACHED_LINES 128

TempoLines::TempoLines (ArdourCanvas::GtkCanvasViewport& canvas_viewport, ArdourCanvas::Group* group, double screen_height)
	: _canvas_viewport (canvas_viewport)
	, _group(group)
	, _clean_left(DBL_MAX)
	, _clean_right(0.0)
	, _height(screen_height)
{
}

void
TempoLines::tempo_map_changed()
{
	_clean_left = DBL_MAX;
	_clean_right = 0.0;

	double_t d = 1.0;
	// TODO: Dirty/slow, but 'needed' for zoom :(
	for (Lines::iterator i = _lines.begin(); i != _lines.end(); d += 1.0) {
		Lines::iterator next = i;
		++next;
		i->second->set_x0 (-d);
		i->second->set_x1 (-d);
		ArdourCanvas::Line* f = i->second;
		_lines.erase(i);
		_lines.insert(make_pair(- d, f));
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
TempoLines::draw (const ARDOUR::TempoMap::BBTPointList::const_iterator& begin, 
		  const ARDOUR::TempoMap::BBTPointList::const_iterator& end, 
		  double samples_per_pixel)
{
	ARDOUR::TempoMap::BBTPointList::const_iterator i;
	ArdourCanvas::Line *line = 0;
	gdouble xpos;
	double  beat_density;

	uint32_t beats = 0;
	uint32_t bars = 0;
	uint32_t color;

	const size_t needed = distance (begin, end);

	ArdourCanvas::Rect const visible = _canvas_viewport.visible_area ();

	/* get the first bar spacing */

	i = end;
	i--;
	bars = (*i).bar - (*begin).bar;
	beats = distance (begin, end) - bars;

	beat_density = (beats * 10.0f) / visible.width ();

	if (beat_density > 4.0f) {
		/* if the lines are too close together, they become useless */
		tempo_map_changed();
		return;
	}

	xpos = rint(((framepos_t)(*i).frame) / (double)samples_per_pixel);
	const double needed_right = xpos;

	i = begin;

	xpos = rint(((framepos_t)(*i).frame) / (double)samples_per_pixel);
	const double needed_left = xpos;

	Lines::iterator left = _lines.lower_bound(xpos); // first line >= xpos

	bool exhausted = (left == _lines.end());
	Lines::iterator li = left;
	if (li != _lines.end())
		line = li->second;

	// Tempo map hasn't changed and we're entirely within a clean
	// range, don't need to do anything.  Yay.
	if (needed_left >= _clean_left && needed_right <= _clean_right) {
		// cout << endl << "*** LINE CACHE PERFECT HIT" << endl;
		return;
	}

	//cout << endl << "*** LINE CACHE MISS" << endl;

	bool invalidated = false;

	for (i = begin; i != end; ++i) {

		if ((*i).is_bar()) {
			color = ARDOUR_UI::config()->canvasvar_MeasureLineBar.get();
		} else {
			if (beat_density > 2.0) {
				continue; /* only draw beat lines if the gaps between beats are large. */
			}
			color = ARDOUR_UI::config()->canvasvar_MeasureLineBeat.get();
		}

		xpos = rint(((framepos_t)(*i).frame) / (double)samples_per_pixel);

		li = _lines.lower_bound(xpos); // first line >= xpos

		line = (li != _lines.end()) ? li->second : 0;
		assert(!line || line->x0() == li->first);
		
		Lines::iterator next = li;
		if (next != _lines.end())
			++next;
		
		exhausted = (next == _lines.end());

		// Hooray, line is perfect
		if (line && line->x0() == xpos) {
			if (li != _lines.end())
				++li;
			
			line->set_outline_color (color);

			// Use existing line, moving if necessary
		} else if (!exhausted) {
			Lines::iterator steal = _lines.end();
			--steal;
			
			// Steal from the right
			if (left->first > needed_left && li != steal && steal->first > needed_right) {
				//cout << "*** STEALING FROM RIGHT" << endl;
				double const x = steal->first;
				line = steal->second;
				_lines.erase(steal);
				line->set_x0 (xpos);
				line->set_x1 (xpos);
				line->set_outline_color (color);
				_lines.insert(make_pair(xpos, line));
				invalidated = true;
				
				// Shift clean range left
				_clean_left = min(_clean_left, xpos);
				_clean_right = min(_clean_right, x);
				
				// Move this line to where we need it
			} else {
				Lines::iterator existing = _lines.find(xpos);
				if (existing != _lines.end()) {
					//cout << "*** EXISTING LINE" << endl;
					li = existing;
					li->second->set_outline_color (color);
				} else {
					//cout << "*** MOVING LINE" << endl;
					const double x1 = line->x0();
					const bool was_clean = x1 >= _clean_left && x1 <= _clean_right;
					invalidated = invalidated || was_clean;
					// Invalidate clean portion (XXX: too harsh?)
					_clean_left  = needed_left;
					_clean_right = needed_right;
					_lines.erase(li);
					line->set_outline_color (color);
					line->set_x0 (xpos);
					line->set_x1 (xpos);
					_lines.insert(make_pair(xpos, line));
				}
			}
			
			// Create a new line
		} else if (_lines.size() < needed || _lines.size() < MAX_CACHED_LINES) {
			//cout << "*** CREATING LINE" << endl;
			/* if we already have a line there ... don't sweat it */
			if (_lines.find (xpos) == _lines.end()) {
				line = new ArdourCanvas::Line (_group);
				line->set_x0 (xpos);
				line->set_x1 (xpos);
				line->set_y0 (0.0);
				line->set_y1 (_height);
				line->set_outline_color (color);
				_lines.insert(make_pair(xpos, line));
			}
			
			// Steal from the left
		} else {
			//cout << "*** STEALING FROM LEFT" << endl;
			if (_lines.find (xpos) == _lines.end()) {
				Lines::iterator steal = _lines.begin();
				double const x = steal->first;
				line = steal->second;
				_lines.erase(steal);
				line->set_outline_color (color);
				line->set_x0 (xpos);
				line->set_x1 (xpos);
				_lines.insert(make_pair(xpos, line));
				invalidated = true;
			
				// Shift clean range right
				_clean_left = max(_clean_left, x);
				_clean_right = max(_clean_right, xpos);
			}
		}
	}

	// Extend range to what we've 'fixed'
	if (!invalidated) {
		_clean_left  = min(_clean_left, needed_left);
		_clean_right = max(_clean_right, needed_right);
	}
}

