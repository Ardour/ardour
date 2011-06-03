/*
  Copyright (C) 2011 Paul Davis

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
#include <cmath>
#include <iostream>

#include "gtkmm2ext/cairocell.h"
#include "gtkmm2ext/utils.h"

using std::string;
using std::map;
using std::cerr;
using std::endl;
using namespace Gtkmm2ext;

CairoCell::CairoCell (int32_t id)
	: _id (id)
	, _visible (true)
	, _xpad (2)
{
	bbox.x = 0;
	bbox.y = 0;
	bbox.width = 0;
	bbox.height = 0;
}

void
CairoColonCell::render (Cairo::RefPtr<Cairo::Context>& context)
{
	/* two very small circles */
	context->arc (bbox.x, bbox.y + (bbox.height/3.0), bbox.width/2.0, 0.0, M_PI*2.0);
	context->fill ();
	context->arc (bbox.x, bbox.y + (2.0 * bbox.height/3.0), bbox.width/2.0, 0.0, M_PI*2.0);
	context->fill ();
}

void
CairoColonCell::set_size (Glib::RefPtr<Pango::Context>& context, const Pango::FontDescription& font)
{
	Pango::FontMetrics metrics = context->get_metrics (font);
	bbox.width = std::max (3.0, (0.25 * metrics.get_approximate_char_width() / PANGO_SCALE));
	bbox.height = (metrics.get_ascent() + metrics.get_descent()) / PANGO_SCALE;
}

CairoTextCell::CairoTextCell (int32_t id, double wc)
	: CairoCell (id)
	, _width_chars (wc)
{
}

void
CairoTextCell::set_text (const std::string& txt)
{
	layout->set_text (txt);
}

void
CairoTextCell::render (Cairo::RefPtr<Cairo::Context>& context)
{
	if (!_visible || _width_chars == 0) {
		return;
	}

	context->move_to (bbox.x, bbox.y);
	pango_cairo_update_layout (context->cobj(), layout->gobj());
	pango_cairo_show_layout (context->cobj(), layout->gobj());
}

void
CairoTextCell::set_size (Glib::RefPtr<Pango::Context>& context, const Pango::FontDescription& font)
{
	if (!layout) {
		layout = Pango::Layout::create (context);
	}

	layout->set_font_description (font);

	Pango::FontMetrics metrics = context->get_metrics (font);

	bbox.width = (_width_chars * metrics.get_approximate_digit_width ()) / PANGO_SCALE;
	bbox.height = (metrics.get_ascent() + metrics.get_descent()) / PANGO_SCALE;
}

CairoEditableText::CairoEditableText ()
	: editing_cell (0)
	, _draw_bg (true)
	, width (0)
	, max_cell_height (0)
	, height (0)
	, _corner_radius (9)
	, _xpad (5)
	, _ypad (5)
{
	add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK |
	            Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::SCROLL_MASK);
	set_flags (Gtk::CAN_FOCUS);

	set_can_default (true);
	set_receives_default (true);
}

CairoEditableText::~CairoEditableText ()
{
	/* we don't own cells */
}

bool
CairoEditableText::on_scroll_event (GdkEventScroll* ev)
{
	CairoCell* cell = find_cell (ev->x, ev->y);

	if (cell) {
		return scroll (ev, cell);
	}

	return false;
}

bool
CairoEditableText::on_focus_in_event (GdkEventFocus* ev)
{
	return false;
}

bool
CairoEditableText::on_focus_out_event (GdkEventFocus* ev)
{
	if (editing_cell) {
		queue_draw_cell (editing_cell);
		editing_cell = 0;
	}
	return false;
}

void
CairoEditableText::add_cell (CairoCell* cell)
{
	Glib::RefPtr<Pango::Context> context = get_pango_context ();
	cell->set_size (context, _font);
	cells.push_back (cell);
	queue_resize ();
	queue_draw ();
}

void
CairoEditableText::clear_cells ()
{
	cells.clear ();
	queue_resize ();
	queue_draw ();
}

