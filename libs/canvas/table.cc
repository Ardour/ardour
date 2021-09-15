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
Table::attach (Item* item, Table::Index const & upper_left, Table::Index const & lower_right, PackOptions row_options, PackOptions col_options, FourDimensions pad)
{
	/* XXX maybe use z-axis to stack elements if the insert fails? Would
	 * involve making Index 3D and using an actual hash function
	 */

	if (cells.insert ({ Index (upper_left.x, upper_left.y), CellInfo (item, row_options, col_options, upper_left, lower_right, pad) }).second) {
		_add (item);
	} else {
		cerr << "Failed to attach at " << upper_left.x << ", " << upper_left.y << " " << lower_right.x << ", " << lower_right.y << endl;
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

	add_child_bounding_boxes (!collapse_on_hide);

	if (_bounding_box) {
#if 0
		Rect r = _bounding_box;

		_bounding_box = r.expand (top_padding + outline_width() + top_margin,
		                          right_padding + outline_width() + right_margin,
		                          bottom_padding + outline_width() + bottom_margin,
		                          left_padding + outline_width() + left_margin);
#endif
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
	uint32_t rowmax = 0;
	uint32_t colmax = 0;

	for (auto& ci : cells) {
		CellInfo const & c (ci.second);

		if (c.lower_right.x > rowmax) {
			rowmax = c.lower_right.x;
		}

		if (c.lower_right.y > colmax) {
			colmax = c.lower_right.y;
		}
	}

	AxisInfos rinfo;
	AxisInfos cinfo;

	rinfo.resize (rowmax+1);
	cinfo.resize (colmax+1);

	for (auto& ci : cells) {

		Distance cw;
		Distance ch;

		CellInfo const & c (ci.second);
		c.content->size_request (cw, ch);

		rinfo[c.upper_left.x].natural_size += cw;
		cinfo[c.upper_left.y].natural_size += ch;
	}

	w = 0;
	h = 0;

	for (auto& ai : rinfo) {
		w = std::max (w, ai.natural_size);
	}

	for (auto& ai : cinfo) {
		h = std::max (h, ai.natural_size);
	}
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

	/* step 1: traverse all current cells and determine how many rows and
	 * columns we need. While doing that, get the current natural size of
	 * each cell.
	 */

	uint32_t rowmax = 0;
	uint32_t colmax = 0;

	row_info.clear ();
	col_info.clear ();

	for (auto& ci : cells) {
		CellInfo const & c (ci.second);
		if (c.lower_right.x > colmax) {
			colmax = c.lower_right.x;
		}
		if (c.lower_right.y > rowmax) {
			rowmax = c.lower_right.y;
		}
	}

	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("cell coordinates indicate rowmax %1 colmax %2 from %3 cells\n", rowmax, colmax, cells.size()));

	row_info.resize (rowmax+1);
	col_info.resize (colmax+1);

	for (auto& ci : cells) {

		CellInfo & c (ci.second);

		c.content->size_request (c.natural_size.x, c.natural_size.y);

		const float hspan = c.lower_right.x - c.upper_left.x;
		const float vspan = c.lower_right.y - c.upper_left.y;

		for (uint32_t row = c.upper_left.x; row != c.lower_right.x; ++row) {

			if (c.row_options & PackExpand) {
				row_info[row].expanders++;
			}

			if (c.row_options & PackShrink) {
				row_info[row].shrinkers++;
			}

			row_info[row].natural_size += c.natural_size.x / hspan;
			col_info[row].natural_size += c.padding.left + c.padding.right;
			col_info[row].natural_size += col_info[row].spacing;

			row_info[row].occupied = true;
		}

		for (uint32_t col = c.upper_left.y; col != c.lower_right.y; ++col) {

			if (c.col_options & PackExpand) {
				col_info[col].expanders++;
			}

			if (c.col_options & PackShrink) {
				col_info[col].shrinkers++;
			}

			col_info[col].natural_size += c.natural_size.y / vspan;
			col_info[col].natural_size += c.padding.up + c.padding.down;
			col_info[col].natural_size += col_info[c.lower_right.y].spacing;

			col_info[col].occupied = true;
		}

	}

#ifndef NDEBUG
	if (DEBUG_ENABLED(DEBUG::CanvasTable)) {
		DEBUG_STR_DECL(a);
		int n = 0;
		for (auto& ai : row_info) {
			DEBUG_STR_APPEND(a, string_compose ("row %1: nwidth %2\n", n+1, ai.natural_size));
			++n;
		}
		DEBUG_TRACE (DEBUG::CanvasTable, DEBUG_STR(a).str());

		DEBUG_STR_DECL(b);
		n = 0;
		for (auto& ai : col_info) {

			DEBUG_STR_APPEND(b, string_compose ("col %1: nheight %2\n", n, ai.natural_size));
			++n;
		}
		DEBUG_TRACE (DEBUG::CanvasTable, DEBUG_STR(b).str());
	}
#endif

	/* rows with nothing in them are still counted as existing. This is a
	 * design decision, not a logic inevitability.
	 */

	const uint32_t rows = rowmax + 1;
	const uint32_t cols = colmax + 1;

	/* Find the tallest column and widest row. This will give us our
	 * "natural size"
	 */

	Distance natural_row_width = 0.;
	Distance natural_col_height = 0.;

	for (AxisInfos::iterator ai = row_info.begin(); ai != row_info.end(); ++ai) {
		natural_row_width = std::max (natural_row_width, ai->natural_size);
	}
	for (AxisInfos::iterator ai = col_info.begin(); ai != col_info.end(); ++ai) {
		natural_col_height = std::max (natural_col_height, ai->natural_size);
	}

	DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("natural width x height = %1 x %2\n", natural_row_width, natural_col_height));

	if (!within) {
		/* within is empty, so this is just for a size request */
		return Duple (natural_row_width, natural_col_height);
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

	if (homogenous) {

		Distance per_cell_width = within.width() / cols - 1;
		Distance per_cell_height = within.height() / rows - 1;

		DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("per-cell: %1 x %2 from %3 and %4/%5\n", per_cell_width, per_cell_height, within, cols, rows));

		/* compute total expansion or contraction that will be
		 * distributed across all rows & cols marked for expand/shrink
		 */

		for (auto & ai : row_info) {
			if (natural_row_width < within.width() && ai.expanders) {
				Distance delta = within.width() - natural_row_width;
				ai.expand = delta / ai.expanders;
			} else if (natural_row_width > within.width() && ai.shrinkers) {
				Distance delta = within.width() - natural_row_width;
				ai.shrink = delta / ai.shrinkers;
			}
		}

		for (auto & ai : col_info) {
			if (natural_col_height < within.height() && ai.expanders) {
				Distance delta = within.height() - natural_col_height;
				ai.expand = delta / ai.expanders;
			} else if (natural_col_height > within.height() && ai.shrinkers) {
				Distance delta = within.height() - natural_col_height;
				ai.shrink = delta / ai.shrinkers;
			}
		}

		for (auto& ci : cells) {

			CellInfo & c (ci.second);

			const float hspan = c.lower_right.x - c.upper_left.x;
			const float vspan = c.lower_right.y - c.upper_left.y;

			Distance w;
			Distance h;
			AxisInfo& col (col_info[c.upper_left.y]);
			AxisInfo& row (col_info[c.upper_left.x]);

			if (c.row_options & PackExpand) {
				w = hspan * (per_cell_width + row.expand);
			} else if (c.row_options & PackShrink) {
				w = hspan * (per_cell_width + row.shrink); /* note: row_shrink is negative */
			} else {
				w = hspan * per_cell_width;
			}

			if (c.col_options & PackExpand) {
				h = vspan * (per_cell_height + col.expand);
			} else if (c.col_options & PackShrink) {
				h = vspan * (per_cell_height + col.shrink); /* note: col_shrink is negative */
			} else {
				h = vspan * per_cell_height;
			}

			// w -= c.padding.left + c.padding.right;
			// w -= col.spacing;

			// h -= c.padding.up + c.padding.down;
			// h -= row.spacing;

			DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("Cell @ %1,%2 - %3,%4 (hspan %7 vspan %8) allocated %5 x %6\n",
			                                                 ci.first.x, ci.first.y, ci.second.lower_right.x, ci.second.lower_right.y, w, h, hspan, vspan));

			c.allocate_size = Duple (w, h);
		}

	} else {

		/* not homogenous */

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

				Rect rect = Rect (hdistance, vdistance + ci->second.padding.up, hdistance + ci->second.allocate_size.x, vdistance + ci->second.allocate_size.y);

				DEBUG_TRACE (DEBUG::CanvasTable, string_compose ("Item @ %1,%2 - %3,%4 size-allocate %5\n",
				                                                 ci->second.upper_left.x,
				                                                 ci->second.upper_left.y,
				                                                 ci->second.lower_right.x,
				                                                 ci->second.lower_right.y,
				                                                 rect));

				ci->second.content->size_allocate (rect);

				hdistance += rect.width() + ci->second.padding.right;
				hdistance += col_info[c].spacing;

				const Distance total_cell_height = rect.height() + ci->second.padding.up + ci->second.padding.down;
				vshift = std::max (vshift, total_cell_height);

			} else {
				/* this cell (r, c) has no item starting within it */
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
