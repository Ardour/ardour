/*
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2018 Ben Loftis <ben@harrisonconsoles.com>
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

#include <assert.h>
#include <gdkmm/cursor.h>

#include "widgets/pane.h"

#include "pbd/i18n.h"

using namespace std;
using namespace Gtk;
using namespace PBD;
using namespace ArdourWidgets;

Pane::Pane (bool h)
	: horizontal (h)
	, did_move (false)
	, divider_width (5)
	, check_fract (false)
{
	using namespace Gdk;

	set_name ("Pane");
	set_has_window (false);

	if (horizontal) {
		drag_cursor = Cursor (SB_H_DOUBLE_ARROW);
	} else {
		drag_cursor = Cursor (SB_V_DOUBLE_ARROW);
	}
}

Pane::~Pane ()
{
	for (Children::iterator c = children.begin(); c != children.end(); ++c) {
		(*c)->show_con.disconnect ();
		(*c)->hide_con.disconnect ();
		if ((*c)->w) {
			(*c)->w->remove_destroy_notify_callback ((*c).get());
			(*c)->w->unparent ();
		}
	}
	children.clear ();
}

void
Pane::set_child_minsize (Gtk::Widget const& w, int32_t minsize)
{
	for (Children::iterator c = children.begin(); c != children.end(); ++c) {
		if ((*c)->w == &w) {
			(*c)->minsize = minsize;
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

	for (Children::iterator c = children.begin(); c != children.end(); ++c) {
		GtkRequisition r;

		if (!(*c)->w->is_visible ()) {
			continue;
		}

		(*c)->w->size_request (r);

		if (horizontal) {
			largest.height = max (largest.height, r.height);
			if ((*c)->minsize) {
				largest.width += (*c)->minsize;
			} else {
				largest.width += r.width;
			}
		} else {
			largest.width = max (largest.width, r.width);
			if ((*c)->minsize) {
				largest.height += (*c)->minsize;
			} else {
				largest.height += r.height;
			}
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
	d->set_name (X_("Divider"));
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
	children.push_back (boost::shared_ptr<Child> (new Child (this, w, 0)));
	Child* kid = children.back ().get();

	w->set_parent (*this);
	/* Gtkmm 2.4 does not correctly arrange for ::on_remove() to be called
	   for custom containers that derive from Gtk::Container. So ... we need
	   to ensure that we hear about child destruction ourselves.
	*/
	w->add_destroy_notify_callback (kid, &Pane::notify_child_destroyed);

	kid->show_con = w->signal_show().connect (sigc::mem_fun (*this, &Pane::handle_child_visibility));
	kid->hide_con = w->signal_hide().connect (sigc::mem_fun (*this, &Pane::handle_child_visibility));

	while (dividers.size() < (children.size() - 1)) {
		add_divider ();
	}
}

void*
Pane::notify_child_destroyed (void* data)
{
	Child* child = reinterpret_cast<Child*> (data);
	return child->pane->child_destroyed (child->w);
}

void*
Pane::child_destroyed (Gtk::Widget* w)
{
	for (Children::iterator c = children.begin(); c != children.end(); ++c) {
		if ((*c)->w == w) {
			(*c)->show_con.disconnect ();
			(*c)->hide_con.disconnect ();
			(*c)->w = NULL; // mark invalid
			children.erase (c);
			break;
		}
	}
	return 0;
}

void
Pane::on_remove (Widget* w)
{
	for (Children::iterator c = children.begin(); c != children.end(); ++c) {
		if ((*c)->w == w) {
			(*c)->show_con.disconnect ();
			(*c)->hide_con.disconnect ();
			w->remove_destroy_notify_callback ((*c).get());
			w->unparent ();
			(*c)->w = NULL; // mark invalid
			children.erase (c);
			break;
		}
	}
}

