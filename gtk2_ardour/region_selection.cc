/*
    Copyright (C) 2006 Paul Davis 

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

#include <algorithm>

#include <ardour/region.h>

#include "region_view.h"
#include "region_selection.h"

using namespace ARDOUR;
using namespace PBD;
using namespace sigc;


bool 
RegionComparator::operator() (const RegionView* a, const RegionView* b) const
{
 	if (a == b) {
 		return false;
 	} else {
 		return a < b;
 	}
}

RegionSelection::RegionSelection ()
{
	_current_start = 0;
	_current_end = 0;
}

RegionSelection::RegionSelection (const RegionSelection& other)
{

	for (RegionSelection::const_iterator i = other.begin(); i != other.end(); ++i) {
		add (*i, false);
	}
	_current_start = other._current_start;
	_current_end = other._current_end;
}



RegionSelection&
RegionSelection::operator= (const RegionSelection& other)
{
	if (this != &other) {

		clear_all();
		
		for (RegionSelection::const_iterator i = other.begin(); i != other.end(); ++i) {
			add (*i, false);
		}

		_current_start = other._current_start;
		_current_end = other._current_end;
	}

	return *this;
}

void
RegionSelection::clear_all()
{
	clear();
	_bylayer.clear();
}

bool RegionSelection::contains (RegionView* rv)
{
	if (this->find (rv) != end()) {
		return true;
	}
	else {
		return false;
	}
	
}

void
RegionSelection::add (RegionView* rv, bool dosort)
{
	if (this->find (rv) != end()) {
		/* we already have it */
		return;
	}

	rv->RegionViewGoingAway.connect (mem_fun(*this, &RegionSelection::remove_it));

	if (rv->region().first_frame() < _current_start || empty()) {
		_current_start = rv->region().first_frame();
	}
	
	if (rv->region().last_frame() > _current_end || empty()) {
		_current_end = rv->region().last_frame();
	}
	
	insert (rv);

	// add to layer sorted list
	add_to_layer (rv);
	
}

void
RegionSelection::remove_it (RegionView *rv)
{
	remove (rv);
}

bool
RegionSelection::remove (RegionView* rv)
{
	RegionSelection::iterator i;

	if ((i = this->find (rv)) != end()) {

		erase (i);

		// remove from layer sorted list
		_bylayer.remove (rv);
		
		if (empty()) {

			_current_start = 0;
			_current_end = 0;

		} else {
			
			Region& region ((*i)->region());

			if (region.first_frame() == _current_start) {
				
				/* reset current start */
				
				jack_nframes_t ref = max_frames;
				
				for (i = begin (); i != end(); ++i) {
					if (region.first_frame() < ref) {
						ref = region.first_frame();
					}
				}
				
				_current_start = ref;
				
			}
			
			if (region.last_frame() == _current_end) {

				/* reset current end */
				
				jack_nframes_t ref = 0;
				
				for (i = begin (); i != end(); ++i) {
					if (region.first_frame() > ref) {
						ref = region.first_frame();
					}
				}
				
				_current_end = ref;
			}
		}

		return true;
	}

	return false;
}

void
RegionSelection::add_to_layer (RegionView * rv)
{
	// insert it into layer sorted position

	list<RegionView*>::iterator i;

	for (i = _bylayer.begin(); i != _bylayer.end(); ++i)
	{
		if (rv->region().layer() < (*i)->region().layer()) {
			_bylayer.insert(i, rv);
			return;
		}
	}

	// insert at end if we get here
	_bylayer.insert(i, rv);
}

struct RegionSortByTime {
    bool operator() (const RegionView* a, const RegionView* b) {
	    return a->region().position() < b->region().position();
    }
};


void
RegionSelection::by_position (list<RegionView*>& foo) const
{
	list<RegionView*>::const_iterator i;
	RegionSortByTime sorter;

	for (i = _bylayer.begin(); i != _bylayer.end(); ++i) {
		foo.push_back (*i);
	}

	foo.sort (sorter);
	return;
}
