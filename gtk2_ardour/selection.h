/*
    Copyright (C) 2000-2003 Paul Davis 

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

    $Id$
*/

#ifndef __ardour_gtk_selection_h__
#define __ardour_gtk_selection_h__

#include <vector>
#include <boost/shared_ptr.hpp>

#include <sigc++/signal.h>

#include "time_selection.h"
#include "region_selection.h"
#include "track_selection.h"
#include "automation_selection.h"
#include "playlist_selection.h"
#include "redirect_selection.h"
#include "point_selection.h"

class TimeAxisView;
class RegionView;
class Selectable;

namespace ARDOUR {
	class Region;
	class AudioRegion;
	class Playlist;
	class Redirect;
	class AutomationList;
}

class Selection : public sigc::trackable 
{
  public:
	enum SelectionType {
		Object = 0x1,
		Range = 0x2
	};

	enum Operation {
		Set,
		Add,
		Toggle,
		Extend
	};

	TrackSelection       tracks;
	RegionSelection      regions;
	TimeSelection        time;
	AutomationSelection  lines;
	PlaylistSelection    playlists;
	RedirectSelection    redirects;
	PointSelection       points;

	Selection() {
		next_time_id = 0;
		clear();
	}

	Selection& operator= (const Selection& other);

	sigc::signal<void> RegionsChanged;
	sigc::signal<void> TracksChanged;
	sigc::signal<void> TimeChanged;
	sigc::signal<void> LinesChanged;
	sigc::signal<void> PlaylistsChanged;
	sigc::signal<void> RedirectsChanged;
	sigc::signal<void> PointsChanged;

	void clear ();
	bool empty();

	void dump_region_layers();

	bool selected (TimeAxisView*);
	bool selected (RegionView*);

	void set (std::list<Selectable*>&);
	void add (std::list<Selectable*>&);
	void toggle (std::list<Selectable*>&);
	
	void set (TimeAxisView*);
	void set (const std::list<TimeAxisView*>&);
	void set (RegionView*);
	void set (std::vector<RegionView*>&);
	long set (TimeAxisView*, nframes_t, nframes_t);
	void set (ARDOUR::AutomationList*);
	void set (boost::shared_ptr<ARDOUR::Playlist>);
	void set (const std::list<boost::shared_ptr<ARDOUR::Playlist> >&);
	void set (boost::shared_ptr<ARDOUR::Redirect>);
	void set (AutomationSelectable*);

	void toggle (TimeAxisView*);
	void toggle (const std::list<TimeAxisView*>&);
	void toggle (RegionView*);
	void toggle (std::vector<RegionView*>&);
	long toggle (nframes_t, nframes_t);
	void toggle (ARDOUR::AutomationList*);
	void toggle (boost::shared_ptr<ARDOUR::Playlist>);
	void toggle (const std::list<boost::shared_ptr<ARDOUR::Playlist> >&);
	void toggle (boost::shared_ptr<ARDOUR::Redirect>);
	void toggle (const std::vector<AutomationSelectable*>&);

	void add (TimeAxisView*);
	void add (const std::list<TimeAxisView*>&);
	void add (RegionView*);
	void add (std::vector<RegionView*>&);
	long add (nframes_t, nframes_t);
	void add (ARDOUR::AutomationList*);
	void add (boost::shared_ptr<ARDOUR::Playlist>);
	void add (const std::list<boost::shared_ptr<ARDOUR::Playlist> >&);
	void add (boost::shared_ptr<ARDOUR::Redirect>);
	
	void remove (TimeAxisView*);
	void remove (const std::list<TimeAxisView*>&);
	void remove (RegionView*);
	void remove (uint32_t selection_id);
	void remove (nframes_t, nframes_t);
	void remove (ARDOUR::AutomationList*);
	void remove (boost::shared_ptr<ARDOUR::Playlist>);
	void remove (const std::list<boost::shared_ptr<ARDOUR::Playlist> >&);
	void remove (boost::shared_ptr<ARDOUR::Redirect>);
	void remove (const list<Selectable*>&);

	void replace (uint32_t time_index, nframes_t start, nframes_t end);
	
	void clear_regions();
	void clear_tracks ();
	void clear_time();
	void clear_lines ();
	void clear_playlists ();
	void clear_redirects ();
	void clear_points ();

	void foreach_region (void (ARDOUR::Region::*method)(void));
	template<class A> void foreach_region (void (ARDOUR::Region::*method)(A), A arg);

  private:
	uint32_t next_time_id;

	void add (std::vector<AutomationSelectable*>&);
};

bool operator==(const Selection& a, const Selection& b);

#endif /* __ardour_gtk_selection_h__ */
