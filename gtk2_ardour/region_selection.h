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

#ifndef __ardour_gtk_region_selection_h__
#define __ardour_gtk_region_selection_h__

#include <set>
#include <list>

#include "pbd/signals.h"
#include "ardour/types.h"

namespace ARDOUR {
	class Playlist;
}

class RegionView;
class TimeAxisView;

/** Class to represent list of selected regions.
 */
class RegionSelection : public std::list<RegionView*>
{
  public:
	RegionSelection();
	RegionSelection (const RegionSelection&);

	RegionSelection& operator= (const RegionSelection&);

	bool add (RegionView*);
	bool remove (RegionView*);
	void sort_by_position_and_track ();

	bool contains (RegionView*) const;
	bool involves (const TimeAxisView&) const;

	void clear_all();

	framepos_t start () const;

	/* "end" collides with list<>::end */

	framepos_t end_frame () const;

	const std::list<RegionView *>& by_layer() const { return _bylayer; }
	void  by_position (std::list<RegionView*>&) const;
	void  by_track (std::list<RegionView*>&) const;

	std::set<boost::shared_ptr<ARDOUR::Playlist> > playlists () const;

  private:
	void remove_it (RegionView*);

	void add_to_layer (RegionView *);

	std::list<RegionView *> _bylayer; ///< list of regions sorted by layer
	PBD::ScopedConnection death_connection;
};

#endif /* __ardour_gtk_region_selection_h__ */
