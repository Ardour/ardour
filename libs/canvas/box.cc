/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016-2017 Paul Davis <paul@linuxaudiosystems.com>
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

#include <algorithm>

#include "pbd/unwind.h"

#include "canvas/box.h"
#include "canvas/rectangle.h"

using namespace ArdourCanvas;

Box::Box (Canvas* canvas, Orientation o)
	: Rectangle (canvas)
	, orientation (o)
	, spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
	, ignore_child_changes (false)
{
}

Box::Box (Item* parent, Orientation o)
	: Rectangle (parent)
	, orientation (o)
	, spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
	, ignore_child_changes (false)
{
}


Box::Box (Item* parent, Duple const & p, Orientation o)
	: Rectangle (parent)
	, orientation (o)
	, spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
	, ignore_child_changes (false)
{
	set_position (p);
}

void
Box::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	if (_fill || _outline) {
		Rectangle::render (area, context);
	}

	Item::render_children (area, context);
}

void
Box::compute_bounding_box () const
{
	_bounding_box = Rect();

	if (_items.empty()) {
		_bounding_box_dirty = false;
		return;
	}

	add_child_bounding_boxes (!collapse_on_hide);

	if (_bounding_box) {
		Rect r = _bounding_box;

		/* left and top margin and padding is already built into the
		 * position of children
		 */

		_bounding_box = r.expand (0.0,
		                          right_padding + outline_width() + right_margin,
		                          bottom_padding + outline_width() + bottom_margin,
		                          0.0);
	}

	_bounding_box_dirty = false;
}

void
Box::set_spacing (double s)
{
	spacing = s;
}

void
Box::set_padding (double t, double r, double b, double l)
{
	double last = t;

	top_padding = t;

	if (r >= 0) {
		last = r;
	}
	right_padding = last;
	if (b >= 0) {
		last = b;
	}
	bottom_padding = last;
	if (l >= 0) {
		last = l;
	}
	left_padding = last;
}

void
Box::set_margin (double t, double r, double b, double l)
{
	double last = t;
	top_margin = t;
	if (r >= 0) {
		last = r;
	}
	right_margin = last;
	if (b >= 0) {
		last = b;
	}
	bottom_margin = last;
	if (l >= 0) {
		last = l;
	}
	left_margin = last;
}

void
Box::set_homogenous (bool yn)
{
	homogenous = yn;
}

void
Box::reset_self ()
{
	if (_bounding_box_dirty) {
		compute_bounding_box ();
	}

	if (!_bounding_box) {
		self->hide ();
		return;
	}

	Rect r (_bounding_box);

	/* XXX need to shrink by margin */

	self->set (r);
}

void
Box::reposition_children ()
{
	Duple previous_edge = Duple (left_margin+left_padding, top_margin+top_padding);
	Distance largest_width = 0;
	Distance largest_height = 0;
	Rect uniform_size;

	if (homogenous) {

		for (std::list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {
			Rect bb = (*i)->bounding_box();
			if (bb) {
				largest_height = std::max (largest_height, bb.height());
				largest_width = std::max (largest_width, bb.width());
			}
		}

		uniform_size = Rect (0, 0, largest_width, largest_height);
	}

	{

		PBD::Unwinder<bool> uw (ignore_child_changes, true);

		for (std::list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {

			(*i)->set_position (previous_edge);

			if (homogenous) {
				(*i)->size_allocate (uniform_size);
			}

			double width;
			double height;

			(*i)->size_request (width, height);

			if (orientation == Vertical) {

				Distance shift = 0;

				if (!(*i)->visible()) {
					/* invisible child */
					if (!collapse_on_hide) {
						/* still add in its size */
						shift += height;
					}
				} else {
					shift += height;
				}

				previous_edge = previous_edge.translate (Duple (0, spacing + shift));

			} else {

				Distance shift = 0;

				if (!(*i)->visible()) {
					if (!collapse_on_hide) {
						shift += width;
					}
				} else {
					shift += width;
				}

				previous_edge = previous_edge.translate (Duple (spacing + shift, 0));
			}
		}
	}

	_bounding_box_dirty = true;
}

void
Box::add (Item* i)
{
	if (!i) {
		return;
	}

	Item::add (i);
	queue_resize ();
}

void
Box::add_front (Item* i)
{
	if (!i) {
		return;
	}

	Item::add_front (i);
	queue_resize ();
}

void
Box::layout ()
{
	bool yes_do_it = _resize_queued;

	Item::layout ();

	if (yes_do_it) {
		reposition_children ();
		compute_bounding_box ();

		const double w = std::max (requested_width, _bounding_box.width());
		const double h = std::max (requested_height, _bounding_box.height());

		set (Rect (get().x0, get().y0, get().x0 + w, get().y0 + h));

		std::cerr << name << " box layed out, reset to " << get() << std::endl;
	}
}

void
Box::child_changed (bool bbox_changed)
{
	/* catch visibility and size changes */

	if (ignore_child_changes) {
		return;
	}

	Item::child_changed (bbox_changed);

	reposition_children ();
}

void
Box::set_collapse_on_hide (bool yn)
{
	if (collapse_on_hide != yn) {
		collapse_on_hide = yn;
		reposition_children ();
	}
}

/*----*/

VBox::VBox (Canvas* c)
	: Box (c, Vertical)
{
}
VBox::VBox (Item* i)
	: Box (i, Vertical)
{
}
VBox::VBox (Item* i, Duple const & position)
	: Box (i, position, Vertical)
{
}

HBox::HBox (Canvas* c)
	: Box (c, Horizontal)
{
}
HBox::HBox (Item* i)
	: Box (i, Horizontal)
{
}
HBox::HBox (Item* i, Duple const & position)
	: Box (i, position, Horizontal)
{
}
