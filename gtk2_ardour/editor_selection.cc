/*
    Copyright (C) 2000-2006 Paul Davis 

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

#include <pbd/stacktrace.h>

#include <ardour/diskstream.h>
#include <ardour/playlist.h>
#include <ardour/route_group.h>

#include "editor.h"
#include "actions.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "automation_line.h"

#include "i18n.h"

using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace Editing;

struct TrackViewByPositionSorter
{
    bool operator() (const TimeAxisView* a, const TimeAxisView *b) {
	    return a->y_position < b->y_position;
    }
};

bool
Editor::extend_selection_to_track (TimeAxisView& view)
{
	if (selection->selected (&view)) {
		/* already selected, do nothing */
		return false;
	}

	if (selection->tracks.empty()) {

		if (!selection->selected (&view)) {
			selection->set (&view);
			return true;
		} else {
			return false;
		}
	} 

	/* something is already selected, so figure out which range of things to add */
	
	TrackViewList to_be_added;
	TrackViewList sorted = track_views;
	TrackViewByPositionSorter cmp;
	bool passed_clicked = false;
	bool forwards = true;

	sorted.sort (cmp);

	if (!selection->selected (&view)) {
		to_be_added.push_back (&view);
	}

	/* figure out if we should go forward or backwards */

	for (TrackViewList::iterator i = sorted.begin(); i != sorted.end(); ++i) {

		if ((*i) == &view) {
			passed_clicked = true;
		}

		if (selection->selected (*i)) {
			if (passed_clicked) {
				forwards = true;
			} else {
				forwards = false;
			}
			break;
		}
	}
			
	passed_clicked = false;

	if (forwards) {

		for (TrackViewList::iterator i = sorted.begin(); i != sorted.end(); ++i) {
					
			if ((*i) == &view) {
				passed_clicked = true;
				continue;
			}
					
			if (passed_clicked) {
				if ((*i)->hidden()) {
					continue;
				}
				if (selection->selected (*i)) {
					break;
				} else if (!(*i)->hidden()) {
					to_be_added.push_back (*i);
				}
			}
		}

	} else {

		for (TrackViewList::reverse_iterator r = sorted.rbegin(); r != sorted.rend(); ++r) {
					
			if ((*r) == &view) {
				passed_clicked = true;
				continue;
			}
					
			if (passed_clicked) {
						
				if ((*r)->hidden()) {
					continue;
				}
						
				if (selection->selected (*r)) {
					break;
				} else if (!(*r)->hidden()) {
					to_be_added.push_back (*r);
				}
			}
		}
	}
			
	if (!to_be_added.empty()) {
		selection->add (to_be_added);
		return true;
	}
	
	return false;
}

void
Editor::select_all_tracks ()
{
	selection->set (track_views);
}

bool
Editor::set_selected_track (TimeAxisView& view, Selection::Operation op, bool no_remove)
{
	bool commit = false;

	switch (op) {
	case Selection::Toggle:
		if (selection->selected (&view)) {
			if (!no_remove) {
				selection->remove (&view);
				commit = true;
			}
		} else {
			selection->add (&view);
			commit = false;
		}
		break;

	case Selection::Add:
		if (!selection->selected (&view)) {
			selection->add (&view);
			commit = true;
		}
		break;

	case Selection::Set:
		if (selection->selected (&view) && selection->tracks.size() == 1) {
			/* no commit necessary */
		} else {
			
			/* reset track selection if there is only 1 other track
			   selected OR if no_remove is not set (its there to 
			   prevent deselecting a multi-track selection
			   when clicking on an already selected track
			   for some reason.
			*/

			if (selection->tracks.empty()) {
				selection->set (&view);
				commit = true;
			} else if (selection->tracks.size() == 1 || !no_remove) {
				selection->set (&view);
				commit = true;
			}
		}
		break;
		
	case Selection::Extend:
		commit = extend_selection_to_track (view);
		break;
	}

	return commit;
}

bool
Editor::set_selected_track_from_click (bool press, Selection::Operation op, bool no_remove)
{
	if (!clicked_trackview) {
		return false;
	}
	
	if (!press) {
		return false;
	}

	return set_selected_track (*clicked_trackview, op, no_remove);
}

