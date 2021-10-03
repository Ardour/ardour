/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/i18n.h"
#include "pbd/unwind.h"

#include "canvas/debug.h"
#include "canvas/table.h"

using namespace ArdourCanvas;
using namespace PBD;

using std::cerr;
using std::endl;


Table::Table (Canvas* canvas)
	: Rectangle (canvas)
	, padding ({ 0 })
	, margin ({ 0 })
	, collapse_on_hide (false)
	, row_homogenous (true)
	, col_homogenous (true)
	, draw_hgrid (false)
	, draw_vgrid (false)
{
	set_layout_sensitive (true);
}

Table::Table (Item* item)
	: Rectangle (item)
	, padding ({ 0 })
	, margin ({ 0 })
	, collapse_on_hide (false)
	, row_homogenous (true)
	, col_homogenous (true)
	, draw_hgrid (false)
	, draw_vgrid (false)
{
	set_layout_sensitive (true);
}

void
Table::attach (Item* item, uint32_t ulx, uint32_t uly, PackOptions row_options, PackOptions col_options, FourDimensions pad)
{
	attach (item, ulx, uly, ulx + 1, uly + 1, row_options, col_options, pad);
}

void
Table::attach_with_span (Item* item, uint32_t ulx, uint32_t uly, uint32_t w, uint32_t h, PackOptions row_options, PackOptions col_options, FourDimensions pad)
{
	attach (item, ulx, uly, ulx + w, uly + h, row_options, col_options, pad);
}

void
Table::attach (Item* item, uint32_t ulx, uint32_t uly, uint32_t lrx, uint32_t lry, PackOptions row_options, PackOptions col_options, FourDimensions pad)
{
	/* XXX maybe use z-axis to stack elements if the insert fails? Would
	 * involve making Index 3D and using an actual hash function
	 */

	std::pair<Cells::iterator,bool> res = cells.insert ({ Index (ulx, uly), CellInfo (item, row_options, col_options, Index (ulx, uly), Index (lrx, lry), pad) });

	if (!res.second) {
		cerr << "Failed to attach at " << ulx << ", " << uly << " " << lrx << ", " << lry << endl;
	}

	_add (item);
	item->size_request (res.first->second.natural_size.x, res.first->second.natural_size.y);

	if (lrx > col_info.size()) {
		col_info.resize (lrx);
	}

	if (lry > row_info.size()) {
		row_info.resize (lry);
	}
}

void
Table::child_changed (bool bbox_changed)
{
	if (ignore_child_changes) {
		return;
	}

	Item::child_changed (bbox_changed);
	size_allocate_children (_allocation);
}

void
Table::compute_bounding_box() const
{
	if (cells.empty()) {
		_bounding_box = Rect();
		bb_clean ();
		return;
	}

	if ((*cells.begin()).second.natural_size == Duple()) {
		/* force basic computation of natural size */
		Duple ns = const_cast<Table*>(this)->compute (Rect());
		_bounding_box = Rect (0, 0, ns.x, ns.y);

	} else {
		/* bounding box was computed in compute() */
	}

	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("bounding box computed as %1\n", _bounding_box));

	bb_clean ();

}

void
Table::set_row_size (uint32_t row, Distance size)
{
	if (row_info.size() <= row) {
		row_info.resize (row+1);
	}
	row_info[row].user_size = size;
}

void
Table::set_col_size (uint32_t col, Distance size)
{
	if (col_info.size() <= col) {
		col_info.resize (col+1);
	}
	col_info[col].user_size = size;
}

void
Table::size_request (Distance& w, Distance& h) const
{
	Duple d = const_cast<Table*>(this)->compute (Rect());

	w = d.x;
	h = d.y;
}

void
Table::layout ()
{
	(void) compute (_allocation);
}

void
Table::size_allocate_children (Rect const & within)
{
	(void) compute (within);
}

