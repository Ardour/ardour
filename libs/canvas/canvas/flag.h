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

#ifndef __CANVAS_FLAG_H__
#define __CANVAS_FLAG_H__

#include <pangomm/fontdescription.h>

#include "canvas/visibility.h"
#include "canvas/types.h"
#include "canvas/container.h"

namespace ArdourCanvas {

class Text;
class Line;
class Rectangle;

class LIBCANVAS_API Flag : public Container
{
public:
	Flag (Canvas *, Distance, Color, Color, Duple, bool invert=false);
	Flag (Item*, Distance, Color, Color, Duple, bool invert=false);

	void set_text (std::string const &);
	void set_height (Distance);

	void set_font_description (Pango::FontDescription);

        bool covers (Duple const &) const;

	double width() const;

private:
	void setup (Distance height, Duple position);

	Color _outline_color;
	Color _fill_color;
	Text* _text;
	Line* _line;
	Rectangle* _rectangle;
	bool _invert;
};

}

#endif // __CANVAS_FLAG_H__
