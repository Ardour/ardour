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

#include <vector>
#include <ardour/tempo.h>
#include "canvas.h"
#include "simpleline.h"

class TempoLines {
public:
	TempoLines(ArdourCanvas::Canvas& canvas, ArdourCanvas::Group* group)
		: _canvas(canvas)
		, _group(group)
	{}
 
	ArdourCanvas::SimpleLine* get_line();

	void draw(ARDOUR::TempoMap::BBTPointList& points, double frames_per_unit);
	void hide();

private:
	typedef std::vector<ArdourCanvas::SimpleLine*> Lines;
	Lines _free_lines;
	Lines _used_lines;

	ArdourCanvas::Canvas& _canvas;
	ArdourCanvas::Group*  _group;
};

#endif /* __ardour_tempo_lines_h__ */
