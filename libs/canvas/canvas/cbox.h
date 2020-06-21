/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef __CANVAS_CBOX_H__
#define __CANVAS_CBOX_H__

#include <list>

#include "canvas/constraint_packer.h"

namespace ArdourCanvas
{

class Rectangle;
class BoxConstrainedItem;

class LIBCANVAS_API cBox : public ConstraintPacker
{
public:
	cBox (Canvas *, Orientation);
	cBox (Item *, Orientation);

	void set_spacing (double s);
	void set_padding (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);
	void set_margin (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);

	/* aliases so that CSS box model terms work */
	void set_border_width (double w) { set_outline_width (w); }
	void set_border_color (Gtkmm2ext::Color c)  { set_outline_color (c); }

	void remove (Item*);

	BoxConstrainedItem* pack_start (Item*, PackOptions primary_axis_packing = PackOptions (0), PackOptions secondary_axis_packing = PackOptions (PackExpand|PackFill));
	BoxConstrainedItem* pack_end (Item*, PackOptions primary_axis_packing = PackOptions (0), PackOptions secondary_axis_packing = PackOptions (PackExpand|PackFill));

	void add_vertical_box_constraints (kiwi::Solver& solver, BoxConstrainedItem* ci, BoxConstrainedItem* prev, double expanded_size, double main_dimenion, double second_dimension, double alloc_dimension);
	void add_horizontal_box_constraints (kiwi::Solver& solver, BoxConstrainedItem* ci, BoxConstrainedItem* prev, double expanded_size, double main_dimenion, double second_dimension, double alloc_dimension);

	void set_collapse_on_hide (bool);
	void set_homogenous (bool);

	void preferred_size(Duple& minimum, Duple& natural) const;
	void size_allocate (Rect const &);

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

  protected:
	Orientation orientation;

	double _spacing;
	double _top_padding;
	double _bottom_padding;
	double _left_padding;
	double _right_padding;
	double _top_margin;
	double _bottom_margin;
	double _left_margin;
	double _right_margin;

	void child_changed (bool bbox_changed);

  private:
	typedef std::list<BoxConstrainedItem*> Order;
	Order order;
	bool collapse_on_hide;
	bool homogenous;

	BoxConstrainedItem* pack (Item*, PackOptions primary_axis_packing, PackOptions secondary_axis_packing);
};

}

#endif /* __CANVAS_CBOX_H__ */
