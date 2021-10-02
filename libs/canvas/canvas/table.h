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

	void set_row_spacing (Distance s);
	void set_col_spacing (Distance s);

	void set_padding (FourDimensions const & padding);

#if 0
	void set_margin (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);

	/* aliases so that CSS box model terms work */
	void set_border_width (double w) { set_outline_width (w); }
	void set_border_color (Gtkmm2ext::Color c)  { set_outline_color (c); }

	void set_collapse_on_hide (bool);

#endif
	void set_homogenous (bool);
	void set_row_homogenous (bool);
	void set_col_homogenous (bool);
	void compute_bounding_box () const;
	void size_request (double& w, double& h) const;
	void size_allocate_children (Rect const & r);

	struct Index {
		Index (uint32_t xv, uint32_t yv) : x (xv), y (yv) {}

		bool operator== (Index const & other) const {
			return x == other.x && y == other.y;
		}

		uint32_t x;
		uint32_t y;
	};

	/* These three functions cannot be used with Table, and will cause a
	   fatal error if called.
	*/
	void add (Item *);
	void add_front (Item *);
	void remove (Item *);

	/* How to place an item in a table
	 */

	void attach (Item*, uint32_t upper_left_x, uint32_t upper_left_y, uint32_t lower_right_x, uint32_t lower_right_y, PackOptions row_options = PackOptions (0), PackOptions col_options = PackOptions (0), FourDimensions padding = FourDimensions (0.));
	void attach (Item*, uint32_t upper_left_x, uint32_t upper_right_y, PackOptions row_options = PackOptions (0), PackOptions col_options = PackOptions (0), FourDimensions padding = FourDimensions (0.));
	void attach_with_span (Item*, uint32_t upper_left_x, uint32_t upper_left_y, uint32_t hspan, uint32_t vspan, PackOptions row_options = PackOptions (0), PackOptions col_options = PackOptions (0), FourDimensions padding = FourDimensions (0.));

	void dettach (Item*);

	void set_row_size (uint32_t row, Distance);
	void set_col_size (uint32_t row, Distance);

  protected:
	void child_changed (bool bbox_changed);

  private:
	FourDimensions padding;
	FourDimensions margin;
	Distance row_spacing;
	Distance col_spacing;
	bool collapse_on_hide;
	bool row_homogenous;
	bool col_homogenous;
	bool draw_hgrid;
	bool draw_vgrid;

	mutable bool ignore_child_changes;

	struct CellInfo {
		Item* content;
		PackOptions row_options;
		PackOptions col_options;
		Index upper_left;
		Index lower_right;
		Duple natural_size;
		Duple allocate_size;
		Rect  full_size;
		FourDimensions padding;

		CellInfo (Item* i, PackOptions ro, PackOptions co, Index const & ul, Index const & lr, FourDimensions const & p)
		          : content (i)
		          , row_options (ro)
		          , col_options (co)
		          , upper_left (ul)
		          , lower_right (lr)
		          , padding (p)
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
		Distance user_size;
		bool     occupied;
		Distance spacing;

		AxisInfo() : expanders (0), shrinkers (0), natural_size (0), expand (0), shrink (0), user_size (0), occupied (false), spacing (0) {}

		void reset () {
			expanders = 0;
			shrinkers = 0;
			natural_size = 0;
			expand = 0;
			shrink = 0;
			/* leave user size alone */
			occupied = false;
			spacing = 0;
		}
	};

	typedef std::vector<AxisInfo> AxisInfos;
	AxisInfos row_info;
	AxisInfos col_info;

	void _add (Item *);
	void _add_front (Item *);
	void _remove (Item *);

	void layout ();
	Duple compute (Rect const &);
};

} /* namespace */

#endif /* __CANVAS_TABLE_H__ */
