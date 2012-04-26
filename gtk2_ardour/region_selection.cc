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

#include "gui_thread.h"
#include "region_view.h"
#include "region_selection.h"
#include "time_axis_view.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

/** Construct an empty RegionSelection.
 */
RegionSelection::RegionSelection ()
{
	RegionView::RegionViewGoingAway.connect (death_connection, MISSING_INVALIDATOR, boost::bind (&RegionSelection::remove_it, this, _1), gui_context());
}

/** Copy constructor.
 *  @param other RegionSelection to copy.
 */
RegionSelection::RegionSelection (const RegionSelection& other)
	: std::list<RegionView*>()
{
	RegionView::RegionViewGoingAway.connect (death_connection, MISSING_INVALIDATOR, boost::bind (&RegionSelection::remove_it, this, _1), gui_context());

	for (RegionSelection::const_iterator i = other.begin(); i != other.end(); ++i) {
		add (*i);
	}
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
 *  @return false if we already had the region or if it cannot be added,
 *          otherwise true.
 */
bool
RegionSelection::add (RegionView* rv)
{
        if (!rv->region()->playlist()) {
                /* not attached to a playlist - selection not allowed.
                   This happens if the user tries to select a region
                   during a capture pass.
                */
                return false;
        }

	if (contains (rv)) {
		/* we already have it */
		return false;
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

	    if (a->get_time_axis_view().order() == b->get_time_axis_view().order()) {
		    return a->region()->position() < b->region()->position();
	    } else {
		    return a->get_time_axis_view().order() < b->get_time_axis_view().order();
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
RegionSelection::involves (const TimeAxisView& tv) const
{
	for (RegionSelection::const_iterator i = begin(); i != end(); ++i) {
		if (&(*i)->get_time_axis_view() == &tv) {
			return true;
		}
	}
	return false;
}

framepos_t
RegionSelection::start () const
{
	framepos_t s = max_framepos;
	for (RegionSelection::const_iterator i = begin(); i != end(); ++i) {
		s = min (s, (*i)->region()->position ());
	}

	if (s == max_framepos) {
		return 0;
	}

	return s;
}

framepos_t
RegionSelection::end_frame () const
{
	framepos_t e = 0;
	for (RegionSelection::const_iterator i = begin(); i != end(); ++i) {
		e = max (e, (*i)->region()->last_frame ());
	}

	return e;
}

/** @return the playlists that the regions in the selection are on */
set<boost::shared_ptr<Playlist> >
RegionSelection::playlists () const
{
	set<boost::shared_ptr<Playlist> > pl;
	for (RegionSelection::const_iterator i = begin(); i != end(); ++i) {
		pl.insert ((*i)->region()->playlist ());
	}

	return pl;
}
