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


ArdourCanvas::SimpleLine *
TempoLines::get_line ()
{
	ArdourCanvas::SimpleLine *line;

	if (_free_lines.empty()) {
		line = new ArdourCanvas::SimpleLine (*_group);
		_used_lines.push_back (line);
	} else {
		line = _free_lines.front();
		_free_lines.erase (_free_lines.begin());
		_used_lines.push_back (line);
	}

	return line;
}


void
TempoLines::hide ()
{
	for (Lines::iterator i = _used_lines.begin(); i != _used_lines.end(); ++i) {
      	(*i)->hide();
		_free_lines.push_back (*i);
	}
	_used_lines.clear ();
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

	_canvas.get_scroll_region (x1, y1, x2, who_cares);
	_canvas.root()->get_bounds(who_cares, who_cares, who_cares, y2);

	// FIXME use canvas height
	//y2 = TimeAxisView::hLargest*5000; // five thousand largest tracks should be enough.. :)
	//y2 = 500000; // five thousand largest tracks should be enough.. :)

	/* get the first bar spacing */

	i = points.end();
	i--;
	bars = (*i).bar - (*points.begin()).bar;
	beats = points.size() - bars;

	beat_density =  (beats * 10.0f) / _canvas.get_width ();

	if (beat_density > 4.0f) {
		/* if the lines are too close together, they become useless
		 */
		return;
	}
	
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
					/* only draw beat lines if the gaps between beats are large.
					*/
					break;
				}
			}

			xpos = rint((*i).frame / (double)frames_per_unit);
			line = get_line ();
			line->property_x1() = xpos;
			line->property_x2() = xpos;
			line->property_y2() = y2;
			line->property_color_rgba() = color;
			//line->raise_to_top();
			line->show();	
			break;
		}
	}
}

