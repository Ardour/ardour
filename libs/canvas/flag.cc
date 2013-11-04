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

#include "canvas/flag.h"
#include "canvas/text.h"
#include "canvas/rectangle.h"
#include "canvas/line.h"

using namespace std;
using namespace ArdourCanvas;

Flag::Flag (Group* parent, Distance height, Color outline_color, Color fill_color, Duple position)
	: Group (parent)
	, _height (height)
	, _outline_color (outline_color)
	, _fill_color (fill_color)
{
	_text = new Text (this);
	_text->set_alignment (Pango::ALIGN_CENTER);
	_text->set_color (_outline_color);

	_line = new Line (this);
	_line->set_outline_color (_outline_color);
	set_height (_height);

	_rectangle = new Rectangle (this);
	_rectangle->set_outline_color (_outline_color);
	_rectangle->set_fill_color (_fill_color);

	_text->raise_to_top ();

	set_position (position);
}

void
Flag::set_text (string const & text)
{
	_text->set (text);
	boost::optional<Rect> bbox = _text->bounding_box ();
	assert (bbox);

	Duple flag_size (bbox.get().width() + 10, bbox.get().height() + 3);
	
	_text->set_position (flag_size / 2);
	_rectangle->set (Rect (0, 0, flag_size.x, flag_size.y));
}

void
Flag::set_height (Distance)
{
	_line->set (Duple (0, 0), Duple (0, _height));
}

bool
Flag::covers (Duple const & point) const
{
	if (_rectangle) {
		return _rectangle->covers (point);
	} 

	return false;
}
