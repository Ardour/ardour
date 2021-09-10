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

#include "pbd/compose.h"
#include "pbd/unwind.h"
#include "pbd/stacktrace.h"

#include "canvas/box.h"
#include "canvas/debug.h"
#include "canvas/rectangle.h"

using namespace ArdourCanvas;
using namespace PBD;

using std::cerr;
using std::endl;

Box::Box (Canvas* canvas, Orientation o)
	: Rectangle (canvas)
	, orientation (o)
	, spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
	, ignore_child_changes (false)
{
	set_layout_sensitive (true);
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
	set_layout_sensitive (true);
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
	set_layout_sensitive (true);
	set_position (p);
	set_outline_width (3);
}

void
Box::compute_bounding_box () const
{
	_bounding_box = Rect();

	if (_items.empty()) {
		bb_clean ();
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

	bb_clean ();
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
Box::_size_allocate (Rect const & alloc)
{
	Rect old_alloc (_allocation);
	Rectangle::_size_allocate (alloc);

	bool width_shrinking = (old_alloc.width() > alloc.width());
	bool height_shrinking = (old_alloc.height() > alloc.height());

	reposition_children (alloc.width(), alloc.height(), width_shrinking, height_shrinking);
}

void
Box::size_allocate_children (Rect const &)
{
	/* do nothing here */
}

void
Box::size_request (Distance& w, Distance& h) const
{
	Duple previous_edge = Duple (left_margin+left_padding, top_margin+top_padding);
	Distance largest_width = 0;
	Distance largest_height = 0;
	Rect uniform_size;

	DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("size request for %1\n", this));
	if (homogenous) {

		for (std::list<Item*>::const_iterator i = _items.begin(); i != _items.end(); ++i) {
			Distance iw, ih;
			(*i)->size_request (iw, ih);

			largest_height = std::max (largest_height, ih);
			largest_width = std::max (largest_width, iw);
		}

		uniform_size = Rect (0, 0, largest_width, largest_height);
		DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("homogenous box, uniform size computed as %1\n", uniform_size));
	}

	Rect r;

	{
		PBD::Unwinder<bool> uw (ignore_child_changes, true);

		for (std::list<Item*>::const_iterator i = _items.begin(); i != _items.end(); ++i) {

			double width;
			double height;
			Rect isize;

			(*i)->size_request (width, height);
			DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, desires %2 x %3\n", (*i)->whoami(), width, height));


			if (homogenous) {
				if (((*i)->pack_options() & PackOptions (PackExpand|PackFill)) == PackOptions (PackExpand|PackFill)) {
					if (orientation == Vertical) {
						/* use the item's own height and our computed width */
						isize = Rect (previous_edge.x, previous_edge.y, previous_edge.x + uniform_size.width(), previous_edge.y + height);
						DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, use computed width to give %2\n", (*i)->whoami(), isize));
					} else {
						/* use the item's own width and our computed height */
						isize = Rect (previous_edge.x, previous_edge.y, previous_edge.x + width, previous_edge.y + uniform_size.height());
						DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, use computed height to give %2\n", (*i)->whoami(), isize));
					}
				} else {
					isize = Rect (previous_edge.x, previous_edge.y, previous_edge.x + width, previous_edge.y + height);
					DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, use item size to give %2\n", (*i)->whoami(), isize));
				}
			} else {
				isize = Rect (previous_edge.x, previous_edge.y, previous_edge.x + width, previous_edge.y + height);
				DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, use item size (non-homogenous) to give %2\n", (*i)->whoami(), isize));
			}

			width = isize.width();
			height = isize.height();

			DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, initial size %2 x %3\n", (*i)->whoami(), width, height));

			r = r.extend (Rect (previous_edge.x, previous_edge.y, previous_edge.x + width, previous_edge.y + height));

			DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\tcumulative rect is now %1\n", r));

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

	/* left and top margins+padding already reflected in child bboxes */

	r = r.expand (0, right_margin + right_padding, bottom_margin + bottom_padding, 0);

	DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("add margin and padding, get %1\n", r));

	w = r.width();
	h = r.height();
}

void
Box::reposition_children (Distance width, Distance height, bool shrink_width, bool shrink_height)
{

	Duple previous_edge = Duple (left_margin+left_padding, top_margin+top_padding);
	Distance largest_width = 0;
	Distance largest_height = 0;
	Rect uniform_size;

	if (width == 0 && height == 0) {
		return;
	}

	DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("allocating children within %1 x %2, shrink/w %3 shrink/h %4\n", width, height, shrink_width, shrink_height));

	if (homogenous) {

		for (std::list<Item*>::const_iterator i = _items.begin(); i != _items.end(); ++i) {
			Distance iw, ih;
			(*i)->size_request (iw, ih);
			if (!shrink_height) {
				largest_height = std::max (largest_height, ih);
			}
			if (!shrink_width) {
				largest_width = std::max (largest_width, iw);
			}
		}

		/* these two represent the width and height available for
		 * contents (i.e. after we've taken "borders" "owned" by this
		 * box into account)
		 */

		const Distance contents_width = width - (left_margin + left_padding + right_margin + right_padding);
		const Distance contents_height = height - (top_margin + top_padding + bottom_margin + bottom_padding);

		Distance item_width;
		Distance item_height;

		if (orientation == Vertical) {
			item_width = contents_width;
			item_height = (contents_height - ((_items.size() - 1) * spacing));;
		} else {
			item_width = (contents_width - ((_items.size() - 1) * spacing));
			item_height = contents_height;
		}

		if (orientation == Vertical) {
			largest_width = std::max (largest_width, item_width);
		}

		if (orientation == Horizontal) {
			largest_height = std::max (largest_height, item_height);
		}

		uniform_size = Rect (0, 0, largest_width, largest_height);
		DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("allocating for homogenous box, uniform size computed as %1\n", uniform_size));
	}

	{
		PBD::Unwinder<bool> uw (ignore_child_changes, true);

		for (std::list<Item*>::const_iterator i = _items.begin(); i != _items.end(); ++i) {

			double width;
			double height;
			Rect isize;

			(*i)->size_request (width, height);

			if (homogenous) {
				if (((*i)->pack_options() & PackOptions (PackExpand|PackFill)) == PackOptions (PackExpand|PackFill)) {
					if (orientation == Vertical) {
						/* use the item's own height and our computed width */
						isize = Rect (previous_edge.x, previous_edge.y, previous_edge.x + uniform_size.width(), previous_edge.y + height);
						DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, use computed width to give %2\n", (*i)->whoami(), isize));
					} else {
						/* use the item's own width and our computed height */
						isize = Rect (previous_edge.x, previous_edge.y, previous_edge.x + width, previous_edge.y + uniform_size.height());
						DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, use computed height to give %2\n", (*i)->whoami(), isize));
					}
				} else {
					isize = Rect (previous_edge.x, previous_edge.y, previous_edge.x + width, previous_edge.y + height);
					DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, use item size to give %2\n", (*i)->whoami(), isize));
				}
			} else {
				isize = Rect (previous_edge.x, previous_edge.y, previous_edge.x + width, previous_edge.y + height);
				DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1, use item size (non-homogenous) to give %2\n", (*i)->whoami(), isize));
			}

			DEBUG_TRACE (DEBUG::CanvasBox|DEBUG::CanvasSizeAllocate, string_compose ("\t%1 allocating %2\n", (*i)->whoami(), isize));
			(*i)->size_allocate (isize);

			width = isize.width();
			height = isize.height();

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
		reposition_children (_allocation.width(), _allocation.height(), false, false);
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

	reposition_children (_allocation.width(), _allocation.height(), false, false);
}

void
Box::set_collapse_on_hide (bool yn)
{
	if (collapse_on_hide != yn) {
		collapse_on_hide = yn;
		reposition_children (_allocation.width(), _allocation.height(), false, false);
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
