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

#include "audio_region_view.h"
#include "debug.h"
#include "gui_thread.h"
#include "midi_cut_buffer.h"
#include "region_gain_line.h"
#include "region_view.h"
#include "selection.h"
#include "selection_templates.h"
#include "time_axis_view.h"
#include "automation_time_axis.h"
#include "public_editor.h"
#include "control_point.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

struct AudioRangeComparator {
	bool operator()(AudioRange a, AudioRange b) {
		return a.start < b.start;
	}
};

Selection::Selection (const PublicEditor* e)
	: tracks (e)
	, editor (e)
	, next_time_id (0)
	, _no_tracks_changed (false)
{
	clear ();

	/* we have disambiguate which remove() for the compiler */

	void (Selection::*track_remove)(TimeAxisView*) = &Selection::remove;
	TimeAxisView::CatchDeletion.connect (*this, MISSING_INVALIDATOR, boost::bind (track_remove, this, _1), gui_context());

	void (Selection::*marker_remove)(ArdourMarker*) = &Selection::remove;
	ArdourMarker::CatchDeletion.connect (*this, MISSING_INVALIDATOR, boost::bind (marker_remove, this, _1), gui_context());

	void (Selection::*point_remove)(ControlPoint*) = &Selection::remove;
	ControlPoint::CatchDeletion.connect (*this, MISSING_INVALIDATOR, boost::bind (point_remove, this, _1), gui_context());
}

#if 0
Selection&
Selection::operator= (const Selection& other)
{
	if (&other != this) {
		regions = other.regions;
		tracks = other.tracks;
		time = other.time;
		lines = other.lines;
		midi_regions = other.midi_regions;
		midi_notes = other.midi_notes;
	}
	return *this;
}
#endif

bool
operator== (const Selection& a, const Selection& b)
{
	return a.regions == b.regions &&
		a.tracks == b.tracks &&
		a.time == b.time &&
		a.lines == b.lines &&
		a.playlists == b.playlists &&
		a.midi_notes == b.midi_notes &&
		a.midi_regions == b.midi_regions;
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
	clear_midi_notes ();
	clear_midi_regions ();
	clear_markers ();
	pending_midi_note_selection.clear();
}

void
Selection::clear_objects ()
{
	clear_regions ();
	clear_points ();
	clear_lines();
	clear_playlists ();
	clear_midi_notes ();
	clear_midi_regions ();
}

void
Selection::clear_tracks ()
{
	if (!tracks.empty()) {
		for (TrackViewList::iterator x = tracks.begin(); x != tracks.end(); ++x) {
			(*x)->set_selected (false);
		}
		tracks.clear ();
		if (!_no_tracks_changed) {
			TracksChanged();
		}
	}
}

