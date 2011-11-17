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
	, _active_state (Gtkmm2ext::ActiveState (0))
	, _visual_state (Gtkmm2ext::VisualState (0))
{

}

CairoWidget::~CairoWidget ()
{
}

bool
CairoWidget::on_expose_event (GdkEventExpose *ev)
{
	cairo_t* cr = gdk_cairo_create (get_window ()->gobj());
	cairo_rectangle (cr, ev->area.x, ev->area.y, ev->area.width, ev->area.height);
	cairo_clip (cr);
	render (cr);
	cairo_destroy (cr);

	return true;
}

/** Marks the widget as dirty, so that render () will be called on
 *  the next GTK expose event.
 */

void
CairoWidget::set_dirty ()
{
	ENSURE_GUI_THREAD (*this, &CairoWidget::set_dirty);
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
CairoWidget::set_active_state (Gtkmm2ext::ActiveState s)
{
	if (_active_state != s) {
		_active_state = s;
		StateChanged ();
	}
}

void
CairoWidget::set_visual_state (Gtkmm2ext::VisualState s)
{
	if (_visual_state != s) {
		_visual_state = s;
		StateChanged ();
	}
}

void
CairoWidget::set_active (bool yn)
{
	/* this is an API simplification for buttons
	   that only use the Active and Normal states.
	*/

	if (yn) {
		set_active_state (Gtkmm2ext::Active);
	} else {
		unset_active_state ();
	}
}

void
CairoWidget::on_state_changed (Gtk::StateType)
{
	/* this will catch GTK-level state changes from calls like
	   ::set_sensitive() 
	*/

	if (get_state() == Gtk::STATE_INSENSITIVE) {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() | Gtkmm2ext::Insensitive));
	} else {
		set_visual_state (Gtkmm2ext::VisualState (visual_state() & ~Gtkmm2ext::Insensitive));
	}

	queue_draw ();
}
