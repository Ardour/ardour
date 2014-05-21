/*
    Copyright (C) 2014 Paul Davis

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

#include <iostream>
#include "canvas/scroll_group.h"

using namespace std;
using namespace ArdourCanvas;

ScrollGroup::ScrollGroup (Group* parent, ScrollSensitivity s)
	: Group (parent)
	, _scroll_sensitivity (s)	
{
}

ScrollGroup::ScrollGroup (Group* parent, Duple position, ScrollSensitivity s)
	: Group (parent, position)
	, _scroll_sensitivity (s)
{
}

void
ScrollGroup::scroll_to (Duple const& d)
{
	Duple base_pos (position().translate (_scroll_offset));

	if (_scroll_sensitivity & ScrollsHorizontally) {
		base_pos.x -= d.x;
		_scroll_offset.x = d.x;
	} 

	if (_scroll_sensitivity & ScrollsVertically) {
		base_pos.y -= d.y;
		_scroll_offset.y = d.y;
	}

	set_position (base_pos);
}

Duple
ScrollGroup::canvas_position() const
{
	/* return the normal "base" position of this item
	   rather its position as affected by any scroll
	   offset.
	*/
	return _position.translate (_scroll_offset);
}