void
Selection::clear_time ()
{
	time.clear();

	TimeChanged ();
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
Selection::clear_midi_notes ()
{
	if (!midi_notes.empty()) {
		for (MidiNoteSelection::iterator x = midi_notes.begin(); x != midi_notes.end(); ++x) {
			delete *x;
		}
		midi_notes.clear ();
		MidiNotesChanged ();
	}

	// clear note selections for MRV's that have note selections
	// this will cause the MRV to be removed from the list
	for (MidiRegionSelection::iterator i = midi_regions.begin();
	     i != midi_regions.end();) {
		MidiRegionSelection::iterator tmp = i;
		++tmp;
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
		if (mrv) {
			mrv->clear_selection();
		}
		i = tmp;
	}
}

void
Selection::clear_midi_regions ()
{
	if (!midi_regions.empty()) {
		midi_regions.clear ();
		MidiRegionsChanged ();
	}
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
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

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
Selection::toggle (const TrackViewList& track_list)
{
	for (TrackViewList::const_iterator i = track_list.begin(); i != track_list.end(); ++i) {
		toggle ((*i));
	}
}

void
Selection::toggle (TimeAxisView* track)
{
	TrackSelection::iterator i;

	if ((i = find (tracks.begin(), tracks.end(), track)) == tracks.end()) {
		track->set_selected (true);
		tracks.push_back (track);
	} else {
		track->set_selected (false);
		tracks.erase (i);
	}

	if (!_no_tracks_changed) {
		TracksChanged();
	}
}

void
Selection::toggle (const MidiNoteSelection& midi_note_list)
{
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	for (MidiNoteSelection::const_iterator i = midi_note_list.begin(); i != midi_note_list.end(); ++i) {
		toggle ((*i));
	}
}

void
Selection::toggle (MidiCutBuffer* midi)
{
	MidiNoteSelection::iterator i;

	if ((i = find (midi_notes.begin(), midi_notes.end(), midi)) == midi_notes.end()) {
		midi_notes.push_back (midi);
	} else {
		/* remember that we own the MCB */
		delete *i;
		midi_notes.erase (i);
	}

	MidiNotesChanged();
}


void
Selection::toggle (RegionView* r)
{
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	RegionSelection::iterator i;

	if ((i = find (regions.begin(), regions.end(), r)) == regions.end()) {
		add (r);
	} else {
		remove (*i);
	}

	RegionsChanged ();
}

void
Selection::toggle (MidiRegionView* mrv)
{
	clear_time();   //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	MidiRegionSelection::iterator i;

	if ((i = find (midi_regions.begin(), midi_regions.end(), mrv)) == midi_regions.end()) {
		add (mrv);
	} else {
		midi_regions.erase (i);
	}

	MidiRegionsChanged ();
}

void
Selection::toggle (vector<RegionView*>& r)
{
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

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
Selection::toggle (framepos_t start, framepos_t end)
{
	clear_objects();  //enforce object/range exclusivity

	AudioRangeComparator cmp;

	/* XXX this implementation is incorrect */

	time.push_back (AudioRange (start, end, ++next_time_id));
	time.consolidate ();
	time.sort (cmp);

	TimeChanged ();

	return next_time_id;
}

void
Selection::add (boost::shared_ptr<Playlist> pl)
{
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	if (find (playlists.begin(), playlists.end(), pl) == playlists.end()) {
		pl->use ();
		playlists.push_back(pl);
		PlaylistsChanged ();
	}
}

void
Selection::add (const list<boost::shared_ptr<Playlist> >& pllist)
{
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

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
Selection::add (const TrackViewList& track_list)
{
	clear_objects();  //enforce object/range exclusivity

	TrackViewList added = tracks.add (track_list);

	if (!added.empty()) {
		for (TrackViewList::iterator x = added.begin(); x != added.end(); ++x) {
			(*x)->set_selected (true);
		}
		if (!_no_tracks_changed) {
			TracksChanged ();
		}
	}
}

void
Selection::add (TimeAxisView* track)
{
	clear_objects();  //enforce object/range exclusivity

	TrackViewList tr;
	track->set_selected (true);
	tr.push_back (track);
	add (tr);
}

void
Selection::add (const MidiNoteSelection& midi_list)
{
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	const MidiNoteSelection::const_iterator b = midi_list.begin();
	const MidiNoteSelection::const_iterator e = midi_list.end();

	if (!midi_list.empty()) {
		midi_notes.insert (midi_notes.end(), b, e);
		MidiNotesChanged ();
	}
}

void
Selection::add (MidiCutBuffer* midi)
{
	/* we take ownership of the MCB */

	if (find (midi_notes.begin(), midi_notes.end(), midi) == midi_notes.end()) {
		midi_notes.push_back (midi);
		MidiNotesChanged ();
	}
}

void
Selection::add (vector<RegionView*>& v)
{
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	/* XXX This method or the add (const RegionSelection&) needs to go
	 */

	bool changed = false;

	for (vector<RegionView*>::iterator i = v.begin(); i != v.end(); ++i) {
		if (find (regions.begin(), regions.end(), (*i)) == regions.end()) {
			changed = regions.add ((*i));
		}
	}

	if (changed) {
		RegionsChanged ();
	}
}

void
Selection::add (const RegionSelection& rs)
{
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	/* XXX This method or the add (const vector<RegionView*>&) needs to go
	 */

	bool changed = false;

	for (RegionSelection::const_iterator i = rs.begin(); i != rs.end(); ++i) {
		if (find (regions.begin(), regions.end(), (*i)) == regions.end()) {
			changed = regions.add ((*i));
		}
	}

	if (changed) {
		RegionsChanged ();
	}
}

void
Selection::add (RegionView* r)
{
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	if (find (regions.begin(), regions.end(), r) == regions.end()) {
		bool changed = regions.add (r);
		if (changed) {
			RegionsChanged ();
		}
	}
}

void
Selection::add (MidiRegionView* mrv)
{
	DEBUG_TRACE(DEBUG::Selection, string_compose("Selection::add MRV %1\n", mrv));

	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	if (find (midi_regions.begin(), midi_regions.end(), mrv) == midi_regions.end()) {
		midi_regions.push_back (mrv);
		/* XXX should we do this? */
		MidiRegionsChanged ();
	}
}

long
Selection::add (framepos_t start, framepos_t end)
{
	clear_objects();  //enforce object/range exclusivity

	AudioRangeComparator cmp;

	/* XXX this implementation is incorrect */

	time.push_back (AudioRange (start, end, ++next_time_id));
	time.consolidate ();
	time.sort (cmp);

	TimeChanged ();

	return next_time_id;
}

void
Selection::move_time (framecnt_t distance)
{
	if (distance == 0) {
		return;
	}

	for (list<AudioRange>::iterator i = time.begin(); i != time.end(); ++i) {
		(*i).start += distance;
		(*i).end += distance;
	}

	TimeChanged ();
}

void
Selection::replace (uint32_t sid, framepos_t start, framepos_t end)
{
	clear_objects();  //enforce object/range exclusivity

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
	clear_time();  //enforce object/range exclusivity
	clear_tracks();  //enforce object/track exclusivity

	boost::shared_ptr<ARDOUR::AutomationList> al
		= boost::dynamic_pointer_cast<ARDOUR::AutomationList>(cl);
	if (!al) {
		warning << "Programming error: Selected list is not an ARDOUR::AutomationList" << endmsg;
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
		track->set_selected (false);
		tracks.erase (i);
		if (!_no_tracks_changed) {
			TracksChanged();
		}
	}
}

void
Selection::remove (const TrackViewList& track_list)
{
	bool changed = false;

	for (TrackViewList::const_iterator i = track_list.begin(); i != track_list.end(); ++i) {

		TrackViewList::iterator x = find (tracks.begin(), tracks.end(), *i);
		if (x != tracks.end()) {
			(*i)->set_selected (false);
			tracks.erase (x);
			changed = true;
		}
	}

	if (changed) {
		if (!_no_tracks_changed) {
			TracksChanged();
		}
	}
}

void
Selection::remove (ControlPoint* p)
{
	PointSelection::iterator i = find (points.begin(), points.end(), p);
	if (i != points.end ()) {
		points.erase (i);
	}
}

void
Selection::remove (const MidiNoteSelection& midi_list)
{
	bool changed = false;

	for (MidiNoteSelection::const_iterator i = midi_list.begin(); i != midi_list.end(); ++i) {

		MidiNoteSelection::iterator x;

		if ((x = find (midi_notes.begin(), midi_notes.end(), (*i))) != midi_notes.end()) {
			midi_notes.erase (x);
			changed = true;
		}
	}

	if (changed) {
		MidiNotesChanged();
	}
}

void
Selection::remove (MidiCutBuffer* midi)
{
	MidiNoteSelection::iterator x;

	if ((x = find (midi_notes.begin(), midi_notes.end(), midi)) != midi_notes.end()) {
		/* remember that we own the MCB */
		delete *x;
		midi_notes.erase (x);
		MidiNotesChanged ();
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
}

void
Selection::remove (MidiRegionView* mrv)
{
	DEBUG_TRACE(DEBUG::Selection, string_compose("Selection::remove MRV %1\n", mrv));

	MidiRegionSelection::iterator x;

	if ((x = find (midi_regions.begin(), midi_regions.end(), mrv)) != midi_regions.end()) {
		midi_regions.erase (x);
		MidiRegionsChanged ();
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
Selection::remove (framepos_t /*start*/, framepos_t /*end*/)
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
	clear_objects();  //enforce object/range exclusivity
	clear_tracks ();
	add (track);
}

void
Selection::set (const TrackViewList& track_list)
{
	clear_objects();  //enforce object/range exclusivity
	clear_tracks ();
	add (track_list);
}

void
Selection::set (const MidiNoteSelection& midi_list)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity
	clear_objects ();
	add (midi_list);
}

void
Selection::set (boost::shared_ptr<Playlist> playlist)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity
	clear_objects ();
	add (playlist);
}

void
Selection::set (const list<boost::shared_ptr<Playlist> >& pllist)
{
	clear_time();  //enforce region/object exclusivity
	clear_objects ();
	add (pllist);
}

void
Selection::set (const RegionSelection& rs)
{
	clear_time();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity
	clear_objects();
	regions = rs;
	RegionsChanged(); /* EMIT SIGNAL */
}

void
Selection::set (MidiRegionView* mrv)
{
	clear_time();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity
	clear_objects ();
	add (mrv);
}

void
Selection::set (RegionView* r, bool /*also_clear_tracks*/)
{
	clear_time();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity
	clear_objects ();
	add (r);
}

void
Selection::set (vector<RegionView*>& v)
{
	clear_time();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity
	clear_objects();

	add (v);
}

/** Set the start and end time of the time selection, without changing
 *  the list of tracks it applies to.
 */
long
Selection::set (framepos_t start, framepos_t end)
{
	clear_objects();  //enforce region/object exclusivity
	clear_time();

	if ((start == 0 && end == 0) || end < start) {
		return 0;
	}

	if (time.empty()) {
		time.push_back (AudioRange (start, end, ++next_time_id));
	} else {
		/* reuse the first entry, and remove all the rest */

		while (time.size() > 1) {
			time.pop_front();
		}
		time.front().start = start;
		time.front().end = end;
	}

	time.consolidate ();

	TimeChanged ();

	return time.front().id;
}

/** Set the start and end of the range selection.  If more than one range
 *  is currently selected, the start of the earliest range and the end of the
 *  latest range are set.  If no range is currently selected, this method
 *  selects a single range from start to end.
 *
 *  @param start New start time.
 *  @param end New end time.
 */
void
Selection::set_preserving_all_ranges (framepos_t start, framepos_t end)
{
	clear_objects();  //enforce region/object exclusivity

	if ((start == 0 && end == 0) || (end < start)) {
		return;
	}

	if (time.empty ()) {
		time.push_back (AudioRange (start, end, ++next_time_id));
	} else {
		time.sort (AudioRangeComparator ());
		time.front().start = start;
		time.back().end = end;
	}

	time.consolidate ();

	TimeChanged ();
}

void
Selection::set (boost::shared_ptr<Evoral::ControlList> ac)
{
	clear_time();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity
	clear_objects();

	add (ac);
}

bool
Selection::selected (ArdourMarker* m)
{
	return find (markers.begin(), markers.end(), m) != markers.end();
}

bool
Selection::selected (TimeAxisView* tv)
{
	return tv->get_selected ();
}

bool
Selection::selected (RegionView* rv)
{
	return find (regions.begin(), regions.end(), rv) != regions.end();
}

bool
Selection::selected (ControlPoint* cp)
{
	return find (points.begin(), points.end(), cp) != points.end();
}

bool
Selection::empty (bool internal_selection)
{
	bool object_level_empty =  regions.empty () &&
		tracks.empty () &&
		points.empty () &&
		playlists.empty () &&
		lines.empty () &&
		time.empty () &&
		playlists.empty () &&
		markers.empty() &&
		midi_regions.empty()
		;

	if (!internal_selection) {
		return object_level_empty;
	}

	/* this is intended to really only apply when using a Selection
	   as a cut buffer.
	*/

	return object_level_empty && midi_notes.empty() && points.empty();
}

void
Selection::toggle (ControlPoint* cp)
{
	clear_time();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	cp->set_selected (!cp->get_selected ());
	PointSelection::iterator i = find (points.begin(), points.end(), cp);
	if (i == points.end()) {
		points.push_back (cp);
	} else {
		points.erase (i);
	}

	PointsChanged (); /* EMIT SIGNAL */
}

void
Selection::toggle (vector<ControlPoint*> const & cps)
{
	clear_time();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	for (vector<ControlPoint*>::const_iterator i = cps.begin(); i != cps.end(); ++i) {
		toggle (*i);
	}
}

void
Selection::toggle (list<Selectable*> const & selectables)
{
	clear_time();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	RegionView* rv;
	ControlPoint* cp;
	vector<RegionView*> rvs;
	vector<ControlPoint*> cps;

	for (std::list<Selectable*>::const_iterator i = selectables.begin(); i != selectables.end(); ++i) {
		if ((rv = dynamic_cast<RegionView*> (*i)) != 0) {
			rvs.push_back (rv);
		} else if ((cp = dynamic_cast<ControlPoint*> (*i)) != 0) {
			cps.push_back (cp);
		} else {
			fatal << _("programming error: ")
			      << X_("unknown selectable type passed to Selection::toggle()")
			      << endmsg;
			abort(); /*NOTREACHED*/
		}
	}

	if (!rvs.empty()) {
		toggle (rvs);
	}

	if (!cps.empty()) {
		toggle (cps);
	}
}

void
Selection::set (list<Selectable*> const & selectables)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity
	clear_objects ();

	add (selectables);
}

void
Selection::add (PointSelection const & s)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	for (PointSelection::const_iterator i = s.begin(); i != s.end(); ++i) {
		points.push_back (*i);
	}
}

void
Selection::add (list<Selectable*> const & selectables)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	RegionView* rv;
	ControlPoint* cp;
	vector<RegionView*> rvs;
	vector<ControlPoint*> cps;

	for (std::list<Selectable*>::const_iterator i = selectables.begin(); i != selectables.end(); ++i) {
		if ((rv = dynamic_cast<RegionView*> (*i)) != 0) {
			rvs.push_back (rv);
		} else if ((cp = dynamic_cast<ControlPoint*> (*i)) != 0) {
			cps.push_back (cp);
		} else {
			fatal << _("programming error: ")
			      << X_("unknown selectable type passed to Selection::add()")
			      << endmsg;
			abort(); /*NOTREACHED*/
		}
	}

	if (!rvs.empty()) {
		add (rvs);
	}

	if (!cps.empty()) {
		add (cps);
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
Selection::add (ControlPoint* cp)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	cp->set_selected (true);
	points.push_back (cp);
	PointsChanged (); /* EMIT SIGNAL */
}

void
Selection::add (vector<ControlPoint*> const & cps)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	for (vector<ControlPoint*>::const_iterator i = cps.begin(); i != cps.end(); ++i) {
		(*i)->set_selected (true);
		points.push_back (*i);
	}
	PointsChanged (); /* EMIT SIGNAL */
}

void
Selection::set (ControlPoint* cp)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	if (cp->get_selected () && points.size () == 1) {
		return;
	}

	for (uint32_t i = 0; i < cp->line().npoints(); ++i) {
		cp->line().nth (i)->set_selected (false);
	}

	clear_objects ();
	add (cp);
}

