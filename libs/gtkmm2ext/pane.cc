/*
    Copyright (C) 2016 Paul Davis

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

#include <gdkmm/cursor.h>
#include "gtkmm2ext/pane.h"

#include "i18n.h"

using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

Pane::Pane (bool h)
	: horizontal (h)
	, did_move (false)
	, divider_width (2)
{
	using namespace Gdk;

	set_name ("Pane");
	set_has_window (false);

	if (horizontal) {
		drag_cursor = Cursor (SB_H_DOUBLE_ARROW);
	} else {
		drag_cursor = Cursor (SB_H_DOUBLE_ARROW);
	}
}

void
Pane::set_child_minsize (Gtk::Widget const& w, int32_t minsize)
{
	for (Children::iterator c = children.begin(); c != children.end(); ++c) {
		if (c->w == &w) {
			c->minsize = minsize;
			break;
		}
	}
}

void
Pane::set_drag_cursor (Gdk::Cursor c)
{
	drag_cursor = c;
}

void
Pane::on_size_request (GtkRequisition* req)
{
	GtkRequisition largest;

	/* iterate over all children, get their size requests */

	/* horizontal pane is as high as its tallest child, including the dividers.
	 * Its width is the sum of the children plus the dividers.
	 *
	 * vertical pane is as wide as its widest child, including the dividers.
	 * Its height is the sum of the children plus the dividers.
	 */

	if (horizontal) {
		largest.width = (children.size()  - 1) * divider_width;
		largest.height = 0;
	} else {
		largest.height = (children.size() - 1) * divider_width;
		largest.width = 0;
	}

	for (Children::iterator child = children.begin(); child != children.end(); ++child) {
		GtkRequisition r;

		child->w->size_request (r);

		if (horizontal) {
			largest.height = max (largest.height, r.height);
			largest.width += r.width;
		} else {
			largest.width = max (largest.width, r.width);
			largest.height += r.height;
		}
	}

	*req = largest;
}

GType
Pane::child_type_vfunc() const
{
	/* We accept any number of any types of widgets */
	return Gtk::Widget::get_type();
}

void
Pane::add_divider ()
{
	Divider* d = new Divider;
	d->signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &Pane::handle_press_event), d), false);
	d->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pane::handle_release_event), d), false);
	d->signal_motion_notify_event().connect (sigc::bind (sigc::mem_fun (*this, &Pane::handle_motion_event), d), false);
	d->signal_enter_notify_event().connect (sigc::bind (sigc::mem_fun (*this, &Pane::handle_enter_event), d), false);
	d->signal_leave_notify_event().connect (sigc::bind (sigc::mem_fun (*this, &Pane::handle_leave_event), d), false);
	d->set_parent (*this);
	d->show ();
	d->fract = 0.5;
	dividers.push_back (d);
}

void
Pane::handle_child_visibility ()
{
	reallocate (get_allocation());
}

void
Pane::on_add (Widget* w)
{
	children.push_back (Child (w, 0));

	w->set_parent (*this);

	w->signal_show().connect (sigc::mem_fun (*this, &Pane::handle_child_visibility));
	w->signal_hide().connect (sigc::mem_fun (*this, &Pane::handle_child_visibility));

	while (dividers.size() < (children.size() - 1)) {
		add_divider ();
	}
}

void
Pane::on_remove (Widget* w)
{
	for (Children::iterator c = children.begin(); c != children.end(); ++c) {
		if (c->w == w) {
			children.erase (c);
			break;
		}
	}

	w->unparent ();
}

void
Pane::on_size_allocate (Gtk::Allocation& alloc)
{
	reallocate (alloc);
	Container::on_size_allocate (alloc);
}

