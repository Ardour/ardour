/*
 * Copyright (C) 2017 Paul Davis <paul@linuxaudiosystems.com>
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
#include <vector>

#include "canvas/grid.h"
#include "canvas/rectangle.h"

using namespace ArdourCanvas;
using std::vector;
using std::max;
using std::cerr;
using std::endl;

Grid::Grid (Canvas* canvas)
	: Item (canvas)
	, row_spacing (0)
	, col_spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
{
	bg = new Rectangle (this);
	bg->set_outline (false);
	bg->set_fill (false);
	bg->hide ();
}

Grid::Grid (Item* parent)
	: Item (parent)
	, row_spacing (0)
	, col_spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
{
	bg = new Rectangle (this);
	bg->set_outline (false);
	bg->set_fill (false);
	bg->hide ();
}

Grid::Grid (Item* parent, Duple const & p)
	: Item (parent, p)
	, row_spacing (0)
	, col_spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (true)
{
	bg = new Rectangle (this);
	bg->set_outline (false);
	bg->set_fill (false);
	bg->hide ();
}

void
Grid::set_homogenous (bool yn)
{
	homogenous = yn;
}

void
Grid::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Item::render_children (area, context);
}

void
Grid::compute_bounding_box () const
{
	_bounding_box = Rect();

	if (_items.empty()) {
		bb_clean ();
		return;
	}

	add_child_bounding_boxes (!collapse_on_hide);

	if (_bounding_box) {
		Rect r = _bounding_box;

		_bounding_box = r.expand (outline_width() + top_margin + top_padding,
		                          outline_width() + right_margin + right_padding,
		                          outline_width() + bottom_margin + bottom_padding,
		                          outline_width() + left_margin + left_padding);
	}

	bb_clean ();
}

void
Grid::set_row_spacing (double s)
{
	row_spacing = s;
}

void
Grid::set_col_spacing (double s)
{
	col_spacing = s;
}

void
Grid::set_padding (double t, double r, double b, double l)
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
Grid::set_margin (double t, double r, double b, double l)
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
Grid::reset_bg ()
{
	if (_bounding_box_dirty) {
		(void) bounding_box ();
	}

	if (!_bounding_box) {
		bg->hide ();
		return;
	}

	Rect r (_bounding_box);

	/* XXX need to shrink by margin */

	bg->set (r);
}

void
Grid::reposition_children ()
{
	uint32_t max_row = 0;
	uint32_t max_col = 0;

	/* since we encourage dynamic and essentially random placement of
	 * children, begin by determining the maximum row and column extents given
	 * our current set of children and placements.
	 */

	for (CoordsByItem::const_iterator c = coords_by_item.begin(); c != coords_by_item.end(); ++c) {
		if (collapse_on_hide && !c->second.item->visible()) {
			continue;
		}
		max_col = max (max_col, (uint32_t) (c->second.x + c->second.col_span));
		max_row = max (max_row, (uint32_t) (c->second.y + c->second.row_span));
	}

	/* Now compute the width of the widest child for each column, and the
	 * height of the tallest child for each row. Store the results in
	 * row_dimens and col_dimens, making sure they are suitably sized first.
	 */

	vector<double> row_dimens;
	vector<double> col_dimens;

	row_dimens.assign (max_row + 1, 0);
	col_dimens.assign (max_col + 1, 0);

	Rect uniform_cell_size;

	if (homogenous) {
		for (std::list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {

			if (*i == bg || (collapse_on_hide && !(*i)->visible())) {
				continue;
			}

			Rect bb = (*i)->bounding_box();

			if (!bb) {
				continue;
			}

			CoordsByItem::const_iterator c = coords_by_item.find (*i);

			uniform_cell_size.x1 = max (uniform_cell_size.x1, (bb.width()/c->second.col_span));
			uniform_cell_size.y1 = max (uniform_cell_size.y1, (bb.height()/c->second.row_span));
		}

		for (uint32_t n = 0; n < max_col; ++n) {
			col_dimens[n] = uniform_cell_size.width();
		}

		for (uint32_t n = 0; n < max_row; ++n) {
			row_dimens[n] = uniform_cell_size.height();
		}

		for (std::list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {

			if (*i == bg || (collapse_on_hide && !(*i)->visible())) {
				/* bg rect is not a normal child */
				continue;
			}

			CoordsByItem::const_iterator c = coords_by_item.find (*i);

			Rect r = uniform_cell_size;
			r.x1 *= c->second.col_span;
			r.y1 *= c->second.row_span;

			(*i)->size_allocate (r);
		}

	} else {
		for (std::list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {

			if (*i == bg || (collapse_on_hide && !(*i)->visible())) {
				/* bg rect is not a normal child */
				continue;
			}

			Rect bb = (*i)->bounding_box();

			if (!bb) {
				continue;
			}

			CoordsByItem::const_iterator c = coords_by_item.find (*i);

			const double per_col_width = bb.width() / c->second.col_span;
			const double per_row_height = bb.height() / c->second.row_span;

			/* set the width of each column spanned by this item
			 */

			for (int n = 0; n < (int) c->second.col_span; ++n) {
				col_dimens[c->second.x + n] = max (col_dimens[c->second.x + n], per_col_width);
			}
			for (int n = 0; n < (int) c->second.row_span; ++n) {
				row_dimens[c->second.y + n] = max (row_dimens[c->second.y + n], per_row_height);
			}
		}
	}

	/* now progressively sum the row and column widths, once we're done:
	 *
	 * col_dimens: transformed into the x coordinate of the left edge of each column.
	 *
	 * row_dimens: transformed into the y coordinate of the upper left of each row,
	 *
	 */

	double current_right_edge = left_margin + left_padding;

	for (uint32_t n = 0; n < max_col; ++n) {
		if (col_dimens[n]) {
			/* a width was defined for this column */
			const double w = col_dimens[n]; /* save width of this column */
			col_dimens[n] = current_right_edge;
			current_right_edge = current_right_edge + w + col_spacing;
		}
	}

	double current_top_edge = top_margin + top_padding;

	for (uint32_t n = 0; n < max_row; ++n) {
		if (row_dimens[n]) {
			/* height defined for this row */
			const double h = row_dimens[n]; /* save height */
			row_dimens[n] = current_top_edge;
			current_top_edge = current_top_edge + h + row_spacing;
		}
	}

	/* position each item at the upper left of its (row, col) coordinate,
	 * given the width of all rows or columns before it.
	 */

	for (std::list<Item*>::iterator i = _items.begin(); i != _items.end(); ++i) {
		CoordsByItem::const_iterator c = coords_by_item.find (*i);

		if (c == coords_by_item.end()) {
			continue;
		}

		/* do this even for hidden items - it will be corrected when
		 * they become visible again.
		 */

		(*i)->set_position (Duple (col_dimens[c->second.x], row_dimens[c->second.y]));
	}

	_bounding_box_dirty = true;
	reset_bg ();
}

void
Grid::place (Item* i, double x, double y, double col_span, double row_span)
{
	ChildInfo ci;

	add (i);

	ci.item = i;
	ci.x = x;
	ci.y = y;
	ci.col_span = max (1.0, col_span);
	ci.row_span = max (1.0, row_span);

	coords_by_item.insert (std::make_pair (i, ci));
	reposition_children ();
}

void
Grid::child_changed (bool bbox_changed)
{
	/* catch visibility and size changes */

	Item::child_changed (bbox_changed);
	reposition_children ();
}

void
Grid::set_collapse_on_hide (bool yn)
{
	if (collapse_on_hide != yn) {
		collapse_on_hide = yn;
		reposition_children ();
	}
}
