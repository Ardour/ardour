/*
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
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

	const uint32_t major_color = UIConfiguration::instance().color_mod("grid line major", "grid line");
	const uint32_t minor_color = UIConfiguration::instance().color_mod("grid line minor", "grid line");
	const uint32_t micro_color = UIConfiguration::instance().color_mod("grid line micro", "grid line");

	for (vector<Ruler::Mark>::const_iterator m = marks.begin(); m != marks.end(); ++m) {

		samplepos_t s = m->position;

		if ((*m).style == ArdourCanvas::Ruler::Mark::Major) {
			lines.add_coord (PublicEditor::instance().sample_to_pixel_unrounded (s), 1.0, major_color);
		} else if ((*m).style == ArdourCanvas::Ruler::Mark::Minor) {
			lines.add_coord (PublicEditor::instance().sample_to_pixel_unrounded (s), 1.0, minor_color);
		} else {
			lines.add_coord (PublicEditor::instance().sample_to_pixel_unrounded (s), 1.0, micro_color);
		}
	}
}