void
Pane::on_size_allocate (Gtk::Allocation& alloc)
{
	reallocate (alloc);
	Container::on_size_allocate (alloc);

	/* minumum pane size constraints */
	Dividers::size_type div = 0;
	for (Dividers::const_iterator d = dividers.begin(); d != dividers.end(); ++d, ++div) {
		// XXX skip dividers that were just hidden in reallocate()
		Pane::set_divider (div, (*d)->fract);
	}
	// TODO this needs tweaking for panes with > 2 children
	// if a child grows, re-check the ones before it.
	assert (dividers.size () < 3);
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
		if (children.front()->w->is_visible ()) {
			children.front()->w->size_allocate (alloc);
		}
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
		if ((*child)->w->is_visible()) {
			break;
		}
		++child;
	}

	for (div = dividers.begin(); child != children.end(); ) {

		Gtk::Allocation child_alloc;

		next = child;

		/* Move on to next *visible* child */

		while (++next != children.end()) {
			if ((*next)->w->is_visible()) {
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
		(*child)->w->size_request (cr);

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

		if ((*child)->minsize) {
			if (horizontal) {
				child_alloc.set_width (max (child_alloc.get_width(), (*child)->minsize));
			} else {
				child_alloc.set_height (max (child_alloc.get_height(), (*child)->minsize));
			}
		} else if (!check_fract && (*child)->w->is_visible ()) {
			if (horizontal) {
				child_alloc.set_width (max (child_alloc.get_width(), cr.width));
			} else {
				child_alloc.set_height (max (child_alloc.get_height(), cr.height));
			}
		}

		if ((*child)->w->is_visible ()) {
			(*child)->w->size_allocate (child_alloc);
		}

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

	for (child = children.begin(), div = dividers.begin(); child != children.end(); ++child) {

		if ((*child)->w->is_visible()) {
			propagate_expose (*((*child)->w), ev);
		}

		if (div != dividers.end()) {
			if ((*div)->is_visible()) {
				propagate_expose (**div, ev);
			}
			++div;
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

	if (did_move && !children.empty()) {
		children.front()->w->queue_resize ();
		did_move = false;
	}

	return false;
}
void
Pane::set_check_divider_position (bool yn)
{
	check_fract = yn;
}

float
Pane::constrain_fract (Dividers::size_type div, float fract)
{
	if (get_allocation().get_width() == 1 && get_allocation().get_height() == 1) {
		/* space not * allocated - * divider being set from startup code. Let it pass,
		 * since our goal is mostly to catch drags to a position that will interfere with window
		 * resizing.
		 */
		return fract;
	}

	if (children.size () <= div + 1) { return fract; } // XXX remove once hidden divs are skipped
	assert(children.size () > div + 1);

	const float size = horizontal ? get_allocation().get_width() : get_allocation().get_height();

	// TODO: optimize: cache in Pane::on_size_request
	Gtk::Requisition prev_req(children.at (div)->w->size_request ());
	Gtk::Requisition next_req(children.at (div + 1)->w->size_request ());
	float prev = (horizontal ? prev_req.width : prev_req.height);
	float next = (horizontal ? next_req.width : next_req.height);

	if (children.at (div)->minsize) {
		prev = children.at (div)->minsize;
	}
	if (children.at (div + 1)->minsize) {
		next = children.at (div + 1)->minsize;
	}

	if (size * fract < prev) {
		return prev / size;
	}
	if (size * (1.f - fract) < next) {
		return 1.f - next / size;
	}

	if (!check_fract) {
		return fract;
	}

#ifdef __APPLE__

	/* On Quartz, if the pane handle (divider) gets to
	   be adjacent to the window edge, you can no longer grab it:
	   any attempt to do so is interpreted by the Quartz window
	   manager ("Finder") as a resize drag on the window edge.
	*/


	if (horizontal) {
		if (div == dividers.size() - 1) {
			if (get_allocation().get_width() * (1.0 - fract) < (divider_width*2)) {
				/* too close to right edge */
				return 1.f - (divider_width * 2.f) / (float) get_allocation().get_width();
			}
		}

		if (div == 0) {
			if (get_allocation().get_width() * fract < (divider_width*2)) {
				/* too close to left edge */
				return (divider_width * 2.f) / (float)get_allocation().get_width();
			}
		}
	} else {
		if (div == dividers.size() - 1) {
			if (get_allocation().get_height() * (1.0 - fract) < (divider_width*2)) {
				/* too close to bottom */
				return 1.f - (divider_width * 2.f) / (float) get_allocation().get_height();
			}
		}

		if (div == 0) {
			if (get_allocation().get_height() * fract < (divider_width*2)) {
				/* too close to top */
				return (divider_width * 2.f) / (float) get_allocation().get_height();
			}
		}
	}
#endif
	return fract;
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
	Dividers::size_type div = 0;

	for (Dividers::iterator di = dividers.begin(); di != dividers.end(); ++di, ++div) {
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
	new_fract = constrain_fract (div, new_fract);
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
	Dividers::iterator d = dividers.begin();

	for (d = dividers.begin(); d != dividers.end() && div != 0; ++d, --div) {
		/* relax */
	}

	if (d == dividers.end()) {
		/* caller is trying to set divider that does not exist
		 * yet.
		 */
		return;
	}

	fract = max (0.0f, min (1.0f, fract));
	fract = constrain_fract (div, fract);
	fract = max (0.0f, min (1.0f, fract));

	if (fract != (*d)->fract) {
		(*d)->fract = fract;
		/* our size hasn't changed, but our internal allocations have */
		reallocate (get_allocation());
		queue_draw ();
	}
}

float
Pane::get_divider (Dividers::size_type div)
{
	Dividers::iterator d = dividers.begin();

	for (d = dividers.begin(); d != dividers.end() && div != 0; ++d, --div) {
		/* relax */
	}

	if (d == dividers.end()) {
		/* caller is trying to set divider that does not exist
		 * yet.
		 */
		return -1.0f;
	}

	return (*d)->fract;
}

void
Pane::forall_vfunc (gboolean include_internals, GtkCallback callback, gpointer callback_data)
{
	/* since the callback could modify the child list(s), make sure we keep
	 * the iterators safe;
	 */
	Children kids (children);
	for (Children::const_iterator c = kids.begin(); c != kids.end(); ++c) {
		if ((*c)->w) {
			callback ((*c)->w->gobj(), callback_data);
		}
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
	Gdk::Color c = (dragging ? get_style()->get_bg (Gtk::STATE_ACTIVE) :
			get_style()->get_bg (get_state()));

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
	d->set_state (Gtk::STATE_ACTIVE);
	return true;
}

bool
Pane::handle_leave_event (GdkEventCrossing*, Divider* d)
{
	d->get_window()->set_cursor ();
	d->set_state (Gtk::STATE_NORMAL);
	d->queue_draw ();
	return true;
}
