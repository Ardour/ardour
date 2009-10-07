/*
    Copyright (C) 2007 Paul Davis 
    Author: Dave Robillard

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

#ifndef __ardour_diamond_h__
#define __ardour_diamond_h__

#include <libgnomecanvasmm/polygon.h>
#include "canvas-note-event.h"
#include "canvas.h"

namespace Gnome {
namespace Canvas {


class Diamond : public Polygon 
{
  public:
	Diamond(Group& group, double height);
	~Diamond ();

	void move_to (double x, double y);
	void move_by (double dx, double dy);
	void set_height(double height);

  protected:
	double _x;
	double _y;
	double _h;
	GnomeCanvasPoints* points;
};


} // namespace Canvas
} // namespace Gnome

#endif /* __ardour_diamond_h__ */