Duple
Table::compute (Rect const & within)
{
	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("\n\nCompute table within rect: %1\n", within));

	if (cells.empty()) {
		return Duple (0, 0);
	}

	uint32_t rows = row_info.size();
	uint32_t cols = col_info.size();

	for (auto & ai : row_info) {
		ai.reset ();
	}

	for (auto & ai : col_info) {
		ai.reset ();
	}

	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("cell coordinates indicate rows %1 cols %2 from %3 cells\n", rows, cols, cells.size()));

	for (auto & ci : cells) {

		CellInfo c (ci.second);

		const float hspan = c.lower_right.x - c.upper_left.x;
		const float vspan = c.lower_right.y - c.upper_left.y;

		uint32_t covered_c_spacings = hspan - 1;
		uint32_t covered_r_spacings = vspan - 1;

		DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("for cell %8 %1,%2 - %3,%4, contents natural size = %5 hspan %6 vspan %7\n",
		                                                 c.upper_left.x,
		                                                 c.upper_left.y,
		                                                 c.lower_right.x,
		                                                 c.lower_right.y,
		                                                 c.natural_size,
		                                                 hspan, vspan,
		                                                 c.content->whoami()));

		/* for every col that this cell occupies, count the number of
		 * expanding/shrinking items, and compute the largest width
		 * for the column (cells)
		 */

		for (uint32_t col = c.upper_left.x; col != c.lower_right.x; ++col) {

			if (c.col_options & PackExpand) {
				col_info[col].expanders++;
			}

			if (c.col_options & PackShrink) {
				col_info[col].shrinkers++;
			}

			/* columns have a natural width
			 *
			 * The natural width of the item is divided across
			 * hspan cols, and then we add the padding and spacing
			 */

			const Distance total_cell_width = (c.natural_size.x / hspan) + c.padding.left + c.padding.right + col_info[col].spacing + (covered_c_spacings * col_spacing);

			/* the col's natural size (width) is the maximum
			 * width of any of the cells within it.
			 */

			col_info[col].natural_size = std::max (col_info[col].natural_size, total_cell_width);
			col_info[col].occupied = true;
		}

		/* for every row that this cell occupies, count the number of
		 * expanding/shrinking items, and compute the largest height
		 * for the row (cells)
		 */

		for (uint32_t row = c.upper_left.y; row != c.lower_right.y; ++row) {

			if (c.row_options & PackExpand) {
				row_info[row].expanders++;
			}

			if (c.row_options & PackShrink) {
				row_info[row].shrinkers++;
			}

			/* rows have a natural height.
			 *
			 * The natural height of the item is divided across
			 * vspan rows, and then we add the padding and spacing
			 */

			const Distance total_cell_height = (c.natural_size.y / vspan) + c.padding.up + c.padding.down  + row_info[row].spacing * (covered_r_spacings * row_spacing);

			/* the row's natural size (height) is the maximum
			 * height of any of the cells within it.
			 */

			row_info[row].natural_size = std::max (row_info[row].natural_size, total_cell_height);
			row_info[row].occupied = true;
		}
	}

	/* rows with nothing in them are still counted as existing. This is a
	 * design decision, not a logic inevitability.
	 */

	/* Find the widest column and tallest row. This will give us our
	 * "natural size"
	 */

	Distance widest_column_width = 0.;
	Distance highest_row_height = 0.;
	Distance inelastic_width = 0;
	Distance inelastic_height = 0;
	uint32_t inelastic_rows = 0;
	uint32_t inelastic_cols = 0;
	Distance total_natural_width = 0;
	Distance total_natural_height = 0;

	for (auto & row : row_info) {

		if (row.user_size) {
			highest_row_height = std::max (highest_row_height, row.user_size);
			inelastic_height += row.user_size;
			inelastic_cols++;
			if (!row_homogenous) {
				total_natural_height += row.user_size;
			}
		} else {
			if (row.expanders == 0 && row.shrinkers == 0) {
				inelastic_rows++;
				inelastic_height += row.natural_size;
			}
			highest_row_height = std::max (highest_row_height, row.natural_size);

			if (!row_homogenous) {
				total_natural_height += row.natural_size;
			}
		}

	}

	for (auto & col : col_info) {

		if (col.user_size) {
			widest_column_width = std::max (widest_column_width, col.user_size);
			inelastic_width += col.user_size;
			inelastic_cols++;
			if (!col_homogenous) {
				total_natural_width += col.user_size;
			}

		} else {
			if (col.expanders == 0 && col.shrinkers == 0) {
				inelastic_cols++;
				inelastic_width += col.natural_size;
			}
			widest_column_width = std::max (widest_column_width, col.natural_size);

			if (!col_homogenous) {
				total_natural_width += col.natural_size;
			}
		}
	}

	if (col_homogenous) {
		/* reset total width using the widest, multiplied by the number
		   of cols, since they wll all be the same height. the values
		   before we do this are cumulative, and do not (necessarily)
		   reflect homogeneity
		*/
		total_natural_width = widest_column_width * cols;
	}

	if (row_homogenous) {
		/* reset total height using the heighest, multiplied by the
		   number of rows, since they wll all be the same height. the
		   values before we do this are cumulative, and do not
		   (necessarily) reflect homogeneity
		*/
		total_natural_height = highest_row_height * rows;
	}

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::CanvasTable)) {
		DEBUG_STR_DECL(a);
		int n = 0;
		for (auto const & row : row_info) {
			DEBUG_STR_APPEND(a, string_compose ("row %1: height %2\n", n+1, row.natural_size));
			++n;
		}
		DEBUG_TRACE (DEBUG::CanvasTable, DEBUG_STR(a).str());

		DEBUG_STR_DECL(b);
		n = 0;
		for (auto const & col : col_info) {

			DEBUG_STR_APPEND(b, string_compose ("col %1: width %2\n", n, col.natural_size));
			++n;
		}
		DEBUG_TRACE (DEBUG::CanvasTable, DEBUG_STR(b).str());
	}
