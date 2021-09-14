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

#include "canvas/table.h"

using namespace ArdourCanvas;
using namespace PBD;

using std::cerr;
using std::endl;


Table::Table (Canvas* canvas)
	: Rectangle (canvas)
{
}

Table::Table (Item* item)
	: Rectangle (item)
{
}

void
Table::attach (Table::Index upper_left, Table::Index lower_right, Item* item, PackOptions row_options, PackOptions col_options)
{
	cells.insert ({ Index (upper_left.x, upper_left.y), CellInfo (item, row_options, col_options, Rect (upper_left.x, upper_left.y, lower_right.x, lower_right.y)) });
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

	bb_clean ();

}

void
Table::size_request (Distance& w, Distance& h) const
{
	int rowmax = 0;
	int colmax = 0;

	for (auto& ci : cells) {
		CellInfo const & c (ci.second);

		if (c.cell.x1 > rowmax) {
			rowmax = c.cell.x1;
		}

		if (c.cell.y1 > colmax) {
			colmax = c.cell.y1;
		}
	}

	AxisInfos rinfo;
	AxisInfos cinfo;

	rinfo.resize (rowmax);
	cinfo.resize (colmax);

	for (auto& ci : cells) {

		Distance cw;
		Distance ch;

		CellInfo const & c (ci.second);
		c.content->size_request (cw, ch);

		rinfo[c.cell.x0].natural_size += cw;
		cinfo[c.cell.y0].natural_size += ch;
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
Table::_size_allocate (Rect const & r)
{
	/* nothing to do here */
}

void
Table::layout ()
{
	size_allocate_children (_allocation);
}

void
Table::size_allocate_children (Rect const & within)
{
	/* step 1: traverse all current cells and determine how many rows and
	 * columns we need. While doing that, get the current natural size of
	 * each cell.
	 */

	int rowmax = 0;
	int colmax = 0;

	for (auto& ci : cells) {
		CellInfo const & c (ci.second);
		if (c.cell.x1 > rowmax) {
			rowmax = c.cell.x1;
		}
		if (c.cell.y1 > colmax) {
			colmax = c.cell.y1;
		}
	}

	row_info.clear ();
	col_info.clear ();

	row_info.resize (rowmax);
	col_info.resize (colmax);

	for (auto& ci : cells) {

		CellInfo & c (ci.second);

		c.content->size_request (c.natural_size.x, c.natural_size.y);

		row_info[c.cell.x0].natural_size += c.natural_size.x;

		if (c.row_options & PackExpand) {
			row_info[c.cell.x0].expanders++;
		}

		if (c.row_options & PackShrink) {
			col_info[c.cell.x0].shrinkers++;
		}

		if (c.cell.x1 != c.cell.x0) {
			row_info[c.cell.x1].natural_size += c.natural_size.x;
			if (c.row_options & PackExpand) {
				row_info[c.cell.x1].expanders++;
			}
			if (c.row_options & PackShrink) {
				row_info[c.cell.x1].shrinkers++;
			}
		}


		col_info[c.cell.y0].natural_size += c.natural_size.y;

		if (c.row_options & PackExpand) {
			col_info[c.cell.y0].expanders++;
		}

		if (c.row_options & PackShrink) {
			col_info[c.cell.y0].shrinkers++;
		}

		if (c.cell.y1 != c.cell.y0) {
			col_info[c.cell.y1].natural_size += c.natural_size.y;

			if (c.col_options & PackExpand) {
				col_info[c.cell.y1].expanders++;
			}
			if (c.row_options & PackShrink) {
				col_info[c.cell.y1].shrinkers++;
			}
		}

	}

	/* rows with nothing in them are still counted as existing. This is a
	 * design decision, not a logic inevitability.
	 */

	const uint32_t rows = rowmax;
	const uint32_t cols = colmax;

	/* Find the tallest column and widest row */

	Distance natural_row_width = 0.;
	Distance natural_col_height = 0.;

	for (AxisInfos::iterator ai = row_info.begin(); ai != row_info.end(); ++ai) {
		natural_row_width = std::max (natural_row_width, ai->natural_size);
	}
	for (AxisInfos::iterator ai = col_info.begin(); ai != col_info.end(); ++ai) {
		natural_col_height = std::max (natural_col_height, ai->natural_size);
	}

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

	Distance per_cell_width = within.width() / cols;

	for (AxisInfos::iterator ai = row_info.begin(); ai != row_info.end(); ++ai) {
		if (natural_row_width < within.width() && ai->expanders) {
			Distance delta = within.width() - natural_row_width;
			ai->expand = delta / ai->expanders;
		} else if (natural_row_width > within.width() && ai->shrinkers) {
			Distance delta = within.width() - natural_row_width;
			ai->shrink = delta / ai->shrinkers;
		}
	}

	Distance per_cell_height = within.height() / rows;

	for (AxisInfos::iterator ai = col_info.begin(); ai != col_info.end(); ++ai) {
		if (natural_col_height < within.height() && ai->expanders) {
			Distance delta = within.height() - natural_col_height;
			ai->expand = delta / ai->expanders;
		} else if (natural_col_height > within.height() && ai->shrinkers) {
			Distance delta = within.height() - natural_col_height;
			ai->shrink = delta / ai->shrinkers;
		}
	}

	for (auto& ci : cells) {

		CellInfo & c (ci.second);

		Distance w;
		Distance h;
		AxisInfo& col (col_info[c.cell.y0]);
		AxisInfo& row (col_info[c.cell.x0]);

		if (c.row_options & PackExpand) {
			w = per_cell_width + row.expand;
		} else if (c.row_options & PackShrink) {
			w = per_cell_width + row.shrink; /* note: row_shrink is negative */
		} else {
			w = per_cell_width;
		}

		if (c.col_options & PackExpand) {
			h = per_cell_height + col.expand;
		} else if (c.col_options & PackShrink) {
			h = per_cell_height + col.shrink; /* note: col_shrink is negative */
		} else {
			h = per_cell_height;
		}

		c.allocate_size = Duple (w, h);
	}

	/* final pass: actually allocate position for each cell */

	Distance hdistance = 0.;
	Distance vdistance = 0.;

	for (uint32_t r = 0; r < rows; ++r) {

		for (uint32_t c = 0; c < cols; ++c) {

			Index idx (r, c);
			Cells::iterator ci = cells.find (idx);

			if (ci != cells.end()) {

				Rect rect = Rect (hdistance, vdistance, hdistance + ci->second.allocate_size.x, vdistance + ci->second.allocate_size.y);
				ci->second.content->size_allocate (rect);
			} else {
				/* this cell (r, c) has no item starting within it */
			}
		}

		hdistance = 0.;
	}
}
