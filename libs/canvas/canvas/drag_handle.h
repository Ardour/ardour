/*
    Copyright (C) 2011-2014 Paul Davis

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

#ifndef __canvas_drag_handle_h__
#define __canvas_drag_handle_h__

#include "canvas/rectangle.h"
#include "canvas/circle.h"

namespace ArdourCanvas
{

class LIBCANVAS_API DragHandle : public Rectangle
{
  public:
	DragHandle (Group *, Rect const &, bool left_side);
	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;

  protected:
	bool _left_side;
};

}


#endif /* __canvas_drag_handle_h__ */