bool
Editor::set_selected_control_point_from_click (Selection::Operation op, bool no_remove)
{
	if (!clicked_control_point) {
		return false;
	}

	/* select this point and any others that it represents */

	double y1, y2;
	nframes_t x1, x2;

	x1 = pixel_to_frame (clicked_control_point->get_x() - 10);
	x2 = pixel_to_frame (clicked_control_point->get_x() + 10);
 	y1 = clicked_control_point->get_x() - 10;
	y2 = clicked_control_point->get_y() + 10;

	return select_all_within (x1, x2, y1, y2, op);
}

void
Editor::get_relevant_audio_tracks (set<AudioTimeAxisView*>& relevant_tracks)
{
	/* step one: get all selected tracks and all tracks in the relevant edit groups */

	for (TrackSelection::iterator ti = selection->tracks.begin(); ti != selection->tracks.end(); ++ti) {

		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(*ti);

		if (!atv) {
			continue;
		}

		RouteGroup* group = atv->route()->edit_group();

		if (group && group->is_active()) {
			
			/* active group for this track, loop over all tracks and get every member of the group */

			for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
				
				AudioTimeAxisView* tatv;
				
				if ((tatv = dynamic_cast<AudioTimeAxisView*> (*i)) != 0) {
					
					if (tatv->route()->edit_group() == group) {
						relevant_tracks.insert (tatv);
					}
				}
			}
		} else {
			relevant_tracks.insert (atv);
		}
	}
}

void
Editor::mapover_audio_tracks (slot<void,AudioTimeAxisView&,uint32_t> sl)
{
	set<AudioTimeAxisView*> relevant_tracks;

	get_relevant_audio_tracks (relevant_tracks);

	uint32_t sz = relevant_tracks.size();

	for (set<AudioTimeAxisView*>::iterator ati = relevant_tracks.begin(); ati != relevant_tracks.end(); ++ati) {
		sl (**ati, sz);
	}
}

void
Editor::mapped_get_equivalent_regions (RouteTimeAxisView& tv, uint32_t ignored, RegionView* basis, vector<RegionView*>* all_equivs)
{
	boost::shared_ptr<Playlist> pl;
	vector<boost::shared_ptr<Region> > results;
	RegionView* marv;
	boost::shared_ptr<Diskstream> ds;

	if ((ds = tv.get_diskstream()) == 0) {
		/* bus */
		return;
	}

	if (&tv == &basis->get_time_axis_view()) {
		/* looking in same track as the original */
		return;
	}

	if ((pl = ds->playlist()) != 0) {
		pl->get_equivalent_regions (basis->region(), results);
	}

	for (vector<boost::shared_ptr<Region> >::iterator ir = results.begin(); ir != results.end(); ++ir) {
		if ((marv = tv.view()->find_view (*ir)) != 0) {
			all_equivs->push_back (marv);
		}
	}
}

void
Editor::get_equivalent_regions (RegionView* basis, vector<RegionView*>& equivalent_regions)
{
	mapover_audio_tracks (bind (mem_fun (*this, &Editor::mapped_get_equivalent_regions), basis, &equivalent_regions));
	
	/* add clicked regionview since we skipped all other regions in the same track as the one it was in */
	
	equivalent_regions.push_back (basis);
}

