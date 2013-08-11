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

#include <algorithm>
#include <cstdlib>

#include "pbd/stacktrace.h"

#include "ardour/midi_region.h"
#include "ardour/playlist.h"
#include "ardour/profile.h"
#include "ardour/route_group.h"
#include "ardour/session.h"

#include "control_protocol/control_protocol.h"

#include "editor.h"
#include "actions.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor_regions.h"
#include "editor_cursors.h"
#include "midi_region_view.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Glib;
using namespace Gtkmm2ext;
using namespace Editing;

struct TrackViewByPositionSorter
{
	bool operator() (const TimeAxisView* a, const TimeAxisView *b) {
		return a->y_position() < b->y_position();
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
	TrackViewList visible_views;
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if ((*i)->marked_for_display()) {
			visible_views.push_back (*i);
		}
	}
	selection->set (visible_views);
}

/** Select clicked_axisview, unless there are no currently selected
 *  tracks, in which case nothing will happen unless `force' is true.
 */
void
Editor::set_selected_track_as_side_effect (Selection::Operation op)
{
	if (!clicked_axisview) {
		return;
	}

	if (!clicked_routeview) {
		return;
	}

	bool had_tracks = !selection->tracks.empty();
	RouteGroup* group = clicked_routeview->route()->route_group();
	RouteGroup& arg (_session->all_route_group());

	switch (op) {
	case Selection::Toggle:
		if (selection->selected (clicked_axisview)) {
			if (arg.is_select() && arg.is_active()) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
					selection->remove(*i);
				}
			} else if (group && group->is_active()) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
					if ((*i)->route_group() == group)
						selection->remove(*i);
				}
			} else {
				selection->remove (clicked_axisview);
			}
		} else {
			if (arg.is_select() && arg.is_active()) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
					selection->add(*i);
				}
			} else if (group && group->is_active()) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
					if ( (*i)->route_group() == group)
						selection->add(*i);
				}
			} else {
				selection->add (clicked_axisview);
			}
		}
		break;

	case Selection::Add:
		if (!had_tracks && arg.is_select() && arg.is_active()) {
			/* nothing was selected already, and all group is active etc. so use
			   all tracks.
			*/
			for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
				selection->add(*i);
			}
		} else if (group && group->is_active()) {
			for (TrackViewList::iterator i  = track_views.begin(); i != track_views.end (); ++i) {
				if ((*i)->route_group() == group)
					selection->add(*i);
			}
		} else {
			selection->add (clicked_axisview);
		}
		break;

	case Selection::Set:
		selection->clear();
		if (!had_tracks && arg.is_select() && arg.is_active()) {
			/* nothing was selected already, and all group is active etc. so use
			   all tracks.
			*/
			for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
				selection->add(*i);
			}
		} else if (group && group->is_active()) {
			for (TrackViewList::iterator i  = track_views.begin(); i != track_views.end (); ++i) {
				if ((*i)->route_group() == group)
					selection->add(*i);
			}
		} else {
			selection->set (clicked_axisview);
		}
		break;

	case Selection::Extend:
		selection->clear();
		break;
	}
}

void
Editor::set_selected_track (TimeAxisView& view, Selection::Operation op, bool no_remove)
{
	switch (op) {
	case Selection::Toggle:
		if (selection->selected (&view)) {
			if (!no_remove) {
				selection->remove (&view);
			}
		} else {
			selection->add (&view);
		}
		break;

	case Selection::Add:
		if (!selection->selected (&view)) {
			selection->add (&view);
		}
		break;

	case Selection::Set:
		selection->set (&view);
		break;

	case Selection::Extend:
		extend_selection_to_track (view);
		break;
	}
}

void
Editor::set_selected_track_from_click (bool press, Selection::Operation op, bool no_remove)
{
	if (!clicked_routeview) {
		return;
	}

	if (!press) {
		return;
	}

	set_selected_track (*clicked_routeview, op, no_remove);
}

bool
Editor::set_selected_control_point_from_click (bool press, Selection::Operation op)
{
	if (!clicked_control_point) {
		return false;
	}

	switch (op) {
	case Selection::Set:
		if (press) {
			selection->set (clicked_control_point);
		}
		break;
	case Selection::Add:
		if (press) {
			selection->add (clicked_control_point);
		}
		break;
	case Selection::Toggle:
		/* This is a bit of a hack; if we Primary-Click-Drag a control
		   point (for push drag) we want the point we clicked on to be
		   selected, otherwise we end up confusingly dragging an
		   unselected point.  So here we ensure that the point is selected
		   after the press, and if we subsequently get a release (meaning no
		   drag occurred) we set things up so that the toggle has happened.
		*/
		if (press && !selection->selected (clicked_control_point)) {
			/* This is the button press, and the control point is not selected; make it so,
			   in case this press leads to a drag.  Also note that having done this, we don't
			   need to toggle again on release.
			*/
			selection->toggle (clicked_control_point);
			_control_point_toggled_on_press = true;
		} else if (!press && !_control_point_toggled_on_press) {
			/* This is the release, and the point wasn't toggled on the press, so do it now */
			selection->toggle (clicked_control_point);
		} else {
			/* Reset our flag */
			_control_point_toggled_on_press = false;
		}
		break;
	case Selection::Extend:
		/* XXX */
		break;
	}

	return true;
}

