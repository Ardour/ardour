/*
 * Copyright (C) 2009 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifndef __gtk2_ardour_canvas_noevent_pixbuf_h__
#define __gtk2_ardour_canvas_noevent_pixbuf_h__

#include <libgnomecanvasmm/pixbuf.h>

namespace Gnome { namespace Canvas {

class NoEventPixbuf : public Pixbuf
{
  public:
    NoEventPixbuf(Group& parent) : Pixbuf (parent) {}

    double point_vfunc(double, double, int, int, GnomeCanvasItem**) {
	    /* return a huge value to tell the canvas that we're never the item for an event */
	    return 9999999999999.0;
    }
};

} } /* namespaces */

#endif /* __gtk2_ardour_canvas_noevent_pixbuf_h__ */