bool
Editor::set_selected_regionview_from_click (bool press, Selection::Operation op, bool no_track_remove)
{
	vector<RegionView*> all_equivalent_regions;
	bool commit = false;

	if (!clicked_regionview || !clicked_audio_trackview) {
		return false;
	}

	if (press) {
		button_release_can_deselect = false;
	} 

	if (op == Selection::Toggle || op == Selection::Set) {


		switch (op) {
		case Selection::Toggle:
			
			if (clicked_regionview->get_selected()) {
				if (press) {

					/* whatever was clicked was selected already; do nothing here but allow
					   the button release to deselect it
					*/

					button_release_can_deselect = true;

				} else {

					if (button_release_can_deselect) {

						/* just remove this one region, but only on a permitted button release */

						selection->remove (clicked_regionview);
						commit = true;

						/* no more deselect action on button release till a new press
						   finds an already selected object.
						*/

						button_release_can_deselect = false;
					}
				} 

			} else {

				if (press) {

					if (selection->selected (clicked_audio_trackview)) {
						get_equivalent_regions (clicked_regionview, all_equivalent_regions);
					} else {
						all_equivalent_regions.push_back (clicked_regionview);
					}

					/* add all the equivalent regions, but only on button press */
					


					if (!all_equivalent_regions.empty()) {
						commit = true;
					}

					selection->add (all_equivalent_regions);
				} 
			}
			break;
			
		case Selection::Set:
			if (!clicked_regionview->get_selected()) {

				if (selection->selected (clicked_audio_trackview)) {
					get_equivalent_regions (clicked_regionview, all_equivalent_regions);
				} else {
					all_equivalent_regions.push_back (clicked_regionview);
				}

				selection->set (all_equivalent_regions);
				commit = true;
			} else {
				/* no commit necessary: clicked on an already selected region */
				goto out;
			}
			break;

		default:
			/* silly compiler */
			break;
		}

	} else if (op == Selection::Extend) {

		list<Selectable*> results;
		nframes_t last_frame;
		nframes_t first_frame;

		/* 1. find the last selected regionview in the track that was clicked in */

		last_frame = 0;
		first_frame = max_frames;

		for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {
			if (&(*x)->get_time_axis_view() == &clicked_regionview->get_time_axis_view()) {

				if ((*x)->region()->last_frame() > last_frame) {
					last_frame = (*x)->region()->last_frame();
				}

				if ((*x)->region()->first_frame() < first_frame) {
					first_frame = (*x)->region()->first_frame();
				}
			}
		}

		/* 2. figure out the boundaries for our search for new objects */

		switch (clicked_regionview->region()->coverage (first_frame, last_frame)) {
		case OverlapNone:
			if (last_frame < clicked_regionview->region()->first_frame()) {
				first_frame = last_frame;
				last_frame = clicked_regionview->region()->last_frame();
			} else {
				last_frame = first_frame;
				first_frame = clicked_regionview->region()->first_frame();
			}
			break;

		case OverlapExternal:
			if (last_frame < clicked_regionview->region()->first_frame()) {
				first_frame = last_frame;
				last_frame = clicked_regionview->region()->last_frame();
			} else {
				last_frame = first_frame;
				first_frame = clicked_regionview->region()->first_frame();
			}
			break;

		case OverlapInternal:
			if (last_frame < clicked_regionview->region()->first_frame()) {
				first_frame = last_frame;
				last_frame = clicked_regionview->region()->last_frame();
			} else {
				last_frame = first_frame;
				first_frame = clicked_regionview->region()->first_frame();
			}
			break;

		case OverlapStart:
		case OverlapEnd:
			/* nothing to do except add clicked region to selection, since it
			   overlaps with the existing selection in this track.
			*/
			break;
		}

		/* 2. find all selectable objects (regionviews in this case) between that one and the end of the
		      one that was clicked.
		*/

		set<AudioTimeAxisView*> relevant_tracks;
		
		get_relevant_audio_tracks (relevant_tracks);

		for (set<AudioTimeAxisView*>::iterator t = relevant_tracks.begin(); t != relevant_tracks.end(); ++t) {
			(*t)->get_selectables (first_frame, last_frame, -1.0, -1.0, results);
		}
		
		/* 3. convert to a vector of audio regions */

		vector<RegionView*> regions;
		
		for (list<Selectable*>::iterator x = results.begin(); x != results.end(); ++x) {
			RegionView* arv;

			if ((arv = dynamic_cast<RegionView*>(*x)) != 0) {
				regions.push_back (arv);
			}
		}

		if (!regions.empty()) {
			selection->add (regions);
			commit = true;
		}
	}

  out:
	return commit;
}

