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

#include "gtkmm2ext/pane.h"

#include "i18n.h"

using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace std;

Pane::Pane (bool h)
	: horizontal (h)
	, dragging (false)
	, divider_width (5)
{
	set_has_window (false);
}

void
Pane::on_size_request (GtkRequisition* req)
{
	GtkRequisition largest;

	/* iterate over all children, get their size requests */

	/* horizontal pane is as high as its tallest child, but has no width
	 * requirement.
	 *
	 * vertical pane is as wide as its widest child, but has no height
	 * requirement.
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

		(*child)->size_request (r);

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
	return Gtk::Widget::get_type();
}

void
Pane::add_divider ()
{
	Divider* d = new Divider;
	d->signal_button_press_event().connect (sigc::bind (sigc::mem_fun (*this, &Pane::handle_press_event), d), false);
	d->signal_button_release_event().connect (sigc::bind (sigc::mem_fun (*this, &Pane::handle_release_event), d), false);
	d->signal_motion_notify_event().connect (sigc::bind (sigc::mem_fun (*this, &Pane::handle_motion_event), d), false);
	d->set_parent (*this);
	d->show ();
	d->fract = 0.5;
	dividers.push_back (d);
}

void
Pane::on_add (Widget* w)
{
	children.push_back (w);

	w->set_parent (*this);

	while (dividers.size() < (children.size() - 1)) {
		add_divider ();
	}
}

void
Pane::on_remove (Widget* w)
{
	w->unparent ();
	children.remove (w);
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
	Children::size_type n = 0;
        int remaining;
        int xpos = alloc.get_x();
        int ypos = alloc.get_y();
        float fract;

        if (children.empty()) {
	        return;
        }

        if (children.size() == 1) {
	        children.front()->size_allocate (alloc);
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

        for (child = children.begin(), div = dividers.begin(); child != children.end(); ++n) {

	        Gtk::Allocation child_alloc;
	        next = child;
	        ++next;

	        child_alloc.set_x (xpos);
	        child_alloc.set_y (ypos);

	        if (n >= dividers.size()) {
		        /* the next child gets all the remaining space */
		        fract = 1.0;
	        } else {
		        /* the next child gets the fraction of the remaining space given by the divider that follows it */
		        fract = dividers[n]->fract;
	        }

	        Gtk::Requisition cr;
	        (*child)->size_request (cr);

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

	        (*child)->size_allocate (child_alloc);
	        ++child;

	        if (child == children.end()) {
		        /* done, no more children, no need for a divider */
		        break;
	        }

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
	        ++div;
        }
}

bool
Pane::on_expose_event (GdkEventExpose* ev)
{
	Children::size_type n = 0;

	for (Children::iterator child = children.begin(); child != children.end(); ++child, ++n) {

		propagate_expose (**child, ev);

		if (n < dividers.size()) {
			propagate_expose (*dividers[n], ev);
		}
        }

        return true;
}

bool
Pane::handle_press_event (GdkEventButton* ev, Divider*)
{
	dragging = true;
	return false;
}

bool
Pane::handle_release_event (GdkEventButton* ev, Divider*)
{
	children.front()->queue_resize ();
	dragging = false;
	return false;
}

bool
Pane::handle_motion_event (GdkEventMotion* ev, Divider* d)
{
	if (!dragging) {
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

	while (dividers.size() <= div) {
		add_divider ();
	}

	if (fract != dividers[div]->fract) {
		dividers[div]->fract = fract;
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
	if (div >= dividers.size()) {
		return -1;
	}

	return dividers[div]->fract;
}

void
Pane::forall_vfunc (gboolean include_internals, GtkCallback callback, gpointer callback_data)
{
	for (Children::iterator w = children.begin(); w != children.end(); ++w) {
		callback ((*w)->gobj(), callback_data);
	}

	if (include_internals) {
		for (Dividers::iterator d = dividers.begin(); d != dividers.end(); ++d) {
			callback (GTK_WIDGET((*d)->gobj()), callback_data);
		}
	}
}

Pane::Divider::Divider ()
	: fract (0.0)
{
	set_events (Gdk::EventMask (Gdk::BUTTON_PRESS|Gdk::BUTTON_RELEASE|Gdk::MOTION_NOTIFY));
}

bool
Pane::Divider::on_expose_event (GdkEventExpose* ev)
{
	Widget::on_expose_event (ev);

	Cairo::RefPtr<Cairo::Context> draw_context = get_window()->create_cairo_context ();
	draw_context->rectangle (ev->area.x, ev->area.y, ev->area.width, ev->area.height);
        draw_context->clip_preserve ();
        draw_context->set_source_rgba (1.0, 0.0, 0.0, 0.6);
        draw_context->fill ();
	return true;
}
