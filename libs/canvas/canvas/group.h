/*
    Copyright (C) 2011-2013 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifndef __CANVAS_GROUP_H__
#define __CANVAS_GROUP_H__

#include <list>
#include <vector>
#include "canvas/item.h"
#include "canvas/types.h"
#include "canvas/lookup_table.h"

namespace ArdourCanvas {

class Group : public Item
{
public:
	explicit Group (Group *);
	explicit Group (Group *, Duple);
	~Group ();

	void render (Rect const &, Cairo::RefPtr<Cairo::Context>) const;
	virtual void compute_bounding_box () const;

	void add (Item *);
	void remove (Item *);
	std::list<Item*> const & items () const {
		return _items;
	}
    
	void raise_child_to_top (Item *);
	void raise_child (Item *, int);
	void lower_child_to_bottom (Item *);
	void child_changed ();

	void add_items_at_point (Duple, std::vector<Item const *> &) const;

        void dump (std::ostream&) const;

	static int default_items_per_cell;

protected:
	
	explicit Group (Canvas *);
	
private:
	friend class ::OptimizingLookupTableTest;
	
	Group (Group const &);
	void ensure_lut () const;
	void invalidate_lut () const;

	/* our items, from lowest to highest in the stack */
	std::list<Item*> _items;

	mutable LookupTable* _lut;
};

}

#endif
