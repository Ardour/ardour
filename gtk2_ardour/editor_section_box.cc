/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2023 Ben Loftis <ben@harrisonaudio.com>
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

#include <cmath>

#include "editor.h"
#include "editor_section_box.h"

using namespace ARDOUR;

SectionBox::SectionBox (Editor& ed, ArdourCanvas::Item* parent)
	: ArdourCanvas::Rectangle (parent)
	, _editor (ed)
{
	set (ArdourCanvas::Rect (0, 0, 0, ArdourCanvas::COORD_MAX));
	set_ignore_events (false);

	set_outline_what (ArdourCanvas::Rectangle::What (Rectangle::LEFT | Rectangle::RIGHT));
	set_outline (true);
	set_fill (true);

	hide ();
}

void
SectionBox::set_position (samplepos_t sample_start, samplepos_t sample_end)
{
	double const new_start = _editor.sample_to_pixel_unrounded (sample_start);
	double const new_end   = _editor.sample_to_pixel_unrounded (sample_end);

	if (rint (new_start) != rint (x0())) {
		set_x0 (new_start + 0.5); // accommodate the 1/2 pixel "line" offset in cairo
	}

	if (rint (new_end) != rint (x1())) {
		set_x1 (new_end + 0.5); // accommodate the 1/2 pixel "line" offset in cairo
	}
}
