/*
    Copyright (C) 2000-2018 Paul Davis

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

#ifndef __ardour_grid_lines_h__
#define __ardour_grid_lines_h__

#include "canvas/line_set.h"
#include "canvas/ruler.h"
#include "ardour/tempo.h"

class GridLines {
public:
	GridLines (ArdourCanvas::Container* group, double screen_height);
	~GridLines ();

	void draw (std::vector<ArdourCanvas::Ruler::Mark> marks);

	void show();
	void hide();

private:

	ArdourCanvas::LineSet lines;
};

#endif /* __ardour_grid_lines_h__ */
