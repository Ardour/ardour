/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef __CANVAS_LOOKUP_TABLE_H__
#define __CANVAS_LOOKUP_TABLE_H__

#include <vector>
#include <boost/multi_array.hpp>

#include "canvas/visibility.h"
#include "canvas/types.h"

class OptimizingLookupTableTest;

namespace ArdourCanvas {

class Item;

class LIBCANVAS_API LookupTable
{
public:
    LookupTable (Item const &);
    virtual ~LookupTable ();

    virtual std::vector<Item*> get (Rect const &) = 0;
    virtual std::vector<Item*> items_at_point (Duple const &) const = 0;
    virtual bool has_item_at_point (Duple const & point) const = 0;

protected:

    Item const & _item;
};

class LIBCANVAS_API DumbLookupTable : public LookupTable
{
public:
	DumbLookupTable (Item const &);

    std::vector<Item*> get (Rect const &);
    std::vector<Item*> items_at_point (Duple const &) const;
    bool has_item_at_point (Duple const & point) const;
};

class LIBCANVAS_API OptimizingLookupTable : public LookupTable
{
public:
    OptimizingLookupTable (Item const &, int);
    ~OptimizingLookupTable ();
    std::vector<Item*> get (Rect const &);
    std::vector<Item*> items_at_point (Duple const &) const;
    bool has_item_at_point (Duple const & point) const;

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
