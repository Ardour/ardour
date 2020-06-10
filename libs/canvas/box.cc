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

#include "canvas/box.h"
#include "canvas/rectangle.h"

using namespace ArdourCanvas;

Box::Box (Canvas* canvas, Orientation o)
	: Item (canvas)
	, orientation (o)
	, spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
{
	self = new Rectangle (this);
	self->set_outline (false);
	self->set_fill (false);
}

Box::Box (Item* parent, Orientation o)
	: Item (parent)
	, orientation (o)
	, spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
{
	self = new Rectangle (this);
	self->set_outline (false);
	self->set_fill (false);
}


Box::Box (Item* parent, Duple const & p, Orientation o)
	: Item (parent, p)
	, orientation (o)
	, spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
{
	self = new Rectangle (this);
	self->set_outline (false);
	self->set_fill (false);
}

void
Box::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
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

		_bounding_box = r.expand (top_padding + outline_width() + top_margin,
		                          right_padding + outline_width() + right_margin,
		                          bottom_padding + outline_width() + bottom_margin,
		                          left_padding + outline_width() + left_margin);
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
	Duple previous_edge (0, 0);
	Distance largest_width = 0;
	Distance largest_height = 0;
	Rect uniform_size;

	if (homogenous) {

		for (std::list<Item*>::iterator i = _items.begin(); ++i != _items.end(); ++i) {
			Rect bb = (*i)->bounding_box();
			if (bb) {
				largest_height = std::max (largest_height, bb.height());
				largest_width = std::max (largest_width, bb.width());
			}
		}

		uniform_size = Rect (0, 0, largest_width, largest_height);
	}

	for (std::list<Item*>::iterator i = _items.begin(); ++i != _items.end(); ++i) {

		(*i)->set_position (previous_edge);

		if (homogenous) {
			(*i)->size_allocate (uniform_size);
		}

		if (orientation == Vertical) {

			Distance shift = 0;

			Rect bb = (*i)->bounding_box();

			if (!(*i)->visible()) {
				/* invisible child */
				if (!collapse_on_hide) {
					/* still add in its size */
					if (bb) {
						shift += bb.height();
						}
				}
			} else {
				if (bb) {
					shift += bb.height();
				}
			}

			previous_edge = previous_edge.translate (Duple (0, spacing + shift));

		} else {

			Distance shift = 0;
			Rect bb = (*i)->bounding_box();

			if (!(*i)->visible()) {
				if (!collapse_on_hide) {
					if (bb) {
						shift += bb.width();
					}
				}
			} else {
				if (bb) {
					shift += bb.width();
				}
			}

			previous_edge = previous_edge.translate (Duple (spacing + shift, 0));
		}
	}

	_bounding_box_dirty = true;
	reset_self ();
}

void
Box::pack_end (Item* i, double extra_padding)
{
	if (!i) {
		return;
	}

	/* prepend new child */
	Item::add_front (i);
	reposition_children ();
}
void
Box::pack_start (Item* i, double extra_padding)
{
	if (!i) {
		return;
	}

	/* append new child */
	Item::add (i);
	reposition_children ();
}

void
Box::add (Item* i)
{
	pack_start (i);
}

void
Box::child_changed (bool bbox_changed)
{
	/* catch visibility and size changes */

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
