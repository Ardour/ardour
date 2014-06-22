/*
    Copyright (C) 2011-2014 Paul Davis
    Original Author: Carl Hetherington <cth@carlh.net>

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

#ifndef __CANVAS_LAYOUT_H__
#define __CANVAS_LAYOUT_H__

#include "canvas/container.h"

namespace ArdourCanvas
{

/** a Layout is a container item that renders all of its children at fixed
 * positions which they control.
 */
class LIBCANVAS_API Layout : public Container
{
public:
	Layout (Canvas *);
	Layout (Item *);
	Layout (Item*, Duple const & position);
	
	void render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;
};

}

#endif