void
Editor::get_onscreen_tracks (TrackViewList& tvl)
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if ((*i)->y_position() < _canvas_height) {
			tvl.push_back (*i);
		}
	}
}

/** Call a slot for a given `basis' track and also for any track that is in the same
 *  active route group with a particular set of properties.
 *
 *  @param sl Slot to call.
 *  @param basis Basis track.
 *  @param prop Properties that active edit groups must share to be included in the map.
 */

void
Editor::mapover_tracks (sigc::slot<void, RouteTimeAxisView&, uint32_t> sl, TimeAxisView* basis, PBD::PropertyID prop) const
{
	RouteTimeAxisView* route_basis = dynamic_cast<RouteTimeAxisView*> (basis);

	if (route_basis == 0) {
		return;
	}

	set<RouteTimeAxisView*> tracks;
	tracks.insert (route_basis);

	RouteGroup* group = route_basis->route()->route_group();

	if (group && group->enabled_property(prop) && group->enabled_property (Properties::active.property_id) ) {

		/* the basis is a member of an active route group, with the appropriate
		   properties; find other members */

		for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
			RouteTimeAxisView* v = dynamic_cast<RouteTimeAxisView*> (*i);
			if (v && v->route()->route_group() == group) {
				tracks.insert (v);
			}
		}
	}

	/* call the slots */
	uint32_t const sz = tracks.size ();

	for (set<RouteTimeAxisView*>::iterator i = tracks.begin(); i != tracks.end(); ++i) {
		sl (**i, sz);
	}
}

/** Call a slot for a given `basis' track and also for any track that is in the same
 *  active route group with a particular set of properties.
 *
 *  @param sl Slot to call.
 *  @param basis Basis track.
 *  @param prop Properties that active edit groups must share to be included in the map.
 */

void
Editor::mapover_tracks_with_unique_playlists (sigc::slot<void, RouteTimeAxisView&, uint32_t> sl, TimeAxisView* basis, PBD::PropertyID prop) const
{
	RouteTimeAxisView* route_basis = dynamic_cast<RouteTimeAxisView*> (basis);
	set<boost::shared_ptr<Playlist> > playlists;

	if (route_basis == 0) {
		return;
	}

	set<RouteTimeAxisView*> tracks;
	tracks.insert (route_basis);

	RouteGroup* group = route_basis->route()->route_group(); // could be null, not a problem

	if (group && group->enabled_property(prop) && group->enabled_property (Properties::active.property_id) ) {

		/* the basis is a member of an active route group, with the appropriate
		   properties; find other members */

		for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
			RouteTimeAxisView* v = dynamic_cast<RouteTimeAxisView*> (*i);

			if (v && v->route()->route_group() == group) {
				
				boost::shared_ptr<Track> t = v->track();
				if (t) {
					if (playlists.insert (t->playlist()).second) {
						/* haven't seen this playlist yet */
						tracks.insert (v);
					}
				} else {
					/* not actually a "Track", but a timeaxis view that
					   we should mapover anyway.
					*/
					tracks.insert (v);
				}
			}
		}
	}

	/* call the slots */
	uint32_t const sz = tracks.size ();

	for (set<RouteTimeAxisView*>::iterator i = tracks.begin(); i != tracks.end(); ++i) {
		sl (**i, sz);
	}
}

void
Editor::mapped_get_equivalent_regions (RouteTimeAxisView& tv, uint32_t, RegionView * basis, vector<RegionView*>* all_equivs) const
{
	boost::shared_ptr<Playlist> pl;
	vector<boost::shared_ptr<Region> > results;
	RegionView* marv;
	boost::shared_ptr<Track> tr;

	if ((tr = tv.track()) == 0) {
		/* bus */
		return;
	}

	if (&tv == &basis->get_time_axis_view()) {
		/* looking in same track as the original */
		return;
	}

	if ((pl = tr->playlist()) != 0) {
		pl->get_equivalent_regions (basis->region(), results);
	}

	for (vector<boost::shared_ptr<Region> >::iterator ir = results.begin(); ir != results.end(); ++ir) {
		if ((marv = tv.view()->find_view (*ir)) != 0) {
			all_equivs->push_back (marv);
		}
	}
}

void
Editor::get_equivalent_regions (RegionView* basis, vector<RegionView*>& equivalent_regions, PBD::PropertyID property) const
{
	mapover_tracks_with_unique_playlists (sigc::bind (sigc::mem_fun (*this, &Editor::mapped_get_equivalent_regions), basis, &equivalent_regions), &basis->get_time_axis_view(), property);

	/* add clicked regionview since we skipped all other regions in the same track as the one it was in */

	equivalent_regions.push_back (basis);
}

