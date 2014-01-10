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
using std::vector;
using std::map;
using std::max;
using std::cerr;
using std::endl;
using namespace Gtkmm2ext;

static const double cairo_font_fudge = 1.5;

CairoFontDescription::CairoFontDescription (Pango::FontDescription& fd)
{
	_size = cairo_font_fudge * (fd.get_size() / PANGO_SCALE);

	switch (fd.get_style()) {
	case Pango::STYLE_NORMAL:
		_slant = Cairo::FONT_SLANT_NORMAL;
		break;
	case Pango::STYLE_OBLIQUE:
		_slant = Cairo::FONT_SLANT_OBLIQUE;
		break;
	case Pango::STYLE_ITALIC:
		_slant = Cairo::FONT_SLANT_ITALIC;
		break;
	}

	switch (fd.get_weight()) {
	case Pango::WEIGHT_ULTRALIGHT:
		_weight = Cairo::FONT_WEIGHT_NORMAL;
		break;

	case Pango::WEIGHT_LIGHT:
		_weight = Cairo::FONT_WEIGHT_NORMAL;
		break;

	case Pango::WEIGHT_NORMAL:
		_weight = Cairo::FONT_WEIGHT_NORMAL;
		break;

	case Pango::WEIGHT_SEMIBOLD:
		_weight = Cairo::FONT_WEIGHT_BOLD;
		break;

	case Pango::WEIGHT_BOLD:
		_weight = Cairo::FONT_WEIGHT_BOLD;
		break;

	case Pango::WEIGHT_ULTRABOLD:
		_weight = Cairo::FONT_WEIGHT_BOLD;
		break;

	case Pango::WEIGHT_HEAVY:
		_weight = Cairo::FONT_WEIGHT_BOLD;
		break;

	}

	face = fd.get_family();
}       

CairoCell::CairoCell (int32_t id)
	: _id (id)
	, _visible (true)
	, _xpad (0)
{
	bbox.x = 0;
	bbox.y = 0;
	bbox.width = 0;
	bbox.height = 0;
}

CairoTextCell::CairoTextCell (int32_t id, double wc, boost::shared_ptr<CairoFontDescription> font)
	: CairoCell (id)
	, _width_chars (wc)
	, _font (font)
	, y_offset (0)
	, x_offset (0)
{
}

void
CairoTextCell::set_text (const std::string& txt)
{
	_text = txt;
}

void
CairoTextCell::render (Cairo::RefPtr<Cairo::Context>& context)
{
	if (!_visible || _width_chars == 0) {
		return;
	}

	context->save ();

	context->rectangle (bbox.x, bbox.y, bbox.width, bbox.height);
	context->clip ();

	_font->apply (context);
	context->move_to (bbox.x, bbox.y + bbox.height + y_offset);
	context->show_text (_text);

	context->restore ();
}

void
CairoTextCell::set_size (Cairo::RefPtr<Cairo::Context>& context)
{
	const uint32_t lim = (uint32_t) ceil (_width_chars);
	vector<char> buf(lim+1);
	uint32_t n;
	double max_width = 0.0;
	double max_height = 0.0;
	Cairo::TextExtents ext;
	double bsum = 0;

	buf[lim] = '\0';

	_font->apply (context);

	for (int digit = 0; digit < 10; digit++) {

		for (n = 0; n < lim; ++n) {
			buf[n] = '0' + digit; 
		}
		
		context->get_text_extents (&buf[0], ext);
		
		max_width = max (ext.width + ext.x_bearing, max_width);
		max_height = max (ext.height, max_height);
		bsum += ext.x_bearing;
	}

	/* add the average x-bearing for all digits as right hand side padding */

	bbox.width = max_width + (bsum/10.0);

  	/* some fonts and some digits get their extents computed "too small", so fudge this
	   by adding 2
	*/
	bbox.height = max_height;
}

CairoCharCell::CairoCharCell (int32_t id, char c)
	: CairoTextCell (id, 1)
{
	_text = c;
}