void
Selection::set (ArdourMarker* m)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity
	markers.clear ();

	add (m);
}

void
Selection::toggle (ArdourMarker* m)
{
	MarkerSelection::iterator i;

	if ((i = find (markers.begin(), markers.end(), m)) == markers.end()) {
		add (m);
	} else {
		remove (m);
	}
}

void
Selection::remove (ArdourMarker* m)
{
	MarkerSelection::iterator i;

	if ((i = find (markers.begin(), markers.end(), m)) != markers.end()) {
		markers.erase (i);
		MarkersChanged();
	}
}

void
Selection::add (ArdourMarker* m)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	if (find (markers.begin(), markers.end(), m) == markers.end()) {
		markers.push_back (m);
		MarkersChanged();
	}
}

void
Selection::add (const list<ArdourMarker*>& m)
{
	clear_time ();  //enforce region/object exclusivity
	clear_tracks();  //enforce object/track exclusivity

	markers.insert (markers.end(), m.begin(), m.end());
	markers.sort ();
	markers.unique ();

	MarkersChanged ();
}

void
MarkerSelection::range (framepos_t& s, framepos_t& e)
{
	s = max_framepos;
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

XMLNode&
Selection::get_state () const
{
	/* XXX: not complete; just sufficient to get track selection state
	   so that re-opening plugin windows for editor mixer strips works
	*/

	char buf[32];
	XMLNode* node = new XMLNode (X_("Selection"));

	for (TrackSelection::const_iterator i = tracks.begin(); i != tracks.end(); ++i) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (*i);
		if (rtv) {
			XMLNode* t = node->add_child (X_("RouteView"));
			t->add_property (X_("id"), atoi (rtv->route()->id().to_s().c_str()));
		} else if (atv) {
			XMLNode* t = node->add_child (X_("AutomationView"));
			t->add_property (X_("id"), atoi (atv->parent_route()->id().to_s().c_str()));
			t->add_property (X_("parameter"), EventTypeMap::instance().to_symbol (atv->parameter ()));
		}
	}

	for (RegionSelection::const_iterator i = regions.begin(); i != regions.end(); ++i) {
		XMLNode* r = node->add_child (X_("Region"));
		r->add_property (X_("id"), atoi ((*i)->region ()->id ().to_s ().c_str()));
	}

	/* midi region views have thir own internal selection. */
	XMLNode* n = NULL;
	list<pair<PBD::ID, std::set<boost::shared_ptr<Evoral::Note<Evoral::Beats> > > > > rid_notes;
	editor->get_per_region_note_selection (rid_notes);
	if (!rid_notes.empty()) {
		n = node->add_child (X_("MIDINote"));
	}
	list<pair<PBD::ID, std::set<boost::shared_ptr<Evoral::Note<Evoral::Beats> > > > >::iterator rn_it;
	for (rn_it = rid_notes.begin(); rn_it != rid_notes.end(); ++rn_it) {
		assert(n); // hint for clang static analysis
		n->add_property (X_("region_id"), atoi((*rn_it).first.to_s().c_str()));

		for (std::set<boost::shared_ptr<Evoral::Note<Evoral::Beats> > >::iterator i = (*rn_it).second.begin(); i != (*rn_it).second.end(); ++i) {
			XMLNode* nc = n->add_child(X_("note"));
			snprintf(buf, sizeof(buf), "%d", (*i)->channel());
			nc->add_property(X_("channel"), string(buf));

			snprintf(buf, sizeof(buf), "%f", (*i)->time().to_double());
			nc->add_property(X_("time"), string(buf));

			snprintf(buf, sizeof(buf), "%d", (*i)->note());
			nc->add_property(X_("note"), string(buf));

			snprintf(buf, sizeof(buf), "%f", (*i)->length().to_double());
			nc->add_property(X_("length"), string(buf));

			snprintf(buf, sizeof(buf), "%d", (*i)->velocity());
			nc->add_property(X_("velocity"), string(buf));

			snprintf(buf, sizeof(buf), "%d", (*i)->off_velocity());
			nc->add_property(X_("off-velocity"), string(buf));
		}
	}

	for (PointSelection::const_iterator i = points.begin(); i != points.end(); ++i) {
		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (&(*i)->line().trackview);
		if (atv) {

			XMLNode* r = node->add_child (X_("ControlPoint"));
			r->add_property (X_("type"), "track");
			r->add_property (X_("route-id"), atoi (atv->parent_route()->id ().to_s ().c_str()));
			r->add_property (X_("automation-list-id"), atoi ((*i)->line().the_list()->id ().to_s ().c_str()));
			r->add_property (X_("parameter"), EventTypeMap::instance().to_symbol ((*i)->line().the_list()->parameter ()));

			snprintf(buf, sizeof(buf), "%d", (*i)->view_index());
			r->add_property (X_("view-index"), string(buf));
			continue;
		}

		AudioRegionGainLine* argl = dynamic_cast<AudioRegionGainLine*> (&(*i)->line());
		if (argl) {
			XMLNode* r = node->add_child (X_("ControlPoint"));
			r->add_property (X_("type"), "region");
			r->add_property (X_("region-id"), atoi (argl->region_view ().region ()->id ().to_s ().c_str()));
			snprintf(buf, sizeof(buf), "%d", (*i)->view_index());
			r->add_property (X_("view-index"), string(buf));
		}

	}

	for (TimeSelection::const_iterator i = time.begin(); i != time.end(); ++i) {
		XMLNode* t = node->add_child (X_("AudioRange"));
		snprintf(buf, sizeof(buf), "%" PRId64, (*i).start);
		t->add_property (X_("start"), string(buf));
		snprintf(buf, sizeof(buf), "%" PRId64, (*i).end);
		t->add_property (X_("end"), string(buf));
	}

	for (MarkerSelection::const_iterator i = markers.begin(); i != markers.end(); ++i) {
		XMLNode* t = node->add_child (X_("Marker"));

		bool is_start;
		Location* loc = editor->find_location_from_marker (*i, is_start);

		t->add_property (X_("id"), atoi (loc->id().to_s().c_str()));
		t->add_property (X_("start"), is_start ? X_("yes") : X_("no"));
	}

	return *node;
}

