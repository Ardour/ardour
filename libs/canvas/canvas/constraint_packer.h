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

class LIBCANVAS_API ConstraintPacker : public Container
{
public:
	ConstraintPacker (Canvas *);
	ConstraintPacker (Item *);

	void add (Item *);
	void add_front (Item *);
	void remove (Item *);
	void constrain (kiwi::Constraint const &);

	virtual ConstrainedItem* add_constrained (Item* item);

	void solve ();
	void apply (kiwi::Solver*);

	void compute_bounding_box () const;

	void  preferred_size (Duple& mininum, Duple& natural) const;
	void size_allocate (Rect const &);

	kiwi::Variable width;
	kiwi::Variable height;

  protected:
	void child_changed (bool bbox_changed);

	typedef std::map<Item*,ConstrainedItem*> ConstrainedItemMap;
	ConstrainedItemMap constrained_map;
	typedef std::list<kiwi::Constraint> ConstraintList;
	ConstraintList constraint_list;
	kiwi::Solver _solver;
	bool in_alloc;
	bool _need_constraint_update;

	void add_constrained_internal (Item*, ConstrainedItem*);

	void add_constraints (kiwi::Solver&, ConstrainedItem*) const;

	void non_const_preferred_size (Duple& mininum, Duple& natural);
	virtual void update_constraints ();
};

}

#endif
