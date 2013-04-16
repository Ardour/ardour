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

#ifndef __ardour_canvas_text_h__
#define __ardour_canvas_text_h__

#include <pangomm/fontdescription.h>
#include <pangomm/layout.h>

#include "canvas/item.h"

namespace ArdourCanvas {

class Text : public Item
{
public:
	Text (Group *);
       ~Text();

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	void compute_bounding_box () const;

	void set (std::string const &);
	void set_color (uint32_t);
	void set_font_description (Pango::FontDescription);
	void set_alignment (Pango::Alignment);

        void clamp_width (double);

        void set_size_chars (int nchars);
        void dump (std::ostream&) const;

private:
	std::string      _text;
	uint32_t         _color;
	Pango::FontDescription* _font_description;
	Pango::Alignment _alignment;
        mutable Cairo::RefPtr<Cairo::ImageSurface> _image;
        mutable Duple _origin;
        mutable double _width;
        mutable double _height;
        mutable bool _need_redraw;
        double _clamped_width;

        void redraw (Cairo::RefPtr<Cairo::Context>) const;
};

}

#endif /* __ardour_canvas_text_h__ */
