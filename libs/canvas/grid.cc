/*
    Copyright (C) 2018 Paul Davis

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
	, spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
{
	self = new Rectangle (this);
	self->set_outline (false);
	self->set_fill (false);
}

Grid::Grid (Item* parent)
	: Item (parent)
	, spacing (0)
	, top_padding (0), right_padding (0), bottom_padding (0), left_padding (0)
	, top_margin (0), right_margin (0), bottom_margin (0), left_margin (0)
	, homogenous (false)
{
	self = new Rectangle (this);
	self->set_outline (false);
	self->set_fill (false);
}

Grid::Grid (Item* parent, Duple const & p)
	: Item (parent, p)
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
Grid::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Item::render_children (area, context);
}

void
Grid::compute_bounding_box () const
{
	_bounding_box = boost::none;

	if (_items.empty()) {
		_bounding_box_dirty = false;
		return;
	}

	add_child_bounding_boxes (!collapse_on_hide);

	if (_bounding_box) {
		Rect r = _bounding_box.get();

		_bounding_box = r.expand (outline_width() + top_margin,
		                          outline_width() + right_margin,
		                          outline_width() + bottom_margin,
		                          outline_width() + left_margin);
	}

	_bounding_box_dirty = false;
}

void
Grid::set_spacing (double s)
{
	spacing = s;
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
Grid::reset_self ()
{
	if (_bounding_box_dirty) {
		compute_bounding_box ();
	}

	if (!_bounding_box) {
		self->hide ();
		return;
	}

	Rect r (_bounding_box.get());

	/* XXX need to shrink by margin */

	self->set (r);
}

void
Grid::reposition_children ()
{
	uint32_t max_row = 0;
	uint32_t max_col = 0;

	/* since we encourage dynamic and essentially random placement of
	 * children, begin by determining the maximum row and column given
	 * our current set of children and placements.
	 */

	for (CoordsByItem::const_iterator c = coords_by_item.begin(); c != coords_by_item.end(); ++c) {
		max_col = max (max_col, (uint32_t) c->second.x);
		max_row = max (max_row, (uint32_t) c->second.y);
	}

	max_row++;
	max_col++;

	/* Now compute the width of the widest child for each column, and the
	 * height of the tallest child for each row.
	 */

	vector<double> row_dimens;
	vector<double> col_dimens;

	row_dimens.assign (max_row, 0);
	col_dimens.assign (max_col, 0);

	for (std::list<Item*>::iterator i = _items.begin(); ++i != _items.end(); ++i) {
		boost::optional<Rect> bb = (*i)->bounding_box();

		if (!bb) {
			continue;
		}

		CoordsByItem::const_iterator c = coords_by_item.find (*i);

		row_dimens[c->second.y] = max (row_dimens[c->second.y], bb.get().height());
		col_dimens[c->second.x] = max (col_dimens[c->second.x]	, bb.get().width());
	}

	/* now sum the row and column widths, so that row_dimens is transformed
	 * into the y coordinate of the upper left of each row, and col_dimens
	 * is transformed into the x coordinate of the left edge of each
	 * column.
	 */

	double prev = row_dimens[0];
	row_dimens[0] = top_margin;

	for (uint32_t n = 1; n < max_row; ++n) {
		row_dimens[n] = row_dimens[n-1] + prev + top_padding + bottom_padding;
		prev = row_dimens[n] + bottom_padding;
	}

	prev = col_dimens[0];
	col_dimens[0] = left_margin;

	for (uint32_t n = 1; n < max_col; ++n) {
		col_dimens[n] = col_dimens[n-1] + prev + left_padding + right_padding;
		prev = col_dimens[n];
	}

	/* position each item at the upper left of its (row, col) coordinate,
	 * given the width of all rows or columns before it.
	 */

	for (std::list<Item*>::iterator i = _items.begin(); ++i != _items.end(); ++i) {
		CoordsByItem::const_iterator c = coords_by_item.find (*i);

		if (c == coords_by_item.end()) {
			continue;
		}

		(*i)->set_position (Duple (col_dimens[c->second.x], row_dimens[c->second.y]));
	}


	_bounding_box_dirty = true;
	reset_self ();
}

void
Grid::place (Item* i, Duple at)
{
	add (i);
	coords_by_item.insert (std::make_pair (i, at));
	reposition_children ();
}

void
Grid::child_changed ()
{
	/* catch visibility and size changes */

	Item::child_changed ();
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
