/*
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#include "canvas/item.h"
#include "canvas/lookup_table.h"

using namespace std;
using namespace ArdourCanvas;

LookupTable::LookupTable (Item const & item)
	: _item (item)
{

}

LookupTable::~LookupTable ()
{

}

DumbLookupTable::DumbLookupTable (Item const & item)
	: LookupTable (item)
{

}

vector<Item *>
DumbLookupTable::get (Rect const &area)
{
	list<Item *> const & items = _item.items ();
	vector<Item *> vitems;
#if 1
	for (list<Item *>::const_iterator i = items.begin(); i != items.end(); ++i) {
		Rect item_bbox = (*i)->bounding_box ();
		if (!item_bbox) continue;
		Rect item = (*i)->item_to_window (item_bbox);
		if (item.intersection (area)) {
			vitems.push_back (*i);
		}
	}
#else
	copy (items.begin(), items.end(), back_inserter (vitems));
#endif
	return vitems;
}

vector<Item *>
DumbLookupTable::items_at_point (Duple const & point) const
{
	/* Point is in window coordinate system */

	list<Item *> const & items (_item.items ());
	vector<Item *> vitems;

	for (list<Item *>::const_iterator i = items.begin(); i != items.end(); ++i) {

		if ((*i)->covers (point)) {
			vitems.push_back (*i);
		}
	}

	return vitems;
}

bool
DumbLookupTable::has_item_at_point (Duple const & point) const
{
	/* Point is in window coordinate system */

	list<Item *> const & items (_item.items ());
	vector<Item *> vitems;

	for (list<Item *>::const_iterator i = items.begin(); i != items.end(); ++i) {

		if (!(*i)->visible()) {
			continue;
		}

		if ((*i)->covers (point)) {
			// std::cerr << "\t\t" << (*i)->whatami() << '/' << (*i)->name << " covers " << point << std::endl;
			return true;

		}
	}

	return false;
}

OptimizingLookupTable::OptimizingLookupTable (Item const & item, int items_per_cell)
	: LookupTable (item)
	, _items_per_cell (items_per_cell)
	, _added (false)
{
	list<Item*> const & items = _item.items ();

	/* number of cells */
	int const cells = items.size() / _items_per_cell;
	/* hence number down each side of the table's square */
	_dimension = max (1, int (rint (sqrt ((double)cells))));

	_cells = new Cell*[_dimension];
	for (int i = 0; i < _dimension; ++i) {
		_cells[i] = new Cell[_dimension];
	}

	/* our item's bounding box in its coordinates */
	Rect bbox = _item.bounding_box ();
	if (!bbox) {
		return;
	}

	_cell_size.x = bbox.width() / _dimension;
	_cell_size.y = bbox.height() / _dimension;
	_offset.x = bbox.x0;
	_offset.y = bbox.y0;

//	cout << "BUILD bbox=" << bbox << ", cellsize=" << _cell_size << ", offset=" << _offset << ", dimension=" << _dimension << "\n";

	for (list<Item*>::const_iterator i = items.begin(); i != items.end(); ++i) {

		/* item bbox in its own coordinates */
		Rect item_bbox = (*i)->bounding_box ();
		if (!item_bbox) {
			continue;
		}

		/* and in the item's coordinates */
		Rect const item_bbox_in_item = (*i)->item_to_parent (item_bbox);

		int x0, y0, x1, y1;
		area_to_indices (item_bbox_in_item, x0, y0, x1, y1);

		/* XXX */
		assert (x0 >= 0);
		assert (y0 >= 0);
		assert (x1 >= 0);
		assert (y1 >= 0);
		//assert (x0 <= _dimension);
		//assert (y0 <= _dimension);
		//assert (x1 <= _dimension);
		//assert (y1 <= _dimension);

		if (x0 > _dimension) {
			cout << "WARNING: item outside bbox by " << (item_bbox_in_item.x0 - bbox.x0) << "\n";
			x0 = _dimension;
		}
		if (x1 > _dimension) {
			cout << "WARNING: item outside bbox by " << (item_bbox_in_item.x1 - bbox.x1) << "\n";
			x1 = _dimension;
		}
		if (y0 > _dimension) {
			cout << "WARNING: item outside bbox by " << (item_bbox_in_item.y0 - bbox.y0) << "\n";
			y0 = _dimension;
		}
		if (y1 > _dimension) {
			cout << "WARNING: item outside bbox by " << (item_bbox_in_item.y1 - bbox.y1) << "\n";
			y1 = _dimension;
		}

		for (int x = x0; x < x1; ++x) {
			for (int y = y0; y < y1; ++y) {
				_cells[x][y].push_back (*i);
			}
		}
	}
}

