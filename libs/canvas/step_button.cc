/*
    Copyright (C) 2017 Paul Davis
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

#include <iostream>

#include <cairomm/context.h>
#include <cairomm/pattern.h>

#include "pbd/compose.h"

#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/step_button.h"
#include "canvas/text.h"

#include "gtkmm2ext/utils.h"

using namespace std;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace Cairo;

StepButton::StepButton (Canvas* canvas, double w, double h, Gtkmm2ext::Color c)
	: Item (canvas)
	, width (w)
	, height (h)
	, label (new Text (canvas))
	, current_value (0)
	, prelight (false)
	, highlight (false)
	, dragging (false)
	, clicking (false)
	, color (c)
{
	label->set (string_compose ("%1", rint (current_value)));
	label->set_color (contrasting_text_color (c));
	label->set_font_description (Pango::FontDescription ("Sans 9"));
	add (label);

	create_patterns ();

	Event.connect (sigc::mem_fun (*this, &StepButton::event_handler));
	label->Event.connect (sigc::mem_fun (*this, &StepButton::event_handler));

	Rect r = label->bounding_box ();
	label->set_position (Duple ((width - r.width())/2.0, (height - r.height())/2.0));
}

void
StepButton::compute_bounding_box () const
{
	_bounding_box = Rect (0, 0, width, height);

	/* Item::bounding_box() will add children */

	bb_clean ();
}

#define CORNER_RADIUS 5

void
StepButton::create_patterns ()
{
	double r, g, b, a;

	inactive_pattern = LinearGradient::create (0.0, 0.0, width, height);
	color_to_rgba (color.darker (0.95).color(), r, g, b, a);
	inactive_pattern->add_color_stop_rgb (0.00, r, g, b);
	color_to_rgba (color.darker (0.85).color(), r, g, b, a);
	inactive_pattern->add_color_stop_rgb (1.00, r, g, b);

	enabled_pattern = LinearGradient::create (0.0, 0.0, width, height);
	color_to_rgba (color.lighter (0.95).color(), r, g, b, a);
	enabled_pattern->add_color_stop_rgb (0.00, r, g, b);
	color_to_rgba (color.lighter (0.85).color(), r, g, b, a);
	enabled_pattern->add_color_stop_rgb (1.00, r, g, b);
}

void
StepButton::set_color (Gtkmm2ext::Color c)
{
	color = c;
	label->set_color (contrasting_text_color (c));
	create_patterns ();
	redraw ();
}

void
StepButton::set_size (double w, double h)
{
	width = w;
	height = h;

	_bounding_box_dirty = true;

	create_patterns ();

	redraw ();
}

void
StepButton::set_value (double val)
{
	val = rint (val);
	if (val <= 0) { val = 0;}
	if (val >= 127) { val = 127;}
	if (val == current_value) {
		return;
	}
	current_value = val;
	label->set (string_compose ("%1", (int) current_value));

	/* move to recenter */
	Rect r = label->bounding_box ();
	label->set_position (Duple ((width - r.width())/2.0, (height - r.height())/2.0));

	redraw ();
}

void
StepButton::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Rect self = item_to_window (_bounding_box, false);
	Rect draw = self.intersection (area);

	if (!draw) {
		return;
	}

	double cr, g, b, a;

	context->save ();
	context->set_operator (OPERATOR_OVER);

	/* basic (rounded) rectangle, with pattern to fill */
	rounded_rectangle (context, self.x0 + 2.5, self.y0 + 2.5, width - 4, height - 4, CORNER_RADIUS);
	if (current_value > 0) {
		color_to_rgba (color.lighter (0.95).color(), cr, g, b, a);
		context->set_source_rgb (cr, g, b);
		context->set_source (enabled_pattern);
		context->fill_preserve ();
		float fc = current_value / 127.f;
		context->set_source_rgba (fc, .6 * fc, .2 * fc, .6);
	} else {
		context->set_source (inactive_pattern);
	}

	context->fill_preserve ();

	/* draw a (hard-coded) black outline around the same shape */

	context->set_line_width (.75);
	context->set_source_rgba (.0, .0, .0, 1.0);
	context->stroke_preserve ();
	context->clip ();

	/* draw several lines on the edges to "shade" them */

	for (int r = 2 * CORNER_RADIUS; r > 0; --r) {

		context->set_line_width (r);

		const float alpha = 0.1 - 0.1 * r / (2 * CORNER_RADIUS + 1.f);
		color_to_rgba (color.darker (0.95).color(), cr, g, b, a);
		context->set_source_rgba (cr, g, b, alpha);

		/* draw a line along the top side */
		context->move_to(self.x0, self.y0 + 2.5);
		context->rel_line_to(width, 0);
		context->stroke();

		/* draw a line down the left side */
		context->move_to(self.x0 + 2.5, self.y0);
		context->rel_line_to(0, height);
		context->stroke();

		/* draw a line along the bottom edge */
		context->set_source_rgba (.0, .0, .0, alpha);
		context->move_to(self.x0 + 2.5, self.y1 - 1.5);
		context->rel_line_to(width - 4, 0);
		context->stroke();

		/* draw a line down the right hand side */
		context->move_to(self.x1 - 2.5, self.y0 + 1.5);
		context->rel_line_to(0, height - 4);
		context->stroke();
	}

	if (highlight) {
		context->set_operator (Cairo::OPERATOR_OVER);
		context->set_source_rgba (1.0, 0.0, 0.0, .2);
		rounded_rectangle (context, self.x0 + 2.5, self.y0 + 2.5, width - 4, height - 4, CORNER_RADIUS);
		context->fill();
	}

	if (prelight) {
		context->set_operator (Cairo::OPERATOR_OVER);
		color_to_rgba (contrasting_text_color (color.color()), cr, g, b, a);
		context->set_source_rgba (cr, g, b, 0.1);
		rounded_rectangle (context, self.x0 + 2.5, self.y0 + 2.5, width - 4, height - 4, CORNER_RADIUS);
		context->fill();
	}

	context->restore ();

	Item::render_children (area, context);
}

void
StepButton::set_highlight (bool yn)
{
	if (highlight != yn) {
		highlight = yn;
		redraw ();
	}
}

bool
StepButton::event_handler (GdkEvent *ev)
{
	if (ev->type == GDK_ENTER_NOTIFY) {
		prelight = true;
		redraw ();
	} else if (ev->type == GDK_LEAVE_NOTIFY) {
		prelight = false;
		redraw ();
	}
	return false;
}
