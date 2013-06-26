/*
    Copyright (C) 2011-2013 Paul Davis
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

#include <gdk/gdk.h>

#include <cairomm/cairomm.h>
#include <gtkmm/label.h>

#include "pbd/stacktrace.h"

#include "canvas/text.h"
#include "canvas/canvas.h"
#include "canvas/utils.h"

using namespace std;
using namespace ArdourCanvas;

Text::Text (Group* parent)
	: Item (parent)
	, _color (0x000000ff)
	, _font_description (0)
	, _alignment (Pango::ALIGN_LEFT)
	, _width (0)
	, _height (0)
	, _need_redraw (false)
	, _clamped_width (COORD_MAX)
{

}

Text::~Text ()
{
	delete _font_description;
}

void
Text::set (string const & text)
{
	begin_change ();
	
	_text = text;

	_need_redraw = true;
	_bounding_box_dirty = true;

	end_change ();
}

void
Text::redraw (Cairo::RefPtr<Cairo::Context> context) const
{
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

	layout->set_text (_text);

	if (_font_description) {
		layout->set_font_description (*_font_description);
	}

	layout->set_alignment (_alignment);
	
	Pango::Rectangle ink_rect = layout->get_ink_extents();
	
	_origin.x = ink_rect.get_x() / Pango::SCALE;
	_origin.y = ink_rect.get_y() / Pango::SCALE;

	_width = _origin.x + (ink_rect.get_width() / Pango::SCALE);
	_height = _origin.y + (ink_rect.get_height() / Pango::SCALE);
	
	_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, _width, _height);

	Cairo::RefPtr<Cairo::Context> img_context = Cairo::Context::create (_image);

	/* and draw, in the appropriate color of course */

	set_source_rgba (img_context, _color);

	layout->show_in_cairo_context (img_context);

	/* text has now been rendered in _image and is ready for blit in
	 * ::render 
	 */

	_need_redraw = false;
}

void
Text::render (Rect const & /*area*/, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_text.empty()) {
		return;
	}

	if (_need_redraw) {
		redraw (context);
	}
	
	Rect self = item_to_window (Rect (0, 0, min (_clamped_width, _width), _height));
	context->rectangle (self.x0, self.y0, self.width(), self.height());
	context->set_source (_image, self.x0, self.y0 - 2);
	context->fill ();
}

void
Text::clamp_width (double w)
{
	_clamped_width = w;
}

void
Text::compute_bounding_box () const
{
	if (!_canvas || _text.empty()) {
		_bounding_box = boost::optional<Rect> ();
		_bounding_box_dirty = false;
		return;
	}

	PangoContext* _pc = gdk_pango_context_get ();
	Glib::RefPtr<Pango::Context> context = Glib::wrap (_pc); // context now owns _pc and will free it
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);
	
	layout->set_text (_text);
	if (_font_description) {
		layout->set_font_description (*_font_description);
	}
	layout->set_alignment (_alignment);
	Pango::Rectangle const r = layout->get_ink_extents ();
	
	_bounding_box = Rect (
		r.get_x() / Pango::SCALE,
		r.get_y() / Pango::SCALE,
		(r.get_x() + r.get_width()) / Pango::SCALE,
		(r.get_y() + r.get_height()) / Pango::SCALE
		);
		
	_bounding_box_dirty = false;
}

void
Text::set_alignment (Pango::Alignment alignment)
{
	begin_change ();
	
	_alignment = alignment;
	_need_redraw = true;
	_bounding_box_dirty = true;
	end_change ();
}

void
Text::set_font_description (Pango::FontDescription font_description)
{
	begin_change ();
	
	_font_description = new Pango::FontDescription (font_description);
	_need_redraw = true;

	_bounding_box_dirty = true;
	end_change ();
}

void
Text::set_color (Color color)
{
	begin_change ();

	_color = color;
	_need_redraw = true;

	end_change ();
}

		
void
Text::dump (ostream& o) const
{
	Item::dump (o);

	o << _canvas->indent() << '\t' << " text = " << _text << endl
	  << _canvas->indent() << " color = " << _color;

	o << endl;
}
