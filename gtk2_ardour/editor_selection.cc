/*
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2017 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015 André Nusser <andre.nusser@googlemail.com>
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
#include <cstdlib>

#include "pbd/unwind.h"

#include "ardour/control_protocol_manager.h"
#include "ardour/midi_region.h"
#include "ardour/playlist.h"
#include "ardour/profile.h"
#include "ardour/route_group.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/vca.h"

#include "editor.h"
#include "editor_drag.h"
#include "editor_routes.h"
#include "editor_sources.h"
#include "actions.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "automation_line.h"
#include "control_point.h"
#include "editor_regions.h"
#include "editor_cursors.h"
#include "keyboard.h"
#include "midi_region_view.h"
#include "sfdb_ui.h"

#include "pbd/i18n.h"

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

	if (!selection->selected (&view)) {
		to_be_added.push_back (&view);
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
	TrackViewList tracks;
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*i);
		if ( rtv && rtv->route()->is_track() ) {
			tracks.push_back (*i);
		}
	}
	PBD::Unwinder<bool> uw (_track_selection_change_without_scroll, true);
	selection->set (tracks);
}

void
Editor::select_all_visible_lanes ()
{
	TrackViewList visible_views;
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if ((*i)->marked_for_display()) {
			visible_views.push_back (*i);
		}
	}
	PBD::Unwinder<bool> uw (_track_selection_change_without_scroll, true);
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

	PBD::Unwinder<bool> uw (_editor_track_selection_change_without_scroll, true);

	RouteGroup* group = NULL;
	if (clicked_routeview) {
		group = clicked_routeview->route()->route_group();
	}

	switch (op) {
	case Selection::Toggle:
		if (selection->selected (clicked_axisview)) {
			if (group && group->is_active() && group->enabled_property(ARDOUR::Properties::group_select.property_id)) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
					if ((*i)->route_group() == group) {
						selection->remove(*i);
					}
				}
			} else {
				selection->remove (clicked_axisview);
			}
		} else {
			if (group && group->is_active() && group->enabled_property(ARDOUR::Properties::group_select.property_id)) {
				for (TrackViewList::iterator i = track_views.begin(); i != track_views.end (); ++i) {
					if ((*i)->route_group() == group) {
						selection->add(*i);
					}
				}
			} else {
				selection->add (clicked_axisview);
			}
		}
		break;

	case Selection::Add:
		if (group && group->is_active() && group->enabled_property(ARDOUR::Properties::group_select.property_id)) {
			for (TrackViewList::iterator i  = track_views.begin(); i != track_views.end (); ++i) {
				if ((*i)->route_group() == group) {
					selection->add(*i);
				}
			}
		} else {
			selection->add (clicked_axisview);
		}
		break;

	case Selection::Set:
		selection->clear();
		if (group && group->is_active() && group->enabled_property(ARDOUR::Properties::group_select.property_id)) {
			for (TrackViewList::iterator i  = track_views.begin(); i != track_views.end (); ++i) {
				if ((*i)->route_group() == group) {
					selection->add(*i);
				}
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
	begin_reversible_selection_op (X_("Set Selected Track"));

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
		selection->add (&view);
		break;

	case Selection::Set:
		selection->set (&view);
		break;

	case Selection::Extend:
		extend_selection_to_track (view);
		break;
	}

	commit_reversible_selection_op ();
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

	bool ret = false;

	switch (op) {
	case Selection::Set:
		if (!selection->selected (clicked_control_point)) {
			selection->set (clicked_control_point);
			ret = true;
		} else {
			/* clicked on an already selected point */
			if (press) {
				break;
			} else {
				if (selection->points.size() > 1) {
					selection->set (clicked_control_point);
					ret = true;
				}
			}
		}
		break;

	case Selection::Add:
		if (press) {
			selection->add (clicked_control_point);
			ret = true;
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
			ret = true;
		} else if (!press && !_control_point_toggled_on_press) {
			/* This is the release, and the point wasn't toggled on the press, so do it now */
			selection->toggle (clicked_control_point);
			ret = true;
		} else {
			/* Reset our flag */
			_control_point_toggled_on_press = false;
		}
		break;
	case Selection::Extend:
		/* XXX */
		break;
	}

	return ret;
}

void
Editor::get_onscreen_tracks (TrackViewList& tvl)
{
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if ((*i)->y_position() < _visible_canvas_height) {
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
Editor::mapover_grouped_routes (sigc::slot<void, RouteUI&> sl, RouteUI* basis, PBD::PropertyID prop) const
{
	set<RouteUI*> routes;

	routes.insert(basis);

	RouteGroup* group = basis->route()->route_group();

	if (group && group->enabled_property(prop) && group->enabled_property (Properties::active.property_id)) {

		/* the basis is a member of an active route group, with the appropriate
		 * properties; find other members */

		for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
			RouteUI* v = dynamic_cast<RouteUI*> (*i);
			if ( v && (v->route() != basis->route()) && v->route()->route_group() == group) {
				routes.insert (v);
			}
		}
	}

	/* call the slots */
	for (set<RouteUI*>::iterator i = routes.begin(); i != routes.end(); ++i) {
		sl (**i);
	}
}

void
Editor::mapover_armed_routes (sigc::slot<void, RouteUI&> sl) const
{
	set<RouteUI*> routes;
	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
		RouteUI* v = dynamic_cast<RouteUI*> (*i);
		if (v && v->route()->is_track()) {
			if ( v->track()->rec_enable_control()->get_value()) {
				routes.insert (v);
			}
		}
	}
	for (set<RouteUI*>::iterator i = routes.begin(); i != routes.end(); ++i) {
		sl (**i);
	}
}

