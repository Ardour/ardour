/*
    Copyright (C) 2021 Paul Davis
    Author: Ben Loftis <ben@harrisonconsoles.com>

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

#ifndef __fitted_canvas_widget__
#define __fitted_canvas_widget__

#include <string>
#include <vector>

#include <pangomm/fontdescription.h>

#include <canvas/canvas.h>
#include <canvas/container.h>
#include <canvas/item.h>
#include <canvas/fill.h>
#include <canvas/outline.h>
#include <canvas/rectangle.h>

#include <gtkmm/layout.h>

#include <ardour/route.h>
#include <ardour/plugin.h>
#include <ardour/plugin_insert.h>
#include <ardour/automation_control.h>

#include "route_ui.h"

/* FittedCanvasWidget has these properties:
 *  it is provided a 'nominal size' on construction, which it will request of gtk
 *  if asked, will resize itself when the user gui/font scale changes
 *  it 'fits' the Item that was first attached to Root  (presumably the top-level widget or container)
 *    the fitted Item will be explicitly resized to fit when the canvas size is allocated
 *    the fitted Item may be a container; it should allocate child positions during size_allocate()
 *  */

class FittedCanvasWidget : public ArdourCanvas::GtkCanvas
{
public:
	/* per gtk convention you may use -1 for width OR height if you don't care about that dimension */
	FittedCanvasWidget(float nominal_width, float nominal_height, bool follow_scale=true);

	/* call if the root item's first child is changed, to force a size-allocate on it */
	void repeat_size_allocation ();

	void on_size_request (Gtk::Requisition* req);  //always returns the nominal size, regardless of children
	void on_size_allocate (Gtk::Allocation& alloc);

private:
	ArdourCanvas::Rect _allocation;
	float _nominal_width;
	float _nominal_height;
	bool  _follow_scale;
};

#endif //__fitted_canvas_widget__
