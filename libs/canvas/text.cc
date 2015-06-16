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
#include <gtkmm/window.h>
#include <gtkmm/label.h>

#include "pbd/stacktrace.h"

#include "canvas/text.h"
#include "canvas/canvas.h"
#include "canvas/utils.h"
#include "canvas/colors.h"

using namespace std;
using namespace ArdourCanvas;


Text::Text (Canvas* c)
	: Item (c)
	, _color (0x000000ff)
	, _font_description (0)
	, _alignment (Pango::ALIGN_LEFT)
	, _width (0)
	, _height (0)
	, _need_redraw (false)
        , _width_correction (-1)
	, _clamped_width (COORD_MAX)
{
	_outline = false;
}

Text::Text (Item* parent)
	: Item (parent)
	, _color (0x000000ff)
	, _font_description (0)
	, _alignment (Pango::ALIGN_LEFT)
	, _width (0)
	, _height (0)
	, _need_redraw (false)
        , _width_correction (-1)
	, _clamped_width (COORD_MAX)
{
	_outline = false;
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
Text::_redraw (Cairo::RefPtr<Cairo::Context> context) const
{
	if (_text.empty()) {
		return;
	}

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

	__redraw (layout);
}

void
Text::_redraw (Glib::RefPtr<Pango::Context> context) const
{
	if (_text.empty()) {
		return;
	}

	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);
	__redraw (layout);
}

void
Text::__redraw (Glib::RefPtr<Pango::Layout> layout) const
{
#ifdef __APPLE__
	if (_width_correction < 0.0) {
		// Pango returns incorrect text width on some OS X
		// So we have to make a correction
		// To determine the correct indent take the largest symbol for which the width is correct
		// and make the calculation
		Gtk::Window win;
		Gtk::Label foo;
		win.add (foo);

		int width = 0;
		int height = 0;
		Glib::RefPtr<Pango::Layout> test_layout = foo.create_pango_layout ("H");
		if (_font_description) {
			test_layout->set_font_description (*_font_description);
		}
		test_layout->get_pixel_size (width, height);

		_width_correction = width*1.5;
	}
#else
        /* don't bother with a conditional here */
        _width_correction = 0.0;
#endif

	layout->set_text (_text);

	if (_font_description) {
		layout->set_font_description (*_font_description);
	}

	layout->set_alignment (_alignment);
    
	int w;
	int h;

	layout->get_pixel_size (w, h);

	_width = w + _width_correction;
	_height = h;

#ifdef __APPLE__
	_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, _width * 2, _height * 2);
#else
	_image = Cairo::ImageSurface::create (Cairo::FORMAT_ARGB32, _width, _height);
#endif
	
	Cairo::RefPtr<Cairo::Context> img_context = Cairo::Context::create (_image);

#ifdef __APPLE__
	/* Below, the rendering scaling is set to support retina display
	 */
	img_context->scale (2, 2);
#endif
	
	/* and draw, in the appropriate color of course */

	if (_outline) {
		set_source_rgba (img_context, _outline_color);
		layout->update_from_cairo_context (img_context);
		pango_cairo_layout_path (img_context->cobj(), layout->gobj());
		img_context->stroke_preserve ();
		set_source_rgba (img_context, _color);
		img_context->fill ();
	} else {
		set_source_rgba (img_context, _color);
		layout->show_in_cairo_context (img_context);
	}

	/* text has now been rendered in _image and is ready for blit in
	 * ::render 
	 */

	_need_redraw = false;
}

void
Text::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_text.empty()) {
		return;
	}

	Rect self = item_to_window (Rect (0, 0, min (_clamped_width, (double)_image->get_width ()), _image->get_height ()));
	boost::optional<Rect> i = self.intersection (area);
	
	if (!i) {
		return;
	}

	if (_need_redraw) {
		_redraw (context);
	}
	
	Rect intersection (i.get());

	context->rectangle (intersection.x0, intersection.y0, intersection.width(), intersection.height());
#ifdef __APPLE__
	/* Below, the rendering scaling is set to support retina display
	 */
	Cairo::Matrix original_matrix = context->get_matrix();
	context->scale (0.5, 0.5);
	context->set_source (_image, self.x0 * 2, self.y0 * 2);
	context->fill ();
	context->set_matrix (original_matrix);
#else
	context->set_source (_image, self.x0, self.y0);
	context->fill ();
#endif
}

void
Text::clamp_width (double w)
{
        begin_change ();
	_clamped_width = w;
        _bounding_box_dirty = true;
        end_change ();
}

void
Text::compute_bounding_box () const
{
	if (!_canvas || _text.empty()) {
		_bounding_box = boost::optional<Rect> ();
		_bounding_box_dirty = false;
		return;
	}

	if (_bounding_box_dirty) {
		if (_need_redraw || !_image) {
			Glib::RefPtr<Pango::Context> context = Glib::wrap (gdk_pango_context_get()); // context now owns C object and will free it
			_redraw (context);
		}
		_bounding_box = Rect (0, 0, min (_clamped_width, (double) _image->get_width()), _image->get_height());
		_bounding_box_dirty = false;
	}
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
        _width_correction = -1.0;

	_bounding_box_dirty = true;
	end_change ();
}

void
Text::set_color (Color color)
{
	begin_change ();

	_color = color;
	if (_outline) {
		set_outline_color (contrasting_text_color (_color));
	}
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


double
Text::text_width() const
{
    if (_need_redraw) {
        redraw ();
    }
    
    return _width;
}
