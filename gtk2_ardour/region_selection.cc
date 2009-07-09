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

#include "ardour/region.h"

#include "region_view.h"
#include "region_selection.h"
#include "time_axis_view.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;

/** Construct an empty RegionSelection.
 */
RegionSelection::RegionSelection ()
{
	RegionView::RegionViewGoingAway.connect (mem_fun(*this, &RegionSelection::remove_it));

	_current_start = 0;
	_current_end = 0;
}

/** Copy constructor.
 *  @param other RegionSelection to copy.
 */
RegionSelection::RegionSelection (const RegionSelection& other)
	: std::list<RegionView*>()
	, sigc::trackable(other)
{
	RegionView::RegionViewGoingAway.connect (mem_fun(*this, &RegionSelection::remove_it));

	for (RegionSelection::const_iterator i = other.begin(); i != other.end(); ++i) {
		add (*i);
	}
	_current_start = other._current_start;
	_current_end = other._current_end;
}

/** operator= to set a RegionSelection to be the same as another.
 *  @param other Other RegionSelection.
 */
RegionSelection&
RegionSelection::operator= (const RegionSelection& other)
{
	if (this != &other) {

		clear_all();
		
		for (RegionSelection::const_iterator i = other.begin(); i != other.end(); ++i) {
			add (*i);
		}

		_current_start = other._current_start;
		_current_end = other._current_end;
	}

	return *this;
}

/** Empty this RegionSelection.
 */
void
RegionSelection::clear_all()
{
	clear();
	_bylayer.clear();
	_current_start = 0;
	_current_end = 0;
}

/**
 *  @param rv RegionView.
 *  @return true if this selection contains rv.
 */
bool RegionSelection::contains (RegionView* rv) const
{
	return find (begin(), end(), rv) != end();
}

/** Add a region to the selection.
 *  @param rv Region to add.
 *  @return false if we already had the region, otherwise true.
 */
bool
RegionSelection::add (RegionView* rv)
{
	if (contains (rv)) {
		/* we already have it */
		return false;
	}

	if (rv->region()->first_frame() < _current_start || empty()) {
		_current_start = rv->region()->first_frame();
	}
	
	if (rv->region()->last_frame() > _current_end || empty()) {
		_current_end = rv->region()->last_frame();
	}
	
	push_back (rv);

	/* add to layer sorted list */

	add_to_layer (rv);

	return true;
}

/** Remove a region from the selection.
 *  @param rv Region to remove.
 */
void
RegionSelection::remove_it (RegionView *rv)
{
	remove (rv);
}

/** Remove a region from the selection.
 *  @param rv Region to remove.
 *  @return true if the region was in the selection, false if not.
 */
bool
RegionSelection::remove (RegionView* rv)
{
	RegionSelection::iterator r;

	if ((r = find (begin(), end(), rv)) != end()) {

		// remove from layer sorted list
		_bylayer.remove (rv);
		
		if (size() == 1) {

			/* this is the last one, so when we delete it
			   we will be empty.
			*/

			_current_start = 0;
			_current_end = 0;

		} else {
			
			boost::shared_ptr<Region> region ((*r)->region());
			
			if (region->first_frame() == _current_start) {
				
				/* reset current start */
				
				nframes_t ref = max_frames;
				
				for (RegionSelection::iterator i = begin (); i != end(); ++i) {
					if (region->first_frame() < ref) {
						ref = region->first_frame();
					}
				}
				
				_current_start = ref;
				
			}
			
			if (region->last_frame() == _current_end) {

				/* reset current end */
				
				nframes_t ref = 0;
				
				for (RegionSelection::iterator i = begin (); i != end(); ++i) {
					if (region->first_frame() > ref) {
						ref = region->first_frame();
					}
				}
				
				_current_end = ref;
			}
		}

		erase (r);

		return true;
	}

	return false;
}

/** Add a region to the list sorted by layer.
 *  @param rv Region to add.
 */
void
RegionSelection::add_to_layer (RegionView * rv)
{
	// insert it into layer sorted position

	list<RegionView*>::iterator i;

	for (i = _bylayer.begin(); i != _bylayer.end(); ++i)
	{
		if (rv->region()->layer() < (*i)->region()->layer()) {
			_bylayer.insert(i, rv);
			return;
		}
	}

	// insert at end if we get here
	_bylayer.insert(i, rv);
}

struct RegionSortByTime {
    bool operator() (const RegionView* a, const RegionView* b) const {
	    return a->region()->position() < b->region()->position();
    }
};


/**
 *  @param foo List which will be filled with the selection's regions
 *  sorted by position.
 */
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

struct RegionSortByTrack {
    bool operator() (const RegionView* a, const RegionView* b) const {
	    
	    /* really, track and position */

	    if (a->get_trackview()->order() == b->get_trackview()->order()) {
		    return a->region()->position() < b->region()->position();
	    } else {
		    return a->get_trackview()->order() < b->get_trackview()->order();
	    }
    }
};


/**
 *  @param List which will be filled with the selection's regions
 *  sorted by track and position.
 */
void
RegionSelection::by_track (list<RegionView*>& foo) const
{
	list<RegionView*>::const_iterator i;
	RegionSortByTrack sorter;

	for (i = _bylayer.begin(); i != _bylayer.end(); ++i) {
		foo.push_back (*i);
	}

	foo.sort (sorter);
	return;
}

/**
 *  @param Sort the selection by position and track.
 */
void
RegionSelection::sort_by_position_and_track ()
{
	RegionSortByTrack sorter;
	sort (sorter);
}

/**
 *  @param tv Track.
 *  @return true if any of the selection's regions are on tv.
 */
bool
RegionSelection::involves (TimeAxisViewPtr tv) const
{
	for (RegionSelection::const_iterator i = begin(); i != end(); ++i) {
		if ((*i)->get_trackview() == tv) {
			return true;
		}
	}
	return false;
}

