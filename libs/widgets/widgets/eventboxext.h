/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _WIDGETS_EVENTBOX_EXT_H_
#define _WIDGETS_EVENTBOX_EXT_H_

#include <gtkmm/eventbox.h>

#include "widgets/visibility.h"

namespace ArdourWidgets {

class LIBWIDGETS_API EventBoxExt : public Gtk::EventBox
{
public:
	EventBoxExt ();
	virtual ~EventBoxExt () {}

protected:
	/* gtk2's gtk/gtkcontainer.c does not
	 * unmap child widgets if the container has a window.
	 *
	 * (this is for historical reasons and optimization
	 * because back in the day each GdkWindow was backed by
	 * an actual windowing system surface).
	 *
	 * In Ardour's case an EventBox is used in the Editor's top-level
	 * and child-widgets (e.g. Canvas::GtkCanvas never receive an unmap.
	 *
	 * However, when switching Tabbable pages, we do need to hide overlays
	 * such as ArdourCanvasOpenGLView
	 *
	 */
	void on_unmap () {
		Gtk::EventBox::on_unmap();
		if (get_child ()) {
			get_child()->unmap();
		}
	}
};

} /* namespace */

#endif