void
Pane::reallocate (Gtk::Allocation const & alloc)
{
        int remaining;
        int xpos = alloc.get_x();
        int ypos = alloc.get_y();
        float fract;

        if (children.empty()) {
	        return;
        }

        if (children.size() == 1) {
	        /* only child gets the full allocation */
	        children.front().w->size_allocate (alloc);
	        return;
        }

        if (horizontal) {
	        remaining = alloc.get_width ();
        } else {
	        remaining = alloc.get_height ();
        }

        Children::iterator child;
        Children::iterator next;
        Dividers::iterator div;

        child = children.begin();

        /* skip initial hidden children */

        while (child != children.end()) {
	        if (child->w->is_visible()) {
		        break;
	        }
	        ++child;
        }

        for (div = dividers.begin(); child != children.end(); ) {

	        Gtk::Allocation child_alloc;

	        next = child;

	        /* Move on to next *visible* child */

	        while (++next != children.end()) {
		        if (next->w->is_visible()) {
			        break;
		        }
	        }

	        child_alloc.set_x (xpos);
	        child_alloc.set_y (ypos);

	        if (next == children.end()) {
		        /* last child gets all the remaining space */
		        fract = 1.0;
	        } else {
		        /* child gets the fraction of the remaining space given by the divider that follows it */
		        fract = (*div)->fract;
	        }

	        Gtk::Requisition cr;
	        child->w->size_request (cr);

	        if (horizontal) {
		        child_alloc.set_width ((gint) floor (remaining * fract));
		        child_alloc.set_height (alloc.get_height());
		        remaining = max (0, (remaining - child_alloc.get_width()));
		        xpos += child_alloc.get_width();
	        } else {
		        child_alloc.set_width (alloc.get_width());
		        child_alloc.set_height ((gint) floor (remaining * fract));
		        remaining = max (0, (remaining - child_alloc.get_height()));
		        ypos += child_alloc.get_height ();
	        }

	        if (child->minsize) {
		        if (horizontal) {
			        child_alloc.set_width (max (child_alloc.get_width(), child->minsize));
		        } else {
			        child_alloc.set_height (max (child_alloc.get_height(), child->minsize));
		        }
	        }

	        child->w->size_allocate (child_alloc);

	        if (next == children.end()) {
		        /* done, no more children, no need for a divider */
		        break;
	        }

	        child = next;

	        /* add a divider between children */

	        Gtk::Allocation divider_allocation;

	        divider_allocation.set_x (xpos);
	        divider_allocation.set_y (ypos);

	        if (horizontal) {
		        divider_allocation.set_width (divider_width);
		        divider_allocation.set_height (alloc.get_height());
		        remaining = max (0, remaining - divider_width);
		        xpos += divider_width;
	        } else {
		        divider_allocation.set_width (alloc.get_width());
		        divider_allocation.set_height (divider_width);
		        remaining = max (0, remaining - divider_width);
		        ypos += divider_width;
	        }

	        (*div)->size_allocate (divider_allocation);
	        (*div)->show ();
	        ++div;
        }

        /* hide all remaining dividers */

        while (div != dividers.end()) {
	        (*div)->hide ();
	        ++div;
        }
}

bool
Pane::on_expose_event (GdkEventExpose* ev)
{
	Children::iterator child;
	Dividers::iterator div;

	for (child = children.begin(), div = dividers.begin(); child != children.end(); ++child, ++div) {

		if (child->w->is_visible()) {
			propagate_expose (*(child->w), ev);
		}

		if ((div != dividers.end()) && (*div)->is_visible()) {
			propagate_expose (**div, ev);
		}
        }

        return true;
}

bool
Pane::handle_press_event (GdkEventButton* ev, Divider* d)
{
	d->dragging = true;
	d->queue_draw ();

	return false;
}

bool
Pane::handle_release_event (GdkEventButton* ev, Divider* d)
{
	d->dragging = false;

	if (did_move) {
		children.front().w->queue_resize ();
		did_move = false;
	}

	return false;
}