void
Editor::mapover_selected_routes (sigc::slot<void, RouteUI&> sl) const
{
	set<RouteUI*> routes;
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
		if (r) {
			routes.insert (r);
		}
	}
	for (set<RouteUI*>::iterator i = routes.begin(); i != routes.end(); ++i) {
		sl (**i);
	}
}

void
Editor::mapover_all_routes (sigc::slot<void, RouteUI&> sl) const
{
	set<RouteUI*> routes;
	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
		RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
		if (r) {
			routes.insert (r);
		}
	}
	for (set<RouteUI*>::iterator i = routes.begin(); i != routes.end(); ++i) {
		sl (**i);
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

	if (group && group->enabled_property(prop) && group->enabled_property (Properties::active.property_id)) {

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
Editor::mapover_all_tracks_with_unique_playlists (sigc::slot<void, RouteTimeAxisView&, uint32_t> sl) const
{
	set<boost::shared_ptr<Playlist> > playlists;

	set<RouteTimeAxisView*> tracks;

	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {
		RouteTimeAxisView* v = dynamic_cast<RouteTimeAxisView*> (*i);

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

void
Editor::get_all_equivalent_regions (RegionView* basis, vector<RegionView*>& equivalent_regions) const
{
	mapover_all_tracks_with_unique_playlists (sigc::bind (sigc::mem_fun (*this, &Editor::mapped_get_equivalent_regions), basis, &equivalent_regions));

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

						/* just remove this one region
						 * (or all equivalent regions
						 * for RippleAll, but only on a
						 * permitted button release
						 */

						if (Config->get_edit_mode() == RippleAll) {
							get_all_equivalent_regions (clicked_regionview, all_equivalent_regions);
							selection->remove (all_equivalent_regions);
						} else {
							selection->remove (clicked_regionview);
						}
						commit = true;

						/* no more deselect action on button release till a new press
						   finds an already selected object.
						*/

						button_release_can_deselect = false;
					}
				}

			} else {

				if (press) {

					if (Config->get_edit_mode() == RippleAll) {
						get_all_equivalent_regions (clicked_regionview, all_equivalent_regions);
					} else {
						if (selection->selected (clicked_routeview)) {
							get_equivalent_regions (clicked_regionview, all_equivalent_regions, ARDOUR::Properties::group_select.property_id);
						} else {
							all_equivalent_regions.push_back (clicked_regionview);
						}
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
				if (Config->get_edit_mode() == RippleAll) {
					get_all_equivalent_regions (clicked_regionview, all_equivalent_regions);
				} else {
					get_equivalent_regions (clicked_regionview, all_equivalent_regions, ARDOUR::Properties::group_select.property_id);
				}
				selection->set (all_equivalent_regions);
				commit = true;
			} else {
				/* clicked on an already selected region */
				if (press)
					goto out;
				else {
					if (selection->regions.size() > 1) {
						/* collapse region selection down to just this one region (and its equivalents) */
						if (Config->get_edit_mode() == RippleAll) {
							get_all_equivalent_regions (clicked_regionview, all_equivalent_regions);
						} else {
							get_equivalent_regions(clicked_regionview, all_equivalent_regions, ARDOUR::Properties::group_select.property_id);
						}
						selection->set(all_equivalent_regions);
						commit = true;
					}
				}
			}
			break;

		default:
			/* silly compiler */
			break;
		}

	} else if (op == Selection::Extend) {

		list<Selectable*> results;
		timepos_t last_pos;
		timepos_t first_pos;
		bool same_track = false;

		/* 1. find the last selected regionview in the track that was clicked in */

		const Temporal::TimeDomain time_domain = (selection->regions.empty() ? Temporal::AudioTime : selection->regions.front()->region()->position().time_domain());
		last_pos = timepos_t (time_domain);
		first_pos = timepos_t::max (time_domain);

		for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {
			if (&(*x)->get_time_axis_view() == &clicked_regionview->get_time_axis_view()) {

				if ((*x)->region()->nt_last() > last_pos) {
					last_pos = (*x)->region()->nt_last();
				}

				if ((*x)->region()->position() < first_pos) {
					first_pos = (*x)->region()->position();
				}

				same_track = true;
			}
		}

		if (same_track) {

			/* 2. figure out the boundaries for our search for new objects */

			switch (clicked_regionview->region()->coverage (first_pos, last_pos)) {
			case Temporal::OverlapNone:
				if (last_pos < clicked_regionview->region()->position()) {
					first_pos = last_pos;
					last_pos = clicked_regionview->region()->nt_last();
				} else {
					last_pos = first_pos;
					first_pos = clicked_regionview->region()->position();
				}
				break;

			case Temporal::OverlapExternal:
				if (last_pos < clicked_regionview->region()->position()) {
					first_pos = last_pos;
					last_pos = clicked_regionview->region()->nt_last();
				} else {
					last_pos = first_pos;
					first_pos = clicked_regionview->region()->position();
				}
				break;

			case Temporal::OverlapInternal:
				if (last_pos < clicked_regionview->region()->position()) {
					first_pos = last_pos;
					last_pos = clicked_regionview->region()->nt_last();
				} else {
					last_pos = first_pos;
					first_pos = clicked_regionview->region()->position();
				}
				break;

			case Temporal::OverlapStart:
			case Temporal::OverlapEnd:
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


			first_pos = clicked_regionview->region()->position();
			last_pos = clicked_regionview->region()->nt_last();

			for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
				if ((*i)->region()->position() < first_pos) {
					first_pos = (*i)->region()->position();
				}
				if ((*i)->region()->end() > last_pos) {
					last_pos = (*i)->region()->nt_last();
				}
			}
		}

		/* 2. find all the tracks we should select in */

		set<RouteTimeAxisView*> relevant_tracks;

		if (Config->get_edit_mode() == RippleAll) {
			for (TrackSelection::iterator i = track_views.begin(); i != track_views.end(); ++i) {
				RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
				if (r) {
					relevant_tracks.insert (r);
				}
			}
		} else {
			for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
				RouteTimeAxisView* r = dynamic_cast<RouteTimeAxisView*> (*i);
				if (r) {
					relevant_tracks.insert (r);
				}
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
					int key = rtv->route()->presentation_info().order ();

					for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {

						RouteTimeAxisView* artv = dynamic_cast<RouteTimeAxisView*>(&(*x)->get_time_axis_view());

						if (artv && artv != rtv) {

							pair<set<RouteTimeAxisView*>::iterator,bool> result;

							result = already_in_selection.insert (artv);

							if (result.second) {
								/* newly added to already_in_selection */

								int d = artv->route()->presentation_info().order ();

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

						int okey = closest->route()->presentation_info().order ();

						if (okey > key) {
							swap (okey, key);
						}

						for (TrackViewList::iterator x = track_views.begin(); x != track_views.end(); ++x) {
							RouteTimeAxisView* artv = dynamic_cast<RouteTimeAxisView*>(*x);
							if (artv && artv != rtv) {

								int k = artv->route()->presentation_info().order ();

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
			(*t)->get_selectables (first_pos, last_pos, -1.0, -1.0, results);
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
		} else if (selection->regions.empty() && !selection->selected (clicked_regionview)) {
			/* ensure that at least the clicked regionview is selected. */
			selection->set (clicked_regionview);
			commit = true;
		}

	}

out:
	return commit;
}

void
Editor::set_selected_midi_region_view (MidiRegionView& mrv)
{
	/* clear note selection in all currently selected MidiRegionViews */

	if (get_selection().regions.contains (&mrv) && get_selection().regions.size() == 1) {
		/* Nothing to do */
		return;
	}

	midi_action (&MidiRegionView::clear_note_selection);
	get_selection().set (&mrv);
}

void
Editor::set_selection (std::list<Selectable*> s, Selection::Operation op)
{
	if (s.empty()) {
		return;
	}
	begin_reversible_selection_op (X_("set selection"));
	switch (op) {
		case Selection::Toggle:
			selection->toggle (s);
			break;
		case Selection::Set:
			selection->set (s);
			break;
		case Selection::Extend:
			selection->add (s);
			break;
		case Selection::Add:
			selection->add (s);
			break;
	}

	commit_reversible_selection_op () ;
}

void
Editor::set_selected_regionview_from_region_list (boost::shared_ptr<Region> region, Selection::Operation op)
{
	vector<RegionView*> regionviews;

	get_regionview_corresponding_to (region, regionviews);

	if (regionviews.empty()) {
		return;
	}

	begin_reversible_selection_op (X_("set selected regions"));

	switch (op) {
	case Selection::Toggle:
		/* XXX this is not correct */
		selection->toggle (regionviews);
		break;
	case Selection::Set:
		selection->set (regionviews);
		break;
	case Selection::Extend:
		selection->add (regionviews);
		break;
	case Selection::Add:
		selection->add (regionviews);
		break;
	}

	commit_reversible_selection_op () ;
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

	begin_reversible_selection_op (X_("set selected regions"));

	selection->set (rv);

	commit_reversible_selection_op () ;

	return true;
}

void
Editor::presentation_info_changed (PropertyChange const & what_changed)
{
	uint32_t n_tracks = 0;
	uint32_t n_busses = 0;
	uint32_t n_vcas = 0;
	uint32_t n_routes = 0;
	uint32_t n_stripables = 0;

	/* We cannot ensure ordering of the handlers for
	 * PresentationInfo::Changed, so we have to do everything in order
	 * here, as a single handler.
	 */

	if (what_changed.contains (Properties::selected)) {
		for (TrackViewList::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			(*i)->set_selected (false);
			(*i)->hide_selection ();
		}
	}

	/* STEP 1: set the GUI selection state (in which TimeAxisViews for the
	 * currently selected stripable/controllable duples are found and added
	 */

	selection->core_selection_changed (what_changed);

	/* STEP 2: update TimeAxisView's knowledge of their selected state
	 */

	if (what_changed.contains (Properties::selected)) {

		StripableNotificationListPtr stripables (new StripableNotificationList);

		switch (selection->tracks.size()) {
		case 0:
			break;
		default:
			set_selected_mixer_strip (*(selection->tracks.back()));
			if (!_track_selection_change_without_scroll && !_editor_track_selection_change_without_scroll) {
				ensure_time_axis_view_is_visible (*(selection->tracks.back()), false);
			}
			break;
		}

		CoreSelection::StripableAutomationControls sc;
		_session->selection().get_stripables (sc);

		for (CoreSelection::StripableAutomationControls::const_iterator i = sc.begin(); i != sc.end(); ++i) {

			AxisView* av = axis_view_by_stripable ((*i).stripable);

			if (!av) {
				continue;
			}

			n_stripables++;

			if (boost::dynamic_pointer_cast<Track> ((*i).stripable)) {
				n_tracks++;
				n_routes++;
			} else if (boost::dynamic_pointer_cast<Route> ((*i).stripable)) {
				n_busses++;
				n_routes++;
			} else if (boost::dynamic_pointer_cast<VCA> ((*i).stripable)) {
				n_vcas++;
			}

			TimeAxisView* tav = dynamic_cast<TimeAxisView*> (av);

			if (!tav) {
				assert (0);
				continue; /* impossible */
			}

			if (!(*i).controllable) {

				/* "parent" track selected */
				tav->set_selected (true);
				tav->reshow_selection (selection->time);

			} else {

				/* possibly a child */

				TimeAxisView::Children c = tav->get_child_list ();

				for (TimeAxisView::Children::iterator j = c.begin(); j != c.end(); ++j) {

					boost::shared_ptr<AutomationControl> control = (*j)->control ();

					if (control != (*i).controllable) {
						continue;
					}

					(*j)->set_selected (true);
					(*j)->reshow_selection (selection->time);
				}
			}

			stripables->push_back ((*i).stripable);
		}

		ActionManager::set_sensitive (ActionManager::stripable_selection_sensitive_actions, (n_stripables > 0));
		ActionManager::set_sensitive (ActionManager::track_selection_sensitive_actions, (n_tracks > 0));
		ActionManager::set_sensitive (ActionManager::bus_selection_sensitive_actions, (n_busses > 0));
		ActionManager::set_sensitive (ActionManager::route_selection_sensitive_actions, (n_routes > 0));
		ActionManager::set_sensitive (ActionManager::vca_selection_sensitive_actions, (n_vcas > 0));

		sensitize_the_right_region_actions (false);

		/* STEP 4: notify control protocols */

		ControlProtocolManager::instance().stripable_selection_changed (stripables);

		if (sfbrowser && _session && !_session->deletion_in_progress()) {
			uint32_t audio_track_cnt = 0;
			uint32_t midi_track_cnt = 0;

			for (TrackSelection::iterator x = selection->tracks.begin(); x != selection->tracks.end(); ++x) {
				AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(*x);

				if (atv) {
					if (atv->is_audio_track()) {
						audio_track_cnt++;
					}

				} else {
					MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(*x);

					if (mtv) {
						if (mtv->is_midi_track()) {
							midi_track_cnt++;
						}
					}
				}
			}

			sfbrowser->reset (audio_track_cnt, midi_track_cnt);
		}
	}

	/* STEP 4: update Editor::track_views */

	PropertyChange soh;

	soh.add (Properties::order);
	soh.add (Properties::hidden);

	if (what_changed.contains (soh)) {
		queue_redisplay_track_views ();
	}
}

void
Editor::track_selection_changed ()
{
	/* reset paste count, so the plaste location doesn't get incremented
	 * if we want to paste in the same place, but different track. */
	paste_count = 0;

	if ( _session->solo_selection_active() )
		play_solo_selection(false);
}

void
Editor::time_selection_changed ()
{
	/* XXX this is superficially inefficient. Hide the selection in all
	 * tracks, then show it in all selected tracks.
	 *
	 * However, if you investigate what this actually does, it isn't
	 * anywhere nearly as bad as it may appear. Remember: nothing is
	 * redrawn or even recomputed during these two loops - that only
	 * happens when we next render ...
	 */

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

	/* propagate into backend, but only when there is no drag or we are at
	 * the end of a drag, otherwise this is too expensive (could case a
	 * locate per mouse motion event.
	 */

	if (_session && !_drags->active()) {
		if (selection->time.length() != 0) {
			_session->set_range_selection (selection->time.start_time(), selection->time.end_time());
		} else {
			_session->clear_range_selection ();
		}
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

/** Sensitize region-based actions.
 *
 *  This method is called from whenever we leave the canvas, either by moving
 *  the pointer out of it, or by popping up a context menu. See
 *  Editor::{entered,left}_track_canvas() for details there.
 */
void
Editor::sensitize_the_right_region_actions (bool because_canvas_crossing)
{
	bool have_selection = false;
	bool have_entered = false;
	bool have_edit_point = false;
	bool have_selected_source = false;
	RegionSelection rs;

	// std::cerr << "STRRA: crossing ? " << because_canvas_crossing << " within ? " << within_track_canvas
	// << std::endl;

	if (!selection->regions.empty()) {
		have_selection = true;
		rs = selection->regions;
	}

	if (entered_regionview) {
		have_entered = true;
		rs.add (entered_regionview);
	}

	if ( _sources->get_single_selection() ) {
		have_selected_source = true;
	}

	if (rs.empty() && !selection->tracks.empty()) {

		/* no selected regions, but some selected tracks.
		 */

		if (_edit_point == EditAtMouse) {
			if (!within_track_canvas) {
				/* pointer is not in canvas, so edit point is meaningless */
				have_edit_point = false;
			} else {
				/* inside canvas. we don't know where the edit
				   point will be when an action is invoked, but
				   assume it could intersect with a region.
				*/
				have_edit_point = true;
			}
		} else {
			RegionSelection at_edit_point;
			timepos_t const where = get_preferred_edit_position (Editing::EDIT_IGNORE_NONE, false, !within_track_canvas);
			get_regions_at (at_edit_point, where, selection->tracks);
			if (!at_edit_point.empty()) {
				have_edit_point = true;
			}
			if (rs.empty()) {
				rs.insert (rs.end(), at_edit_point.begin(), at_edit_point.end());
			}
		}
	}

	//std::cerr << "\tfinal have selection: " << have_selection
	// << " have entered " << have_entered
	// << " have edit point " << have_edit_point
	// << " EP = " << enum_2_string (_edit_point)
	// << std::endl;

	typedef std::map<std::string,RegionAction> RegionActionMap;

	_ignore_region_action = true;

	for (RegionActionMap::iterator x = region_action_map.begin(); x != region_action_map.end(); ++x) {
		RegionActionTarget tgt = x->second.target;
		bool sensitive = false;

		if ((tgt & SelectedRegions) && have_selection) {
			sensitive = true;
		} else if ((tgt & EnteredRegions) && have_entered) {
			sensitive = true;
		} else if ((tgt & EditPointRegions) && have_edit_point) {
			sensitive = true;
		} else if ((tgt & ListSelection) && have_selected_source ) {
			sensitive = true;
		}

		x->second.action->set_sensitive (sensitive);
	}

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
	bool have_transients = false;

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

		if (r->position_time_domain() == Temporal::BeatTime) {
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

		if (r->has_transients ()){
			have_transients = true;
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

	_region_actions->get_action("split-region-at-transients")->set_sensitive (have_transients);

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
		_region_actions->get_action("legatize-region")->set_sensitive (false);
		_region_actions->get_action("remove-overlap")->set_sensitive (false);
		_region_actions->get_action("transform-region")->set_sensitive (false);
		_region_actions->get_action("fork-region")->set_sensitive (false);
		_region_actions->get_action("insert-patch-change-context")->set_sensitive (false);
		_region_actions->get_action("insert-patch-change")->set_sensitive (false);
		_region_actions->get_action("transpose-region")->set_sensitive (false);
	} else {
		editor_menu_actions->get_action("RegionMenuMIDI")->set_sensitive (true);
		/* others were already marked sensitive */
	}

	/* ok, moving along... */

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

		_region_actions->get_action("loudness-analyze-region")->set_sensitive (false);
		_region_actions->get_action("spectral-analyze-region")->set_sensitive (false);
		_region_actions->get_action("reset-region-gain-envelopes")->set_sensitive (false);
		_region_actions->get_action("toggle-region-gain-envelope-active")->set_sensitive (false);
		_region_actions->get_action("pitch-shift-region")->set_sensitive (false);
		_region_actions->get_action("strip-region-silence")->set_sensitive (false);
		_region_actions->get_action("show-rhythm-ferret")->set_sensitive (false);

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

	vector<Widget*> proxies = a->get_proxies();
	for (vector<Widget*>::iterator p = proxies.begin(); p != proxies.end(); ++p) {
		Gtk::CheckMenuItem* cmi = dynamic_cast<Gtk::CheckMenuItem*> (*p);
		if (cmi) {
			cmi->set_inconsistent (have_position_lock_style_music && have_position_lock_style_audio);
		}
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

	/* Todo: insert-region-from-source-list */
	/* XXX: should also check that there is a track of the appropriate type for the selected region */
#if 0
	if (_edit_point == EditAtMouse || _regions->get_single_selection() == 0 || selection->tracks.empty()) {
		_region_actions->get_action("insert-region-from-source-list")->set_sensitive (false);
	} else {
		_region_actions->get_action("insert-region-from-source-list")->set_sensitive (true);
	}
#endif

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

	sensitize_the_right_region_actions (false);

	/* propagate into backend */
	assert (_session);

	if (!selection->regions.empty()) {
		_session->set_object_selection (selection->regions.start_time(), selection->regions.end_time());
	} else {
		_session->clear_object_selection ();
	}

	if (_session->solo_selection_active()) {
		play_solo_selection(false);
	}

	/* set nudge button color */
	if (! get_regions_from_selection_and_entered().empty()) {
		/* nudge regions */
		nudge_forward_button.set_name ("nudge button");
		nudge_backward_button.set_name ("nudge button");
	} else {
		/* nudge marker or playhead */
		nudge_forward_button.set_name ("transport button");
		nudge_backward_button.set_name ("transport button");
	}

	//there are a few global Editor->Select actions which select regions even if you aren't in Object mode.
	//if regions are selected, we must always force the mouse mode to Object...
	//... otherwise the user is confusingly left with selected regions that can't be manipulated.
	if (!selection->regions.empty() && !internal_editing()) {

		/* if in MouseAudition and there's just 1 region selected
		 * (i.e. we just clicked on it), leave things as they are
		 */

		if (selection->regions.size() > 1 || mouse_mode != Editing::MouseAudition) {
			set_mouse_mode (MouseObject, false);
		}
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

	begin_reversible_selection_op (X_("Select All in Track"));

	clicked_routeview->get_selectables (timepos_t(), timepos_t::max (Temporal::AudioTime), 0, DBL_MAX, touched);

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

	commit_reversible_selection_op ();
}

bool
Editor::select_all_internal_edit (Selection::Operation)
{
	bool selected = false;

	RegionSelection copy (selection->regions);

	for (RegionSelection::iterator i = copy.begin(); i != copy.end(); ++i) {
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
		if (mrv) {
			mrv->select_all_notes ();
			selected = true;
		}
	}

	MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(entered_regionview);
	if (mrv) {
		mrv->select_all_notes ();
		selected = true;
	}

	return selected;
}

void
Editor::select_all_objects (Selection::Operation op)
{
	list<Selectable *> touched;

	if (internal_editing() && select_all_internal_edit(op)) {
		return;  // Selected notes
	}

	TrackViewList ts;

	if (selection->tracks.empty()) {
		ts = track_views;
	} else {
		ts = selection->tracks;
	}

	for (TrackViewList::iterator iter = ts.begin(); iter != ts.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (timepos_t(), timepos_t::max (Temporal::AudioTime), 0, DBL_MAX, touched);
	}

	begin_reversible_selection_op (X_("select all"));
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
		/* meaningless, because we're selecting everything */
		break;
	}
	commit_reversible_selection_op ();
}

void
Editor::invert_selection_in_track ()
{
	list<Selectable *> touched;

	if (!clicked_routeview) {
		return;
	}

	begin_reversible_selection_op (X_("Invert Selection in Track"));
	clicked_routeview->get_inverted_selectables (*selection, touched);
	selection->set (touched);
	commit_reversible_selection_op ();
}

void
Editor::invert_selection ()
{

	if (internal_editing()) {
		MidiRegionSelection ms = selection->midi_regions();
		for (MidiRegionSelection::iterator i = ms.begin(); i != ms.end(); ++i) {
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
			if (mrv) {
				mrv->invert_selection ();
			}
		}
		return;
	}

	if (!selection->tracks.empty()) {

		TrackViewList inverted;

		for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
			if (!(*iter)->selected()) {
				inverted.push_back (*iter);
			}
		}

		begin_reversible_selection_op (X_("Invert Track Selection"));
		selection->set (inverted);
		commit_reversible_selection_op ();

	} else {

		list<Selectable *> touched;

		for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
			if ((*iter)->hidden()) {
				continue;
			}
			(*iter)->get_inverted_selectables (*selection, touched);
		}

		begin_reversible_selection_op (X_("Invert ObjectSelection"));
		selection->set (touched);
		commit_reversible_selection_op ();
	}
}

/** @param start Start time in session samples.
 *  @param end End time in session samples.
 *  @param top Top (lower) y limit in trackview coordinates (ie 0 at the top of the track view)
 *  @param bottom Bottom (higher) y limit in trackview coordinates (ie 0 at the top of the track view)
 *  @param preserve_if_selected true to leave the current selection alone if we're adding to the selection and all of the selectables
 *  within the region are already selected.
 */
void
Editor::select_all_within (timepos_t const & start, timepos_t const & end, double top, double bot, const TrackViewList& tracklist, Selection::Operation op, bool preserve_if_selected)
{
	list<Selectable*> found;

	for (TrackViewList::const_iterator iter = tracklist.begin(); iter != tracklist.end(); ++iter) {

		if ((*iter)->hidden()) {
			continue;
		}

		(*iter)->get_selectables (start, end, top, bot, found);
	}

	if (found.empty()) {
		selection->clear_objects();
		selection->clear_time ();
		return;
	}

	if (preserve_if_selected && op != Selection::Toggle) {
		list<Selectable*>::iterator i = found.begin();
		while (i != found.end() && (*i)->selected()) {
			++i;
		}

		if (i == found.end()) {
			return;
		}
	}

	begin_reversible_selection_op (X_("select all within"));
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

	commit_reversible_selection_op ();
}

void
Editor::set_selection_from_region ()
{
	if (selection->regions.empty()) {
		return;
	}

	/* find all the tracks that have selected regions */

	set<TimeAxisView*> tracks;

	for (RegionSelection::const_iterator r = selection->regions.begin(); r != selection->regions.end(); ++r) {
		tracks.insert (&(*r)->get_time_axis_view());
	}

	TrackViewList tvl;
	tvl.insert (tvl.end(), tracks.begin(), tracks.end());

	/* select range (this will clear the region selection) */

	selection->set (selection->regions.start_time(), selection->regions.end_time());

	/* and select the tracks */

	selection->set (tvl);

	if (!get_smart_mode () || !(mouse_mode == Editing::MouseObject) ) {
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
	begin_reversible_selection_op (X_("set selection from range"));

	selection->set (loc.start(), loc.end());

	// if no tracks are selected, enable all tracks
	// (_something_ has to be selected for any range selection, otherwise the user won't see anything)
	if (selection->tracks.empty()) {
		select_all_visible_lanes();
	}

	commit_reversible_selection_op ();

	if (!get_smart_mode () || mouse_mode != Editing::MouseObject) {
		set_mouse_mode (MouseRange, false);
	}
}

void
Editor::select_all_selectables_using_time_selection ()
{
	list<Selectable *> touched;

	if (selection->time.empty()) {
		return;
	}

	timepos_t start = selection->time[clicked_selection].start();
	timepos_t end = selection->time[clicked_selection].end();

	const timecnt_t distance = start.distance (end);

	if (distance.is_negative () || distance.is_zero ())  {
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

	begin_reversible_selection_op (X_("select all from range"));
	selection->set (touched);
	commit_reversible_selection_op ();
}


void
Editor::select_all_selectables_using_punch()
{
	Location* location = _session->locations()->auto_punch_location();
	list<Selectable *> touched;

	if (location == 0 || (location->length_samples() <= 1)) {
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
		(*iter)->get_selectables (location->start(), location->end(), 0, DBL_MAX, touched);
	}
	begin_reversible_selection_op (X_("select all from punch"));
	selection->set (touched);
	commit_reversible_selection_op ();

}

void
Editor::select_all_selectables_using_loop()
{
	Location* location = _session->locations()->auto_loop_location();
	list<Selectable *> touched;

	if (location == 0 || (location->length_samples() <= 1)) {
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
		(*iter)->get_selectables (location->start(), location->end(), 0, DBL_MAX, touched);
	}
	begin_reversible_selection_op (X_("select all from loop"));
	selection->set (touched);
	commit_reversible_selection_op ();

}

void
Editor::select_all_selectables_using_cursor (EditorCursor *cursor, bool after)
{
	timepos_t start;
	timepos_t end;
	list<Selectable *> touched;

	if (after) {
		start = timepos_t (cursor->current_sample());
		end = timepos_t (_session->current_end_sample());
	} else {
		if (cursor->current_sample() > 0) {
			start = timepos_t();
			end = timepos_t (cursor->current_sample() - 1);
		} else {
			return;
		}
	}

	if (internal_editing()) {
		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
			if (mrv) {
				mrv->select_range (start, end);
			}
		}
		return;
	}

	if (after) {
		begin_reversible_selection_op (X_("select all after cursor"));
	} else {
		begin_reversible_selection_op (X_("select all before cursor"));
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
	commit_reversible_selection_op ();
}

void
Editor::select_all_selectables_using_edit (bool after, bool from_context_menu)
{
	timepos_t start;
	timepos_t end;
	list<Selectable *> touched;

	if (after) {
		start = get_preferred_edit_position(EDIT_IGNORE_NONE, from_context_menu);
		end = timepos_t (_session->current_end_sample());
	} else {
		if ((end = get_preferred_edit_position(EDIT_IGNORE_NONE, from_context_menu)) > 1) {
			start = timepos_t ();
			end = end.decrement();
		} else {
			return;
		}
	}

	if (internal_editing()) {
		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(*i);
			mrv->select_range (start, end);
		}
		return;
	}

	if (after) {
		begin_reversible_selection_op (X_("select all after edit"));
	} else {
		begin_reversible_selection_op (X_("select all before edit"));
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
	commit_reversible_selection_op ();
}

void
Editor::select_all_selectables_between (bool within)
{
	timepos_t start;
	timepos_t end;
	list<Selectable *> touched;

	if (!get_edit_op_range (start, end)) {
		return;
	}

	if (internal_editing()) {
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
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched, within);
	}

	begin_reversible_selection_op (X_("Select all Selectables Between"));
	selection->set (touched);
	commit_reversible_selection_op ();
}

void
Editor::get_regionviews_at_or_after (timepos_t const & pos, RegionSelection& regions)
{
	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		(*iter)->get_regionviews_at_or_after (pos, regions);
	}
}

void
Editor::select_range_between ()
{
	timepos_t start;
	timepos_t end;

	if (!selection->time.empty()) {
		selection->clear_time ();
	}

	if (!get_edit_op_range (start, end)) {
		return;
	}

	if (!get_smart_mode () || mouse_mode != Editing::MouseObject) {
		set_mouse_mode (MouseRange, false);
	}

	begin_reversible_selection_op (X_("Select Range Between"));
	selection->set (start, end);
	commit_reversible_selection_op ();
}

bool
Editor::get_edit_op_range (timepos_t& start, timepos_t& end) const
{
	/* if an explicit range exists, use it */

	if ((mouse_mode == MouseRange || get_smart_mode()) &&  !selection->time.empty()) {
		/* we know that these are ordered */
		start = selection->time.start_time();
		end = selection->time.end_time();
		return true;
	} else {
		start = timepos_t ();
		end = timepos_t ();
		return false;
	}
}

void
Editor::deselect_all ()
{
	begin_reversible_selection_op (X_("Deselect All"));
	selection->clear ();
	commit_reversible_selection_op ();
}

long
Editor::select_range (timepos_t const & s, timepos_t const & e)
{
	begin_reversible_selection_op (X_("Select Range"));
	selection->add (clicked_axisview);
	selection->time.clear ();
	long ret = selection->set (s, e);
	commit_reversible_selection_op ();
	return ret;
}

void
Editor::catch_up_on_midi_selection ()
{
	RegionSelection regions;

	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}

		MidiTimeAxisView* matv = dynamic_cast<MidiTimeAxisView*> (*iter);
		if (!matv) {
			continue;
		}

		matv->get_regions_with_selected_data (regions);
	}

	if (!regions.empty()) {
		selection->set (regions);
	}
}

struct ViewStripable {
	TimeAxisView* tav;
	boost::shared_ptr<Stripable> stripable;

	ViewStripable (TimeAxisView* t, boost::shared_ptr<Stripable> s)
		: tav (t), stripable (s) {}
};

void
Editor::move_selected_tracks (bool up)
{
	TimeAxisView* scroll_to = 0;
	StripableList sl;
	_session->get_stripables (sl);

	if (sl.size() < 2) {
		/* nope */
		return;
	}

	sl.sort (Stripable::Sorter());

	std::list<ViewStripable> view_stripables;

	/* build a list that includes time axis view information */

	for (StripableList::const_iterator sli = sl.begin(); sli != sl.end(); ++sli) {
		TimeAxisView* tv = time_axis_view_from_stripable (*sli);
		view_stripables.push_back (ViewStripable (tv, *sli));
	}

	/* for each selected stripable, move it above or below the adjacent
	 * stripable that has a time-axis view representation here. If there's
	 * no such representation, then
	 */

	list<ViewStripable>::iterator unselected_neighbour;
	list<ViewStripable>::iterator vsi;

	{
		PresentationInfo::ChangeSuspender cs;

		if (up) {
			unselected_neighbour = view_stripables.end ();
			vsi = view_stripables.begin();

			while (vsi != view_stripables.end()) {

				if (vsi->stripable->is_selected()) {

					if (unselected_neighbour != view_stripables.end()) {

						PresentationInfo::order_t unselected_neighbour_order = unselected_neighbour->stripable->presentation_info().order();
						PresentationInfo::order_t my_order = vsi->stripable->presentation_info().order();

						unselected_neighbour->stripable->set_presentation_order (my_order);
						vsi->stripable->set_presentation_order (unselected_neighbour_order);

						if (!scroll_to) {
							scroll_to = vsi->tav;
						}
					}

				} else {

					if (vsi->tav) {
						unselected_neighbour = vsi;
					}

				}

				++vsi;
			}

		} else {

			unselected_neighbour = view_stripables.end();
			vsi = unselected_neighbour;

			do {

				--vsi;

				if (vsi->stripable->is_selected()) {

					if (unselected_neighbour != view_stripables.end()) {

						PresentationInfo::order_t unselected_neighbour_order = unselected_neighbour->stripable->presentation_info().order();
						PresentationInfo::order_t my_order = vsi->stripable->presentation_info().order();

						unselected_neighbour->stripable->set_presentation_order (my_order);
						vsi->stripable->set_presentation_order (unselected_neighbour_order);

						if (!scroll_to) {
							scroll_to = vsi->tav;
						}
					}

				} else {

					if (vsi->tav) {
						unselected_neighbour = vsi;
					}

				}

			} while (vsi != view_stripables.begin());
		}
	}

	if (scroll_to) {
		ensure_time_axis_view_is_visible (*scroll_to, false);
	}
}