RegionSelection
Editor::get_equivalent_regions (RegionSelection & basis, PBD::PropertyID prop) const
{
	RegionSelection equivalent;

	for (RegionSelection::const_iterator i = basis.begin(); i != basis.end(); ++i) {

		vector<RegionView*> eq;

		mapover_tracks_with_unique_playlists (
			sigc::bind (sigc::mem_fun (*this, &Editor::mapped_get_equivalent_regions), *i, &eq),
			&(*i)->get_time_axis_view(), prop);

		for (vector<RegionView*>::iterator j = eq.begin(); j != eq.end(); ++j) {
			equivalent.add (*j);
		}

		equivalent.add (*i);
	}

	return equivalent;
}

int
Editor::get_regionview_count_from_region_list (boost::shared_ptr<Region> region)
{
	int region_count = 0;

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {

		RouteTimeAxisView* tatv;

		if ((tatv = dynamic_cast<RouteTimeAxisView*> (*i)) != 0) {

			boost::shared_ptr<Playlist> pl;
			vector<boost::shared_ptr<Region> > results;
			RegionView* marv;
			boost::shared_ptr<Track> tr;

			if ((tr = tatv->track()) == 0) {
				/* bus */
				continue;
			}

			if ((pl = (tr->playlist())) != 0) {
				pl->get_region_list_equivalent_regions (region, results);
			}

			for (vector<boost::shared_ptr<Region> >::iterator ir = results.begin(); ir != results.end(); ++ir) {
				if ((marv = tatv->view()->find_view (*ir)) != 0) {
					region_count++;
				}
			}

		}
	}

	return region_count;
}


