/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __ardour_canvas_text_h__
#define __ardour_canvas_text_h__

#include <pangomm/fontdescription.h>
#include <pangomm/layout.h>

#include "canvas/visibility.h"
#include "canvas/item.h"

namespace ArdourCanvas {

class LIBCANVAS_API Text : public Item
{
public:
	Text (Canvas*);
	Text (Item*);
	~Text();

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	Gtkmm2ext::Color color () const { return _color; }
	void set_color (Gtkmm2ext::Color);

	void set (std::string const &);
	void set_font_description (Pango::FontDescription);
	void set_alignment (Pango::Alignment);

	void clamp_width (double);

	double width() const;
	double height() const;

	void set_size_chars (int nchars);
	void dump (std::ostream&) const;

	std::string text() const { return _text; }
	double text_width() const;

private:
	std::string             _text;
	Gtkmm2ext::Color        _color;
	Pango::FontDescription* _font_description;
	Pango::Alignment        _alignment;
	mutable Cairo::RefPtr<Cairo::ImageSurface> _image;
	mutable Duple           _origin;
	mutable double          _width;
	mutable double          _height;
	mutable bool            _need_redraw;
	mutable double          _width_correction;
	double                  _clamped_width;

	void _redraw () const;
};

}

#endif /* __ardour_canvas_text_h__ */
