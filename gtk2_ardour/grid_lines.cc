/*
    Copyright (C) 2002-2018 Paul Davis

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
#include "canvas/ruler.h"

#include "grid_lines.h"
#include "public_editor.h"
#include "rgb_macros.h"
#include "ui_config.h"

using namespace std;
using namespace ArdourCanvas;

GridLines::GridLines (Container* group, double)
	: lines (group, LineSet::Vertical)
{
	lines.set_extent (COORD_MAX);
}

GridLines::~GridLines ()
{
}

void
GridLines::show ()
{
	lines.show ();
}

void
GridLines::hide ()
{
	lines.hide ();
}

void
GridLines::draw (std::vector<Ruler::Mark>     marks)
{
	lines.clear();
	
	const uint32_t c = UIConfiguration::instance().color_mod("measure line beat", "measure line beat");

	for (vector<Ruler::Mark>::const_iterator m = marks.begin(); m != marks.end(); ++m) {

		samplepos_t s = m->position;
		lines.add (PublicEditor::instance().sample_to_pixel_unrounded (s), 1.0, c);

	}
}

