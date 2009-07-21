/*
    Copyright (C) 2002 Paul Davis 

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
#include <sigc++/bind.h>
#include "pbd/error.h"
#include "pbd/stacktrace.h"

#include "ardour/playlist.h"
#include "ardour/rc_configuration.h"

#include "region_view.h"
#include "selection.h"
#include "selection_templates.h"
#include "time_axis_view.h"
#include "automation_time_axis.h"
#include "public_editor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;

struct AudioRangeComparator {
    bool operator()(AudioRange a, AudioRange b) {
	    return a.start < b.start;
    }
};

Selection&
Selection::operator= (const Selection& other)
{
	if (&other != this) {
		regions = other.regions;
		tracks = other.tracks;
		time = other.time;
		lines = other.lines;
	}
	return *this;
}

bool
operator== (const Selection& a, const Selection& b)
{
	return a.regions == b.regions &&
		a.tracks == b.tracks &&
		a.time.track == b.time.track &&
		a.time.group == b.time.group && 
		a.time == b.time &&
		a.lines == b.lines &&
		a.playlists == b.playlists;
}

/** Clear everything from the Selection */
void
Selection::clear ()
{
	clear_tracks ();
	clear_regions ();
	clear_points ();
	clear_lines();
	clear_time ();
	clear_playlists ();
}

void
Selection::dump_region_layers()
{
	cerr << "region selection layer dump" << endl;
	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		cerr << "layer: " << (int)(*i)->region()->layer() << endl;
	}
}


void
Selection::clear_regions ()
{
	if (!regions.empty()) {
		regions.clear_all ();
		RegionsChanged();
	}
}

void
Selection::clear_tracks ()
{
	if (!tracks.empty()) {
		tracks.clear ();
		TracksChanged();
	}
}

void
Selection::clear_time ()
{
	time.track = 0;
	time.group = 0;
	time.clear();

	TimeChanged ();
}

void
Selection::clear_playlists ()
{
	/* Selections own their playlists */

	for (PlaylistSelection::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		/* selections own their own regions, which are copies of the "originals". make them go away */
		(*i)->drop_regions ();
		(*i)->release ();
	}

	if (!playlists.empty()) {
		playlists.clear ();
		PlaylistsChanged();
	}
}

void
Selection::clear_lines ()
{
	if (!lines.empty()) {
		lines.clear ();
		LinesChanged();
	}
}

void
Selection::clear_markers ()
{
	if (!markers.empty()) {
		markers.clear ();
		MarkersChanged();
	}
}

void
Selection::toggle (boost::shared_ptr<Playlist> pl)
{
	PlaylistSelection::iterator i;

	if ((i = find (playlists.begin(), playlists.end(), pl)) == playlists.end()) {
		pl->use ();
		playlists.push_back(pl);
	} else {
		playlists.erase (i);
	}

	PlaylistsChanged ();
}

void
Selection::toggle (const list<TimeAxisView*>& track_list)
{
	for (list<TimeAxisView*>::const_iterator i = track_list.begin(); i != track_list.end(); ++i) {
		toggle ( (*i) );
	}
}

void
Selection::toggle (TimeAxisView* track)
{
	TrackSelection::iterator i;
	
	if ((i = find (tracks.begin(), tracks.end(), track)) == tracks.end()) {
		void (Selection::*pmf)(TimeAxisView*) = &Selection::remove;
		track->GoingAway.connect (sigc::bind (mem_fun (*this, pmf), track));
		tracks.push_back (track);
	} else {
		tracks.erase (i);
	}

	TracksChanged();
}

void
Selection::toggle (RegionView* r)
{
	RegionSelection::iterator i;

	if ((i = find (regions.begin(), regions.end(), r)) == regions.end()) {
		add (r);
	} else {
		remove (*i);
	}

	RegionsChanged ();
}

void
Selection::toggle (vector<RegionView*>& r)
{
	RegionSelection::iterator i;

	for (vector<RegionView*>::iterator x = r.begin(); x != r.end(); ++x) {
		if ((i = find (regions.begin(), regions.end(), (*x))) == regions.end()) {
			add ((*x));
		} else {
			remove (*x);
		}
	}

	RegionsChanged ();
}

long
Selection::toggle (nframes_t start, nframes_t end)
{
	AudioRangeComparator cmp;

	/* XXX this implementation is incorrect */

	time.push_back (AudioRange (start, end, next_time_id++));
	time.consolidate ();
	time.sort (cmp);
	
	TimeChanged ();

	return next_time_id - 1;
}

