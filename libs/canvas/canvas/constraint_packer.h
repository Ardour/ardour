/*
 * Copyright (C) 2020 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_CONSTRAINT_PACKER_H__
#define __CANVAS_CONSTRAINT_PACKER_H__

#include <list>
#include <map>

#include "canvas/container.h"
#include "kiwi/kiwi.h"

namespace ArdourCanvas
{

class Rectangle;
class ConstrainedItem;
class BoxConstrainedItem;

class LIBCANVAS_API ConstraintPacker : public Container
{
public:
	ConstraintPacker (Canvas *, Orientation o = Horizontal);
	ConstraintPacker (Item *, Orientation o = Horizontal);

	void set_spacing (double s);
	void set_padding (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);
	void set_margin (double top, double right = -1.0, double bottom = -1.0, double left = -1.0);

	/* aliases so that CSS box model terms work */
	void set_border_width (double w) { set_outline_width (w); }
	void set_border_color (Gtkmm2ext::Color c)  { set_outline_color (c); }


	void add (Item *);
	void add_front (Item *);
	void remove (Item *);
	void constrain (kiwi::Constraint const &);

	BoxConstrainedItem* pack_start (Item*, PackOptions primary_axis_packing = PackOptions (0), PackOptions secondary_axis_packing = PackOptions (PackExpand|PackFill));
	BoxConstrainedItem* pack_end (Item*, PackOptions primary_axis_packing = PackOptions (0), PackOptions secondary_axis_packing = PackOptions (PackExpand|PackFill));

	virtual ConstrainedItem* add_constrained (Item* item);

	void solve ();
	void apply (kiwi::Solver*);

	void compute_bounding_box () const;

	void size_allocate (Rect const &);
	void size_request (Distance& w, Distance& h) const;

	void render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

	kiwi::Variable width;
	kiwi::Variable height;

  protected:
	void child_changed (bool bbox_changed);

	Orientation _orientation;
	double _spacing;
	double _top_padding;
	double _bottom_padding;
	double _left_padding;
	double _right_padding;
	double _top_margin;
	double _bottom_margin;
	double _left_margin;
	double _right_margin;

	kiwi::Variable expanded_item_size;

	typedef std::map<Item*,ConstrainedItem*> ConstrainedItemMap;
	ConstrainedItemMap constrained_map;
	typedef std::list<kiwi::Constraint> ConstraintList;
	ConstraintList constraint_list;
	kiwi::Solver _solver;
	bool in_alloc;
	bool _need_constraint_update;

	void add_constrained_internal (Item*, ConstrainedItem*);

	void add_constraints (kiwi::Solver&, ConstrainedItem*) const;

	void non_const_size_request (Distance& w, Distance& h);
	virtual void update_constraints ();

	void add_vertical_box_constraints (kiwi::Solver& solver, BoxConstrainedItem* ci, BoxConstrainedItem* prev, double main_dimenion, double second_dimension, kiwi::Variable& alloc_var);
	void add_horizontal_box_constraints (kiwi::Solver& solver, BoxConstrainedItem* ci, BoxConstrainedItem* prev, double main_dimenion, double second_dimension, kiwi::Variable& alloc_var);

  private:
	typedef std::list<BoxConstrainedItem*> BoxPackedItems;
	BoxPackedItems packed;

	BoxConstrainedItem* pack (Item*, PackOptions primary_axis_packing, PackOptions secondary_axis_packing);
	void box_size_request (Distance& w, Distance& h) const;
};

}

#endif
