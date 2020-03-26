/*
 * Copyright (C) 2018 Paul Davis <paul@linuxaudiosystems.com>
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

#include <cairomm/context.h>

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/rgb_macros.h"

#include "canvas/note.h"
#include "canvas/debug.h"

using namespace std;
using namespace ArdourCanvas;

bool Note::_show_velocity_bars = true;

void
Note::set_show_velocity_bars (bool yn)
{
	_show_velocity_bars = yn;
}

Note::Note (Canvas* c)
	: Rectangle (c)
	, _velocity (0.0)
	, _velocity_color (0)
{
}

Note::Note (Item* parent)
	: Rectangle (parent)
	, _velocity (0.0)
	, _velocity_color (0)
{
}

void
Note::set_velocity (double fract)
{
	_velocity = max (0.0, min (1.0, fract));
	redraw ();
}

void
Note::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rectangle::render (area, context);

	if (_show_velocity_bars && _velocity > 0.0) {

		Rect self (item_to_window (Rectangle::get().translate (_position), false));

		if ((self.y1 - self.y0) < ((outline_width() * 2) + 1)) {
			/* not tall enough to show a velocity bar */
			return;
		}

		/* 2 pixel margin above and below (taking outline width into account).
		   outline_width() margin on left.
		   set width based on velocity.
		*/
		const double center = (self.y1 - self.y0) * 0.5;
		self.y1  = self.y0 + center + 2;
		self.y0  = self.y0 + center - 1;
		const double width = (self.x1 - self.x0) - (2 * outline_width());
		self.x0  = self.x0 + outline_width();
		self.x1  = self.x0 + (width * _velocity);

		const Rect draw = self.intersection (area);

		if (!draw) {
			return;
		}

		Gtkmm2ext::set_source_rgba (context, _velocity_color);
		context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
		context->fill ();
	}
}

void
Note::set_fill_color (Gtkmm2ext::Color c)
{
	Fill::set_fill_color (c);
	_velocity_color = UINT_INTERPOLATE (c, 0x000000ff, 0.5);
}