void
Selection::add (boost::shared_ptr<Playlist> pl)
{
	if (find (playlists.begin(), playlists.end(), pl) == playlists.end()) {
		pl->use ();
		playlists.push_back(pl);
		PlaylistsChanged ();
	}
}

void
Selection::add (const list<boost::shared_ptr<Playlist> >& pllist)
{
	bool changed = false;

	for (list<boost::shared_ptr<Playlist> >::const_iterator i = pllist.begin(); i != pllist.end(); ++i) {
		if (find (playlists.begin(), playlists.end(), (*i)) == playlists.end()) {
			(*i)->use ();
			playlists.push_back (*i);
			changed = true;
		}
	}
	
	if (changed) {
		PlaylistsChanged ();
	}
}

void
Selection::add (const list<TimeAxisView*>& track_list)
{
	list<TimeAxisView*> added = tracks.add (track_list);

	for (list<TimeAxisView*>::const_iterator i = added.begin(); i != added.end(); ++i) {
		void (Selection::*pmf)(TimeAxisView*) = &Selection::remove;
		(*i)->GoingAway.connect (sigc::bind (mem_fun (*this, pmf), (*i)));
	}
	
	if (!added.empty()) {
		TracksChanged ();
	}
}

void
Selection::add (TimeAxisView* track)
{
	if (find (tracks.begin(), tracks.end(), track) == tracks.end()) {
		void (Selection::*pmf)(TimeAxisView*) = &Selection::remove;
		track->GoingAway.connect (sigc::bind (mem_fun (*this, pmf), track));
		tracks.push_back (track);
		TracksChanged();
	}
}

void
Selection::add (vector<RegionView*>& v)
{
	/* XXX This method or the add (const RegionSelection&) needs to go
	 */

	bool changed = false;
	
	for (vector<RegionView*>::iterator i = v.begin(); i != v.end(); ++i) {
		if (find (regions.begin(), regions.end(), (*i)) == regions.end()) {
			changed = regions.add ((*i));
			if (Config->get_link_region_and_track_selection() && changed) {
				add (&(*i)->get_trackview());
			}
		}
	}

	if (changed) {
		RegionsChanged ();
	}
}

void
Selection::add (const RegionSelection& rs)
{
	/* XXX This method or the add (const vector<RegionView*>&) needs to go
	 */

	bool changed = false;
	
	for (RegionSelection::const_iterator i = rs.begin(); i != rs.end(); ++i) {
		if (find (regions.begin(), regions.end(), (*i)) == regions.end()) {
			changed = regions.add ((*i));
			if (Config->get_link_region_and_track_selection() && changed) {
				add (&(*i)->get_trackview());
			}
		}
	}
	
	if (changed) {
		RegionsChanged ();
	}
}

void
Selection::add (RegionView* r)
{
	if (find (regions.begin(), regions.end(), r) == regions.end()) {
		regions.add (r);
		if (Config->get_link_region_and_track_selection()) {
			add (&r->get_trackview());
		}
		RegionsChanged ();
	}
}

long
Selection::add (nframes_t start, nframes_t end)
{
	AudioRangeComparator cmp;

	/* XXX this implementation is incorrect */

	time.push_back (AudioRange (start, end, next_time_id++));
	time.consolidate ();
	time.sort (cmp);
	
	TimeChanged ();

	return next_time_id - 1;
}

void
Selection::replace (uint32_t sid, nframes_t start, nframes_t end)
{
	for (list<AudioRange>::iterator i = time.begin(); i != time.end(); ++i) {
		if ((*i).id == sid) {
			time.erase (i);
			time.push_back (AudioRange(start,end, sid));

			/* don't consolidate here */


			AudioRangeComparator cmp;
			time.sort (cmp);

			TimeChanged ();
			break;
		}
	}
}

void
Selection::add (boost::shared_ptr<Evoral::ControlList> cl)
{
	boost::shared_ptr<ARDOUR::AutomationList> al
		= boost::dynamic_pointer_cast<ARDOUR::AutomationList>(cl);
	if (!al) {
		warning << "Programming error: Selected list is not an ARDOUR::AutomationList" << endmsg;
		return;
		return;
	}
	if (find (lines.begin(), lines.end(), al) == lines.end()) {
		lines.push_back (al);
		LinesChanged();
	}
}

void
Selection::remove (TimeAxisView* track)
{
	list<TimeAxisView*>::iterator i;
	if ((i = find (tracks.begin(), tracks.end(), track)) != tracks.end()) {
		tracks.erase (i);
		TracksChanged();
	}
}