#endif

	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("widest col width x highest row height = %1 x %2, inelastic: %3 x %4 ir x ic %5 x %6\n", widest_column_width, highest_row_height, inelastic_width, inelastic_height,
	                                                 inelastic_rows, inelastic_cols));

	if (!within) {
		/* within is empty, so this is just for a size request */

		DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("total natural width x height = %1 x %2 + %3 , %4\n", total_natural_width, total_natural_height,
		                                                 ((cols - 1) * col_spacing) + padding.left + padding.right,
		                                                 ((rows - 1) * col_spacing) + padding.up + padding.down));

		return Duple (total_natural_width + ((cols - 1) * col_spacing) + padding.left + padding.right,
		              total_natural_height + ((rows - 1) * col_spacing) + padding.up + padding.down);
	}

	/* actually doing allocation, so prevent endless loop between here and
	 * ::child_changed()
	 */

	PBD::Unwinder<bool> uw (ignore_child_changes, true);

	/* step two: compare the natural size to the size we've been given
	 *
	 * If the natural size is less than the allocated size, then find the
	 * difference, divide it by the number of expanding items per
	 * (row|col). Divide the total size by the number of (rows|cols), then
	 * iterate. Allocate expanders the per-cell size plus the extra for
	 * expansion. Allocate shrinkers/default just the per-cell size.
	 *
	 * If the natural size if greated than the allocated size, find the
	 * difference, divide it by the number of shrinking items per
	 * (row|col). Divide the total size by the number of (rows|cols), then
	 * iterate. Allocate shrinkers the per-cell size minus the excess for
	 * shrinking. Allocate expanders/default just the per-cell size.
	 *
	 */

	const uint32_t elastic_rows = rows - inelastic_rows;
	const uint32_t elastic_cols = cols - inelastic_cols;
	Distance elastic_col_width = 0;
	Distance elastic_row_height = 0;

	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("vr,vc %1 x %2\n", elastic_rows, elastic_cols));

	if (row_homogenous) {

		/* all columns must have the same width and height */
		elastic_row_height = (within.height() - ((rows - 1) * row_spacing) - padding.up - padding.down) / rows;
	} else {
		if (elastic_rows) {
			const Distance elastic_non_spacing_non_padding_height = within.height() - inelastic_height - ((rows - 1) * row_spacing) - padding.up - padding.down;
			elastic_row_height = elastic_non_spacing_non_padding_height / elastic_rows;
		}
	}

	if (col_homogenous) {
		elastic_col_width = (within.width() - ((cols - 1) * col_spacing) - padding.left - padding.right) / cols;
	} else {
		if (elastic_cols) {
			const Distance elastic_non_spacing_non_padding_width = within.width() - inelastic_width - ((cols - 1) * col_spacing) - padding.left - padding.right;
			elastic_col_width =  elastic_non_spacing_non_padding_width / elastic_cols;
		}

	}

	for (auto & ci : cells) {

		CellInfo & c (ci.second);

		const float hspan = c.lower_right.x - c.upper_left.x;
		const float vspan = c.lower_right.y - c.upper_left.y;

		AxisInfo& col (col_info[c.upper_left.x]);
		AxisInfo& row (row_info[c.upper_left.y]);

		Distance w;
		Distance h;

		if (col.user_size) {
			w = col.user_size;
		} else if (c.row_options & PackExpand) {
			w = hspan * elastic_col_width + ((hspan - 1) * col_spacing);
		} else if (c.row_options & PackShrink) {
			w = hspan * elastic_col_width + ((hspan - 1) * col_spacing);
		} else {
			/* normal col, not expanding or shrinking */
			w = c.natural_size.x;
		}

		if (row.user_size) {
			h = col.user_size;
		} else if (c.row_options & PackExpand) {
			h = vspan * elastic_row_height + ((vspan - 1) * row_spacing);
		} else if (c.row_options & PackShrink) {
			h = vspan * elastic_row_height + ((vspan - 1) * row_spacing);
		} else {
			/* normal row, not expanding or shrinking */
			h = c.natural_size.y;
		}

		/* Reduce the allocate width x height to account for cell
		 * padding and individual column/row spacing. Do not adjust for
		 * global padding or global column/row spacing, since that was
		 * already accounted for when we computed
		 * elastic_{row_height,col_width}
		 */

		w -= c.padding.left + c.padding.right;
		w -= col.spacing;

		h -= c.padding.up + c.padding.down;
		h -= row.spacing;

		if (w < 0 || w > within.width()) {
			/* can't do anything */
			return Duple (within.width(), within.height());
		}

		if (h < 0 || h > within.height()) {
			/* can't do anything */
			return Duple (within.width(), within.height());
		}

		DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("Cell %9 @ %1,%2 - %3,%4 (hspan %7 vspan %8) allocated %5 x %6\n",
		                                                 ci.first.x, ci.first.y, ci.second.lower_right.x, ci.second.lower_right.y, w, h, hspan, vspan, ci.second.content->whoami()));

		c.allocate_size = Duple (w, h);
	}

	/* final pass: actually allocate position for each cell. Do this in a
	 * row,col order so that we can set up position based on all cells
	 * above and left of whichever one we are working on.
	 */

	Distance vpos = padding.up;
	Distance hpos;

	for (uint32_t r = 0; r < rows; ++r) {

		hpos = padding.left;
		Distance vshift = 0;

		for (uint32_t c = 0; c < cols; ++c) {

			Index idx (c, r);

			Cells::iterator ci = cells.find (idx);

			if (ci != cells.end()) {

				Rect rect = Rect (hpos + ci->second.padding.left,                              /* x0 */
				                  vpos + ci->second.padding.up,                                /* y0 */
				                  hpos + ci->second.padding.left + ci->second.allocate_size.x, /* x1 */
				                  vpos + ci->second.padding.up + ci->second.allocate_size.y);  /* y1 */

				DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("Item %7 @ %1,%2 - %3,%4 size-allocate %5 vs %6\n",
				                                                 ci->second.upper_left.x,
				                                                 ci->second.upper_left.y,
				                                                 ci->second.lower_right.x,
				                                                 ci->second.lower_right.y,
				                                                 rect, ci->second.allocate_size,
				                                                 ci->second.content->whoami()));


				ci->second.content->size_allocate (rect);
				ci->second.full_size = rect;

				if (col_homogenous || (ci->second.col_options & PackOptions (PackExpand|PackShrink))) {
					/* homogenous forces all col widths to
					   the same value, and/or the cell
					   is allowed to expand/shrink to the
					   allotted variable column width.
					*/
					hpos = padding.left + (elastic_col_width * (c + 1));
				} else {
					/* not homogeneous, and no
					   expand/shrink being applied to
					   contents. We need to skip over to
					   the start of the next column here.
					   But ... we can't just use the
					   allocation rect, since that is
					   probably too small/too large.

					   So... where is the start of the next
					   column. Well, it's at the greater of
					   (a) right edge of this cell's
					   natural box OR (b) wherever the nth
					   elastic column would be.
					*/

					/* rect already includes padding.left */
					hpos = std::max (rect.x1 + ci->second.padding.right, padding.left + (elastic_col_width * (c + 1)));
				}

				if (row_homogenous || (ci->second.row_options & PackOptions (PackExpand|PackShrink))) {
					/* homogenous forces all row heights to
					   the same value, and/or the cell
					   is allowed to expand/shrink to the
					   allotted variable row height.
					*/
					vshift = std::max (vshift, elastic_row_height);
				} else {
					/* rect already includes padding.up */
					vshift = std::max (vshift, rect.height() + ci->second.padding.down);
				}

				/* when this row is done, we'll shift down by
				   the largest cell height so far for this row.
				*/

			} else {

				/* cell is empty, just adjust horizontal &
				   vertical shift values to get to the next
				   cell

				*/
				if (col_homogenous) {
					hpos = elastic_col_width * (c + 1);
				} else {
					hpos += col_info[c].natural_size;
				}

				if (row_homogenous) {
					vshift = std::max (vshift, elastic_row_height);
				} else {
					vshift = std::max (vshift, row_info[r].natural_size);
				}
			}

			if (c < (cols - 1)) {
				hpos += col_info[c].spacing;
				hpos += col_spacing;
			}

		} /* end of a row */


		/* add per-row and global row-spacing to the vertical
		   shift when we reach the end of the row.
		*/

		vshift += row_info[r].spacing;
		vshift += row_spacing;
		vpos += vshift;
	}

	/* We never take padding.right into account for layout, and  hpos is reset to zero as we start a
	 * new row, but we want this set correctly as we exit the loop
	 */

	hpos += padding.right;

	/* set bounding box so that we don't have to do it again in ::compute_bounding_box() */

	_bounding_box = Rect (0, 0, hpos, vpos);

	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("table bbox in compute() %1\n", _bounding_box));

	/* return our size */

	return Duple (hpos, vpos);
}