bool
Editor::set_selected_regionview_from_click (bool press, Selection::Operation op)
{
	vector<RegionView*> all_equivalent_regions;
	bool commit = false;

	if (!clicked_regionview || !clicked_routeview) {
		return false;
	}

	if (press) {
		button_release_can_deselect = false;
	}

	if (op == Selection::Toggle || op == Selection::Set) {

		switch (op) {
		case Selection::Toggle:
			if (selection->selected (clicked_regionview)) {
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

					if (selection->selected (clicked_routeview)) {
						get_equivalent_regions (clicked_regionview, all_equivalent_regions, ARDOUR::Properties::select.property_id);
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
			if (!selection->selected (clicked_regionview)) {
				get_equivalent_regions (clicked_regionview, all_equivalent_regions, ARDOUR::Properties::select.property_id);
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
		framepos_t last_frame;
		framepos_t first_frame;
		bool same_track = false;

		/* 1. find the last selected regionview in the track that was clicked in */

		last_frame = 0;
		first_frame = max_framepos;

		for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {
			if (&(*x)->get_time_axis_view() == &clicked_regionview->get_time_axis_view()) {

				if ((*x)->region()->last_frame() > last_frame) {
					last_frame = (*x)->region()->last_frame();
				}

				if ((*x)->region()->first_frame() < first_frame) {
					first_frame = (*x)->region()->first_frame();
				}

				same_track = true;
			}
		}

		if (same_track) {

			/* 2. figure out the boundaries for our search for new objects */

			switch (clicked_regionview->region()->coverage (first_frame, last_frame)) {
			case Evoral::OverlapNone:
				if (last_frame < clicked_regionview->region()->first_frame()) {
					first_frame = last_frame;
					last_frame = clicked_regionview->region()->last_frame();
				} else {
					last_frame = first_frame;
					first_frame = clicked_regionview->region()->first_frame();
				}
				break;

			case Evoral::OverlapExternal:
				if (last_frame < clicked_regionview->region()->first_frame()) {
					first_frame = last_frame;
					last_frame = clicked_regionview->region()->last_frame();
				} else {
					last_frame = first_frame;
					first_frame = clicked_regionview->region()->first_frame();
				}
				break;

			case Evoral::OverlapInternal:
				if (last_frame < clicked_regionview->region()->first_frame()) {
					first_frame = last_frame;
					last_frame = clicked_regionview->region()->last_frame();
				} else {
					last_frame = first_frame;
					first_frame = clicked_regionview->region()->first_frame();
				}
				break;

			case Evoral::OverlapStart:
			case Evoral::OverlapEnd:
				/* nothing to do except add clicked region to selection, since it
				   overlaps with the existing selection in this track.
				*/
				break;
			}

		} else {

			/* click in a track that has no regions selected, so extend vertically
			   to pick out all regions that are defined by the existing selection
			   plus this one.
			*/


			first_frame = clicked_regionview->region()->position();
			last_frame = clicked_regionview->region()->last_frame();

			for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
				if ((*i)->region()->position() < first_frame) {
					first_frame = (*i)->region()->position();
				}
				if ((*i)->region()->last_frame() + 1 > last_frame) {
					last_frame = (*i)->region()->last_frame();
				}
			}
		}

		/* 2. find all the tracks we should select in */

		set<RouteTimeAxisView*> relevant_tracks;

		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
			if (r) {
				relevant_tracks.insert (r);
			}
		}

		set<RouteTimeAxisView*> already_in_selection;

		if (relevant_tracks.empty()) {

			/* no tracks selected .. thus .. if the
			   regionview we're in isn't selected
			   (i.e. we're about to extend to it), then
			   find all tracks between the this one and
			   any selected ones.
			*/

			if (!selection->selected (clicked_regionview)) {

				RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (&clicked_regionview->get_time_axis_view());

				if (rtv) {

					/* add this track to the ones we will search */

					relevant_tracks.insert (rtv);

					/* find the track closest to this one that
					   already a selected region.
					*/

					RouteTimeAxisView* closest = 0;
					int distance = INT_MAX;
					int key = rtv->route()->order_key (EditorSort);

					for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {

						RouteTimeAxisView* artv = dynamic_cast<RouteTimeAxisView*>(&(*x)->get_time_axis_view());

						if (artv && artv != rtv) {

							pair<set<RouteTimeAxisView*>::iterator,bool> result;

							result = already_in_selection.insert (artv);

							if (result.second) {
								/* newly added to already_in_selection */

								int d = artv->route()->order_key (EditorSort);

								d -= key;

								if (abs (d) < distance) {
									distance = abs (d);
									closest = artv;
								}
							}
						}
					}

					if (closest) {

						/* now add all tracks between that one and this one */

						int okey = closest->route()->order_key (EditorSort);

						if (okey > key) {
							swap (okey, key);
						}

						for (TrackViewList::iterator x = track_views.begin(); x != track_views.end(); ++x) {
							RouteTimeAxisView* artv = dynamic_cast<RouteTimeAxisView*>(*x);
							if (artv && artv != rtv) {

								int k = artv->route()->order_key (EditorSort);

								if (k >= okey && k <= key) {

									/* in range but don't add it if
									   it already has tracks selected.
									   this avoids odd selection
									   behaviour that feels wrong.
									*/

									if (find (already_in_selection.begin(),
									          already_in_selection.end(),
									          artv) == already_in_selection.end()) {

										relevant_tracks.insert (artv);
									}
								}
							}
						}
					}
				}
			}
		}

		/* 3. find all selectable objects (regionviews in this case) between that one and the end of the
		   one that was clicked.
		*/

		for (set<RouteTimeAxisView*>::iterator t = relevant_tracks.begin(); t != relevant_tracks.end(); ++t) {
			(*t)->get_selectables (first_frame, last_frame, -1.0, -1.0, results);
		}

		/* 4. convert to a vector of regions */

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

	get_regions_corresponding_to (region, all_equivalent_regions, region->whole_file());

	if (all_equivalent_regions.empty()) {
		return;
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
Editor::set_selected_regionview_from_map_event (GdkEventAny* /*ev*/, StreamView* sv, boost::weak_ptr<Region> weak_r)
{
	RegionView* rv;
	boost::shared_ptr<Region> r (weak_r.lock());

	if (!r) {
		return true;
	}

	if ((rv = sv->find_view (r)) == 0) {
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
	switch (selection->tracks.size()) {
	case 0:
		break;
	default:
		set_selected_mixer_strip (*(selection->tracks.front()));
		break;
	}

	RouteNotificationListPtr routes (new RouteNotificationList);

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {

		bool yn = (find (selection->tracks.begin(), selection->tracks.end(), *i) != selection->tracks.end());

		(*i)->set_selected (yn);

		TimeAxisView::Children c = (*i)->get_child_list ();
		for (TimeAxisView::Children::iterator j = c.begin(); j != c.end(); ++j) {
			(*j)->set_selected (find (selection->tracks.begin(), selection->tracks.end(), j->get()) != selection->tracks.end());
		}

		if (yn) {
			(*i)->reshow_selection (selection->time);
		} else {
			(*i)->hide_selection ();
		}


		if (yn) {
			RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*i);
			if (rtav) {
				routes->push_back (rtav->route());
			}
		}
	}

	ActionManager::set_sensitive (ActionManager::track_selection_sensitive_actions, !selection->tracks.empty());

	/* notify control protocols */
	
	ControlProtocol::TrackSelectionChanged (routes);
}

void
Editor::time_selection_changed ()
{
	if (Profile->get_sae()) {
		return;
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->hide_selection ();
	}

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		(*i)->show_selection (selection->time);
	}

	if (selection->time.empty()) {
		ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, false);
	} else {
		ActionManager::set_sensitive (ActionManager::time_selection_sensitive_actions, true);
	}

	if (_session && Config->get_always_play_range() && !_session->transport_rolling() && !selection->time.empty()) {
		_session->request_locate (selection->time.start());
	}
}

/** Set all region actions to have a given sensitivity */
void
Editor::sensitize_all_region_actions (bool s)
{
	Glib::ListHandle<Glib::RefPtr<Action> > all = _region_actions->get_actions ();

	for (Glib::ListHandle<Glib::RefPtr<Action> >::iterator i = all.begin(); i != all.end(); ++i) {
		(*i)->set_sensitive (s);
	}

	_all_region_actions_sensitized = s;
}

/** Sensitize region-based actions based on the selection ONLY, ignoring the entered_regionview.
 *  This method should be called just before displaying a Region menu.  When a Region menu is not
 *  currently being shown, all region actions are sensitized so that hotkey-triggered actions
 *  on entered_regionviews work without having to check sensitivity every time the selection or
 *  entered_regionview changes.
 *
 *  This method also sets up toggle action state as appropriate.
 */
void
Editor::sensitize_the_right_region_actions ()
{

	RegionSelection rs = get_regions_from_selection_and_entered ();
	sensitize_all_region_actions (!rs.empty ());

	_ignore_region_action = true;

	/* Look through the regions that are selected and make notes about what we have got */

	bool have_audio = false;
	bool have_multichannel_audio = false;
	bool have_midi = false;
	bool have_locked = false;
	bool have_unlocked = false;
	bool have_video_locked = false;
	bool have_video_unlocked = false;
	bool have_position_lock_style_audio = false;
	bool have_position_lock_style_music = false;
	bool have_muted = false;
	bool have_unmuted = false;
	bool have_opaque = false;
	bool have_non_opaque = false;
	bool have_not_at_natural_position = false;
	bool have_envelope_active = false;
	bool have_envelope_inactive = false;
	bool have_non_unity_scale_amplitude = false;
	bool have_compound_regions = false;
	bool have_inactive_fade_in = false;
	bool have_inactive_fade_out = false;
	bool have_active_fade_in = false;
	bool have_active_fade_out = false;

	for (list<RegionView*>::const_iterator i = rs.begin(); i != rs.end(); ++i) {

		boost::shared_ptr<Region> r = (*i)->region ();
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (r);

		if (ar) {
			have_audio = true;
			if (ar->n_channels() > 1) {
				have_multichannel_audio = true;
			}
		}

		if (boost::dynamic_pointer_cast<MidiRegion> (r)) {
			have_midi = true;
		}

		if (r->is_compound()) {
			have_compound_regions = true;
		}

		if (r->locked()) {
			have_locked = true;
		} else {
			have_unlocked = true;
		}

		if (r->video_locked()) {
			have_video_locked = true;
		} else {
			have_video_unlocked = true;
		}

		if (r->position_lock_style() == MusicTime) {
			have_position_lock_style_music = true;
		} else {
			have_position_lock_style_audio = true;
		}

		if (r->muted()) {
			have_muted = true;
		} else {
			have_unmuted = true;
		}

		if (r->opaque()) {
			have_opaque = true;
		} else {
			have_non_opaque = true;
		}

		if (!r->at_natural_position()) {
			have_not_at_natural_position = true;
		}

		if (ar) {
			if (ar->envelope_active()) {
				have_envelope_active = true;
			} else {
				have_envelope_inactive = true;
			}

			if (ar->scale_amplitude() != 1) {
				have_non_unity_scale_amplitude = true;
			}

			if (ar->fade_in_active ()) {
				have_active_fade_in = true;
			} else {
				have_inactive_fade_in = true;
			}

			if (ar->fade_out_active ()) {
				have_active_fade_out = true;
			} else {
				have_inactive_fade_out = true;
			}
		}
	}

	if (rs.size() > 1) {
		_region_actions->get_action("show-region-list-editor")->set_sensitive (false);
		_region_actions->get_action("show-region-properties")->set_sensitive (false);
		_region_actions->get_action("rename-region")->set_sensitive (false);
		if (have_audio) {
			/* XXX need to check whether there is than 1 per
			   playlist, because otherwise this makes no sense.
			*/
			_region_actions->get_action("combine-regions")->set_sensitive (true);
		} else {
			_region_actions->get_action("combine-regions")->set_sensitive (false);
		}
	} else if (rs.size() == 1) {
		_region_actions->get_action("add-range-markers-from-region")->set_sensitive (false);
		_region_actions->get_action("close-region-gaps")->set_sensitive (false);
		_region_actions->get_action("combine-regions")->set_sensitive (false);
	}

	if (!have_multichannel_audio) {
		_region_actions->get_action("split-multichannel-region")->set_sensitive (false);
	}

	if (!have_midi) {
		editor_menu_actions->get_action("RegionMenuMIDI")->set_sensitive (false);
		_region_actions->get_action("show-region-list-editor")->set_sensitive (false);
		_region_actions->get_action("quantize-region")->set_sensitive (false);
		_region_actions->get_action("fork-region")->set_sensitive (false);
		_region_actions->get_action("insert-patch-change-context")->set_sensitive (false);
		_region_actions->get_action("insert-patch-change")->set_sensitive (false);
		_region_actions->get_action("transpose-region")->set_sensitive (false);
	} else {
		editor_menu_actions->get_action("RegionMenuMIDI")->set_sensitive (true);
		/* others were already marked sensitive */
	}

	if (_edit_point == EditAtMouse) {
		_region_actions->get_action("set-region-sync-position")->set_sensitive (false);
		_region_actions->get_action("trim-front")->set_sensitive (false);
		_region_actions->get_action("trim-back")->set_sensitive (false);
		_region_actions->get_action("split-region")->set_sensitive (false);
		_region_actions->get_action("place-transient")->set_sensitive (false);
	}

	if (have_compound_regions) {
		_region_actions->get_action("uncombine-regions")->set_sensitive (true);
	} else {
		_region_actions->get_action("uncombine-regions")->set_sensitive (false);
	}

	if (have_audio) {

		if (have_envelope_active && !have_envelope_inactive) {
			Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-gain-envelope-active"))->set_active ();
		} else if (have_envelope_active && have_envelope_inactive) {
			// Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-gain-envelope-active"))->set_inconsistent ();
		}

	} else {

		_region_actions->get_action("analyze-region")->set_sensitive (false);
		_region_actions->get_action("reset-region-gain-envelopes")->set_sensitive (false);
		_region_actions->get_action("toggle-region-gain-envelope-active")->set_sensitive (false);
		_region_actions->get_action("pitch-shift-region")->set_sensitive (false);

	}

	if (!have_non_unity_scale_amplitude || !have_audio) {
		_region_actions->get_action("reset-region-scale-amplitude")->set_sensitive (false);
	}

	Glib::RefPtr<ToggleAction> a = Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-lock"));
	a->set_active (have_locked && !have_unlocked);
	if (have_locked && have_unlocked) {
		// a->set_inconsistent ();
	}

	a = Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-video-lock"));
	a->set_active (have_video_locked && !have_video_unlocked);
	if (have_video_locked && have_video_unlocked) {
		// a->set_inconsistent ();
	}

	a = Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-lock-style"));
	a->set_active (have_position_lock_style_music && !have_position_lock_style_audio);

	if (have_position_lock_style_music && have_position_lock_style_audio) {
		// a->set_inconsistent ();
	}

	a = Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-mute"));
	a->set_active (have_muted && !have_unmuted);
	if (have_muted && have_unmuted) {
		// a->set_inconsistent ();
	}

	a = Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-opaque-region"));
	a->set_active (have_opaque && !have_non_opaque);
	if (have_opaque && have_non_opaque) {
		// a->set_inconsistent ();
	}

	if (!have_not_at_natural_position) {
		_region_actions->get_action("naturalize-region")->set_sensitive (false);
	}

	/* XXX: should also check that there is a track of the appropriate type for the selected region */
	if (_edit_point == EditAtMouse || _regions->get_single_selection() == 0 || selection->tracks.empty()) {
		_region_actions->get_action("insert-region-from-region-list")->set_sensitive (false);
	} else {
		_region_actions->get_action("insert-region-from-region-list")->set_sensitive (true);
	}

	a = Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-fade-in"));
	a->set_active (have_active_fade_in && !have_inactive_fade_in);
	if (have_active_fade_in && have_inactive_fade_in) {
		// a->set_inconsistent ();
	}

	a = Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-fade-out"));
	a->set_active (have_active_fade_out && !have_inactive_fade_out);

	if (have_active_fade_out && have_inactive_fade_out) {
		// a->set_inconsistent ();
	}
	
	bool const have_active_fade = have_active_fade_in || have_active_fade_out;
	bool const have_inactive_fade = have_inactive_fade_in || have_inactive_fade_out;

	a = Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-fades"));
	a->set_active (have_active_fade && !have_inactive_fade);

	if (have_active_fade && have_inactive_fade) {
		// a->set_inconsistent ();
	}
	
	_ignore_region_action = false;

	_all_region_actions_sensitized = false;
}


void
Editor::region_selection_changed ()
{
	_regions->block_change_connection (true);
	editor_regions_selection_changed_connection.block(true);

	if (_region_selection_change_updates_region_list) {
		_regions->unselect_all ();
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->set_selected_regionviews (selection->regions);
	}

	if (_region_selection_change_updates_region_list) {
		_regions->set_selected (selection->regions);
	}

	_regions->block_change_connection (false);
	editor_regions_selection_changed_connection.block(false);

	if (!_all_region_actions_sensitized) {
		/* This selection change might have changed what region actions
		   are allowed, so sensitize them all in case a key is pressed.
		*/
		sensitize_all_region_actions (true);
	}

	if (_session && !_session->transport_rolling() && !selection->regions.empty()) {
		maybe_locate_with_edit_preroll (selection->regions.start());
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

	if (!clicked_routeview) {
		return;
	}

	clicked_routeview->get_selectables (0, max_framepos, 0, DBL_MAX, touched);

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
Editor::select_all_internal_edit (Selection::Operation)
{
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
		if (mrv) {
			mrv->select_all_notes ();
		}
	}
}

void
Editor::select_all (Selection::Operation op)
{
	list<Selectable *> touched;

	TrackViewList ts;

	if (selection->tracks.empty()) {
		if (entered_track) {
			ts.push_back (entered_track);
		} else {
			ts = track_views;
		}
	} else {
		ts = selection->tracks;
	}

	if (_internal_editing) {

		bool midi_selected = false;

		for (TrackViewList::iterator iter = ts.begin(); iter != ts.end(); ++iter) {
			if ((*iter)->hidden()) {
				continue;
			}
			
			RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*iter);

			if (rtav && rtav->is_midi_track()) {
				midi_selected = true;
				break;
			}
		}

		if (midi_selected) {
			select_all_internal_edit (op);
			return;
		}
	}

	for (TrackViewList::iterator iter = ts.begin(); iter != ts.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (0, max_framepos, 0, DBL_MAX, touched);
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

	if (!clicked_routeview) {
		return;
	}

	clicked_routeview->get_inverted_selectables (*selection, touched);
	selection->set (touched);
}

void
Editor::invert_selection ()
{
	list<Selectable *> touched;

	if (_internal_editing) {
		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
			if (mrv) {
				mrv->invert_selection ();
			}
		}
		return;
	}

	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_inverted_selectables (*selection, touched);
	}

	selection->set (touched);
}

/** @param start Start time in session frames.
 *  @param end End time in session frames.
 *  @param top Top (lower) y limit in trackview coordinates (ie 0 at the top of the track view)
 *  @param bottom Bottom (higher) y limit in trackview coordinates (ie 0 at the top of the track view)
 *  @param preserve_if_selected true to leave the current selection alone if we're adding to the selection and all of the selectables
 *  within the region are already selected.
 */
void
Editor::select_all_within (framepos_t start, framepos_t end, double top, double bot, const TrackViewList& tracklist, Selection::Operation op, bool preserve_if_selected)
{
	list<Selectable*> found;

	for (TrackViewList::const_iterator iter = tracklist.begin(); iter != tracklist.end(); ++iter) {

		if ((*iter)->hidden()) {
			continue;
		}

		(*iter)->get_selectables (start, end, top, bot, found);
	}

	if (found.empty()) {
		return;
	}

	if (preserve_if_selected && op != Selection::Toggle) {
		list<Selectable*>::iterator i = found.begin();
		while (i != found.end() && (*i)->get_selected()) {
			++i;
		}

		if (i == found.end()) {
			return;
		}
	}

	begin_reversible_command (_("select all within"));
	switch (op) {
	case Selection::Add:
		selection->add (found);
		break;
	case Selection::Toggle:
		selection->toggle (found);
		break;
	case Selection::Set:
		selection->set (found);
		break;
	case Selection::Extend:
		/* not defined yet */
		break;
	}

	commit_reversible_command ();
}

void
Editor::set_selection_from_region ()
{
	if (selection->regions.empty()) {
		return;
	}

	selection->set (selection->regions.start(), selection->regions.end_frame());
	if (!Profile->get_sae()) {
		set_mouse_mode (Editing::MouseRange, false);
	}
}

void
Editor::set_selection_from_punch()
{
	Location* location;

	if ((location = _session->locations()->auto_punch_location()) == 0)  {
		return;
	}

	set_selection_from_range (*location);
}

void
Editor::set_selection_from_loop()
{
	Location* location;

	if ((location = _session->locations()->auto_loop_location()) == 0)  {
		return;
	}
	set_selection_from_range (*location);
}

void
Editor::set_selection_from_range (Location& loc)
{
	begin_reversible_command (_("set selection from range"));
	selection->set (loc.start(), loc.end());
	commit_reversible_command ();

	if (!Profile->get_sae()) {
		set_mouse_mode (Editing::MouseRange, false);
	}
}

void
Editor::select_all_selectables_using_time_selection ()
{
	list<Selectable *> touched;

	if (selection->time.empty()) {
		return;
	}

	framepos_t start = selection->time[clicked_selection].start;
	framepos_t end = selection->time[clicked_selection].end;

	if (end - start < 1)  {
		return;
	}

	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
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
	Location* location = _session->locations()->auto_punch_location();
	list<Selectable *> touched;

	if (location == 0 || (location->end() - location->start() <= 1))  {
		return;
	}


	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
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
	Location* location = _session->locations()->auto_loop_location();
	list<Selectable *> touched;

	if (location == 0 || (location->end() - location->start() <= 1))  {
		return;
	}


	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
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
Editor::select_all_selectables_using_cursor (EditorCursor *cursor, bool after)
{
	framepos_t start;
	framepos_t end;
	list<Selectable *> touched;

	if (after) {
		start = cursor->current_frame;
		end = _session->current_end_frame();
	} else {
		if (cursor->current_frame > 0) {
			start = 0;
			end = cursor->current_frame - 1;
		} else {
			return;
		}
	}

	if (_internal_editing) {
		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
			if (mrv) {
				mrv->select_range (start, end);
			}
		}
		return;
	}

	if (after) {
		begin_reversible_command (_("select all after cursor"));
	} else {
		begin_reversible_command (_("select all before cursor"));
	}

	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}
	selection->set (touched);
	commit_reversible_command ();
}

void
Editor::select_all_selectables_at_cursor (EditorCursor *cursor)
{
	framepos_t start;
	framepos_t end;
	list<Selectable *> touched;

	start = cursor->current_frame;
	end = cursor->current_frame+1;

	if (_internal_editing) {
		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
			if (mrv) {
				mrv->select_range (start, end);
			}
		}
		return;
	}

	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}
	selection->set (touched);
}