void
CairoCharCell::set_size (Cairo::RefPtr<Cairo::Context>& context)
{
	Cairo::TextExtents ext;

	_font->apply (context);
	
	{
		const char* buf = "8";
		context->get_text_extents (buf, ext);
		/* same height as an "8" */
		bbox.height = ext.height;
	}

	{
		const char* buf = ":";
		context->get_text_extents (buf, ext);
		bbox.width = ext.width + (2.0 * ext.x_bearing);
		/* center vertically */
		y_offset = (ext.height - bbox.height) / 2.0;
	}
}

CairoEditableText::CairoEditableText (boost::shared_ptr<CairoFontDescription> font)
	: editing_cell (0)
	, _draw_bg (true)
	, max_cell_width (0)
	, max_cell_height (0)
	, _corner_radius (9)
	, _xpad (0)
	, _ypad (0)
{
	set_font (font);

	add_events (Gdk::POINTER_MOTION_HINT_MASK | Gdk::SCROLL_MASK | Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK |
	            Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK | Gdk::SCROLL_MASK);
	set_flags (Gtk::CAN_FOCUS);

	set_can_default (true);
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
CairoEditableText::on_focus_in_event (GdkEventFocus*)
{
	return false;
}

bool
CairoEditableText::on_focus_out_event (GdkEventFocus*)
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
	cells.push_back (cell);
	
	CairoTextCell* tc = dynamic_cast<CairoTextCell*>(cell);

	if (tc) {
		tc->set_font (_font);
	}

	queue_resize ();
}

void
CairoEditableText::clear_cells ()
{
	cells.clear ();
	queue_resize ();
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
	Glib::RefPtr<Gdk::Window> win = get_window ();

	if (!win) {
		std::cerr << "CET: no window to draw on\n";
		return false;
	}

	Cairo::RefPtr<Cairo::Context> context = win->create_cairo_context();

	if (cells.empty()) {
		return true;
	}

	context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	context->clip ();

	Gtk::Allocation alloc = get_allocation ();
	double width = alloc.get_width();
	double height = alloc.get_height ();
		
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
CairoEditableText::set_cell_sizes ()
{
	Glib::RefPtr<Gdk::Window> win = get_window();

	if (!win) {
		return;
	}
	
	Cairo::RefPtr<Cairo::Context> context = win->create_cairo_context();
	
	if (!context) {
		return;
	}

	for (CellMap::iterator i = cells.begin(); i != cells.end(); ++i) {
		(*i)->set_size (context);
	}
}

void
CairoEditableText::on_size_request (GtkRequisition* req)
{
	set_cell_sizes ();

	max_cell_width = 0;
	max_cell_height = 0;
	
	for (CellMap::iterator i = cells.begin(); i != cells.end(); ++i) {
		max_cell_width += (*i)->width();
		max_cell_height = std::max ((double) (*i)->height(), max_cell_height);
	}

	req->width = max_cell_width;
	req->height = max_cell_height;
}

void
CairoEditableText::on_size_allocate (Gtk::Allocation& alloc)
{
	Misc::on_size_allocate (alloc);

	/* position each cell so that its centered in the allocated space
	 */

	double x = (alloc.get_width() - max_cell_width)/2.0;
	double y = (alloc.get_height() - max_cell_height)/2.0;

	CellMap::iterator i = cells.begin();

	while (i != cells.end()) {
		CairoCell* cell = (*i);

		cell->set_position (x, y);
		x += cell->width ();

		if (++i != cells.end()) {
			/* only add cell padding intra-cellularly */
			x += cell->xpad();
		} else {
			break;
		}
	}
}

void
CairoEditableText::set_font (Pango::FontDescription& fd)
{
	boost::shared_ptr<CairoFontDescription> cd (new CairoFontDescription (fd));
	set_font (cd);
}

void
CairoEditableText::set_font (boost::shared_ptr<CairoFontDescription> fd)
{
	for (CellMap::iterator i = cells.begin(); i != cells.end(); ++i) {
		CairoTextCell* tc = dynamic_cast<CairoTextCell*>(*i);
		if (tc && (!tc->font() || tc->font() == _font)) {
			tc->set_font (fd);
		}
	}

	_font = fd;

	queue_resize ();
	queue_draw ();
}

