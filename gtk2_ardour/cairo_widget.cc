/*
    Copyright (C) 2009 Paul Davis

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

#include "cairo_widget.h"
#include "gui_thread.h"

CairoWidget::CairoWidget ()
	: _width (1)
	, _height (1)
	, _active_state (CairoWidget::ActiveState (0))
	, _visual_state (CairoWidget::VisualState (0))
	, _pixmap (0)
	, _dirty (true)
{

}

CairoWidget::~CairoWidget ()
{
	if (_pixmap) {
		g_object_unref (_pixmap);
	}
}

bool
CairoWidget::on_expose_event (GdkEventExpose *event)
{
	Gdk::Rectangle const exposure (
		event->area.x, event->area.y, event->area.width, event->area.height
		);

	Gdk::Rectangle r = exposure;
	Gdk::Rectangle content (0, 0, _width, _height);
	bool intersects;
	r.intersect (content, intersects);

	if (intersects) {

		GdkDrawable* drawable = get_window()->gobj ();

		if (_dirty) {

			if (_pixmap) {
				g_object_unref (_pixmap);
			}

			_pixmap = gdk_pixmap_new (drawable, _width, _height, -1);

			cairo_t* cr = gdk_cairo_create (_pixmap);
			render (cr);
			cairo_destroy (cr);

			_dirty = false;
		}

		gdk_draw_drawable (
			drawable,
			get_style()->get_fg_gc (Gtk::STATE_NORMAL)->gobj(),
			_pixmap,
 			r.get_x(),
 			r.get_y(),
 			r.get_x(),
 			r.get_y(),
 			r.get_width(),
 			r.get_height()
			);
	}

	return true;
}

/** Marks the widget as dirty, so that render () will be called on
 *  the next GTK expose event.
 */

void
CairoWidget::set_dirty ()
{
	ENSURE_GUI_THREAD (*this, &CairoWidget::set_dirty)

	_dirty = true;
	queue_draw ();
}

/** Handle a size allocation.
 *  @param alloc GTK allocation.
 */
void
CairoWidget::on_size_allocate (Gtk::Allocation& alloc)
{
	Gtk::EventBox::on_size_allocate (alloc);

	_width = alloc.get_width ();
	_height = alloc.get_height ();

	set_dirty ();
}

Gdk::Color
CairoWidget::get_parent_bg ()
{
        Widget* parent;

	parent = get_parent ();

        while (parent && !parent->get_has_window()) {
                parent = parent->get_parent();
        }

        if (parent && parent->get_has_window()) {
		return parent->get_style ()->get_bg (parent->get_state());
        } 

	return get_style ()->get_bg (get_state());
}

void
CairoWidget::set_active_state (CairoWidget::ActiveState s)
{
	if (_active_state != s) {
		_active_state = s;
		StateChanged ();
	}
}

void
CairoWidget::set_visual_state (CairoWidget::VisualState s)
{
	if (_visual_state != s) {
		_visual_state = s;
		StateChanged ();
	}
}
