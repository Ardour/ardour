/*
    Copyright (C) 2014 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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

#include <algorithm>
#include <cairomm/context.h>

#include <pangomm/layout.h>

#include "canvas/ruler.h"
#include "canvas/types.h"
#include "canvas/debug.h"
#include "canvas/utils.h"
#include "canvas/canvas.h"

using namespace std;
using namespace ArdourCanvas;

Ruler::Ruler (Canvas* c, const Metric& m)
	: Rectangle (c)
	, _metric (m)
	, _lower (0)
	, _upper (0)
	, _need_marks (true)
{
}

Ruler::Ruler (Canvas* c, const Metric& m, Rect const& r)
	: Rectangle (c, r)
	, _metric (m)
	, _lower (0)
	, _upper (0)
	, _need_marks (true)
{
}

Ruler::Ruler (Group* g, const Metric& m)
	: Rectangle (g)
	, _metric (m)
	, _lower (0)
	, _upper (0)
	, _need_marks (true)
{
}

Ruler::Ruler (Group* g, const Metric& m, Rect const& r)
	: Rectangle (g, r)
	, _metric (m)
	, _lower (0)
	, _upper (0)
	, _need_marks (true)
{
}

void
Ruler::set_range (double l, double u)
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
	_font_description = new Pango::FontDescription (fd);
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
	boost::optional<Rect> i = self.intersection (area);

	if (!i) {
		return;
	}

	Rect intersection (i.get());

	Distance height = self.height();

	if (_need_marks) {
		marks.clear ();
		_metric.get_marks (marks, _lower, _upper, 50);
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
	if (_font_description) {
		layout->set_font_description (*_font_description);
	}

	for (vector<Mark>::const_iterator m = marks.begin(); m != marks.end(); ++m) {
		Duple pos;

		pos.x = floor ((m->position - _lower) / _metric.units_per_pixel);
		pos.y = self.y1; /* bottom edge */

		if (_outline_width == 1.0) {
			/* Cairo single pixel line correction */
			cr->move_to (pos.x + 0.5, pos.y);
		} else {
			cr->move_to (pos.x, pos.y);
		}
		
		switch (m->style) {
		case Mark::Major:
			cr->rel_line_to (0, -height);
			break;
		case Mark::Minor:
			cr->rel_line_to (0, -height/2.0);
			break;
		case Mark::Micro:
			cr->rel_line_to (0, -height/4.0);
			break;
		}
		cr->stroke ();

		/* and the text */

		if (!m->label.empty()) {
			Pango::Rectangle logical;

			layout->set_text (m->label);
			logical = layout->get_pixel_logical_extents ();
			
			cr->move_to (pos.x + 2.0, self.y0 + logical.get_y());
			layout->show_in_cairo_context (cr);
		}
	}

	/* done! */
}