void
Editor::set_selected_regionview_from_region_list (boost::shared_ptr<Region> region, Selection::Operation op)
{
	vector<RegionView*> all_equivalent_regions;

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		
		RouteTimeAxisView* tatv;
		
		if ((tatv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {
			
			boost::shared_ptr<Playlist> pl;
			vector<boost::shared_ptr<Region> > results;
			RegionView* marv;
			boost::shared_ptr<Diskstream> ds;
			
			if ((ds = tatv->get_diskstream()) == 0) {
				/* bus */
				continue;
			}
			
			if ((pl = (ds->playlist())) != 0) {
				pl->get_region_list_equivalent_regions (region, results);
			}
			
			for (vector<boost::shared_ptr<Region> >::iterator ir = results.begin(); ir != results.end(); ++ir) {
				if ((marv = tatv->view()->find_view (*ir)) != 0) {
					all_equivalent_regions.push_back (marv);
				}
			}
			
		}
	}
	
	begin_reversible_command (_("set selected regions"));
	
	switch (op) {
	case Selection::Toggle:
		/* XXX this is not correct */
		selection->toggle (all_equivalent_regions);
		break;
	case Selection::Set:
		selection->set (all_equivalent_regions);
		break;
	case Selection::Extend:
		selection->add (all_equivalent_regions);
		break;
	case Selection::Add:
		selection->add (all_equivalent_regions);
		break;
	}

	commit_reversible_command () ;
}

bool
Editor::set_selected_regionview_from_map_event (GdkEventAny* ev, StreamView* sv, boost::weak_ptr<Region> weak_r)
{
	RegionView* rv;
	boost::shared_ptr<Region> r (weak_r.lock());

	if (!r) {
		return true;
	}

	boost::shared_ptr<AudioRegion> ar;

	if ((ar = boost::dynamic_pointer_cast<AudioRegion> (r)) == 0) {
		return true;
	}

	if ((rv = sv->find_view (ar)) == 0) {
		return true;
	}

	/* don't reset the selection if its something other than 
	   a single other region.
	*/

	if (selection->regions.size() > 1) {
		return true;
	}
	
	begin_reversible_command (_("set selected regions"));
	
	selection->set (rv);

	commit_reversible_command () ;

	return true;
}

void
Editor::track_selection_changed ()
{
	switch (selection->tracks.size()){
	case 0:
		break;
	default:
		set_selected_mixer_strip (*(selection->tracks.front()));
		break;
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected (false);
		if (mouse_mode == MouseRange) {
			(*i)->hide_selection ();
		}
	}

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		(*i)->set_selected (true);
		if (mouse_mode == MouseRange) {
			(*i)->show_selection (selection->time);
		}
	}
}

void
Editor::time_selection_changed ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->hide_selection ();
	}

	if (selection->tracks.empty()) {
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			(*i)->show_selection (selection->time);
		}
	} else {
		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			(*i)->show_selection (selection->time);
		}
	}

	if (selection->time.empty()) {
		ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, false);
	} else {
		ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, true);
	}
}

void
Editor::region_selection_changed ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected_regionviews (selection->regions);
	}
}

void
Editor::point_selection_changed ()
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected_points (selection->points);
	}
}

void
Editor::select_all_in_track (Selection::Operation op)
{
	list<Selectable *> touched;

	if (!clicked_trackview) {
		return;
	}
	
	clicked_trackview->get_selectables (0, max_frames, 0, DBL_MAX, touched);

	switch (op) {
	case Selection::Toggle:
		selection->add (touched);
		break;
	case Selection::Set:
		selection->set (touched);
		break;
	case Selection::Extend:
		/* meaningless, because we're selecting everything */
		break;
	case Selection::Add:
		selection->add (touched);
		break;
	}
}

void
Editor::select_all (Selection::Operation op)
{
	list<Selectable *> touched;
	
	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (0, max_frames, 0, DBL_MAX, touched);
	}
	begin_reversible_command (_("select all"));
	switch (op) {
	case Selection::Add:
		selection->add (touched);
		break;
	case Selection::Toggle:
		selection->add (touched);
		break;
	case Selection::Set:
		selection->set (touched);
		break;
	case Selection::Extend:
		/* meaningless, because we're selecting everything */
		break;
	}
	commit_reversible_command ();
}

void
Editor::invert_selection_in_track ()
{
	list<Selectable *> touched;

	if (!clicked_trackview) {
		return;
	}
	
	clicked_trackview->get_inverted_selectables (*selection, touched);
	selection->set (touched);
}

void
Editor::invert_selection ()
{
	list<Selectable *> touched;
	
	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_inverted_selectables (*selection, touched);
	}

	selection->set (touched);
}

bool
Editor::select_all_within (nframes_t start, nframes_t end, double top, double bot, Selection::Operation op)
{
	list<Selectable*> touched;
	list<Selectable*>::size_type n = 0;
	TrackViewList touched_tracks;

	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}

		n = touched.size();

		(*iter)->get_selectables (start, end, top, bot, touched);

		if (n != touched.size()) {
			touched_tracks.push_back (*iter);
		}
	}

	if (!touched_tracks.empty()) {
		switch (op) {
		case Selection::Add:
			selection->add (touched_tracks);
			break;
		case Selection::Toggle:
			selection->toggle (touched_tracks);
			break;
		case Selection::Set:
			selection->set (touched_tracks);
			break;
		case Selection::Extend:
			/* not defined yet */
			break;
		}
	}
		
	begin_reversible_command (_("select all within"));
	switch (op) {
	case Selection::Add:
		selection->add (touched);
		break;
	case Selection::Toggle:
		selection->toggle (touched);
		break;
	case Selection::Set:
		selection->set (touched);
		break;
	case Selection::Extend:
		/* not defined yet */
		break;
	}

	commit_reversible_command ();
	return !touched.empty();
}

