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

#ifndef __CANVAS_GRID_H__
#define __CANVAS_GRID_H__

#include <map>

#include "canvas/item.h"

namespace ArdourCanvas
{

class Rectangle;

/** Canvas container that renders its children in a grid layout
 */
class LIBCANVAS_API Grid : public Item
{
public:
	Grid (Canvas *);
	Grid (Item *);
	Grid (Item *, Duple const & position);

	void set_row_spacing (double s);
	void set_col_spacing (double s);

	void set_padding (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);
	void set_margin (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);

	/* aliases so that CSS box model terms work */
	void set_border_width (double w) { set_outline_width (w); }
	void set_border_color (Gtkmm2ext::Color c)  { set_outline_color (c); }

	void place (Item*, double x, double y, double col_span = 1, double row_span = 1);

	void set_collapse_on_hide (bool);
	void set_homogenous (bool);

	void compute_bounding_box () const;
	void render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

  protected:
	double row_spacing;
	double col_spacing;
	double top_padding, right_padding, bottom_padding, left_padding;
	double top_margin, right_margin, bottom_margin, left_margin;

	void child_changed (bool bbox_changed);
  private:
	struct ChildInfo {
		Item* item;
		double x;
		double y;
		double col_span;
		double row_span;
	};

	typedef std::map<Item*,ChildInfo> CoordsByItem;
	CoordsByItem coords_by_item;

	Rectangle *bg;
	bool collapse_on_hide;
	bool homogenous;

	void reset_bg ();
	void reposition_children ();
};

}

#endif