void
CairoEditableText::set_width_chars (CairoTextCell* cell, uint32_t wc)
{
	if (cell) {
		cell->set_width_chars (wc);
		queue_resize ();
	}
}

void
CairoEditableText::set_text (CairoTextCell* cell, const string& text)
{
	cell->set_text (text);
	queue_draw_cell (cell);
}

bool
CairoEditableText::on_expose_event (GdkEventExpose* ev)
{
	Cairo::RefPtr<Cairo::Context> context = get_window()->create_cairo_context();

	if (cells.empty()) {
		return true;
	}

	context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	context->clip ();

	if (_draw_bg) {
		context->set_source_rgba (bg_r, bg_g, bg_b, bg_a);
		if (_corner_radius) {
			rounded_rectangle (context, 0, 0, width, height, _corner_radius);
		} else {
			context->rectangle (0, 0, width, height);
		}
		context->fill ();
	}

	for (CellMap::iterator i = cells.begin(); i != cells.end(); ++i) {

		CairoCell* cell = (*i);

		/* is cell inside the expose area?
		 */

		if (cell->intersects (ev->area)) {
			if (cell == editing_cell) {
				context->set_source_rgba (edit_r, edit_b, edit_g, edit_a);
			} else {
				context->set_source_rgba (r, g, b, a);
			}

			cell->render (context);
		}
	}

	return true;
}

void
CairoEditableText::queue_draw_cell (CairoCell* cell)
{
	Glib::RefPtr<Gdk::Window> win = get_window();

	if (!win) {
		return;
	}

	Gdk::Rectangle r;

	r.set_x (cell->x());
	r.set_y (cell->y());
	r.set_width (cell->width());
	r.set_height (cell->height());

	Gdk::Region rg (r);
	win->invalidate_region (rg, true);
}

CairoCell*
CairoEditableText::find_cell (uint32_t x, uint32_t y)
{
	for (CellMap::iterator i = cells.begin(); i != cells.end(); ++i) {
		if ((*i)->covers (x, y)) {
			return (*i);
		}
	}

	return 0;
}

bool
CairoEditableText::on_button_press_event (GdkEventButton* ev)
{
	CairoCell* cell = find_cell (ev->x, ev->y);
	return button_press (ev, cell);
}

bool
CairoEditableText::on_button_release_event (GdkEventButton* ev)
{
	CairoCell* cell = find_cell (ev->x, ev->y);
	return button_release (ev, cell);
}

void
CairoEditableText::start_editing (CairoCell* cell)
{
	stop_editing ();

	if (cell) {
		editing_cell = cell;
		queue_draw_cell (cell);
		grab_focus ();
	}
}

void
CairoEditableText::stop_editing ()
{
	if (editing_cell) {
		queue_draw_cell (editing_cell);
		editing_cell = 0;
	}
}

void
CairoEditableText::on_size_request (GtkRequisition* req)
{
	double x = 0;

	max_cell_height = 0;

	x = _xpad;

	CellMap::iterator i = cells.begin(); 

	while (i != cells.end()) {
		CairoCell* cell = (*i);

		if (cell->visible()) {
			cell->set_position (x, _ypad);
		}

		x += cell->width();
		max_cell_height = std::max ((double) cell->height(), max_cell_height);

		++i;

		if (i != cells.end()) {
			/* only add cell padding intra-cellularly */
			x += cell->xpad();
		} else {
			break;
		}
	}

	x += _xpad;

	req->width = x;
	req->height = max_cell_height + (_ypad * 2);
}

void
CairoEditableText::on_size_allocate (Gtk::Allocation& alloc)
{
	Misc::on_size_allocate (alloc);

	width = alloc.get_width();
	height = alloc.get_height();
}

void
CairoEditableText::set_font (const std::string& str)
{
	set_font (Pango::FontDescription (str));
}

void
CairoEditableText::set_font (const Pango::FontDescription& fd)
{
	Glib::RefPtr<Pango::Context> context = get_pango_context ();

	_font = fd;

	for (CellMap::iterator i = cells.begin(); i != cells.end(); ++i) {
		(*i)->set_size (context, _font);
	}

	queue_resize ();
	queue_draw ();
}
