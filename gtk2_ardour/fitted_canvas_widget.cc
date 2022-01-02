/*
    Copyright (C) 2019 Paul Davis
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

#include <algorithm>
#include <cairomm/context.h>

#include <pangomm/layout.h>

#include "pbd/compose.h"

#include "gtkmm2ext/keyboard.h"
#include "gtkmm2ext/utils.h"

#include "canvas/canvas.h"
#include "canvas/debug.h"
#include "canvas/utils.h"

#include "actions.h"
#include "gui_thread.h"
#include "timers.h"
#include "utils.h"

#include "fitted_canvas_widget.h"

using namespace std;
using namespace ArdourCanvas;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ARDOUR_UI_UTILS;

/** a gtk widget with fixed-size semantics */
FittedCanvasWidget::FittedCanvasWidget (float w, float h, bool follow_scale)
{
	_nominal_width  = w;
	_nominal_height = h;
	_follow_scale   = follow_scale;

	/* our rendering speed suffers if we re-render knobs simply because
	 * they are in-between 2 meters that got invalidated (for example)
	 */
	//	set_single_exposure(false);
#ifdef __APPLE__
	//	use_intermediate_surface (false);
#endif
}

void
FittedCanvasWidget::on_size_request (Gtk::Requisition* req)
{
	const double scale = _follow_scale ? UIConfiguration::instance ().get_ui_scale () : 1;
	if (_nominal_width > 0) {
		req->width = nominal_width() * scale;
	}
	if (_nominal_height > 0) {
		req->height = nominal_height() * scale;
	}
}

void
FittedCanvasWidget::on_size_allocate (Gtk::Allocation& alloc)
{
	GtkCanvas::on_size_allocate (alloc);
	repeat_size_allocation ();
}

void
FittedCanvasWidget::repeat_size_allocation ()
{
	if (_root.items ().empty ()) {
		return;
	}

	Gtk::Allocation a = get_allocation ();
	_root.items ().front ()->size_allocate (ArdourCanvas::Rect (0, 0, a.get_width (), a.get_height ()));
}
