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

#ifndef __CANVAS_LOOKUP_TABLE_H__
#define __CANVAS_LOOKUP_TABLE_H__

#include <vector>
#include <boost/multi_array.hpp>
#include "canvas/types.h"

class OptimizingLookupTableTest;

namespace ArdourCanvas {

class Item;
class Group;

class LookupTable
{
public:
	LookupTable (Group const &);
	virtual ~LookupTable ();

	virtual std::vector<Item*> get (Rect const &) = 0;
	virtual std::vector<Item*> items_at_point (Duple) const = 0;

protected:
	
	Group const & _group;
};

class DumbLookupTable : public LookupTable
{
public:
	DumbLookupTable (Group const &);

	std::vector<Item*> get (Rect const &);
	std::vector<Item*> items_at_point (Duple) const;
};

class OptimizingLookupTable : public LookupTable
{
public:
	OptimizingLookupTable (Group const &, int);
	~OptimizingLookupTable ();
	std::vector<Item*> get (Rect const &);
	std::vector<Item*> items_at_point (Duple) const;

	static int default_items_per_cell;

private:

	void area_to_indices (Rect const &, int &, int &, int &, int &) const;
	void point_to_indices (Duple, int &, int &) const;

	friend class ::OptimizingLookupTableTest;

	typedef std::vector<Item*> Cell;
	int _items_per_cell;
	int _dimension;
	Duple _cell_size;
	Duple _offset;
	Cell** _cells;
	bool _added;
};

}

#endif
