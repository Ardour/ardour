/*
 * Copyright (C) 2013-2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
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
	Flag (Canvas *, Distance, Gtkmm2ext::Color, Gtkmm2ext::Color, Duple, bool invert=false);
	Flag (Item*, Distance, Gtkmm2ext::Color, Gtkmm2ext::Color, Duple, bool invert=false);

	void set_text (std::string const &);
	void set_height (Distance);

	void set_font_description (Pango::FontDescription);

	bool covers (Duple const &) const;

	double width() const;

private:
	void setup (Distance height, Duple position);

	Gtkmm2ext::Color _outline_color;
	Gtkmm2ext::Color _fill_color;
	Text* _text;
	Line* _line;
	Rectangle* _rectangle;
	bool _invert;
};

}

#endif // __CANVAS_FLAG_H__
