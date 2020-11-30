/*
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2016 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2016-2017 Robin Gareus <robin@gareus.org>
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

#include <algorithm>

#include "ardour/region.h"

#include "gui_thread.h"
#include "midi_region_view.h"
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
	pending.clear ();
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

bool RegionSelection::contains (boost::shared_ptr<ARDOUR::Region> region) const
{
	for (const_iterator r = begin (); r != end (); ++r) {
		if ((*r)->region () == region) {
			return true;
		}
	}
	return false;
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
		pending.remove (rv->region()->id());
		erase (r);
		return true;
	}

	return false;
}

bool
RegionSelection::remove (vector<RegionView*> rv)
{
	RegionSelection::iterator r;
	bool removed_at_least_one = false;

	for (vector<RegionView*>::iterator rx = rv.begin(); rx != rv.end(); ++rx) {
		if ((r = find (begin(), end(), *rx)) != end()) {

			// remove from layer sorted list
			_bylayer.remove (*rx);
			pending.remove ((*rx)->region()->id());
			erase (r);
			removed_at_least_one = true;
		}
	}

	return removed_at_least_one;
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

timepos_t
RegionSelection::start_time () const
{
	if (empty()) {
		return timepos_t ();
	}

	timepos_t s = timepos_t::max (front()->region()->position().time_domain());

	for (RegionSelection::const_iterator i = begin(); i != end(); ++i) {
		s = min (s, (*i)->region()->position ());
	}

	if (s == timepos_t::max (front()->region()->position().time_domain())) {
	    return timepos_t ();
	}

	return s;
}

timepos_t
RegionSelection::end_time () const
{
	if (empty()) {
		return timepos_t ();
	}

	timepos_t e (timepos_t::zero (front()->region()->position().time_domain()));

	for (RegionSelection::const_iterator i = begin(); i != end(); ++i) {
		e = max (e, (*i)->region()->end ());
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

size_t
RegionSelection::n_midi_regions () const
{
	size_t count = 0;

	for (const_iterator r = begin(); r != end(); ++r) {
		MidiRegionView* const mrv = dynamic_cast<MidiRegionView*> (*r);
		if (mrv) {
			++count;
		}
	}

	return count;
}

ARDOUR::RegionList
RegionSelection::regionlist () const
{
	ARDOUR::RegionList rl;
	for (const_iterator r = begin (); r != end (); ++r) {
		rl.push_back ((*r)->region ());
	}
	return rl;
}
