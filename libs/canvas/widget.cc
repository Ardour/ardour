/*
    Copyright (C) 2014 Paul Davis

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
#include "pbd/stacktrace.h"
#include "pbd/compose.h"

#include "canvas/canvas.h"
#include "canvas/widget.h"
#include "canvas/debug.h"
#include "canvas/utils.h"

using namespace std;
using namespace ArdourCanvas;

Widget::Widget (Canvas* c, CairoWidget& w)
	: Item (c)
	, _widget (w)
{
	Event.connect (sigc::mem_fun (*this, &Widget::event_proxy));
	w.set_canvas_widget ();
	w.QueueDraw.connect (sigc::mem_fun(*this, &Widget::queue_draw));
	w.QueueResize.connect (sigc::mem_fun(*this, &Widget::queue_resize));
}

Widget::Widget (Item* parent, CairoWidget& w)
	: Item (parent)
	, _widget (w)
{
	Event.connect (sigc::mem_fun (*this, &Widget::event_proxy));
	w.set_canvas_widget ();
	w.QueueDraw.connect (sigc::mem_fun(*this, &Widget::queue_draw));
	w.QueueResize.connect (sigc::mem_fun(*this, &Widget::queue_resize));
}

bool
Widget::event_proxy (GdkEvent* ev)
{
	/* XXX need to translate coordinate into widget's own coordinate space */
	return _widget.event (ev);
}

bool
Widget::queue_draw ()
{
	begin_visual_change ();
	end_visual_change ();
	return true;
}

bool
Widget::queue_resize ()
{
	begin_change ();
	end_change ();
	return true;
}

void
Widget::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	//std::cerr << "Render widget " << name << " @ " << position() << endl;

	if (!_bounding_box) {
		std::cerr << "no bbox\n";
		return;
	}

	Rect self = item_to_window (_bounding_box);
	Rect r = self.intersection (area);

	if (!r) {
		std::cerr << "no intersection\n";
		return;
	}

	Rect draw = r;
	cairo_rectangle_t crect;
	crect.x = draw.x0;
	crect.y = draw.y0;
	crect.height = draw.height();
	crect.width = draw.width();

	Duple p = position_offset();

	context->save ();
	context->translate (p.x, p.y);
	//context->rectangle (draw.x0, draw.y0, draw.width(), draw.height());
	//context->clip ();

	_widget.render (context->cobj(), &crect);

	context->restore ();
}

void
Widget::size_allocate (Rect const & r)
{
	Item::size_allocate (r);
	Gtk::Allocation alloc;
	alloc.set_x (0);
	alloc.set_y (0);
	alloc.set_width (r.width());
	alloc.set_height (r.height());
	_widget.size_allocate (alloc);
}

void
Widget::compute_bounding_box () const
{
	std::cerr << "cbbox for widget\n";

	GtkRequisition req = { 0, 0 };
	Gtk::Allocation alloc;

	_widget.size_request (req);

	std::cerr << "widget wants " << req.width << " x " << req.height << "\n";

	_bounding_box = Rect (0, 0, req.width, req.height);

	/* make sure the widget knows that it got what it asked for */
	alloc.set_x (0);
	alloc.set_y (0);
	alloc.set_width (req.width);
	alloc.set_height (req.height);

	_widget.size_allocate (alloc);

	_bounding_box_dirty = false;
}
