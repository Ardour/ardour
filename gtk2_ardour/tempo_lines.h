/*
    Copyright (C) 2000-2007 Paul Davis

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

#ifndef __ardour_tempo_lines_h__
#define __ardour_tempo_lines_h__

#include "ardour/tempo.h"

#include "canvas/line_set.h"

class TempoLines {
public:
	TempoLines (ArdourCanvas::Container* group, double screen_height);

	void tempo_map_changed();

	void draw (const ARDOUR::TempoMap::BBTPointList::const_iterator& begin, 
	           const ARDOUR::TempoMap::BBTPointList::const_iterator& end,
	           unsigned                                              divisions,
	           ARDOUR::framecnt_t                                    leftmost_frame,
	           ARDOUR::framecnt_t                                    frame_rate);

	void show();
	void hide();

private:
	void draw_ticks (const ARDOUR::TempoMap::BBTPointList::const_iterator& b,
	                 unsigned                                              divisions,
	                 ARDOUR::framecnt_t                                    leftmost_frame,
	                 ARDOUR::framecnt_t                                    frame_rate);

	ArdourCanvas::LineSet lines;
};

#endif /* __ardour_tempo_lines_h__ */
