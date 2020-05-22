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

#include "canvas/item.h"
#include "kiwi/kiwi.h"

namespace ArdourCanvas
{

class Rectangle;
class ConstrainedItem;

class LIBCANVAS_API ConstraintPacker : public Item
{
public:
	ConstraintPacker (Canvas *);
	ConstraintPacker (Item *);

	void add (Item *);
	void remove (Item *);
	void constrain (kiwi::Constraint const &);

	void solve ();
	void apply ();

	void compute_bounding_box () const;
	void render (Rect const & area, Cairo::RefPtr<Cairo::Context> context) const;

	void size_allocate (Rect const &);

  protected:
	void child_changed ();

  private:
	typedef std::map<Item*,ConstrainedItem*> ConstraintMap;
	ConstraintMap constraint_map;

	kiwi::Solver _solver;
	kiwi::Variable width;
	kiwi::Variable height;

	Rectangle *self;
	bool collapse_on_hide;

	void reset_self ();
	void reposition_children ();
};

}

#endif
