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

#ifndef __CANVAS_TABLE_H__
#define __CANVAS_TABLE_H__

#include <unordered_map>

#include "canvas/rectangle.h"

namespace ArdourCanvas
{

class Rectangle;

class LIBCANVAS_API Table : public Rectangle
{
public:
	Table (Canvas *);
	Table (Item *);

#if 0
	void set_spacing (double s);
	void set_padding (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);
	void set_margin (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);

	/* aliases so that CSS box model terms work */
	void set_border_width (double w) { set_outline_width (w); }
	void set_border_color (Gtkmm2ext::Color c)  { set_outline_color (c); }

	void set_collapse_on_hide (bool);
	void set_homogenous (bool);
#endif

	void compute_bounding_box () const;
	void size_request (double& w, double& h) const;
	void size_allocate_children (Rect const & r);
	void _size_allocate (Rect const & r);

	struct Index {
		Index (uint32_t xv, uint32_t yv) : x (xv), y (yv) {}

		bool operator== (Index const & other) const {
			return x == other.x && y == other.y;
		}

		uint32_t x;
		uint32_t y;
	};

	void attach (Index upper_left, Index lower_right, Item*, PackOptions row_options = PackOptions (0), PackOptions col_options = PackOptions (0));

  protected:
	void child_changed (bool bbox_changed);

  private:
	Distance top_margin;
	Distance right_margin;
	Distance bottom_margin;
	Distance left_margin;
	Distance row_spacing;
	Distance col_spacing;

	bool collapse_on_hide;
	bool homogenous;
	bool draw_hgrid;
	bool draw_vgrid;

	mutable bool ignore_child_changes;

	struct CellInfo {
		Item* content;
		PackOptions row_options;
		PackOptions col_options;
		Rect  cell;
		Duple natural_size;
		Duple allocate_size;
		Duple full_size;

		CellInfo (Item* i, PackOptions ro, PackOptions co, Rect c)
		          : content (i)
		          , row_options (ro)
		          , col_options (co)
		          , cell (c)
		          {}
	};

	struct index_hash {
		std::size_t operator() (Table::Index const & i) const {
			/* use upper left coordinate for hash */
			return ((uint64_t) i.x << 32) | i.y;
		}
	};

	/* cell lookup, indexed by upper left corner only
	 */

	typedef std::unordered_map<Index, CellInfo, index_hash> Cells;
	Cells cells;

	struct AxisInfo {
		uint32_t expanders;
		uint32_t shrinkers;
		Distance natural_size;
		Distance expand;
		Distance shrink;

		AxisInfo() : expanders (0), shrinkers (0), natural_size (0) {}
	};

	typedef std::vector<AxisInfo> AxisInfos;
	AxisInfos row_info;
	AxisInfos col_info;

	void layout ();
};

} /* namespace */

#endif /* __CANVAS_TABLE_H__ */
