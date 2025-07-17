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

#include "canvas/button.h"
#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/button.h"
#include "canvas/text.h"

#include "gtkmm2ext/utils.h"

using namespace std;
using namespace ArdourCanvas;
using namespace Gtkmm2ext;
using namespace Cairo;

Button::Button (Canvas* canvas, double w, double h, Pango::FontDescription const & font_description)
	: Rectangle (canvas)
	, width (w)
	, height (h)
	, _label (new Text (canvas))
	, prelight (false)
	, highlight (false)
	, clicking (false)
{
	_label->set_font_description (font_description);
	init ();
}

Button::Button (Item* parent, double w, double h, Pango::FontDescription const & font_description)
	: Rectangle (parent)
	, _label (new Text (this))
	, prelight (false)
	, highlight (false)
	, clicking (false)
{
	_label->set_font_description (font_description);
	init ();
}


Button::Button (Canvas* canvas, std::string const & str, Pango::FontDescription const & font_description)
	: Rectangle (canvas)
	, _label (new Text (canvas))
	, prelight (false)
	, highlight (false)
	, clicking (false)
{
	_label->set_font_description (font_description);
	_label->set (str);

	Rect r = _label->bounding_box();

	width = r.width();
	height = r.height();

	init ();
}

Button::Button (Item* parent, std::string const & str, Pango::FontDescription const & font_description)
	: Rectangle (parent)
	, _label (new Text (this))
	, prelight (false)
	, highlight (false)
	, clicking (false)
{
	_label->set_font_description (font_description);
	_label->set (str);

	Rect r = _label->bounding_box();

	width = r.width();
	height = r.height();

	init ();
}

void
Button::init ()
{
	Event.connect (sigc::mem_fun (*this, &Button::event_handler));
	_label->Event.connect (sigc::mem_fun (*this, &Button::event_handler));

	Rect r = _label->bounding_box ();
	_label->set_position (Duple ((width - r.width())/2.0, (height - r.height())/2.0));

	set_size_request (width, height);
}

void
Button::compute_bounding_box () const
{
	_bounding_box = Rect (0, 0, width, height);

	/* Item::bounding_box() will add children */

	set_bbox_clean ();
}

#define CORNER_RADIUS 5

void
Button::set_size (double w, double h)
{
	width = w;
	height = h;

	set_bbox_dirty ();
	redraw ();
}

void
Button::set_label (std::string const & str)
{
	_label->set (str);

	Rect r = _label->bounding_box ();

	/* alter our own size request to fit text + padding */
	set_size_request (r.width(), r.height());

	/* move to recenter */

	_label->set_position (Duple ((width - r.width())/2.0, (height - r.height())/2.0));

	redraw ();
}

void
Button::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
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
Button::set_highlight (bool yn)
{
	if (highlight != yn) {
		highlight = yn;
		redraw ();
	}
}

bool
Button::event_handler (GdkEvent *ev)
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

std::string
Button::label() const
{
	return _label->text();
}
