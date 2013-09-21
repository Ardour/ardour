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

#include <list>
#include "ardour/tempo.h"

class TempoLines {
public:
	TempoLines(ArdourCanvas::Canvas& canvas, ArdourCanvas::Group* group, double screen_height);

	void tempo_map_changed();

	void draw (const ARDOUR::TempoMap::BBTPointList::const_iterator& begin, 
		   const ARDOUR::TempoMap::BBTPointList::const_iterator& end, 
		   double frames_per_unit);

	void show();
	void hide();

private:
        typedef std::list<ArdourCanvas::Line*> Lines;
	Lines _lines;
        Lines _cache;

        ArdourCanvas::Canvas& _canvas;
	ArdourCanvas::Group*  _group;
	double                _height;
};

#endif /* __ardour_tempo_lines_h__ */
