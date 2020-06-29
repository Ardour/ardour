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

#ifndef __CANVAS_CONSTRAINED_ITEM_H__
#define __CANVAS_CONSTRAINED_ITEM_H__

#include "kiwi/kiwi.h"

#include "canvas/types.h"
#include "canvas/visibility.h"

namespace ArdourCanvas
{

class Item;
class ConstraintPacker;


class /* LIBCANVAS_API */ ConstrainedItem
{
  public:
	ConstrainedItem (Item& i);
	virtual ~ConstrainedItem ();

	Item& item() { return _item; }

	kiwi::Variable& left () { return _left; }
	kiwi::Variable& right () { return _right; }
	kiwi::Variable& top () { return _top; }
	kiwi::Variable& bottom () { return _bottom; }
	kiwi::Variable& width () { return _width; }
	kiwi::Variable& height () { return _height; }
	kiwi::Variable& center_x () { return _center_x; }
	kiwi::Variable& center_y () { return _center_y; }

	kiwi::Variable const & left () const { return _left; }
	kiwi::Variable const & right () const { return _right; }
	kiwi::Variable const & top () const { return _top; }
	kiwi::Variable const & bottom () const { return _bottom; }
	kiwi::Variable const & width () const { return _width; }
	kiwi::Variable const & height () const { return _height; }
	kiwi::Variable const & center_x () const { return _center_x; }
	kiwi::Variable const & center_y () const { return _center_y; }

	kiwi::Variable& left_padding () { return _left_padding; }
	kiwi::Variable& right_padding () { return _right_padding; }
	kiwi::Variable& top_padding () { return _top_padding; }
	kiwi::Variable& bottom_padding () { return _bottom_padding; }

	void constrained (ConstraintPacker const & parent);
	virtual bool involved (kiwi::Constraint const &) const;

	std::vector<kiwi::Constraint> const & constraints() const { return _constraints; }
	void add_constraint (kiwi::Constraint const & c) { _constraints.push_back (c); }

	ConstrainedItem& at (Duple const &);
	ConstrainedItem& size (Duple const &);
	ConstrainedItem& box (Rect const &);

	ConstrainedItem& left_of (ConstrainedItem const &, Distance pad = 0);
	ConstrainedItem& right_of (ConstrainedItem const &, Distance pad = 0);
	ConstrainedItem& above (ConstrainedItem const &, Distance pad = 0);
	ConstrainedItem& below (ConstrainedItem const &, Distance pad = 0);
	ConstrainedItem& x_centered (ConstrainedItem const &, Distance offset = 0);
	ConstrainedItem& y_centered (ConstrainedItem const &, Distance offset = 0);
	ConstrainedItem& centered_on (ConstrainedItem const &, Distance xoffset = 0, Distance yoffset = 0);
	ConstrainedItem& top_aligned_with (ConstrainedItem const &, Distance offset = 0);
	ConstrainedItem& bottom_aligned_with (ConstrainedItem const &, Distance offset = 0);
	ConstrainedItem& left_aligned_with (ConstrainedItem const &, Distance offset = 0);
	ConstrainedItem& right_aligned_with (ConstrainedItem const &, Distance offset = 0);
	ConstrainedItem& same_size_as (ConstrainedItem const &, Distance wdelta = 0, Distance hdelta = 0);
	ConstrainedItem& same_width_as (ConstrainedItem const &, Distance delta = 0);
	ConstrainedItem& same_height_as (ConstrainedItem const &, Distance delta = 0);

	virtual void dump (std::ostream&);

  protected:
	Item& _item;
	std::vector<kiwi::Constraint> _constraints;

	kiwi::Variable _left;
	kiwi::Variable _right;
	kiwi::Variable _top;
	kiwi::Variable _bottom;
	kiwi::Variable _width;
	kiwi::Variable _height;
	kiwi::Variable _left_padding;
	kiwi::Variable _right_padding;
	kiwi::Variable _top_padding;
	kiwi::Variable _bottom_padding;

	/* derived */

	kiwi::Variable _center_x;
	kiwi::Variable _center_y;
};

class /* LIBCANVAS_API */ BoxConstrainedItem : public ConstrainedItem
{
  public:
	BoxConstrainedItem (Item& i, PackOptions primary_axis_opts, PackOptions secondary_axis_opts);
	~BoxConstrainedItem ();

	virtual bool involved (kiwi::Constraint const &) const;

	kiwi::Variable& left_margin () { return _left_margin; }
	kiwi::Variable& right_margin () { return _right_margin; }
	kiwi::Variable& top_margin () { return _top_margin; }
	kiwi::Variable& bottom_margin () { return _bottom_margin; }

	PackOptions primary_axis_pack_options() const { return _primary_axis_pack_options; }
	PackOptions secondary_axis_pack_options() const { return _secondary_axis_pack_options; }

	void dump (std::ostream&);

  private:
	kiwi::Variable _left_margin;
	kiwi::Variable _right_margin;
	kiwi::Variable _top_margin;
	kiwi::Variable _bottom_margin;

	PackOptions _primary_axis_pack_options;
	PackOptions _secondary_axis_pack_options;
};

}

#endif
