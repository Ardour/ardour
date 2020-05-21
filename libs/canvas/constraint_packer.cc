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

#include "kiwi/kiwi.h"

#include "canvas/constraint_packer.h"
#include "canvas/rectangle.h"

using namespace ArdourCanvas;

ConstraintPacker::ConstraintPacker (Canvas* canvas)
	: Item (canvas)
{
}

ConstraintPacker::ConstraintPacker (Item* parent)
	: Item (parent)
{
}

void
ConstraintPacker::render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const
{
	Item::render_children (area, context);
}

void
ConstraintPacker::compute_bounding_box () const
{
	_bounding_box = Rect();

	if (_items.empty()) {
		_bounding_box_dirty = false;
		return;
	}

	add_child_bounding_boxes (!collapse_on_hide);

	_bounding_box_dirty = false;
}

void
ConstraintPacker::reset_self ()
{
	if (_bounding_box_dirty) {
		compute_bounding_box ();
	}

	if (!_bounding_box) {
		self->hide ();
		return;
	}

	Rect r (_bounding_box);

	/* XXX need to shrink by margin */

	self->set (r);
}

void
ConstraintPacker::reposition_children ()
{
	_bounding_box_dirty = true;
	reset_self ();
}
void
ConstraintPacker::child_changed ()
{
	/* catch visibility and size changes */

	Item::child_changed ();
	reposition_children ();
}