void
Table::add (Item*)
{
	fatal << _("programming error: add() cannot be used with Canvas::Table; use attach() instead") << endmsg;
}


void
Table::add_front (Item*)
{
	fatal << _("programming error: add_front() cannot be used with Canvas::Table; use attach() instead") << endmsg;
}

void
Table::remove (Item*)
{
	fatal << _("programming error: remove() cannot be used with Canvas::Table; use detach() instead") << endmsg;
}

void
Table::_add (Item* i)
{
	if (!i) {
		return;
	}
	Item::add (i);
	queue_resize ();
}


void
Table::_add_front (Item* i)
{
	if (!i) {
		return;
	}
	Item::add_front (i);
	queue_resize ();
}

void
Table::_remove (Item* i)
{
	if (!i) {
		return;
	}
	Item::remove (i);
	queue_resize ();
}

void
Table::set_row_spacing (Distance d)
{
	row_spacing = d;
	queue_resize ();
}

void
Table::set_col_spacing (Distance d)
{
	col_spacing = d;
	queue_resize ();
}

void
Table::set_homogenous (bool yn)
{
	row_homogenous = yn;
	col_homogenous = yn;
	queue_resize ();
}

void
Table::set_row_homogenous (bool yn)
{
	row_homogenous = yn;
	queue_resize ();
}

void
Table::set_col_homogenous (bool yn)
{
	col_homogenous = yn;
	queue_resize ();
}

void
Table::set_padding (FourDimensions const & p)
{
	padding = p;
	queue_resize ();
}