void
Selection::remove (const list<TimeAxisView*>& track_list)
{
	bool changed = false;

	for (list<TimeAxisView*>::const_iterator i = track_list.begin(); i != track_list.end(); ++i) {

		list<TimeAxisView*>::iterator x;

		if ((x = find (tracks.begin(), tracks.end(), (*i))) != tracks.end()) {
			tracks.erase (x);
			changed = true;
		}
	}

	if (changed) {
		TracksChanged();
	}
}

void
Selection::remove (boost::shared_ptr<Playlist> track)
{
	list<boost::shared_ptr<Playlist> >::iterator i;
	if ((i = find (playlists.begin(), playlists.end(), track)) != playlists.end()) {
		playlists.erase (i);
		PlaylistsChanged();
	}
}

void
Selection::remove (const list<boost::shared_ptr<Playlist> >& pllist)
{
	bool changed = false;

	for (list<boost::shared_ptr<Playlist> >::const_iterator i = pllist.begin(); i != pllist.end(); ++i) {

		list<boost::shared_ptr<Playlist> >::iterator x;

		if ((x = find (playlists.begin(), playlists.end(), (*i))) != playlists.end()) {
			playlists.erase (x);
			changed = true;
		}
	}

	if (changed) {
		PlaylistsChanged();
	}
}

void
Selection::remove (RegionView* r)
{
	if (regions.remove (r)) {
		RegionsChanged ();
	}

	if (Config->get_link_region_and_track_selection() && !regions.involves (r->get_trackview())) {
		remove (&r->get_trackview());
	}
}


void
Selection::remove (uint32_t selection_id)
{
	if (time.empty()) {
		return;
	}

	for (list<AudioRange>::iterator i = time.begin(); i != time.end(); ++i) {
		if ((*i).id == selection_id) {
			time.erase (i);
						
			TimeChanged ();
			break;
		}
	}
}

void
Selection::remove (nframes_t /*start*/, nframes_t /*end*/)
{
}

void
Selection::remove (boost::shared_ptr<ARDOUR::AutomationList> ac)
{
	AutomationSelection::iterator i;
	if ((i = find (lines.begin(), lines.end(), ac)) != lines.end()) {
		lines.erase (i);
		LinesChanged();
	}
}

void
Selection::set (TimeAxisView* track)
{
	clear_tracks ();
	add (track);
}

void
Selection::set (const list<TimeAxisView*>& track_list)
{
	clear_tracks ();
	add (track_list);
}

void
Selection::set (boost::shared_ptr<Playlist> playlist)
{
	clear_playlists ();
	add (playlist);
}

void
Selection::set (const list<boost::shared_ptr<Playlist> >& pllist)
{
	clear_playlists ();
	add (pllist);
}

void
Selection::set (const RegionSelection& rs)
{
	clear_regions();
	regions = rs;
	RegionsChanged(); /* EMIT SIGNAL */
}

void
Selection::set (RegionView* r, bool also_clear_tracks)
{
	clear_regions ();
	if (also_clear_tracks) {
		clear_tracks ();
	}
	add (r);
}

void
Selection::set (vector<RegionView*>& v)
{
	clear_regions ();
	if (Config->get_link_region_and_track_selection()) {
		clear_tracks ();
		// make sure to deselect any automation selections
		clear_points();
	}
	add (v);
}

long
Selection::set (TimeAxisView* track, nframes_t start, nframes_t end)
{
	if ((start == 0 && end == 0) || end < start) {
		return 0;
	}

	if (time.empty()) {
		time.push_back (AudioRange (start, end, next_time_id++));
	} else {
		/* reuse the first entry, and remove all the rest */

		while (time.size() > 1) {
			time.pop_front();
		}
		time.front().start = start;
		time.front().end = end;
	}

	if (track) {
		time.track = track;
		time.group = track->route_group();
	} else {
		time.track = 0;
		time.group = 0;
	}

	time.consolidate ();

	TimeChanged ();

	return time.front().id;
}

void
Selection::set (boost::shared_ptr<Evoral::ControlList> ac)
{
	lines.clear();
	add (ac);
}

bool
Selection::selected (Marker* m)
{
	return find (markers.begin(), markers.end(), m) != markers.end();
}

bool
Selection::selected (TimeAxisView* tv)
{
	return find (tracks.begin(), tracks.end(), tv) != tracks.end();
}

