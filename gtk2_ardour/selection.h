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

*/

#ifndef __ardour_gtk_selection_h__
#define __ardour_gtk_selection_h__

#include <vector>
#include <boost/shared_ptr.hpp>
#include <boost/noncopyable.hpp>

#include <sigc++/signal.h>

#include "pbd/signals.h"

#include "time_selection.h"
#include "region_selection.h"
#include "track_selection.h"
#include "automation_selection.h"
#include "playlist_selection.h"
#include "processor_selection.h"
#include "point_selection.h"
#include "marker_selection.h"
#include "midi_selection.h"

class TimeAxisView;
class RegionView;
class Selectable;
class PublicEditor;
class MidiRegionView;
class AutomationLine;
class ControlPoint;


namespace ARDOUR {
	class Region;
	class AudioRegion;
	class Playlist;
	class Processor;
	class AutomationList;
}

namespace Evoral {
	class ControlList;
}

/// Lists of selected things

/** The Selection class holds lists of selected items (tracks, regions, etc. etc.). */

class Selection : public sigc::trackable, public PBD::ScopedConnectionList
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
	PointSelection       points;
	MarkerSelection      markers;
	MidiRegionSelection  midi_regions;

	/** only used when this class is used as a cut buffer */
	MidiNoteSelection    midi_notes;

	Selection (PublicEditor const * e);

	// Selection& operator= (const Selection& other);

	sigc::signal<void> RegionsChanged;
	sigc::signal<void> TracksChanged;
	sigc::signal<void> TimeChanged;
	sigc::signal<void> LinesChanged;
	sigc::signal<void> PlaylistsChanged;
	sigc::signal<void> PointsChanged;
	sigc::signal<void> MarkersChanged;
	sigc::signal<void> MidiNotesChanged;
	sigc::signal<void> MidiRegionsChanged;

	void block_tracks_changed (bool);

	void clear ();
	bool empty (bool internal_selection = false);

	void dump_region_layers();

	bool selected (TimeAxisView*);
	bool selected (RegionView*);
	bool selected (Marker*);

	void set (std::list<Selectable*> const &);
	void add (std::list<Selectable*> const &);
	void toggle (std::list<Selectable*> const &);

	void set (TimeAxisView*);
	void set (const TrackViewList&);
	void set (const MidiNoteSelection&);
	void set (RegionView*, bool also_clear_tracks = true);
	void set (MidiRegionView*);
	void set (std::vector<RegionView*>&);
	long set (framepos_t, framepos_t);
	void set_preserving_all_ranges (framepos_t, framepos_t);
	void set (boost::shared_ptr<Evoral::ControlList>);
	void set (boost::shared_ptr<ARDOUR::Playlist>);
	void set (const std::list<boost::shared_ptr<ARDOUR::Playlist> >&);
	void set (ControlPoint *);
	void set (Marker*);
	void set (const RegionSelection&);

	void toggle (TimeAxisView*);
	void toggle (const TrackViewList&);
	void toggle (const MidiNoteSelection&);
	void toggle (RegionView*);
	void toggle (MidiRegionView*);
	void toggle (MidiCutBuffer*);
	void toggle (std::vector<RegionView*>&);
	long toggle (framepos_t, framepos_t);
	void toggle (ARDOUR::AutomationList*);
	void toggle (boost::shared_ptr<ARDOUR::Playlist>);
	void toggle (const std::list<boost::shared_ptr<ARDOUR::Playlist> >&);
	void toggle (ControlPoint *);
	void toggle (std::vector<ControlPoint*> const &);
	void toggle (Marker*);

	void add (TimeAxisView*);
	void add (const TrackViewList&);
	void add (const MidiNoteSelection&);
	void add (RegionView*);
	void add (MidiRegionView*);
	void add (MidiCutBuffer*);
	void add (std::vector<RegionView*>&);
	long add (framepos_t, framepos_t);
	void add (boost::shared_ptr<Evoral::ControlList>);
	void add (boost::shared_ptr<ARDOUR::Playlist>);
	void add (const std::list<boost::shared_ptr<ARDOUR::Playlist> >&);
	void add (ControlPoint *);
	void add (std::vector<ControlPoint*> const &);
	void add (Marker*);
	void add (const std::list<Marker*>&);
	void add (const RegionSelection&);
	void add (const PointSelection&);
	void remove (TimeAxisView*);
	void remove (const TrackViewList&);
	void remove (const MidiNoteSelection&);
	void remove (RegionView*);
	void remove (MidiRegionView*);
	void remove (MidiCutBuffer*);
	void remove (uint32_t selection_id);
	void remove (framepos_t, framepos_t);
	void remove (boost::shared_ptr<ARDOUR::AutomationList>);
	void remove (boost::shared_ptr<ARDOUR::Playlist>);
	void remove (const std::list<boost::shared_ptr<ARDOUR::Playlist> >&);
	void remove (const std::list<Selectable*>&);
	void remove (Marker*);
	void remove (ControlPoint *);

	void remove_regions (TimeAxisView *);

	void replace (uint32_t time_index, framepos_t start, framepos_t end);

	void clear_regions();
	void clear_tracks ();
	void clear_time();
	void clear_lines ();
	void clear_playlists ();
	void clear_points ();
	void clear_markers ();
	void clear_midi_notes ();
	void clear_midi_regions ();

	void foreach_region (void (ARDOUR::Region::*method)(void));
	void foreach_regionview (void (RegionView::*method)(void));
	void foreach_midi_regionview (void (MidiRegionView::*method)(void));
	template<class A> void foreach_region (void (ARDOUR::Region::*method)(A), A arg);

	XMLNode& get_state () const;
	int set_state (XMLNode const &, int);

  private:
	PublicEditor const * editor;
	uint32_t next_time_id;
	bool _no_tracks_changed;
};

bool operator==(const Selection& a, const Selection& b);

#endif /* __ardour_gtk_selection_h__ */
