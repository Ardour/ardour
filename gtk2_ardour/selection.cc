/*
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2006-2017 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2017 Tim Mayberry <mojofunk@gmail.com>
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
#include <sigc++/bind.h>

#include "pbd/error.h"
#include "pbd/types_convert.h"

#include "ardour/evoral_types_convert.h"
#include "ardour/playlist.h"
#include "ardour/rc_configuration.h"
#include "ardour/selection.h"

#include "control_protocol/control_protocol.h"

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
#include "triggerbox_ui.h"
#include "vca_time_axis.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

struct TimelineRangeComparator {
	bool operator()(TimelineRange a, TimelineRange b) {
		return a.start() < b.start();
	}
};

Selection::Selection (const PublicEditor* e, bool mls)
	: editor (e)
	, next_time_id (0)
	, manage_libardour_selection (mls)
{
	clear ();

	/* we have disambiguate which remove() for the compiler */

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
		a.midi_notes == b.midi_notes;
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
	clear_markers ();
	clear_triggers ();
	pending_midi_note_selection.clear();
}

void
Selection::clear_objects (bool with_signal)
{
	clear_regions (with_signal);
	clear_points (with_signal);
	clear_lines(with_signal);
	clear_playlists (with_signal);
	clear_midi_notes (with_signal);
	clear_triggers (with_signal);
}

