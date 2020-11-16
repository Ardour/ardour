/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <algorithm>
#include <cairomm/context.h>

#include <pangomm/layout.h>

#include "canvas/ruler.h"
#include "canvas/types.h"
#include "canvas/debug.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

Ruler::Ruler (Canvas* c, const Metric& m)
	: Rectangle (c)
	, _metric (&m)
	, _lower (0)
	, _upper (0)
	, _divide_height (-1.0)
	, _font_description (0)
	, _second_font_description (0)
	, _need_marks (true)
{
}

Ruler::Ruler (Canvas* c, const Metric& m, Rect const& r)
	: Rectangle (c, r)
	, _metric (&m)
	, _lower (0)
	, _upper (0)
	, _divide_height (-1.0)
	, _font_description (0)
	, _second_font_description (0)
	, _need_marks (true)
{
}

Ruler::Ruler (Item* parent, const Metric& m)
	: Rectangle (parent)
	, _metric (&m)
	, _lower (0)
	, _upper (0)
	, _divide_height (-1.0)
	, _font_description (0)
	, _second_font_description (0)
	, _need_marks (true)
{
}

Ruler::Ruler (Item* parent, const Metric& m, Rect const& r)
	: Rectangle (parent, r)
	, _metric (&m)
	, _lower (0)
	, _upper (0)
	, _divide_height (-1.0)
	, _font_description (0)
	, _second_font_description (0)
	, _need_marks (true)
{
}

void
Ruler::set_range (int64_t l, int64_t u)
{
	begin_visual_change ();
	_lower = l;
	_upper = u;
	_need_marks = true;
	end_visual_change ();
}

void
Ruler::set_font_description (Pango::FontDescription fd)
{
	begin_visual_change ();
	delete _font_description;
	_font_description = new Pango::FontDescription (fd);
	end_visual_change ();
}

void
Ruler::set_second_font_description (Pango::FontDescription fd)
{
	begin_visual_change ();
	delete _second_font_description;
	_second_font_description = new Pango::FontDescription (fd);
	end_visual_change ();
}


void
Ruler::render (Rect const & area, Cairo::RefPtr<Cairo::Context> cr) const
{
	if (_lower == _upper) {
		/* nothing to draw */
		return;
	}

	Rect self (item_to_window (get()));
	Rect i = self.intersection (area);

	if (!i) {
		return;
	}

	Rect intersection (i);

	Distance height = self.height();

	if (_need_marks) {
		marks.clear ();
		_metric->get_marks (marks, _lower, _upper, 50);
		_need_marks = false;
	}

	/* draw background */

	setup_fill_context (cr);
	cr->rectangle (intersection.x0, intersection.y0, intersection.width(), intersection.height());
	cr->fill ();

	/* switch to outline context */

	setup_outline_context (cr);

	/* draw line on lower edge as a separator */

	if (_outline_width == 1.0) {
		/* Cairo single pixel line correction */
		cr->move_to (self.x0, self.y1-0.5);
		cr->line_to (self.x1, self.y1-0.5);
	} else {
		cr->move_to (self.x0, self.y1);
		cr->line_to (self.x1, self.y1);
	}
	cr->stroke ();

	/* draw ticks + text */

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (cr);

	Pango::FontDescription* last_font_description = 0;

	for (vector<Mark>::const_iterator m = marks.begin(); m != marks.end(); ++m) {
		Duple pos;
		Pango::FontDescription* fd = _font_description;

		pos.x = floor ((m->position - _lower) / _metric->units_per_pixel);
		pos.y = self.y1; /* bottom edge */

		if (_outline_width == 1.0) {
			/* Cairo single pixel line correction */
			cr->move_to (pos.x + 0.5, pos.y);
		} else {
			cr->move_to (pos.x, pos.y);
		}

		switch (m->style) {
			case Mark::Major:
				if (_divide_height >= 0) {
					cr->rel_line_to (0, -_divide_height);
				} else {
					cr->rel_line_to (0, -height);
				}
				if (_second_font_description) {
					fd = _second_font_description;
				}
				break;
			case Mark::Minor:
				cr->rel_line_to (0, -height/3.0);
				break;
			case Mark::Micro:
				cr->rel_line_to (0, -height/5.0);
				break;
		}
		cr->stroke ();

		if (fd != last_font_description) {
			layout->set_font_description (*fd);
			last_font_description = fd;
		}

		/* and the text */

		if (!m->label.empty()) {
			Pango::Rectangle logical;

			layout->set_text (m->label);
			logical = layout->get_pixel_logical_extents ();

			if (_divide_height >= 0) {
				cr->move_to (pos.x + 2.0, self.y0 + _divide_height + logical.get_y() + 2.0); /* 2 pixel padding below divider */
			} else {
				cr->move_to (pos.x + 2.0, self.y0 + logical.get_y() + .5 * (height - logical.get_height()));
			}
			layout->show_in_cairo_context (cr);
		}
	}

	if (_divide_height >= 0.0) {

		cr->set_line_width (1.0);

		Gtkmm2ext::set_source_rgba (cr, _divider_color_top);
		cr->move_to (self.x0, self.y0 + _divide_height-1.0+0.5);
		cr->line_to (self.x1, self.y0 + _divide_height-1.0+0.5);
		cr->stroke ();

		Gtkmm2ext::set_source_rgba (cr, _divider_color_bottom);
		cr->move_to (self.x0, self.y0 + _divide_height+0.5);
		cr->line_to (self.x1, self.y0 + _divide_height+0.5);
		cr->stroke ();


	}

	/* done! */
}

void
Ruler::set_divide_height (double h)
{
        _divide_height = h;
}

void
Ruler::set_divide_colors (Gtkmm2ext::Color t, Gtkmm2ext::Color b)
{
        _divider_color_bottom = b;
        _divider_color_top = t;
}

void
Ruler::set_metric (const Metric& m)
{
        _metric = &m;
        _need_marks = true;
        redraw ();
}
