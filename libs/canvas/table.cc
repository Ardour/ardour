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
	, collapse_on_hide (false)
	, homogenous (true)
	, draw_hgrid (false)
	, draw_vgrid (false)
{
	set_layout_sensitive (true);
}

Table::Table (Item* item)
	: Rectangle (item)
	, collapse_on_hide (false)
	, homogenous (true)
	, draw_hgrid (false)
	, draw_vgrid (false)
{
	set_layout_sensitive (true);
}

void
Table::attach (Item* item, Coord ulx, Coord uly, Coord lrx, Coord lry, PackOptions row_options, PackOptions col_options, FourDimensions pad)
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
	_bounding_box = Rect();
	if (cells.empty()) {
		bb_clean ();
		return;
	}

	if ((*cells.begin()).second.natural_size == Duple()) {
		/* force basic computation of natural size */
		Duple ns = const_cast<Table*>(this)->compute (Rect());
		_bounding_box = Rect (0, 0, ns.x, ns.y);

	} else {

		for (auto const & cell : cells) {
			if (_bounding_box) {
				_bounding_box = _bounding_box.extend (cell.second.full_size);
			} else {
				_bounding_box = cell.second.full_size;
			}
		}
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
	cerr << "\n\nLAYOUT\n\n";
	size_allocate_children (_allocation);
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

		DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("for cell %1,%2 - %3,%4, contents natural size = %5 hspan %6 vspan %7\n",
		                                                 c.upper_left.x,
		                                                 c.upper_left.y,
		                                                 c.lower_right.x,
		                                                 c.lower_right.y,
		                                                 c.natural_size,
		                                                 hspan, vspan));

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
			const Distance total_cell_width = (c.natural_size.x / hspan) + c.padding.left + c.padding.right + col_info[col].spacing;

			/* column width must be large enough to hold the
			 * largest cell
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

			const Distance total_cell_height = (c.natural_size.y / vspan) + c.padding.up + c.padding.down  + row_info[row].spacing;

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

	Distance col_natural_width = 0.;
	Distance row_natural_height = 0.;
	Distance inelastic_width = 0;
	Distance inelastic_height = 0;
	uint32_t inelastic_rows = 0;
	uint32_t inelastic_cols = 0;

	for (auto & ai : row_info) {

		ai.natural_size += ai.spacing;

		if (ai.user_size) {
			row_natural_height = std::max (row_natural_height, ai.user_size);
			inelastic_height += ai.user_size;
			inelastic_cols++;
		} else {
			if (ai.expanders == 0 && ai.shrinkers == 0) {
				inelastic_rows++;
				inelastic_height += ai.natural_size;
			}
			row_natural_height = std::max (row_natural_height, ai.natural_size);
		}
	}

	for (auto & ai : col_info) {

		ai.natural_size += ai.spacing;

		if (ai.user_size) {
			col_natural_width = std::max (col_natural_width, ai.user_size);
			inelastic_width += ai.user_size;
			inelastic_cols++;
		} else {
			if (ai.expanders == 0 && ai.shrinkers == 0) {
				inelastic_cols++;
				inelastic_width += ai.natural_size;
			}
			col_natural_width = std::max (col_natural_width, ai.natural_size);
		}

	}

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::CanvasTable)) {
		DEBUG_STR_DECL(a);
		int n = 0;
		for (auto& ai : row_info) {
			DEBUG_STR_APPEND(a, string_compose ("row %1: height %2\n", n+1, ai.natural_size));
			++n;
		}
		DEBUG_TRACE (DEBUG::CanvasTable, DEBUG_STR(a).str());

		DEBUG_STR_DECL(b);
		n = 0;
		for (auto& ai : col_info) {

			DEBUG_STR_APPEND(b, string_compose ("col %1: width %2\n", n, ai.natural_size));
			++n;
		}
		DEBUG_TRACE (DEBUG::CanvasTable, DEBUG_STR(b).str());
	}
#endif

	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("natural width x height = %1 x %2, inelastic: %3 x %4 ir x ic %5 x %6\n", col_natural_width, row_natural_height, inelastic_width, inelastic_height,
	                                                 inelastic_rows, inelastic_cols));

	if (!within) {
		/* within is empty, so this is just for a size request */
		return Duple (col_natural_width * cols, row_natural_height * rows);
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


	uint32_t variable_size_rows = rows - inelastic_rows;
	uint32_t variable_size_cols = cols - inelastic_cols;
	Distance variable_col_width = 0;
	Distance variable_row_height = 0;

	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("vr,vc %1 x %2\n", variable_size_rows, variable_size_cols));

	if (variable_size_cols) {
		variable_col_width = (within.width() - inelastic_width) / variable_size_cols;
	}

	if (variable_size_rows) {
		variable_row_height = (within.height() - inelastic_height) / variable_size_rows;
	}

	for (auto & ci : cells) {

		CellInfo & c (ci.second);

		const float hspan = c.lower_right.x - c.upper_left.x;
		const float vspan = c.lower_right.y - c.upper_left.y;

		AxisInfo& col (col_info[c.upper_left.y]);
		AxisInfo& row (col_info[c.upper_left.x]);

		Distance w;
		Distance h;

		if (col.user_size) {
			w = col.user_size;
		} else if (c.row_options & PackExpand) {
			w = hspan * variable_col_width;
		} else if (c.row_options & PackShrink) {
			w = hspan * variable_col_width;
		} else {
			/* normal col, not expanding or shrinking */
			w = c.natural_size.x;
		}

		if (row.user_size) {
			h = col.user_size;
		} else if (c.row_options & PackExpand) {
			h = vspan * variable_row_height;
		} else if (c.row_options & PackShrink) {
			h = vspan * variable_row_height;
		} else {
			/* normal row, not expanding or shrinking */
			h = c.natural_size.y;
		}

		w -= c.padding.left + c.padding.right;
		w -= col.spacing;

		h -= c.padding.up + c.padding.down;
		h -= row.spacing;

		DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("Cell @ %1,%2 - %3,%4 (hspan %7 vspan %8) allocated %5 x %6\n",
		                                                 ci.first.x, ci.first.y, ci.second.lower_right.x, ci.second.lower_right.y, w, h, hspan, vspan));

		c.allocate_size = Duple (w, h);
	}

	/* final pass: actually allocate position for each cell. Do this in a
	 * row,col order so that we can set up position based on all cells
	 * above and left of whichever one we are working on.
	 */

	Distance hdistance = 0.;
	Distance vdistance = 0.;

	for (uint32_t r = 0; r < rows; ++r) {

		Distance vshift = 0;
		hdistance = 0;

		for (uint32_t c = 0; c < cols; ++c) {

			Index idx (c, r);

			Cells::iterator ci = cells.find (idx);

			if (ci != cells.end()) {

				hdistance += ci->second.padding.left;

				Rect rect = Rect (hdistance, vdistance + ci->second.padding.up, hdistance + ci->second.allocate_size.x, vdistance + ci->second.padding.up + ci->second.allocate_size.y);

				DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("Item @ %1,%2 - %3,%4 size-allocate %5 vs %6\n",
				                                                 ci->second.upper_left.x,
				                                                 ci->second.upper_left.y,
				                                                 ci->second.lower_right.x,
				                                                 ci->second.lower_right.y,
				                                                 rect, ci->second.allocate_size));


				ci->second.content->size_allocate (rect);
				ci->second.full_size = rect;

				if (homogenous) {
					hdistance = variable_col_width * (c + 1);
				} else {
					hdistance += rect.width() + ci->second.padding.right;
					hdistance += col_info[c].spacing;
				}

				Distance total_cell_height;

				if (homogenous) {
					total_cell_height = variable_row_height;
				} else {
					/* rect already includes padding.up */
					total_cell_height = rect.height() + ci->second.padding.down;
				}

				vshift = std::max (vshift, total_cell_height);

			} else {
				/* cell is empty, just adjust horizontal &
				   vertical shift values to get to the next
				   cell
				*/
				hdistance = variable_col_width * (c + 1);
				vshift = std::max (vshift, variable_row_height);
			}

		}
		vshift += row_info[r].spacing;
		vdistance += vshift;
	}

	return Duple (hdistance, vdistance);
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