void
Selection::clear_time (bool with_signal)
{
	time.clear();
	if (with_signal) {
		TimeChanged ();
	}
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
Selection::clear_regions (bool with_signal)
{
	if (!regions.empty()) {
		regions.clear_all ();
		if (with_signal) {
			RegionsChanged();
		}
	}
}

void
Selection::clear_midi_notes (bool with_signal)
{
	/* Remember: MIDI notes are only stored here if we're using a Selection
	   object as a cut buffer.
	*/

	if (!midi_notes.empty()) {
		for (MidiNoteSelection::iterator x = midi_notes.begin(); x != midi_notes.end(); ++x) {
			delete *x;
		}
		midi_notes.clear ();
		if (with_signal) {
			MidiNotesChanged ();
		}
	}
}

void
Selection::clear_playlists (bool with_signal)
{
	/* Selections own their playlists */

	for (PlaylistSelection::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		/* selections own their own regions, which are copies of the "originals". make them go away */
		(*i)->drop_regions ();
		(*i)->release ();
	}

	if (!playlists.empty()) {
		playlists.clear ();
		if (with_signal) {
			PlaylistsChanged();
		}
	}
}

void
Selection::clear_lines (bool with_signal)
{
	if (!lines.empty()) {
		lines.clear ();
		if (with_signal) {
			LinesChanged();
		}
	}
}

void
Selection::clear_markers (bool with_signal)
{
	if (!markers.empty()) {
		markers.clear ();
		if (with_signal) {
			MarkersChanged();
		}
	}
}

void
Selection::clear_triggers (bool with_signal)
{
	if (!triggers.empty()) {
		triggers.clear ();
		if (with_signal) {
			TriggersChanged ();
		}
	}
}

RegionSelection
Selection::trigger_regionview_proxy () const
{
	RegionSelection rs;
	return rs;
}

void
Selection::toggle (boost::shared_ptr<Playlist> pl)
{
	clear_time(); // enforce object/range exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

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
Selection::toggle (const MidiNoteSelection& midi_note_list)
{
	clear_time(); // enforce object/range exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

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
	clear_time(); // enforce object/range exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

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
	clear_time(); // enforce object/range exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

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
Selection::toggle (timepos_t const & start, timepos_t const & end)
{
	clear_objects(); // enforce object/range exclusivity

	TimelineRangeComparator cmp;

	/* XXX this implementation is incorrect */

	time.push_back (TimelineRange (start, end, ++next_time_id));
	time.consolidate ();
	time.sort (cmp);

	TimeChanged ();

	return next_time_id;
}

void
Selection::add (boost::shared_ptr<Playlist> pl)
{

	if (find (playlists.begin(), playlists.end(), pl) == playlists.end()) {
		clear_time(); // enforce object/range exclusivity
		clear_tracks(); // enforce object/track exclusivity
		clear_triggers(); // enforce trigger exclusivity
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
		clear_time(); // enforce object/range exclusivity
		clear_tracks(); // enforce object/track exclusivity
		clear_triggers(); // enforce trigger exclusivity
		PlaylistsChanged ();
	}
}

void
Selection::add (const MidiNoteSelection& midi_list)
{
	const MidiNoteSelection::const_iterator b = midi_list.begin();
	const MidiNoteSelection::const_iterator e = midi_list.end();

	if (!midi_list.empty()) {
		clear_time(); // enforce object/range exclusivity
		clear_tracks(); // enforce object/track exclusivity
		clear_triggers(); // enforce trigger exclusivity
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
	/* XXX This method or the add (const RegionSelection&) needs to go
	 */

	bool changed = false;

	for (vector<RegionView*>::iterator i = v.begin(); i != v.end(); ++i) {
		if (find (regions.begin(), regions.end(), (*i)) == regions.end()) {
			changed = regions.add ((*i));
		}
	}

	if (changed) {
		clear_time(); // enforce object/range exclusivity
		clear_tracks(); // enforce object/track exclusivity
		clear_triggers ();
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
		}
	}

	if (changed) {
		clear_time(); // enforce object/range exclusivity
		clear_tracks(); // enforce object/track exclusivity
		clear_triggers ();
		RegionsChanged ();
	}
}

void
Selection::add (RegionView* r)
{
	if (find (regions.begin(), regions.end(), r) == regions.end()) {
		bool changed = regions.add (r);
		if (changed) {
			clear_time(); // enforce object/range exclusivity
			clear_tracks(); // enforce object/track exclusivity
			clear_triggers ();
			RegionsChanged ();
		}
	}
}

long
Selection::add (timepos_t const & start, timepos_t const & end)
{
	clear_objects(); // enforce object/range exclusivity

	TimelineRangeComparator cmp;

	/* XXX this implementation is incorrect */

	time.push_back (TimelineRange (start, end, ++next_time_id));
	time.consolidate ();
	time.sort (cmp);

	TimeChanged ();

	return next_time_id;
}

void
Selection::move_time (timecnt_t const & distance)
{
	if (distance.is_zero ()) {
		return;
	}

	for (list<TimelineRange>::iterator i = time.begin(); i != time.end(); ++i) {
		(*i).start() += distance;
		(*i).end() += distance;
	}

	TimeChanged ();
}

void
Selection::replace (uint32_t sid, timepos_t const & start, timepos_t const & end)
{
	clear_objects(); // enforce object/range exclusivity

	for (list<TimelineRange>::iterator i = time.begin(); i != time.end(); ++i) {
		if ((*i).id == sid) {
			time.erase (i);
			time.push_back (TimelineRange (start,end, sid));

			/* don't consolidate here */


			TimelineRangeComparator cmp;
			time.sort (cmp);

			TimeChanged ();
			break;
		}
	}
}

void
Selection::add (boost::shared_ptr<Evoral::ControlList> cl)
{
	boost::shared_ptr<ARDOUR::AutomationList> al = boost::dynamic_pointer_cast<ARDOUR::AutomationList>(cl);

	if (!al) {
		warning << "Programming error: Selected list is not an ARDOUR::AutomationList" << endmsg;
		return;
	}

	if (!cl->empty()) {
		clear_time(); // enforce object/range exclusivity
		clear_tracks(); // enforce object/track exclusivity
		clear_triggers(); // enforce trigger exclusivity
	}

	/* The original may change so we must store a copy (not a pointer) here.
	 * e.g AutomationLine rewrites the list with gain mapping.
	 * the downside is that we can't perform duplicate checks.
	 * This code was changed in response to #6842
	 */
	lines.push_back (boost::shared_ptr<ARDOUR::AutomationList> (new ARDOUR::AutomationList(*al)));
	LinesChanged();
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
Selection::remove (vector<RegionView*> rv)
{
	if (regions.remove (rv)) {
		RegionsChanged ();
	}
}

void
Selection::remove (uint32_t selection_id)
{
	if (time.empty()) {
		return;
	}

	for (list<TimelineRange>::iterator i = time.begin(); i != time.end(); ++i) {
		if ((*i).id == selection_id) {
			time.erase (i);

			TimeChanged ();
			break;
		}
	}
}

void
Selection::remove (samplepos_t /*start*/, samplepos_t /*end*/)
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
Selection::set (const MidiNoteSelection& midi_list)
{
	if (!midi_list.empty()) {
		clear_time (); // enforce region/object exclusivity
		clear_tracks(); // enforce object/track exclusivity
		clear_triggers(); // enforce trigger exclusivity
	}
	clear_objects ();
	add (midi_list);
}

void
Selection::set (boost::shared_ptr<Playlist> playlist)
{
	if (playlist) {
		clear_time (); // enforce region/object exclusivity
		clear_tracks(); // enforce object/track exclusivity
		clear_triggers(); // enforce trigger exclusivity
	}
	clear_objects ();
	add (playlist);
}

void
Selection::set (const list<boost::shared_ptr<Playlist> >& pllist)
{
	if (!pllist.empty()) {
		clear_time(); // enforce region/object exclusivity
	}
	clear_objects ();
	add (pllist);
}

void
Selection::set (const RegionSelection& rs)
{
	if (!rs.empty()) {
		clear_time(); // enforce region/object exclusivity
		clear_tracks(); // enforce object/track exclusivity
	}
	clear_objects();
	regions = rs;
	RegionsChanged(); /* EMIT SIGNAL */
}

void
Selection::set (RegionView* r, bool /*also_clear_tracks*/)
{
	if (r) {
		clear_time(); // enforce region/object exclusivity
		clear_tracks(); // enforce object/track exclusivity
	}
	clear_objects ();
	add (r);
}

void
Selection::set (vector<RegionView*>& v)
{
	if (!v.empty()) {
		clear_time(); // enforce region/object exclusivity
		clear_tracks(); // enforce object/track exclusivity
	}

	clear_objects();

	add (v);
}

/** Set the start and end time of the time selection, without changing
 *  the list of tracks it applies to.
 */
long
Selection::set (timepos_t const & start, timepos_t const & end)
{
	clear_objects(); // enforce region/object exclusivity
	clear_time();

	if ((start.is_zero () && end.is_zero ()) || end < start) {
		return 0;
	}

	if (time.empty()) {
		time.push_back (TimelineRange (start, end, ++next_time_id));
	} else {
		/* reuse the first entry, and remove all the rest */

		while (time.size() > 1) {
			time.pop_front();
		}
		time.front().start() = start;
		time.front().end() = end;
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
Selection::set_preserving_all_ranges (timepos_t const & start, timepos_t const & end)
{
	clear_objects(); // enforce region/object exclusivity

	if ((start.is_zero () && end.is_zero ()) || (end < start)) {
		return;
	}

	if (time.empty ()) {
		time.push_back (TimelineRange (start, end, ++next_time_id));
	} else {
		time.sort (TimelineRangeComparator ());
		time.front().set_start (start);
		time.back().set_end (end);
	}

	time.consolidate ();

	TimeChanged ();
}

void
Selection::set (boost::shared_ptr<Evoral::ControlList> ac)
{
	clear_time(); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_objects();

	add (ac);
}

bool
Selection::selected (ArdourMarker* m) const
{
	return find (markers.begin(), markers.end(), m) != markers.end();
}

bool
Selection::selected (RegionView* rv) const
{
	return find (regions.begin(), regions.end(), rv) != regions.end();
}

bool
Selection::selected (ControlPoint* cp) const
{
	return find (points.begin(), points.end(), cp) != points.end();
}

bool
Selection::empty (bool internal_selection)
{
	bool object_level_empty = regions.empty () &&
		tracks.empty () &&
		points.empty () &&
		playlists.empty () &&
		lines.empty () &&
		time.empty () &&
		playlists.empty () &&
		markers.empty() &&
		triggers.empty()
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
	clear_time(); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

	cp->set_selected (!cp->selected ());
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
	clear_time(); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

	for (vector<ControlPoint*>::const_iterator i = cps.begin(); i != cps.end(); ++i) {
		toggle (*i);
	}
}

void
Selection::toggle (list<Selectable*> const & selectables)
{
	clear_time(); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

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
	clear_time (); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_objects ();

	add (selectables);
}

void
Selection::add (PointSelection const & s)
{
	clear_time (); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

	for (PointSelection::const_iterator i = s.begin(); i != s.end(); ++i) {
		points.push_back (*i);
	}
}

void
Selection::add (list<Selectable*> const & selectables)
{
	clear_time (); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

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
Selection::clear_points (bool with_signal)
{
	if (!points.empty()) {
		points.clear ();
		if (with_signal) {
			PointsChanged ();
		}
	}
}

void
Selection::add (ControlPoint* cp)
{
	clear_time (); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

	cp->set_selected (true);
	points.push_back (cp);
	PointsChanged (); /* EMIT SIGNAL */
}

void
Selection::add (vector<ControlPoint*> const & cps)
{
	clear_time (); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

	for (vector<ControlPoint*>::const_iterator i = cps.begin(); i != cps.end(); ++i) {
		(*i)->set_selected (true);
		points.push_back (*i);
	}
	PointsChanged (); /* EMIT SIGNAL */
}

void
Selection::set (ControlPoint* cp)
{
	clear_time (); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

	if (cp->selected () && points.size () == 1) {
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
	clear_time ();  // enforce region/object exclusivity
	clear_tracks();  // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity
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
	clear_time (); //enforce region/object exclusivity
	clear_tracks(); //enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

	if (find (markers.begin(), markers.end(), m) == markers.end()) {
		markers.push_back (m);
		MarkersChanged();
	}
}

void
Selection::add (const list<ArdourMarker*>& m)
{
	clear_time (); // enforce region/object exclusivity
	clear_tracks(); // enforce object/track exclusivity
	clear_triggers(); // enforce trigger exclusivity

	markers.insert (markers.end(), m.begin(), m.end());
	markers.sort ();
	markers.unique ();

	MarkersChanged ();
}

void
MarkerSelection::range (timepos_t& s, timepos_t& e)
{
	if (empty()) {
		s = timepos_t::zero (Temporal::AudioTime);
		e = timepos_t::zero (Temporal::AudioTime);
		return;
	}

	s = timepos_t::max (front()->position().time_domain());
	e = timepos_t::zero (front()->position().time_domain());

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

	XMLNode* node = new XMLNode (X_("Selection"));

	for (TrackSelection::const_iterator i = tracks.begin(); i != tracks.end(); ++i) {
		StripableTimeAxisView* stv = dynamic_cast<StripableTimeAxisView*> (*i);
		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (*i);
		if (stv) {
			XMLNode* t = node->add_child (X_("StripableView"));
			t->set_property (X_("id"), stv->stripable()->id ());
		} else if (atv) {
			XMLNode* t = node->add_child (X_("AutomationView"));
			t->set_property (X_("id"), atv->parent_stripable()->id ());
			t->set_property (X_("parameter"), EventTypeMap::instance().to_symbol (atv->parameter ()));
			t->set_property (X_("ctrl_id"), atv->control()->id());
		}
	}

	if (!regions.empty()) {
		XMLNode* parent = node->add_child (X_("Regions"));
		for (RegionSelection::const_iterator i = regions.begin(); i != regions.end(); ++i) {
			XMLNode* r = parent->add_child (X_("Region"));
			r->set_property (X_("id"), (*i)->region ()->id ());
		}
	}

	/* midi region views have thir own internal selection. */
	list<pair<PBD::ID, std::set<boost::shared_ptr<Evoral::Note<Temporal::Beats> > > > > rid_notes;
	editor->get_per_region_note_selection (rid_notes);

	list<pair<PBD::ID, std::set<boost::shared_ptr<Evoral::Note<Temporal::Beats> > > > >::iterator rn_it;
	for (rn_it = rid_notes.begin(); rn_it != rid_notes.end(); ++rn_it) {
		XMLNode* n = node->add_child (X_("MIDINotes"));
		n->set_property (X_("region-id"), (*rn_it).first);

		for (std::set<boost::shared_ptr<Evoral::Note<Temporal::Beats> > >::iterator i = (*rn_it).second.begin(); i != (*rn_it).second.end(); ++i) {
			XMLNode* nc = n->add_child(X_("note"));
			nc->set_property(X_("note-id"), (*i)->id());
		}
	}

	for (PointSelection::const_iterator i = points.begin(); i != points.end(); ++i) {
		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (&(*i)->line().trackview);
		if (atv) {

			XMLNode* r = node->add_child (X_("ControlPoint"));
			r->set_property (X_("type"), "track");
			r->set_property (X_("route-id"), atv->parent_stripable()->id ());
			r->set_property (X_("automation-list-id"), (*i)->line().the_list()->id ());
			r->set_property (X_("parameter"), EventTypeMap::instance().to_symbol ((*i)->line().the_list()->parameter ()));
			r->set_property (X_("view-index"), (*i)->view_index());
			continue;
		}

		AudioRegionGainLine* argl = dynamic_cast<AudioRegionGainLine*> (&(*i)->line());
		if (argl) {
			XMLNode* r = node->add_child (X_("ControlPoint"));
			r->set_property (X_("type"), "region");
			r->set_property (X_("region-id"), argl->region_view ().region ()->id ());
			r->set_property (X_("view-index"), (*i)->view_index());
		}

	}

	for (TimeSelection::const_iterator i = time.begin(); i != time.end(); ++i) {
		XMLNode* t = node->add_child (X_("TimelineRange"));
		t->set_property (X_("start"), (*i).start());
		t->set_property (X_("end"), (*i).end());
	}

	for (MarkerSelection::const_iterator i = markers.begin(); i != markers.end(); ++i) {
		XMLNode* t = node->add_child (X_("Marker"));

		bool is_start;
		Location* loc = editor->find_location_from_marker (*i, is_start);

		t->set_property (X_("id"), loc->id());
		t->set_property (X_("start"), is_start);
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
	clear_markers ();
	clear_triggers();

	/* NOTE: stripable/time-axis-view selection is saved/restored by
	 * ARDOUR::CoreSelection, not this Selection object
	 */

	PBD::ID id;
	XMLNodeList children = node.children ();

	for (XMLNodeList::const_iterator i = children.begin(); i != children.end(); ++i) {
		if ((*i)->name() == X_("Regions")) {
			RegionSelection selected_regions;
			XMLNodeList children = (*i)->children ();
			for (XMLNodeList::const_iterator ci = children.begin(); ci != children.end(); ++ci) {
				PBD::ID id;

				if (!(*ci)->get_property (X_("id"), id)) {
					continue;
				}

				RegionSelection rs;
				editor->get_regionviews_by_id (id, rs);

				if (!rs.empty ()) {
					for (RegionSelection::const_iterator i = rs.begin(); i != rs.end(); ++i) {
						selected_regions.push_back (*i);
					}
				} else {
					/*
					  regionviews haven't been constructed - stash the region IDs
					  so we can identify them in Editor::region_view_added ()
					*/
					regions.pending.push_back (id);
				}
			}

			if (!selected_regions.empty()) {
				add (selected_regions);
			}

		} else if ((*i)->name() == X_("MIDINotes")) {

			if (!(*i)->get_property (X_("region-id"), id)) {
				assert (false);
			}

			RegionSelection rs;

			editor->get_regionviews_by_id (id, rs); // there could be more than one

			std::list<Evoral::event_id_t> notes;
			XMLNodeList children = (*i)->children ();

			for (XMLNodeList::const_iterator ci = children.begin(); ci != children.end(); ++ci) {
				Evoral::event_id_t id;
				if ((*ci)->get_property (X_ ("note-id"), id)) {
					notes.push_back (id);
				}
			}

			for (RegionSelection::iterator rsi = rs.begin(); rsi != rs.end(); ++rsi) {
				MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (*rsi);
				if (mrv) {
					mrv->select_notes(notes, false);
				}
			}

			if (rs.empty()) {
				/* regionviews containing these notes don't yet exist on the canvas.*/
				pending_midi_note_selection.push_back (make_pair (id, notes));
			}

		} else if ((*i)->name() == X_("ControlPoint")) {
			XMLProperty const * prop_type = (*i)->property (X_("type"));

			assert(prop_type);

			if (prop_type->value () == "track") {

				PBD::ID route_id;
				PBD::ID alist_id;
				std::string param;
				uint32_t view_index;

				if (!(*i)->get_property (X_("route-id"), route_id) ||
				    !(*i)->get_property (X_("automation-list-id"), alist_id) ||
				    !(*i)->get_property (X_("parameter"), param) ||
				    !(*i)->get_property (X_("view-index"), view_index)) {
					assert(false);
				}

				StripableTimeAxisView* stv = editor->get_stripable_time_axis_by_id (route_id);
				vector <ControlPoint *> cps;

				if (stv) {
					boost::shared_ptr<AutomationLine> li = stv->automation_child_by_alist_id (alist_id);
					if (li) {
						ControlPoint* cp = li->nth(view_index);
						if (cp) {
							cps.push_back (cp);
							cp->show();
						}
					}
				}
				if (!cps.empty()) {
					add (cps);
				}
			} else if (prop_type->value () == "region") {

				PBD::ID region_id;
				uint32_t view_index;
				if (!(*i)->get_property (X_("region-id"), region_id) ||
				    !(*i)->get_property (X_("view-index"), view_index)) {
					continue;
				}

				RegionSelection rs;
				editor->get_regionviews_by_id (region_id, rs);

				if (!rs.empty ()) {
					vector <ControlPoint *> cps;
					for (RegionSelection::iterator rsi = rs.begin(); rsi != rs.end(); ++rsi) {
						AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*rsi);
						if (arv) {
							boost::shared_ptr<AudioRegionGainLine> gl = arv->get_gain_line ();
							ControlPoint* cp = gl->nth(view_index);
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

		} else if ((*i)->name() == X_("TimelineRange")) {
			timepos_t start;
			timepos_t end;

			if (!(*i)->get_property (X_("start"), start) || !(*i)->get_property (X_("end"), end)) {
				assert(false);
			}

			add (start, end);

		} else if ((*i)->name() == X_("AutomationView")) {

			// XXX is this even used? -> StripableAutomationControl
			std::string param;
			PBD::ID ctrl_id (0);

			if (!(*i)->get_property (X_("id"), id) || !(*i)->get_property (X_("parameter"), param)) {
				assert (false);
			}

			StripableTimeAxisView* stv = editor->get_stripable_time_axis_by_id (id);

			if (stv && (*i)->get_property (X_("control_id"), ctrl_id)) {
				boost::shared_ptr<AutomationTimeAxisView> atv = stv->automation_child (EventTypeMap::instance().from_symbol (param), ctrl_id);

				/* the automation could be for an entity that was never saved
				 * in the session file. Don't freak out if we can't find
				 * it.
				 */

				if (atv) {
					add (atv.get());
				}
			}

		} else if ((*i)->name() == X_("Marker")) {

			bool is_start;
			if (!(*i)->get_property (X_("id"), id) || !(*i)->get_property (X_("start"), is_start)) {
				assert(false);
			}

			ArdourMarker* m = editor->find_marker_from_location_id (id, is_start);
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

/* TIME AXIS VIEW ... proxy for Stripable/Controllable
 *
 * public methods just modify the CoreSelection; PresentationInfo::Changed will
 * trigger Selection::core_selection_changed() and we will update our own data
 * structures there.
 */

void
Selection::toggle (const TrackViewList& track_list)
{
	TrackViewList t = add_grouped_tracks (track_list);

	CoreSelection& selection (editor->session()->selection());
	PresentationInfo::ChangeSuspender cs;

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		boost::shared_ptr<Stripable> s = (*i)->stripable ();
		boost::shared_ptr<AutomationControl> c = (*i)->control ();
		selection.toggle (s, c);
	}
}

void
Selection::toggle (TimeAxisView* track)
{
	TrackViewList tr;
	tr.push_back (track);
	toggle (tr);
}

void
Selection::add (TrackViewList const & track_list)
{
	TrackViewList t = add_grouped_tracks (track_list);

	CoreSelection& selection (editor->session()->selection());
	PresentationInfo::ChangeSuspender cs;

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		boost::shared_ptr<Stripable> s = (*i)->stripable ();
		boost::shared_ptr<AutomationControl> c = (*i)->control ();
		selection.add (s, c);
	}
}

void
Selection::add (TimeAxisView* track)
{
	TrackViewList tr;
	tr.push_back (track);
	add (tr);
}

void
Selection::remove (TimeAxisView* track)
{
	TrackViewList tvl;
	tvl.push_back (track);
	remove (tvl);
}

void
Selection::remove (const TrackViewList& t)
{
	CoreSelection& selection (editor->session()->selection());
	PresentationInfo::ChangeSuspender cs;

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		boost::shared_ptr<Stripable> s = (*i)->stripable ();
		boost::shared_ptr<AutomationControl> c = (*i)->control ();
		selection.remove (s, c);
	}
}

void
Selection::set (TimeAxisView* track)
{
	TrackViewList tvl;
	tvl.push_back (track);
	set (tvl);
}

void
Selection::set (const TrackViewList& track_list)
{
	TrackViewList t = add_grouped_tracks (track_list);

	CoreSelection& selection (editor->session()->selection());

#if 1 // crazy optimization hack
	/* check is the selection actually changed, ignore NO-OPs
	 *
	 * There are excessive calls from EditorRoutes::selection_changed():
	 * Every click calls selection_changed() even if it doesn't change.
	 * Also re-ordering tracks calls into this due to gtk's odd DnD signal
	 * messaging (row removed, re-added).
	 *
	 * Re-ordering a row results in at least 2 calls to selection_changed()
	 * without actual change. Calling selection.clear_stripables()
	 * and re-adding the same tracks every time in turn emits changed signals.
	 */
	bool changed = false;
	CoreSelection::StripableAutomationControls sac;
	selection.get_stripables (sac);
	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		boost::shared_ptr<Stripable> s = (*i)->stripable ();
		boost::shared_ptr<AutomationControl> c = (*i)->control ();
		bool found = false;
		for (CoreSelection::StripableAutomationControls::iterator j = sac.begin (); j != sac.end (); ++j) {
			if (j->stripable == s && j->controllable == c) {
				found = true;
				sac.erase (j);
				break;
			}
		}
		if (!found) {
			changed = true;
			break;
		}
	}
	if (!changed && sac.size() == 0) {
		return;
	}
#endif

	PresentationInfo::ChangeSuspender cs;

	selection.clear_stripables ();

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		boost::shared_ptr<Stripable> s = (*i)->stripable ();
		boost::shared_ptr<AutomationControl> c = (*i)->control ();
		selection.add (s, c);
	}
}

void
Selection::clear_tracks (bool)
{
	if (!manage_libardour_selection) {
		return;
	}

	Session* s = editor->session();
	if (s) {
		CoreSelection& selection (s->selection());
		selection.clear_stripables ();
	}
}

bool
Selection::selected (TimeAxisView* tv) const
{
	Session* session = editor->session();

	if (!session) {
		return false;
	}

	CoreSelection& selection (session->selection());
	boost::shared_ptr<Stripable> s = tv->stripable ();
	boost::shared_ptr<AutomationControl> c = tv->control ();

	if (c) {
		return selection.selected (c);
	}

	return selection.selected (s);
}

TrackViewList
Selection::add_grouped_tracks (TrackViewList const & t)
{
	TrackViewList added;

	for (TrackSelection::const_iterator i = t.begin(); i != t.end(); ++i) {
		if (dynamic_cast<VCATimeAxisView*> (*i)) {
			continue;
		}

		/* select anything in the same select-enabled route group */
		ARDOUR::RouteGroup* rg = (*i)->route_group ();

		if (rg && rg->is_active() && rg->is_select ()) {

			TrackViewList tr = editor->axis_views_from_routes (rg->route_list ());

			for (TrackViewList::iterator j = tr.begin(); j != tr.end(); ++j) {

				/* Do not add the trackview passed in as an
				 * argument, because we want that to be on the
				 * end of the list.
				 */

				if (*j != *i) {
					if (!added.contains (*j)) {
						added.push_back (*j);
					}
				}
			}
		}
	}

	/* now add the the trackview's passed in as actual arguments */
	added.insert (added.end(), t.begin(), t.end());

	return added;
}

#if 0
static void dump_tracks (Selection const & s)
{
	cerr << "--TRACKS [" << s.tracks.size() << ']' << ":\n";
	for (TrackViewList::const_iterator x = s.tracks.begin(); x != s.tracks.end(); ++x) {
		cerr << (*x)->name() << ' ' << (*x)->stripable() << " C = " << (*x)->control() << endl;
	}
	cerr << "///\n";
}
#endif

void
Selection::core_selection_changed (PropertyChange const & what_changed)
{
	PropertyChange pc;

	pc.add (Properties::selected);

	if (!what_changed.contains (pc)) {
		return;
	}

	CoreSelection& selection (editor->session()->selection());

	if (selection.selected()) {
		clear_objects(); // enforce object/range exclusivity
	}

	tracks.clear (); // clear stage for whatever tracks are now selected (maybe none)

	CoreSelection::StripableAutomationControls sac;
	selection.get_stripables (sac);

	for (CoreSelection::StripableAutomationControls::const_iterator i = sac.begin(); i != sac.end(); ++i) {
		AxisView* av;
		TimeAxisView* tav;
		if ((*i).controllable) {
			av = editor->axis_view_by_control ((*i).controllable);
		} else {
			av = editor->axis_view_by_stripable ((*i).stripable);
		}

		tav = dynamic_cast<TimeAxisView*>(av);
		if (tav) {
			tracks.push_back (tav);
		}
	}

	TracksChanged();
}

MidiRegionSelection
Selection::midi_regions ()
{
	MidiRegionSelection ms;

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
		if (mrv) {
			ms.add (mrv);
		}
	}

	return ms;
}

bool
Selection::selected (TriggerEntry* te) const
{
	return find (triggers.begin(), triggers.end(), te) != triggers.end();
}

void
Selection::set (TriggerEntry* te)
{
#if 0
	clear();
#else
	clear_tracks ();
	clear_regions ();
	clear_points ();
	clear_lines ();
	clear_time ();
	clear_playlists ();
	clear_midi_notes ();
	clear_markers ();
	pending_midi_note_selection.clear();
#endif
	clear_triggers (te ? false: true); /* Do not emit signal here, add() emits signal */
	add (te);
}

void
Selection::add (TriggerEntry* te)
{
	triggers.push_back (te);
	TriggersChanged ();
}

void
Selection::remove (TriggerEntry* te)
{
	TriggerSelection::iterator e = find (triggers.begin(), triggers.end(), te);

	if (e != triggers.end()) {
		triggers.erase (e);
		TriggersChanged ();
	}
}

void
Selection::toggle (TriggerEntry* te)
{
	TriggerSelection::iterator e;

	if ((e = find (triggers.begin(), triggers.end(), te)) != triggers.end()) {
		add (te);
	} else {
		triggers.erase (e);
	}
	TriggersChanged ();
}

