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

#include <iostream>

#include "canvas/item.h"
#include "canvas/types.h"
#include "canvas/constrained_item.h"

using namespace ArdourCanvas;

using std::cerr;
using std::endl;
using kiwi::Constraint;
using kiwi::Variable;

ConstrainedItem::ConstrainedItem (Item& i)
	: _item (i)
	, _left (_item.name + " left")
	, _right (_item.name + " right")
	, _top (_item.name + " top")
	, _bottom (_item.name + " bottom")
	, _width (_item.name + " width")
	, _height (_item.name + " height")
{
}

ConstrainedItem::~ConstrainedItem ()
{
}

void
ConstrainedItem::constrained (ConstraintPacker const & parent)
{
	/* our variables should be set. Deliver computed size to item */

	Rect r (_left.value(), _top.value(), _right.value(), _bottom.value());
	//cerr << _item.whatami() << '/' << _item.name << " constrained-alloc " << r << endl;
	_item.size_allocate (r);
}

void
ConstrainedItem::dump (std::ostream& out)
{
	out << _item.name << " value dump:\n\n";

	out << '\t' << "left: " << _left.value() << '\n';
	out << '\t' << "right: " << _right.value() << '\n';
	out << '\t' << "top: " << _top.value() << '\n';
	out << '\t' << "bottom: " << _bottom.value() << '\n';
	out << '\t' << "width: " << _width.value() << '\n';
	out << '\t' << "height: " << _height.value() << '\n';
}

bool
ConstrainedItem::involved (Constraint const & c) const
{
	if (c.involves (_left) ||
	    c.involves (_right) ||
	    c.involves (_top) ||
	    c.involves (_bottom) ||
	    c.involves (_width) ||
	    c.involves (_height)) {
		return true;
	}

	return false;
}

/*** BoxConstrainedItem */

BoxConstrainedItem::BoxConstrainedItem (Item& parent, PackOptions primary_axis_opts, PackOptions secondary_axis_opts)
	: ConstrainedItem (parent)
	, _center_x (_item.name + " center_x")
	, _center_y (_item.name + " center_y")
	, _left_margin (_item.name + " left_margin")
	, _right_margin (_item.name + " right_margin")
	, _top_margin (_item.name + " top_margin")
	, _bottom_margin (_item.name + " bottom_margin")
	, _primary_axis_pack_options (primary_axis_opts)
	, _secondary_axis_pack_options (secondary_axis_opts)
{
}

BoxConstrainedItem::~BoxConstrainedItem ()
{
}

bool
BoxConstrainedItem::involved (Constraint const & c) const
{
	if (ConstrainedItem::involved (c)) {
		return true;
	}

	if (c.involves (_center_x) ||
	    c.involves (_center_y) ||
	    c.involves (_left_margin) ||
	    c.involves (_right_margin) ||
	    c.involves (_top_margin) ||
	    c.involves (_bottom_margin)) {
		return true;
	}

	return false;
}

void
BoxConstrainedItem::dump (std::ostream& out)
{
	ConstrainedItem::dump (out);

	out << '\t' << "center_x: " << _center_x.value() << '\n';
	out << '\t' << "center_y: " << _center_y.value() << '\n';
	out << '\t' << "left_margin: " << _left_margin.value() << '\n';
	out << '\t' << "right_margin: " << _right_margin.value() << '\n';
	out << '\t' << "top_margin: " << _top_margin.value() << '\n';
	out << '\t' << "bottom_margin: " << _bottom_margin.value() << '\n';

	out << '\t' << "right_padding: " << _right_padding.value() << '\n';
	out << '\t' << "left_padding: " << _left_padding.value() << '\n';
	out << '\t' << "top_padding: " << _top_padding.value() << '\n';
	out << '\t' << "bottom_padding: " << _bottom_padding.value() << '\n';
}

