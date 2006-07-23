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
#include <sigc++/signal.h>
#include <ardour/types.h>

using std::list;
using std::set;

class RegionView;

struct RegionComparator {
    bool operator() (const RegionView* a, const RegionView* b) const;
};

class RegionSelection : public set<RegionView*, RegionComparator>, public sigc::trackable
{
  public:
	RegionSelection();
	RegionSelection (const RegionSelection&);

	RegionSelection& operator= (const RegionSelection&);

	void add (RegionView*, bool dosort = true);
	bool remove (RegionView*);
	bool contains (RegionView*);

	void clear_all();
	
	jack_nframes_t start () const {
		return _current_start;
	}

	/* collides with list<>::end */

	jack_nframes_t end_frame () const { 
		return _current_end;
	}

	const list<RegionView *> & by_layer() const { return _bylayer; }
	void  by_position (list<RegionView*>&) const;
	
  private:
	void remove_it (RegionView*);

	void add_to_layer (RegionView *);
	
	jack_nframes_t _current_start;
	jack_nframes_t _current_end;

	list<RegionView *> _bylayer;
};

#endif /* __ardour_gtk_region_selection_h__ */