bool
Selection::selected (RegionView* rv)
{
	return find (regions.begin(), regions.end(), rv) != regions.end();
}

bool
Selection::empty ()
{
	return regions.empty () &&
		tracks.empty () &&
		points.empty () && 
		playlists.empty () && 
		lines.empty () &&
		time.empty () &&
		playlists.empty () &&
		markers.empty()
		;
}

void
Selection::toggle (const vector<AutomationSelectable*>& autos)
{
	for (vector<AutomationSelectable*>::const_iterator x = autos.begin(); x != autos.end(); ++x) {
		if ((*x)->get_selected()) {
			points.remove (**x);
		} else {
			points.push_back (**x);
		}

		delete *x;
	}

	PointsChanged (); /* EMIT SIGNAL */
}

void
Selection::toggle (list<Selectable*>& selectables)
{
	RegionView* rv;
	AutomationSelectable* as;
	vector<RegionView*> rvs;
	vector<AutomationSelectable*> autos;

	for (std::list<Selectable*>::iterator i = selectables.begin(); i != selectables.end(); ++i) {
		if ((rv = dynamic_cast<RegionView*> (*i)) != 0) {
			rvs.push_back (rv);
		} else if ((as = dynamic_cast<AutomationSelectable*> (*i)) != 0) {
			autos.push_back (as);
		} else {
			fatal << _("programming error: ")
			      << X_("unknown selectable type passed to Selection::toggle()")
			      << endmsg;
			/*NOTREACHED*/
		}
	}

	if (!rvs.empty()) {
		toggle (rvs);
	} 

	if (!autos.empty()) {
		toggle (autos);
	} 
}

void
Selection::set (list<Selectable*>& selectables)
{
	clear_regions();
	clear_points ();
	add (selectables);
}


void
Selection::add (list<Selectable*>& selectables)
{
	RegionView* rv;
	AutomationSelectable* as;
	vector<RegionView*> rvs;
	vector<AutomationSelectable*> autos;

	for (std::list<Selectable*>::iterator i = selectables.begin(); i != selectables.end(); ++i) {
		if ((rv = dynamic_cast<RegionView*> (*i)) != 0) {
			rvs.push_back (rv);
		} else if ((as = dynamic_cast<AutomationSelectable*> (*i)) != 0) {
			autos.push_back (as);
		} else {
			fatal << _("programming error: ")
			      << X_("unknown selectable type passed to Selection::add()")
			      << endmsg;
			/*NOTREACHED*/
		}
	}

	if (!rvs.empty()) {
		add (rvs);
	} 

	if (!autos.empty()) {
		add (autos);
	} 
}

void
Selection::clear_points ()
{
	if (!points.empty()) {
		points.clear ();
		PointsChanged ();
	}
}

void
Selection::add (vector<AutomationSelectable*>& autos)
{
	for (vector<AutomationSelectable*>::iterator i = autos.begin(); i != autos.end(); ++i) {
		points.push_back (**i);
	}

	PointsChanged ();
}

void
Selection::set (Marker* m)
{
	clear_markers ();
	add (m);
}

void
Selection::toggle (Marker* m)
{
	MarkerSelection::iterator i;
	
	if ((i = find (markers.begin(), markers.end(), m)) == markers.end()) {
		add (m);
	} else {
		remove (m);
	}
}

void
Selection::remove (Marker* m)
{
	MarkerSelection::iterator i;

	if ((i = find (markers.begin(), markers.end(), m)) != markers.end()) {
		markers.erase (i);
		MarkersChanged();
	}
}

void
Selection::add (Marker* m)
{
	if (find (markers.begin(), markers.end(), m) == markers.end()) {

		/* disambiguate which remove() for the compiler */
		
		void (Selection::*pmf)(Marker*) = &Selection::remove;

		m->GoingAway.connect (bind (mem_fun (*this, pmf), m));

		markers.push_back (m);
		MarkersChanged();
	}
}

void
Selection::add (const list<Marker*>& m)
{
	markers.insert (markers.end(), m.begin(), m.end());
	MarkersChanged ();
}

void
MarkerSelection::range (nframes64_t& s, nframes64_t& e)
{
	s = max_frames;
	e = 0;

	for (MarkerSelection::iterator i = begin(); i != end(); ++i) {

		if ((*i)->position() < s) {
			s = (*i)->position();
		} 

		if ((*i)->position() > e) {
			e = (*i)->position();
		}
	}

	s = std::min (s, e);
	e = std::max (s, e);
}
