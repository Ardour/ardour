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

#include "canvas/lookup_table.h"
#include "canvas/group.h"

using namespace std;
using namespace ArdourCanvas;

LookupTable::LookupTable (Group const & group)
	: _group (group)
{

}

LookupTable::~LookupTable ()
{

}

DumbLookupTable::DumbLookupTable (Group const & group)
	: LookupTable (group)
{

}

vector<Item *>
DumbLookupTable::get (Rect const &)
{
	list<Item *> const & items = _group.items ();
	vector<Item *> vitems;
	copy (items.begin(), items.end(), back_inserter (vitems));
	return vitems;
}

vector<Item *>
DumbLookupTable::items_at_point (Duple const & point) const
{
	/* Point is in canvas coordinate system */

	list<Item *> const & items (_group.items ());
	vector<Item *> vitems;

	for (list<Item *>::const_iterator i = items.begin(); i != items.end(); ++i) {

		if ((*i)->covers (point)) {
			// std::cerr << "\t\t" << (*i)->whatami() << '/' << (*i)->name << " covers " << point << std::endl;
			vitems.push_back (*i);
		}
	}

	return vitems;
}

bool
DumbLookupTable::has_item_at_point (Duple const & point) const
{
	/* Point is in canvas coordinate system */

	list<Item *> const & items (_group.items ());
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

OptimizingLookupTable::OptimizingLookupTable (Group const & group, int items_per_cell)
	: LookupTable (group)
	, _items_per_cell (items_per_cell)
	, _added (false)
{
	list<Item*> const & items = _group.items ();

	/* number of cells */
	int const cells = items.size() / _items_per_cell;
	/* hence number down each side of the table's square */
	_dimension = max (1, int (rint (sqrt ((double)cells))));

	_cells = new Cell*[_dimension];
	for (int i = 0; i < _dimension; ++i) {
		_cells[i] = new Cell[_dimension];
	}

	/* our group's bounding box in its coordinates */
	boost::optional<Rect> bbox = _group.bounding_box ();
	if (!bbox) {
		return;
	}

	_cell_size.x = bbox.get().width() / _dimension;
	_cell_size.y = bbox.get().height() / _dimension;
	_offset.x = bbox.get().x0;
	_offset.y = bbox.get().y0;

//	cout << "BUILD bbox=" << bbox.get() << ", cellsize=" << _cell_size << ", offset=" << _offset << ", dimension=" << _dimension << "\n";

	for (list<Item*>::const_iterator i = items.begin(); i != items.end(); ++i) {

		/* item bbox in its own coordinates */
		boost::optional<Rect> item_bbox = (*i)->bounding_box ();
		if (!item_bbox) {
			continue;
		}

		/* and in the group's coordinates */
		Rect const item_bbox_in_group = (*i)->item_to_parent (item_bbox.get ());

		int x0, y0, x1, y1;
		area_to_indices (item_bbox_in_group, x0, y0, x1, y1);

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
			cout << "WARNING: item outside bbox by " << (item_bbox_in_group.x0 - bbox.get().x0) << "\n";
			x0 = _dimension;
		}
		if (x1 > _dimension) {
			cout << "WARNING: item outside bbox by " << (item_bbox_in_group.x1 - bbox.get().x1) << "\n";
			x1 = _dimension;
		}
		if (y0 > _dimension) {
			cout << "WARNING: item outside bbox by " << (item_bbox_in_group.y0 - bbox.get().y0) << "\n";
			y0 = _dimension;
		}
		if (y1 > _dimension) {
			cout << "WARNING: item outside bbox by " << (item_bbox_in_group.y1 - bbox.get().y1) << "\n";
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
		boost::optional<Rect> const item_bbox = (*i)->bounding_box ();
		if (item_bbox) {
			Rect parent_bbox = (*i)->item_to_parent (item_bbox.get ());
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
		boost::optional<Rect> const item_bbox = (*i)->bounding_box ();
		if (item_bbox) {
			Rect parent_bbox = (*i)->item_to_parent (item_bbox.get ());
			if (parent_bbox.contains (point)) {
				return true;
			}
		}
	}

	return false;
}
	
/** @param area Area in our owning group's coordinates */
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

