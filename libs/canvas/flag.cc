/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016 Nick Mainsbridge <mainsbridge@gmail.com>
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

#include "canvas/flag.h"
#include "canvas/text.h"
#include "canvas/rectangle.h"
#include "canvas/line.h"

using namespace std;
using namespace ArdourCanvas;

Flag::Flag (Canvas* canvas, Distance height, Gtkmm2ext::Color outline_color, Gtkmm2ext::Color fill_color, Duple position, bool invert)
	: Container (canvas)
	, _outline_color (outline_color)
	, _fill_color (fill_color)
	, _invert (invert)
{
	setup (height, position);
}

Flag::Flag (Item* parent, Distance height, Gtkmm2ext::Color outline_color, Gtkmm2ext::Color fill_color, Duple position, bool invert)
	: Container (parent)
	, _outline_color (outline_color)
	, _fill_color (fill_color)
	, _invert (invert)
{
	setup (height, position);
}

void
Flag::setup (Distance height, Duple position)
{
	_text = new Text (this);
	_text->set_alignment (Pango::ALIGN_CENTER);
	_text->set_color (_outline_color);

	_line = new Line (this);
	_line->set_outline_color (_outline_color);

	_rectangle = new Rectangle (this);
	_rectangle->set_outline_color (_outline_color);
	_rectangle->set_fill_color (_fill_color);

	_text->raise_to_top ();

	set_height (height);
	set_position (position);
}

void
Flag::set_font_description (Pango::FontDescription font_description)
{
	_text->set_font_description (font_description);
}

void
Flag::set_text (string const & text)
{
	if (text == _text->text()) {
		return;
	} else if (text.empty ()) {
		_text->set (" ");
	} else {
		_text->set (text);
	}

	Rect bbox = _text->bounding_box ();
	assert (bbox);

	Duple flag_size (bbox.width() + 10, bbox.height() + 4);

	if (_invert) {
		const Distance h = fabs(_line->y1() - _line->y0());
		_text->set_position (Duple (5, h - flag_size.y + 2));
		_rectangle->set (Rect (0, h - flag_size.y, flag_size.x, h));
	} else {
		_text->set_position (Duple (5, 2));
		_rectangle->set (Rect (0, 0, flag_size.x, flag_size.y));
	}
}

void
Flag::set_height (Distance h)
{
	_line->set (Duple (0, 0), Duple (0, h));

	if (_invert) {
		Rect bbox = _text->bounding_box ();
		if (bbox) {
			Duple flag_size (bbox.width() + 10, bbox.height() + 4);
			_rectangle->set (Rect (0, h - flag_size.y, flag_size.x, h));
			_text->set_position (Duple (5, h - flag_size.y + 2));
		}
	}
}

bool
Flag::covers (Duple const & point) const
{
	if (_rectangle) {
		return _rectangle->covers (point);
	}

	return false;
}

double
Flag::width () const
{
	Rect bbox = _text->bounding_box ();
	assert (bbox);

	return bbox.width() + 10;
}