void
Editor::select_all_selectables_using_edit (bool after)
{
	framepos_t start;
	framepos_t end;
	list<Selectable *> touched;

	if (after) {
		start = get_preferred_edit_position();
		end = _session->current_end_frame();
	} else {
		if ((end = get_preferred_edit_position()) > 1) {
			start = 0;
			end -= 1;
		} else {
			return;
		}
	}

	if (_internal_editing) {
		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
			mrv->select_range (start, end);
		}
		return;
	}

	if (after) {
		begin_reversible_command (_("select all after edit"));
	} else {
		begin_reversible_command (_("select all before edit"));
	}

	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}
	selection->set (touched);
	commit_reversible_command ();
}

void
Editor::select_all_selectables_between (bool /*within*/)
{
	framepos_t start;
	framepos_t end;
	list<Selectable *> touched;

	if (!get_edit_op_range (start, end)) {
		return;
	}

	if (_internal_editing) {
		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
			mrv->select_range (start, end);
		}
		return;
	}

	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator iter = ts->begin(); iter != ts->end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}

	selection->set (touched);
}

void
Editor::select_range_between ()
{
	framepos_t start;
	framepos_t end;

	if ( !selection->time.empty() ) {
		selection->clear_time ();
	}

	if (!get_edit_op_range (start, end)) {
		return;
	}

	set_mouse_mode (MouseRange);
	selection->set (start, end);
}