int
Selection::set_state (XMLNode const & node, int)
{
	if (node.name() != X_("Selection")) {
		return -1;
	}

	clear_regions ();
	clear_midi_notes ();
	clear_points ();
	clear_time ();
	clear_tracks ();
	clear_markers ();

	XMLNodeList children = node.children ();
	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == X_("RouteView")) {

			XMLProperty* prop_id = (*i)->property (X_("id"));
			assert (prop_id);
			PBD::ID id (prop_id->value ());
			RouteTimeAxisView* rtv = editor->get_route_view_by_route_id (id);
			if (rtv) {
				add (rtv);
			}

		} else if ((*i)->name() == X_("Region")) {
			XMLProperty* prop_id = (*i)->property (X_("id"));
			assert (prop_id);
			PBD::ID id (prop_id->value ());

			RegionSelection rs;
			editor->get_regionviews_by_id (id, rs);

			if (!rs.empty ()) {
				add (rs);
			} else {
				/*
				  regionviews haven't been constructed - stash the region IDs
				  so we can identify them in Editor::region_view_added ()
				*/
				regions.pending.push_back (id);
			}

		} else if ((*i)->name() == X_("MIDINote")) {
			XMLProperty* prop_region_id = (*i)->property (X_("region-id"));

			assert (prop_region_id);

			PBD::ID const id (prop_region_id->value ());
			RegionSelection rs;

			editor->get_regionviews_by_id (id, rs); // there could be more than one

			std::list<boost::shared_ptr<Evoral::Note<Evoral::Beats> > > notes;
			XMLNodeList children = (*i)->children ();

			for (XMLNodeList::const_iterator ci = children.begin(); ci != children.end(); ++ci) {
				XMLProperty* prop_channel = (*ci)->property (X_("channel"));
				XMLProperty* prop_time = (*ci)->property (X_("time"));
				XMLProperty* prop_note = (*ci)->property (X_("note"));
				XMLProperty* prop_length = (*ci)->property (X_("length"));
				XMLProperty* prop_velocity = (*ci)->property (X_("velocity"));
				XMLProperty* prop_off_velocity = (*ci)->property (X_("off-velocity"));

				assert (prop_channel);
				assert (prop_time);
				assert (prop_note);
				assert (prop_length);
				assert (prop_velocity);
				assert (prop_off_velocity);

				uint8_t channel = atoi(prop_channel->value());
				Evoral::Beats time (atof(prop_time->value()));
				Evoral::Beats length (atof(prop_length->value()));
				uint8_t note = atoi(prop_note->value());
				uint8_t velocity = atoi(prop_velocity->value());
				uint8_t off_velocity = atoi(prop_off_velocity->value());
				boost::shared_ptr<Evoral::Note<Evoral::Beats> > the_note
					(new Evoral::Note<Evoral::Beats>  (channel, time, length, note, velocity));
				the_note->set_off_velocity (off_velocity);

				notes.push_back (the_note);
			}

			for (RegionSelection::iterator rsi = rs.begin(); rsi != rs.end(); ++rsi) {
				MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (*rsi);
				if (mrv) {
					mrv->select_notes(notes);
				}
			}

			if (rs.empty()) {
				/* regionviews containing these notes don't yet exist on the canvas.*/
				pending_midi_note_selection.push_back (make_pair (id, notes));
			}

		} else if  ((*i)->name() == X_("ControlPoint")) {
			XMLProperty* prop_type = (*i)->property (X_("type"));

			assert(prop_type);

			if (prop_type->value () == "track") {

				XMLProperty* prop_route_id = (*i)->property (X_("route-id"));
				XMLProperty* prop_alist_id = (*i)->property (X_("automation-list-id"));
				XMLProperty* prop_parameter = (*i)->property (X_("parameter"));
				XMLProperty* prop_view_index = (*i)->property (X_("view-index"));

				assert (prop_route_id);
				assert (prop_alist_id);
				assert (prop_parameter);
				assert (prop_view_index);

				PBD::ID route_id (prop_route_id->value ());
				RouteTimeAxisView* rtv = editor->get_route_view_by_route_id (route_id);
				vector <ControlPoint *> cps;

				if (rtv) {
					boost::shared_ptr<AutomationTimeAxisView> atv = rtv->automation_child (EventTypeMap::instance().from_symbol (prop_parameter->value ()));
					if (atv) {
						list<boost::shared_ptr<AutomationLine> > lines = atv->lines();
						for (list<boost::shared_ptr<AutomationLine> > ::iterator li = lines.begin(); li != lines.end(); ++li) {
							if ((*li)->the_list()->id() == prop_alist_id->value()) {
								ControlPoint* cp = (*li)->nth(atol(prop_view_index->value().c_str()));
								if (cp) {
									cps.push_back (cp);
									cp->show();
								}
							}
						}
					}
				}
				if (!cps.empty()) {
					add (cps);
				}
			} else if (prop_type->value () == "region") {
				XMLProperty* prop_region_id = (*i)->property (X_("region-id"));
				XMLProperty* prop_view_index = (*i)->property (X_("view-index"));

				if (!prop_region_id || !prop_view_index) {
					continue;
				}

				PBD::ID region_id (prop_region_id->value ());
				RegionSelection rs;
				editor->get_regionviews_by_id (region_id, rs);

				if (!rs.empty ()) {
					vector <ControlPoint *> cps;
					for (RegionSelection::iterator rsi = rs.begin(); rsi != rs.end(); ++rsi) {
						AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*rsi);
						if (arv) {
							boost::shared_ptr<AudioRegionGainLine> gl = arv->get_gain_line ();
							ControlPoint* cp = gl->nth(atol(prop_view_index->value().c_str()));
							if (cp) {
								cps.push_back (cp);
								cp->show();
							}
						}
					}
					if (!cps.empty()) {
						add (cps);
					}
				}
			}

		} else if  ((*i)->name() == X_("AudioRange")) {
			XMLProperty* prop_start = (*i)->property (X_("start"));
			XMLProperty* prop_end = (*i)->property (X_("end"));

			assert (prop_start);
			assert (prop_end);

			framepos_t s (atol (prop_start->value ().c_str()));
			framepos_t e (atol (prop_end->value ().c_str()));

			set_preserving_all_ranges (s, e);

		} else if ((*i)->name() == X_("AutomationView")) {

			XMLProperty* prop_id = (*i)->property (X_("id"));
			XMLProperty* prop_parameter = (*i)->property (X_("parameter"));

			assert (prop_id);
			assert (prop_parameter);

			PBD::ID id (prop_id->value ());
			RouteTimeAxisView* rtv = editor->get_route_view_by_route_id (id);

			if (rtv) {
				boost::shared_ptr<AutomationTimeAxisView> atv = rtv->automation_child (EventTypeMap::instance().from_symbol (prop_parameter->value ()));

				/* the automation could be for an entity that was never saved
				   in the session file. Don't freak out if we can't find
				   it.
				*/

				if (atv) {
					add (atv.get());
				}
			}

		} else if ((*i)->name() == X_("Marker")) {

			XMLProperty* prop_id = (*i)->property (X_("id"));
			XMLProperty* prop_start = (*i)->property (X_("start"));
			assert (prop_id);
			assert (prop_start);

			PBD::ID id (prop_id->value ());
			ArdourMarker* m = editor->find_marker_from_location_id (id, string_is_affirmative (prop_start->value ()));
			if (m) {
				add (m);
			}

		}

	}

	return 0;
}

void
Selection::remove_regions (TimeAxisView* t)
{
	RegionSelection::iterator i = regions.begin();
	while (i != regions.end ()) {
		RegionSelection::iterator tmp = i;
		++tmp;

		if (&(*i)->get_time_axis_view() == t) {
			remove (*i);
		}

		i = tmp;
	}
}

void
Selection::block_tracks_changed (bool yn)
{
	_no_tracks_changed = yn;
}