void
Editor::set_selection_from_audio_region ()
{
	if (selection->regions.empty()) {
		return;
	}

	RegionView* rv = *(selection->regions.begin());
	boost::shared_ptr<Region> region = rv->region();
	
	begin_reversible_command (_("set selection from region"));
	selection->set (0, region->position(), region->last_frame());
	commit_reversible_command ();

	set_mouse_mode (Editing::MouseRange, false);
}

void
Editor::set_selection_from_punch()
{
	Location* location;

	if ((location = session->locations()->auto_punch_location()) == 0)  {
		return;
	}

	set_selection_from_range (*location);
}

void
Editor::set_selection_from_loop()
{
	Location* location;

	if ((location = session->locations()->auto_loop_location()) == 0)  {
		return;
	}
	set_selection_from_range (*location);
}

void
Editor::set_selection_from_range (Location& loc)
{
	begin_reversible_command (_("set selection from range"));
	selection->set (0, loc.start(), loc.end());
	commit_reversible_command ();

	set_mouse_mode (Editing::MouseRange, false);
}

void
Editor::select_all_selectables_using_time_selection ()
{
	list<Selectable *> touched;

	if (selection->time.empty()) {
		return;
	}

	nframes_t start = selection->time[clicked_selection].start;
	nframes_t end = selection->time[clicked_selection].end;

	if (end - start < 1)  {
		return;
	}

	for (TrackViewList::iterator iter = selection->tracks.begin(); iter != selection->tracks.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end - 1, 0, DBL_MAX, touched);
	}

	begin_reversible_command (_("select all from range"));
	selection->set (touched);
	commit_reversible_command ();
}


void
Editor::select_all_selectables_using_punch()
{
	Location* location = session->locations()->auto_punch_location();
	list<Selectable *> touched;

	if (location == 0 || (location->end() - location->start() <= 1))  {
		return;
	}

	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (location->start(), location->end() - 1, 0, DBL_MAX, touched);
	}
	begin_reversible_command (_("select all from punch"));
	selection->set (touched);
	commit_reversible_command ();

}

void
Editor::select_all_selectables_using_loop()
{
	Location* location = session->locations()->auto_loop_location();
	list<Selectable *> touched;

	if (location == 0 || (location->end() - location->start() <= 1))  {
		return;
	}

	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (location->start(), location->end() - 1, 0, DBL_MAX, touched);
	}
	begin_reversible_command (_("select all from loop"));
	selection->set (touched);
	commit_reversible_command ();

}

void
Editor::select_all_selectables_using_cursor (Cursor *cursor, bool after)
{
        nframes_t start;
	nframes_t end;
	list<Selectable *> touched;

	if (after) {
		begin_reversible_command (_("select all after cursor"));
		start = cursor->current_frame ;
		end = session->current_end_frame();
	} else {
		if (cursor->current_frame > 0) {
			begin_reversible_command (_("select all before cursor"));
			start = 0;
			end = cursor->current_frame - 1;
		} else {
			return;
		}
	}

	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}
	selection->set (touched);
	commit_reversible_command ();
}

void
Editor::select_all_selectables_between_cursors (Cursor *cursor, Cursor *other_cursor)
{
        nframes_t start;
	nframes_t end;
	list<Selectable *> touched;
	bool  other_cursor_is_first = cursor->current_frame > other_cursor->current_frame;

	if (cursor->current_frame == other_cursor->current_frame) {
		return;
	}

	begin_reversible_command (_("select all between cursors"));
	if (other_cursor_is_first) {
		start = other_cursor->current_frame;
		end = cursor->current_frame - 1;
		
	} else {
		start = cursor->current_frame;
		end = other_cursor->current_frame - 1;
	}
	
	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}
	selection->set (touched);
	commit_reversible_command ();
}