bool
Editor::get_edit_op_range (framepos_t& start, framepos_t& end) const
{
	framepos_t m;
	bool ignored;

	/* if an explicit range exists, use it */

	if (!selection->time.empty()) {
		/* we know that these are ordered */
		start = selection->time.start();
		end = selection->time.end_frame();
		return true;
	}

	if (!mouse_frame (m, ignored)) {
		/* mouse is not in a canvas, try playhead+selected marker.
		   this is probably most true when using menus.
		*/

		if (selection->markers.empty()) {
			return false;
		}

		start = selection->markers.front()->position();
		end = _session->audible_frame();

	} else {

		switch (_edit_point) {
		case EditAtPlayhead:
			if (selection->markers.empty()) {
				/* use mouse + playhead */
				start = m;
				end = _session->audible_frame();
			} else {
				/* use playhead + selected marker */
				start = _session->audible_frame();
				end = selection->markers.front()->position();
			}
			break;

		case EditAtMouse:
			/* use mouse + selected marker */
			if (selection->markers.empty()) {
				start = m;
				end = _session->audible_frame();
			} else {
				start = selection->markers.front()->position();
				end = m;
			}
			break;

		case EditAtSelectedMarker:
			/* use mouse + selected marker */
			if (selection->markers.empty()) {

				MessageDialog win (_("No edit range defined"),
				                   false,
				                   MESSAGE_INFO,
				                   BUTTONS_OK);

				win.set_secondary_text (
					_("the edit point is Selected Marker\nbut there is no selected marker."));


				win.set_default_response (RESPONSE_CLOSE);
				win.set_position (Gtk::WIN_POS_MOUSE);
				win.show_all();

				win.run ();

				return false; // NO RANGE
			}
			start = selection->markers.front()->position();
			end = m;
			break;
		}
	}

	if (start == end) {
		return false;
	}

	if (start > end) {
		swap (start, end);
	}

	/* turn range into one delimited by start...end,
	   not start...end-1
	*/

	end++;

	return true;
}

void
Editor::deselect_all ()
{
	selection->clear ();
}

long
Editor::select_range (framepos_t s, framepos_t e)
{
	selection->add (clicked_axisview);
	selection->time.clear ();
	return selection->set (s, e);
}