bool
Pane::handle_motion_event (GdkEventMotion* ev, Divider* d)
{
	did_move = true;

	if (!d->dragging) {
		return true;
	}

	/* determine new position for handle */

	float new_fract;
	int px, py;

	d->translate_coordinates (*this, ev->x, ev->y, px, py);

	Dividers::iterator prev = dividers.end();

	for (Dividers::iterator di = dividers.begin(); di != dividers.end(); ++di) {
		if (*di == d) {
			break;
		}
		prev = di;
	}

	int space_remaining;
	int prev_edge;

	if (horizontal) {
		if (prev != dividers.end()) {
			prev_edge = (*prev)->get_allocation().get_x() + (*prev)->get_allocation().get_width();
		} else {
			prev_edge = 0;
		}
		space_remaining = get_allocation().get_width() - prev_edge;
		new_fract = (float) (px - prev_edge) / space_remaining;
	} else {
		if (prev != dividers.end()) {
			prev_edge = (*prev)->get_allocation().get_y() + (*prev)->get_allocation().get_height();
		} else {
			prev_edge = 0;
		}
		space_remaining = get_allocation().get_height() - prev_edge;
		new_fract = (float) (py - prev_edge) / space_remaining;
	}

	new_fract = min (1.0f, max (0.0f, new_fract));

	if (new_fract != d->fract) {
		d->fract = new_fract;
		reallocate (get_allocation ());
		queue_draw ();
	}

	return true;
}

void
Pane::set_divider (Dividers::size_type div, float fract)
{
	bool redraw = false;

	Dividers::iterator d = dividers.begin();

	while (div--) {
		++d;
		if (d == dividers.end()) {
			/* caller is trying to set divider that does not exist
			 * yet.
			 */
			return;
		}
	}

	if (fract != (*d)->fract) {
		(*d)->fract = fract;
		redraw = true;
	}

	if (redraw) {
		/* our size hasn't changed, but our internal allocations have */
		reallocate (get_allocation());
		queue_draw ();
	}
}

float
Pane::get_divider (Dividers::size_type div)
{
	Dividers::iterator d = dividers.begin();

	while (div--) {
		++d;
		if (d == dividers.end()) {
			/* caller is trying to set divider that does not exist
			 * yet.
			 */
			return -1.0f;
		}
	}

	return (*d)->fract;
}

void
Pane::forall_vfunc (gboolean include_internals, GtkCallback callback, gpointer callback_data)
{
	/* since the callback could modify the child list(s), make sure we keep
	 * the iterators safe;
	 */

	for (Children::iterator c = children.begin(); c != children.end(); ) {
		Children::iterator next = c;
		++next;
		callback (c->w->gobj(), callback_data);
		c = next;
	}

	if (include_internals) {
		for (Dividers::iterator d = dividers.begin(); d != dividers.end(); ) {
			Dividers::iterator next = d;
			++next;
			callback (GTK_WIDGET((*d)->gobj()), callback_data);
			d = next;
		}
	}
}

Pane::Divider::Divider ()
	: fract (0.0)
	, dragging (false)
{
	set_events (Gdk::EventMask (Gdk::BUTTON_PRESS|
	                            Gdk::BUTTON_RELEASE|
	                            Gdk::MOTION_NOTIFY|
	                            Gdk::ENTER_NOTIFY|
	                            Gdk::LEAVE_NOTIFY));
}

bool
Pane::Divider::on_expose_event (GdkEventExpose* ev)
{
	Gdk::Color c = (dragging ? get_style()->get_fg (Gtk::STATE_ACTIVE) :
	                get_style()->get_fg (get_state()));

	Cairo::RefPtr<Cairo::Context> draw_context = get_window()->create_cairo_context ();
	draw_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
        draw_context->clip_preserve ();
        draw_context->set_source_rgba (c.get_red_p(), c.get_green_p(), c.get_blue_p(), 1.0);
        draw_context->fill ();

        return true;
}

bool
Pane::handle_enter_event (GdkEventCrossing*, Divider* d)
{
	d->get_window()->set_cursor (drag_cursor);
	d->set_state (Gtk::STATE_SELECTED);
	return true;
}

bool
Pane::handle_leave_event (GdkEventCrossing*, Divider* d)
{
	d->get_window()->set_cursor ();
	d->set_state (Gtk::STATE_NORMAL);
	return true;
}