void
OptimizingLookupTable::area_to_indices (Rect const & area, int& x0, int& y0, int& x1, int& y1) const
{
	if (_cell_size.x == 0 || _cell_size.y == 0) {
		x0 = y0 = x1 = y1 = 0;
		return;
	}

	Rect const offset_area = area.translate (-_offset);

	x0 = floor (offset_area.x0 / _cell_size.x);
	y0 = floor (offset_area.y0 / _cell_size.y);
	x1 = ceil  (offset_area.x1 / _cell_size.x);
	y1 = ceil  (offset_area.y1 / _cell_size.y);
}

OptimizingLookupTable::~OptimizingLookupTable ()
{
	for (int i = 0; i < _dimension; ++i) {
		delete[] _cells[i];
	}

	delete[] _cells;
}

void
OptimizingLookupTable::point_to_indices (Duple point, int& x, int& y) const
{
	if (_cell_size.x == 0 || _cell_size.y == 0) {
		x = y = 0;
		return;
	}

	Duple const offset_point = point - _offset;

	x = floor (offset_point.x / _cell_size.x);
	y = floor (offset_point.y / _cell_size.y);
}

vector<Item*>
OptimizingLookupTable::items_at_point (Duple const & point) const
{
	int x;
	int y;
	point_to_indices (point, x, y);

	if (x >= _dimension) {
		cout << "WARNING: x=" << x << ", dim=" << _dimension << ", px=" << point.x << " cellsize=" << _cell_size << "\n";
	}

	if (y >= _dimension) {
		cout << "WARNING: y=" << y << ", dim=" << _dimension << ", py=" << point.y << " cellsize=" << _cell_size << "\n";
	}

	/* XXX: hmm */
	x = min (_dimension - 1, x);
	y = min (_dimension - 1, y);

	assert (x >= 0);
	assert (y >= 0);

	Cell const & cell = _cells[x][y];
	vector<Item*> items;
	for (Cell::const_iterator i = cell.begin(); i != cell.end(); ++i) {
		Rect const item_bbox = (*i)->bounding_box ();
		if (item_bbox) {
			Rect parent_bbox = (*i)->item_to_parent (item_bbox);
			if (parent_bbox.contains (point)) {
				items.push_back (*i);
			}
		}
	}

	return items;
}

bool
OptimizingLookupTable::has_item_at_point (Duple const & point) const
{
	int x;
	int y;
	point_to_indices (point, x, y);

	if (x >= _dimension) {
		cout << "WARNING: x=" << x << ", dim=" << _dimension << ", px=" << point.x << " cellsize=" << _cell_size << "\n";
	}

	if (y >= _dimension) {
		cout << "WARNING: y=" << y << ", dim=" << _dimension << ", py=" << point.y << " cellsize=" << _cell_size << "\n";
	}

	/* XXX: hmm */
	x = min (_dimension - 1, x);
	y = min (_dimension - 1, y);

	assert (x >= 0);
	assert (y >= 0);

	Cell const & cell = _cells[x][y];
	vector<Item*> items;
	for (Cell::const_iterator i = cell.begin(); i != cell.end(); ++i) {
		Rect const item_bbox = (*i)->bounding_box ();
		if (item_bbox) {
			Rect parent_bbox = (*i)->item_to_parent (item_bbox);
			if (parent_bbox.contains (point)) {
				return true;
			}
		}
	}

	return false;
}

/** @param area Area in our owning item's coordinates */
vector<Item*>
OptimizingLookupTable::get (Rect const & area)
{
	list<Item*> items;
	int x0, y0, x1, y1;
	area_to_indices (area, x0, y0, x1, y1);

	/* XXX: hmm... */
	x0 = min (_dimension - 1, x0);
	y0 = min (_dimension - 1, y0);
	x1 = min (_dimension, x1);
	y1 = min (_dimension, y1);

	for (int x = x0; x < x1; ++x) {
		for (int y = y0; y < y1; ++y) {
			for (Cell::const_iterator i = _cells[x][y].begin(); i != _cells[x][y].end(); ++i) {
				if (find (items.begin(), items.end(), *i) == items.end ()) {
					items.push_back (*i);
				}
			}
		}
	}

	vector<Item*> vitems;
	copy (items.begin (), items.end (), back_inserter (vitems));

	return vitems;
}

