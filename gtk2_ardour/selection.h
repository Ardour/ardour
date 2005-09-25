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

#include <sigc++/signal_system.h>

#include "time_selection.h"
#include "region_selection.h"
#include "track_selection.h"
#include "automation_selection.h"
#include "playlist_selection.h"
#include "redirect_selection.h"
#include "point_selection.h"

class TimeAxisView;
class AudioRegionView;
class Selectable;

class Selection : public SigC::Object 
{
  public:
	enum SelectionType {
		Object = 0x1,
		Range = 0x2
	};

	TrackSelection       tracks;
	AudioRegionSelection audio_regions;
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

	SigC::Signal0<void> RegionsChanged;
	SigC::Signal0<void> TracksChanged;
	SigC::Signal0<void> TimeChanged;
	SigC::Signal0<void> LinesChanged;
	SigC::Signal0<void> PlaylistsChanged;
	SigC::Signal0<void> RedirectsChanged;
	SigC::Signal0<void> PointsChanged;

	void clear ();
	bool empty();

	void dump_region_layers();

	bool selected (TimeAxisView*);
	bool selected (AudioRegionView*);

	void set (list<Selectable*>&);
	void add (list<Selectable*>&);
	
	void set (TimeAxisView*);
	void set (const list<TimeAxisView*>&);
	void set (AudioRegionView*);
	void set (std::vector<AudioRegionView*>&);
	long set (TimeAxisView*, jack_nframes_t, jack_nframes_t);
	void set (ARDOUR::AutomationList*);
	void set (ARDOUR::Playlist*);
	void set (const list<ARDOUR::Playlist*>&);
	void set (ARDOUR::Redirect*);
	void set (AutomationSelectable*);

	void add (TimeAxisView*);
	void add (const list<TimeAxisView*>&);
	void add (AudioRegionView*);
	void add (std::vector<AudioRegionView*>&);
	long add (jack_nframes_t, jack_nframes_t);
	void add (ARDOUR::AutomationList*);
	void add (ARDOUR::Playlist*);
	void add (const list<ARDOUR::Playlist*>&);
	void add (ARDOUR::Redirect*);

	void remove (TimeAxisView*);
	void remove (const list<TimeAxisView*>&);
	void remove (AudioRegionView*);
	void remove (uint32_t selection_id);
	void remove (jack_nframes_t, jack_nframes_t);
	void remove (ARDOUR::AutomationList*);
	void remove (ARDOUR::Playlist*);
	void remove (const list<ARDOUR::Playlist*>&);
	void remove (ARDOUR::Redirect*);

	void replace (uint32_t time_index, jack_nframes_t start, jack_nframes_t end);
	
	void clear_audio_regions();
	void clear_tracks ();
	void clear_time();
	void clear_lines ();
	void clear_playlists ();
	void clear_redirects ();
	void clear_points ();

	void foreach_audio_region (void (ARDOUR::AudioRegion::*method)(void));
	void foreach_audio_region (void (ARDOUR::Region::*method)(void));
	template<class A> void foreach_audio_region (void (ARDOUR::AudioRegion::*method)(A), A arg);
	template<class A> void foreach_audio_region (void (ARDOUR::Region::*method)(A), A arg);

  private:
	uint32_t next_time_id;

	void add (vector<AutomationSelectable*>&);
};

bool operator==(const Selection& a, const Selection& b);

#endif /* __ardour_gtk_selection_h__ */
