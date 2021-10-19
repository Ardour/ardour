/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <gdk/gdk.h>

#include <cairomm/cairomm.h>
#include <gtkmm/window.h>
#include <gtkmm/label.h>

#include "pbd/i18n.h"

#include "canvas/text.h"
#include "canvas/canvas.h"
#include "gtkmm2ext/colors.h"

using namespace std;
using namespace ArdourCanvas;

Text::FontSizeMaps Text::font_size_maps;

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
	, _height_based_on_allocation (false)
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
	, _height_based_on_allocation (false)
{
	_outline = false;
}

Text::~Text ()
{
	delete _font_description;
}

void
Text::set_height_based_on_allocation (bool yn)
{
	/* assumed to be set during construction, so we do not schedule a
	 * redraw after changing this.
	 */

	_height_based_on_allocation = yn;
}

void
Text::set (string const & text)
{
	if (text == _text) {
		return;
	}

	begin_change ();

	_text = text;

	_need_redraw = true;
	_bounding_box_dirty = true;

	end_change ();
}

double
Text::width () const
{
	if (_need_redraw) {
		_redraw  ();
	}
	return _width;
}

double
Text::height () const
{
	if (_need_redraw) {
		_redraw  ();
	}
	return _height;
}

void
Text::_redraw () const
{
	/* XXX we should try to remove this assertion someday. Nothing wrong
	   with empty text.
	*/
	assert (!_text.empty());
	assert (_canvas);
	Glib::RefPtr<Pango::Context> context = _canvas->get_pango_context();
	Glib::RefPtr<Pango::Layout> layout = Pango::Layout::create (context);

#if 0 // def __APPLE__ // Looks like this is no longer needed 2017-03-11, pango 1.36.8, pangomm 2.34.0
	if (_width_correction < 0.0) {
		// Pango returns incorrect text width on some OS X
		// So we have to make a correction
		// To determine the correct indent take the largest symbol for which the width is correct
		// and make the calculation
		Gtk::Window win;
		Gtk::Label foo;
		win.add (foo);
		win.ensure_style ();

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
		Gtkmm2ext::set_source_rgba (img_context, _outline_color);
		layout->update_from_cairo_context (img_context);
		pango_cairo_layout_path (img_context->cobj(), layout->gobj());
		img_context->stroke_preserve ();
		Gtkmm2ext::set_source_rgba (img_context, _color);
		img_context->fill ();
	} else {
		Gtkmm2ext::set_source_rgba (img_context, _color);
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

	const Rect r (0, 0, min (_clamped_width, (double)_image->get_width ()), _image->get_height ());
	Rect self = item_to_window (r);
	Rect intersection = self.intersection (area);

	if (!intersection) {
		return;
	}

	if (_need_redraw) {
		_redraw ();
	}

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
	if (_clamped_width == w) {
		return;
	}
	begin_change ();
	_clamped_width = w;
	_bounding_box_dirty = true;
	end_change ();
}

void
Text::compute_bounding_box () const
{
	if (!_canvas || _text.empty()) {
		_bounding_box = Rect ();
		bb_clean ();
		return;
	}

	if (_bounding_box_dirty) {
#ifdef __APPLE__
		const float retina_factor = 0.5;
#else
		const float retina_factor = 1.0;
#endif
		if (_need_redraw || !_image) {
			_redraw ();
		}
		_bounding_box = Rect (0, 0, min (_clamped_width, (double) _image->get_width() * retina_factor), _image->get_height() * retina_factor);
		bb_clean ();
	}
}

void
Text::set_alignment (Pango::Alignment alignment)
{
	if (alignment == _alignment) {
		return;
	}

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
Text::set_color (Gtkmm2ext::Color color)
{
	if (color == _color) {
		return;
	}

	begin_change ();

	_color = color;
	if (_outline) {
		set_outline_color (Gtkmm2ext::contrasting_text_color (_color));
	}
	_need_redraw = true;

	end_change ();
}


void
Text::dump (ostream& o) const
{
	Item::dump (o);

	o << _canvas->indent() << '\t' << " text = " << _text << endl
	  << _canvas->indent() << " color = 0x" << hex << _color << dec;

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

double
Text::text_height() const
{
    if (_need_redraw) {
        redraw ();
    }

    return _height;
}

void
Text::_size_allocate (Rect const & r)
{
	Item::_size_allocate (r);

	if (!layout_sensitive()) {
		/* not doing this */
		return;
	}

	if (!_height_based_on_allocation) {
		/* non-resizable text */
		return;
	}

	int font_size = font_size_for_height (r.height(), _font_description->get_family(), _canvas->get_pango_context());

	if (font_size) {
		char font_name[32];
		std::string family = "Sans"; // UIConfiguration::instance().get_ui_font_family();
		snprintf (font_name, sizeof (font_name), "%s %d", family.c_str(), font_size);
		Pango::FontDescription pfd (font_name);
		set_font_description (pfd);
		show ();
	} else {
		hide ();
	}
}

int
Text::font_size_for_height (Distance height, std::string const & font_family, Glib::RefPtr<Pango::Context> const & ctxt)
{
	FontSizeMaps::iterator fsM = font_size_maps.find (font_family);  /* map of maps */

	if (fsM == font_size_maps.end()) {
		fsM = font_size_maps.insert (make_pair (font_family, FontSizeMap())).first;
	}

	FontSizeMap::iterator fsm = fsM->second.find (height); /* map of point size -> pixel height */

	if (fsm != fsM->second.end()) {
		return fsm->second;
	}

	Glib::RefPtr<Pango::Layout> l (Pango::Layout::create (ctxt));
	int font_size = 0;
	char font_name[32];

	/* Translators: Xg is a nonsense string that should include the
	   highest glyph and a glyph with the lowest descender
	*/

	l->set_text (_("Xg"));

	for (uint32_t pt = 5; pt < 24; ++pt) {

		snprintf (font_name, sizeof (font_name), "%s %d", font_family.c_str(), pt);
		Pango::FontDescription pfd (font_name);
		l->set_font_description (pfd);

		int w, h;
		l->get_pixel_size (w, h);
		if (h > height) {
			font_size = pt - 1;
			break;
		}
	}

	if (font_size) {
		fsM->second.insert (make_pair (height, font_size));
	}

	return font_size;
}

void
Text::drop_height_maps ()
{
	font_size_maps.clear ();
}
