/*
 * Copyright (C) 2005-2006 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2005-2009 Sampo Savolainen <v2@iki.fi>
 * Copyright (C) 2005-2018 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Karsten Wiese <fzuuzf@googlemail.com>
 * Copyright (C) 2006-2015 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2017 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2013-2016 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2013-2017 John Emmas <john@creativepost.co.uk>
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

/* Note: public Editor methods are documented in public_editor.h */

#include <unistd.h>

#include <cstdlib>
#include <cmath>
#include <string>
#include <limits>
#include <map>
#include <set>

#include <gtkmm/messagedialog.h>

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"
#include "pbd/unwind.h"
#include "pbd/whitespace.h"
#include "pbd/stateful_diff_command.h"

#include "temporal/tempo.h"

#include "gtkmm2ext/utils.h"

#include "widgets/choice.h"
#include "widgets/popup.h"
#include "widgets/prompter.h"

#include "ardour/audioengine.h"
#include "ardour/audio_track.h"
#include "ardour/audioregion.h"
#include "ardour/boost_debug.h"
#include "ardour/clip_library.h"
#include "ardour/dB.h"
#include "ardour/location.h"
#include "ardour/midi_region.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/playlist_factory.h"
#include "ardour/profile.h"
#include "ardour/quantize.h"
#include "ardour/legatize.h"
#include "ardour/region_factory.h"
#include "ardour/reverse.h"
#include "ardour/selection.h"
#include "ardour/session.h"
#include "ardour/session_playlists.h"
#include "ardour/source.h"
#include "ardour/strip_silence.h"
#include "ardour/transient_detector.h"
#include "ardour/transport_master_manager.h"
#include "ardour/transpose.h"
#include "ardour/triggerbox.h"
#include "ardour/vca_manager.h"

#include "canvas/canvas.h"

#include "actions.h"
#include "ardour_message.h"
#include "ardour_ui.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "audio_time_axis.h"
#include "automation_region_view.h"
#include "automation_time_axis.h"
#include "control_point.h"
#include "debug.h"
#include "editing.h"
#include "editor.h"
#include "editor_cursors.h"
#include "editor_drag.h"
#include "editor_regions.h"
#include "editor_sources.h"
#include "editor_routes.h"
#include "gui_thread.h"
#include "insert_remove_time_dialog.h"
#include "interthread_progress_window.h"
#include "item_counts.h"
#include "keyboard.h"
#include "midi_region_view.h"
#include "mixer_ui.h"
#include "mixer_strip.h"
#include "mouse_cursors.h"
#include "normalize_dialog.h"
#include "note.h"
#include "paste_context.h"
#include "patch_change_dialog.h"
#include "quantize_dialog.h"
#include "region_gain_line.h"
#include "rgb_macros.h"
#include "route_time_axis.h"
#include "selection.h"
#include "selection_templates.h"
#include "streamview.h"
#include "strip_silence_dialog.h"
#include "time_axis_view.h"
#include "timers.h"
#include "transpose_dialog.h"
#include "transform_dialog.h"
#include "triggerbox_ui.h"
#include "ui_config.h"
#include "utils.h"
#include "vca_time_axis.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ArdourWidgets;
using namespace Editing;
using namespace Temporal;

using Gtkmm2ext::Keyboard;

/***********************************************************************
  Editor operations
 ***********************************************************************/

void
Editor::undo (uint32_t n)
{
	if (_session && _session->actively_recording()) {
		/* no undo allowed while recording. Session will check also,
		   but we don't even want to get to that.
		*/
		return;
	}

	if (_drags->active ()) {
		_drags->abort ();
	}
	paste_count = 0;

	if (_session) {
		_session->undo (n);
		if (_session->undo_depth() == 0) {
			undo_action->set_sensitive(false);
		}
		redo_action->set_sensitive(true);
		begin_selection_op_history ();
	}
}

void
Editor::redo (uint32_t n)
{
	if (_session && _session->actively_recording()) {
		/* no redo allowed while recording. Session will check also,
		   but we don't even want to get to that.
		*/
		return;
	}

	if (_drags->active ()) {
		_drags->abort ();
	}
	paste_count = 0;

	if (_session) {
	_session->redo (n);
		if (_session->redo_depth() == 0) {
			redo_action->set_sensitive(false);
		}
		undo_action->set_sensitive(true);
		begin_selection_op_history ();
	}
}

void
Editor::split_regions_at (timepos_t const & where, RegionSelection& regions)
{
	bool frozen = false;

	list<boost::shared_ptr<Playlist> > used_playlists;
	list<RouteTimeAxisView*> used_trackviews;

	if (regions.empty()) {
		return;
	}

	begin_reversible_command (_("split"));


	if (regions.size() == 1) {
		/* TODO:  if splitting a single region, and snap-to is using
		 region boundaries, maybe we shouldn't pay attention to them? */
	} else {
		frozen = true;
		EditorFreeze(); /* Emit Signal */
	}

	for (RegionSelection::iterator a = regions.begin(); a != regions.end(); ) {

		RegionSelection::iterator tmp;

		/* XXX this test needs to be more complicated, to make sure we really
		   have something to split.
		*/

		if (!(*a)->region()->covers (where)) {
			++a;
			continue;
		}

		tmp = a;
		++tmp;

		boost::shared_ptr<Playlist> pl = (*a)->region()->playlist();

		if (!pl) {
			a = tmp;
			continue;
		}

		if (!pl->frozen()) {
			/* we haven't seen this playlist before */

			/* remember used playlists so we can thaw them later */
			used_playlists.push_back(pl);

			TimeAxisView& tv = (*a)->get_time_axis_view();
			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (&tv);
			if (rtv) {
				used_trackviews.push_back (rtv);
			}
			pl->freeze();
		}


		if (pl) {
			pl->clear_changes ();
			pl->split_region ((*a)->region(), where);
			_session->add_command (new StatefulDiffCommand (pl));
		}

		a = tmp;
	}

	latest_regionviews.clear ();

	vector<sigc::connection> region_added_connections;

	for (list<RouteTimeAxisView*>::iterator i = used_trackviews.begin(); i != used_trackviews.end(); ++i) {
		region_added_connections.push_back ((*i)->view()->RegionViewAdded.connect (sigc::mem_fun(*this, &Editor::collect_new_region_view)));
	}

	while (used_playlists.size() > 0) {
		list <boost::shared_ptr<Playlist > >::iterator i = used_playlists.begin();
		(*i)->thaw();
		used_playlists.pop_front();
	}

	for (vector<sigc::connection>::iterator c = region_added_connections.begin(); c != region_added_connections.end(); ++c) {
		(*c).disconnect ();
	}

	if (frozen){
		EditorThaw(); /* Emit Signal */
	}

	if (_session->abort_empty_reversible_command ()) {
		/* no change was made */
		return;
	}

	RegionSelectionAfterSplit rsas = Config->get_region_selection_after_split();

	//if the user has "Clear Selection" as their post-split behavior, then clear the selection
	if (!latest_regionviews.empty() && (rsas == None)) {
		selection->clear_objects();
		selection->clear_time();
		//but leave track selection intact
	}

	//if the user doesn't want to preserve the "Existing" selection, then clear the selection
	if (!(rsas & Existing)) {
		selection->clear_objects();
		selection->clear_time();
	}

	//if the user wants newly-created regions to be selected, then select them:
	if (mouse_mode == MouseObject) {
		for (RegionSelection::iterator ri = latest_regionviews.begin(); ri != latest_regionviews.end(); ri++) {
			if ((*ri)->region()->position() < where) {
				// new regions created before the split
				if (rsas & NewlyCreatedLeft) {
					selection->add (*ri);
				}
			} else {
				// new regions created after the split
				if (rsas & NewlyCreatedRight) {
					selection->add (*ri);
				}
			}
		}
	}

	commit_reversible_command ();
}

/** Move one extreme of the current range selection.  If more than one range is selected,
 *  the start of the earliest range or the end of the latest range is moved.
 *
 *  @param move_end true to move the end of the current range selection, false to move
 *  the start.
 *  @param next true to move the extreme to the next region boundary, false to move to
 *  the previous.
 */
void
Editor::move_range_selection_start_or_end_to_region_boundary (bool move_end, bool next)
{
	if (selection->time.start_time() == selection->time.end_time()) {
		return;
	}

	timepos_t start = selection->time.start_time ();
	timepos_t end = selection->time.end_time ();

	/* the position of the thing we may move */
	timepos_t pos = move_end ? end : start;
	int dir = next ? 1 : -1;

	/* so we don't find the current region again */
	if (dir > 0 || pos.is_positive()) {
		pos.increment ();
	}

	timepos_t const target = get_region_boundary (pos, dir, true, false);
	if (target.is_negative ()) {
		return;
	}

	if (move_end) {
		end = target;
	} else {
		start = target;
	}

	if (end < start) {
		return;
	}

	begin_reversible_selection_op (_("alter selection"));
	selection->set_preserving_all_ranges (start, end);
	commit_reversible_selection_op ();
}

bool
Editor::nudge_forward_release (GdkEventButton* ev)
{
	if (ev->state & Keyboard::PrimaryModifier) {
		nudge_forward (false, true);
	} else {
		nudge_forward (false, false);
	}
	return false;
}

bool
Editor::nudge_backward_release (GdkEventButton* ev)
{
	if (ev->state & Keyboard::PrimaryModifier) {
		nudge_backward (false, true);
	} else {
		nudge_backward (false, false);
	}
	return false;
}


void
Editor::nudge_forward (bool next, bool force_playhead)
{
	timecnt_t distance;
	timecnt_t next_distance;

	if (!_session) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!force_playhead && !rs.empty()) {

		begin_reversible_command (_("nudge regions forward"));

		for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());

			distance = get_nudge_distance (r->position(), next_distance);

			if (next) {
				distance = next_distance;
			}

			r->clear_changes ();
			r->set_position (r->position() + distance);
			_session->add_command (new StatefulDiffCommand (r));
		}

		commit_reversible_command ();


	} else if (!force_playhead && !selection->markers.empty()) {

		bool is_start;
		bool in_command = false;

		for (MarkerSelection::iterator i = selection->markers.begin(); i != selection->markers.end(); ++i) {

			Location* loc = find_location_from_marker ((*i), is_start);

			if (loc) {

				XMLNode& before (loc->get_state());

				if (is_start) {
					distance = get_nudge_distance (loc->start(), next_distance);
					if (next) {
						distance = next_distance;
					}
					if (timepos_t (timecnt_t::max (distance.time_domain()) - distance) > loc->start() + loc->length()) {
						loc->set_start (loc->start() + distance, false);
					} else {
						loc->set_start (timepos_t::max (loc->length().time_domain()).earlier (loc->length()), false);
					}
				} else {
					distance = get_nudge_distance (loc->end(), next_distance);
					if (next) {
						distance = next_distance;
					}
					if (timepos_t (timecnt_t::max (distance.time_domain()) - distance) > loc->end()) {
						loc->set_end (loc->end() + distance, false);
					} else {
						loc->set_end (timepos_t::max (loc->end().time_domain()), false);
					}
					if (loc->is_session_range()) {
						_session->set_session_range_is_free (false);
					}
				}
				if (!in_command) {
					begin_reversible_command (_("nudge location forward"));
					in_command = true;
				}
				XMLNode& after (loc->get_state());
				_session->add_command (new MementoCommand<Location>(*loc, &before, &after));
			}
		}

		if (in_command) {
			commit_reversible_command ();
		}
	} else {
		distance = get_nudge_distance (timepos_t (playhead_cursor()->current_sample ()), next_distance);
		_session->request_locate ((timepos_t (playhead_cursor()->current_sample ()) + distance).samples());
	}
}

void
Editor::nudge_backward (bool next, bool force_playhead)
{
	timecnt_t distance;
	timecnt_t next_distance;

	if (!_session) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!force_playhead && !rs.empty()) {

		begin_reversible_command (_("nudge regions backward"));

		for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());

			distance = get_nudge_distance (r->position(), next_distance);

			if (next) {
				distance = next_distance;
			}

			r->clear_changes ();

			if (r->position() > distance) {
				r->set_position (r->position().earlier (distance));
			} else {
				r->set_position (timepos_t());
			}
			_session->add_command (new StatefulDiffCommand (r));
		}

		commit_reversible_command ();

	} else if (!force_playhead && !selection->markers.empty()) {

		bool is_start;
		bool in_command = false;

		for (MarkerSelection::iterator i = selection->markers.begin(); i != selection->markers.end(); ++i) {

			Location* loc = find_location_from_marker ((*i), is_start);

			if (loc) {

				XMLNode& before (loc->get_state());

				if (is_start) {
					distance = get_nudge_distance (loc->start(), next_distance);
					if (next) {
						distance = next_distance;
					}
					if (timepos_t (distance) < loc->start()) {
						loc->set_start (loc->start().earlier (distance), false);
					} else {
						loc->set_start (timepos_t(), false);
					}
				} else {
					distance = get_nudge_distance (loc->end(), next_distance);

					if (next) {
						distance = next_distance;
					}

					if (timepos_t (distance + loc->length()) < loc->end()) {
						loc->set_end (loc->end().earlier (distance), false);
					} else {
						loc->set_end (timepos_t (loc->length()), false);
					}
					if (loc->is_session_range()) {
						_session->set_session_range_is_free (false);
					}
				}
				if (!in_command) {
					begin_reversible_command (_("nudge location forward"));
					in_command = true;
				}
				XMLNode& after (loc->get_state());
				_session->add_command (new MementoCommand<Location>(*loc, &before, &after));
			}
		}
		if (in_command) {
			commit_reversible_command ();
		}

	} else {

		if (_playhead_cursor->current_sample () > distance.samples()) {
			_session->request_locate ((timepos_t (_playhead_cursor->current_sample ()).earlier (distance)).samples());
		} else {
			_session->goto_start();
		}
	}
}

void
Editor::nudge_forward_capture_offset ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	begin_reversible_command (_("nudge forward"));

	samplepos_t const distance = _session->worst_output_latency();

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		boost::shared_ptr<Region> r ((*i)->region());

		r->clear_changes ();
		r->set_position (r->position() + timecnt_t (distance));
		_session->add_command(new StatefulDiffCommand (r));
	}

	commit_reversible_command ();
}

void
Editor::nudge_backward_capture_offset ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	begin_reversible_command (_("nudge backward"));

	timepos_t const distance (_session->worst_output_latency());

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		boost::shared_ptr<Region> r ((*i)->region());

		r->clear_changes ();

		if (r->position() > distance) {
			r->set_position (r->position().earlier (distance));
		} else {
			r->set_position (timepos_t ());
		}
		_session->add_command(new StatefulDiffCommand (r));
	}

	commit_reversible_command ();
}

struct RegionSelectionPositionSorter {
	bool operator() (RegionView* a, RegionView* b) {
		return a->region()->position() < b->region()->position();
	}
};

void
Editor::sequence_regions ()
{
	timepos_t r_end;
	timepos_t r_end_prev;

	int iCount=0;

	if (!_session) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();
	rs.sort(RegionSelectionPositionSorter());

	if (!rs.empty()) {

		bool in_command = false;

		for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());

			r->clear_changes();

			if(r->locked())
			{
				continue;
			}
			if(r->position_locked())
			{
				continue;
			}
			if(iCount>0)
			{
				r_end_prev=r_end;
				r->set_position(r_end_prev);
			}

			if (!in_command) {
				begin_reversible_command (_("sequence regions"));
				in_command = true;
			}
			_session->add_command (new StatefulDiffCommand (r));

			r_end=r->position() + r->length();

			iCount++;
		}

		if (in_command) {
			commit_reversible_command ();
		}
	}
}


/* DISPLAY MOTION */

void
Editor::move_to_start ()
{
	_session->goto_start ();
}

void
Editor::move_to_end ()
{

	_session->request_locate (_session->current_end_sample());
}

void
Editor::build_region_boundary_cache ()
{

	/* TODO:  maybe set a timer so we don't recalutate when lots of changes are coming in */
	/* TODO:  maybe somehow defer this until session is fully loaded.  */

	if (!_region_boundary_cache_dirty)
		return;

	timepos_t pos;
	vector<RegionPoint> interesting_points;
	boost::shared_ptr<Region> r;
	TrackViewList tracks;
	bool at_end = false;

	region_boundary_cache.clear ();

	if (_session == 0) {
		return;
	}

	bool maybe_first_sample = false;

	if (UIConfiguration::instance().get_snap_to_region_start()) {
		interesting_points.push_back (Start);
		maybe_first_sample = true;
	}

	if (UIConfiguration::instance().get_snap_to_region_end()) {
		interesting_points.push_back (End);
	}

	if (UIConfiguration::instance().get_snap_to_region_sync()) {
		interesting_points.push_back (SyncPoint);
	}

	/* if no snap selections are set, boundary cache should be left empty */
	if ( interesting_points.empty() ) {
		_region_boundary_cache_dirty = false;
		return;
	}

	TimeAxisView *ontrack = 0;
	TrackViewList tlist;

	tlist = track_views.filter_to_unique_playlists ();

	if (maybe_first_sample) {
		TrackViewList::const_iterator i;
		for (i = tlist.begin(); i != tlist.end(); ++i) {
			boost::shared_ptr<Playlist> pl = (*i)->playlist();
			if (pl && pl->count_regions_at (timepos_t())) {
				region_boundary_cache.push_back (timepos_t());
				break;
			}
		}
	}

	/* allow regions to snap to the video start (if any) as if it were a "region" */
	if (ARDOUR_UI::instance()->video_timeline) {
		ARDOUR::samplepos_t vo = ARDOUR_UI::instance()->video_timeline->get_video_start_offset();
		if (std::find (region_boundary_cache.begin(), region_boundary_cache.end(), vo) == region_boundary_cache.end()) {
			region_boundary_cache.push_back (timepos_t (ARDOUR_UI::instance()->video_timeline->get_video_start_offset()));
		}
	}

	std::pair<timepos_t, timepos_t> ext = session_gui_extents (false);
	timepos_t session_end = ext.second;

	while (pos < session_end && !at_end) {

		timepos_t rpos;
		timepos_t lpos = session_end;

		for (vector<RegionPoint>::iterator p = interesting_points.begin(); p != interesting_points.end(); ++p) {

			if ((r = find_next_region (pos, *p, 1, tlist, &ontrack)) == 0) {
				if (*p == interesting_points.back()) {
					at_end = true;
				}
				/* move to next point type */
				continue;
			}

			switch (*p) {
			case Start:
				rpos = r->position();
				break;

			case End:
				rpos = r->end();
				break;

			case SyncPoint:
				rpos = r->sync_position ();
				break;

			default:
				break;
			}

			if (rpos < lpos) {
				lpos = rpos;
			}

			/* prevent duplicates, but we don't use set<> because we want to be able
			   to sort later.
			*/

			vector<timepos_t>::iterator ri;

			for (ri = region_boundary_cache.begin(); ri != region_boundary_cache.end(); ++ri) {
				if (*ri == rpos) {
					break;
				}
			}

			if (ri == region_boundary_cache.end()) {
				region_boundary_cache.push_back (rpos);
			}
		}

		pos = lpos.increment();
	}

	/* finally sort to be sure that the order is correct */

	sort (region_boundary_cache.begin(), region_boundary_cache.end());

	_region_boundary_cache_dirty = false;
}

boost::shared_ptr<Region>
Editor::find_next_region (timepos_t const & pos, RegionPoint point, int32_t dir, TrackViewList& tracks, TimeAxisView **ontrack)
{
	TrackViewList::iterator i;
	timecnt_t closest = timecnt_t::max (pos.time_domain());
	boost::shared_ptr<Region> ret;
	timepos_t rpos;

	timepos_t track_pos;

	for (i = tracks.begin(); i != tracks.end(); ++i) {

		timecnt_t distance;
		boost::shared_ptr<Region> r;

		track_pos = pos;

		if ((r = (*i)->find_next_region (track_pos, point, dir)) == 0) {
			continue;
		}

		switch (point) {
		case Start:
			rpos = r->position ();
			break;

		case End:
			rpos = r->end ();
			break;

		case SyncPoint:
			rpos = r->sync_position ();
			break;
		}

		if (rpos > pos) {
			distance = pos.distance (rpos);
		} else {
			distance = rpos.distance (pos);
		}

		if (distance < closest) {
			closest = distance;
			if (ontrack != 0) {
				*ontrack = (*i);
			}
			ret = r;
		}
	}

	return ret;
}

timepos_t
Editor::find_next_region_boundary (timepos_t const & pos, int32_t dir, const TrackViewList& tracks)
{
	timecnt_t distance = timecnt_t::max (pos.time_domain());
	timepos_t current_nearest = timepos_t::max (pos.time_domain());

	for (TrackViewList::const_iterator i = tracks.begin(); i != tracks.end(); ++i) {
		timepos_t contender;
		timecnt_t d;

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);

		if (!rtv) {
			continue;
		}

		if ((contender = rtv->find_next_region_boundary (pos, dir)) < 0) {
			continue;
		}

		d = contender.distance (pos);

		if (d < distance) {
			current_nearest = contender;
			distance = d;
		}
	}

	return current_nearest;
}

timepos_t
Editor::get_region_boundary (timepos_t const & pos, int32_t dir, bool with_selection, bool only_onscreen)
{
	timepos_t target;
	TrackViewList tvl;

	if (with_selection && Config->get_region_boundaries_from_selected_tracks()) {

		if (!selection->tracks.empty()) {

			target = find_next_region_boundary (pos, dir, selection->tracks);

		} else {

			if (only_onscreen || Config->get_region_boundaries_from_onscreen_tracks()) {
				get_onscreen_tracks (tvl);
				target = find_next_region_boundary (pos, dir, tvl);
			} else {
				target = find_next_region_boundary (pos, dir, track_views);
			}
		}

	} else {

		if (only_onscreen || Config->get_region_boundaries_from_onscreen_tracks()) {
			get_onscreen_tracks (tvl);
			target = find_next_region_boundary (pos, dir, tvl);
		} else {
			target = find_next_region_boundary (pos, dir, track_views);
		}
	}

	return target;
}

void
Editor::cursor_to_region_boundary (bool with_selection, int32_t dir)
{
	timepos_t pos (_playhead_cursor->current_sample ());
	timepos_t target;

	if (!_session) {
		return;
	}

	// so we don't find the current region again..
	if (dir > 0 || pos > 0) {
		pos = pos.increment();
	}

	if ((target = get_region_boundary (pos, dir, with_selection, false)) < 0) {
		return;
	}

	_session->request_locate (target.samples());
}

void
Editor::cursor_to_next_region_boundary (bool with_selection)
{
	cursor_to_region_boundary (with_selection, 1);
}

void
Editor::cursor_to_previous_region_boundary (bool with_selection)
{
	cursor_to_region_boundary (with_selection, -1);
}

void
Editor::cursor_to_region_point (EditorCursor* cursor, RegionPoint point, int32_t dir)
{
	boost::shared_ptr<Region> r;
	timepos_t pos (cursor->current_sample ());

	if (!_session) {
		return;
	}

	TimeAxisView *ontrack = 0;

	// so we don't find the current region again..
	if (dir > 0 || pos.is_positive())
		pos = pos.increment();

	if (!selection->tracks.empty()) {

		r = find_next_region (pos, point, dir, selection->tracks, &ontrack);

	} else if (clicked_axisview) {

		TrackViewList t;
		t.push_back (clicked_axisview);

		r = find_next_region (pos, point, dir, t, &ontrack);

	} else {

		r = find_next_region (pos, point, dir, track_views, &ontrack);
	}

	if (r == 0) {
		return;
	}

	switch (point) {
	case Start:
		pos = r->position ();
		break;

	case End:
		pos = r->nt_last ();
		break;

	case SyncPoint:
		pos = r->sync_position ();
		break;
	}

	if (cursor == _playhead_cursor) {
		_session->request_locate (pos.samples());
	} else {
		cursor->set_position (pos.samples());
	}
}

void
Editor::cursor_to_next_region_point (EditorCursor* cursor, RegionPoint point)
{
	cursor_to_region_point (cursor, point, 1);
}

void
Editor::cursor_to_previous_region_point (EditorCursor* cursor, RegionPoint point)
{
	cursor_to_region_point (cursor, point, -1);
}

void
Editor::cursor_to_selection_start (EditorCursor *cursor)
{
	timepos_t pos;

	switch (mouse_mode) {
	case MouseObject:
		if (!selection->regions.empty()) {
			pos = selection->regions.start_time();
		}
		break;

	case MouseRange:
		if (!selection->time.empty()) {
			pos = selection->time.start_time ();
		}
		break;

	default:
		return;
	}

	if (cursor == _playhead_cursor) {
		_session->request_locate (pos.samples());
	} else {
		cursor->set_position (pos.samples());
	}
}

void
Editor::cursor_to_selection_end (EditorCursor *cursor)
{
	timepos_t pos;

	switch (mouse_mode) {
	case MouseObject:
		if (!selection->regions.empty()) {
			pos = selection->regions.end_time();
		}
		break;

	case MouseRange:
		if (!selection->time.empty()) {
			pos = selection->time.end_time ();
		}
		break;

	default:
		return;
	}

	if (cursor == _playhead_cursor) {
		_session->request_locate (pos.samples());
	} else {
		cursor->set_position (pos.samples());
	}
}

void
Editor::selected_marker_to_region_boundary (bool with_selection, int32_t dir)
{
	timepos_t target;
	Location* loc;
	bool ignored;

	if (!_session) {
		return;
	}

	if (selection->markers.empty()) {
		samplepos_t mouse;
		bool ignored;

		if (!mouse_sample (mouse, ignored)) {
			return;
		}

		add_location_mark (timepos_t (mouse));
	}

	if ((loc = find_location_from_marker (selection->markers.front(), ignored)) == 0) {
		return;
	}

	timepos_t pos = loc->start();

	// so we don't find the current region again..
	if (dir > 0 || pos.is_positive()) {
		pos = pos.increment();
	}

	if ((target = get_region_boundary (pos, dir, with_selection, false)) < 0) {
		return;
	}

	loc->move_to (target);
}

void
Editor::selected_marker_to_next_region_boundary (bool with_selection)
{
	selected_marker_to_region_boundary (with_selection, 1);
}

void
Editor::selected_marker_to_previous_region_boundary (bool with_selection)
{
	selected_marker_to_region_boundary (with_selection, -1);
}

void
Editor::selected_marker_to_region_point (RegionPoint point, int32_t dir)
{
	boost::shared_ptr<Region> r;
	timepos_t pos;
	Location* loc;
	bool ignored;

	if (!_session || selection->markers.empty()) {
		return;
	}

	if ((loc = find_location_from_marker (selection->markers.front(), ignored)) == 0) {
		return;
	}

	TimeAxisView *ontrack = 0;

	pos = loc->start();

	// so we don't find the current region again..
	if (dir>0 || pos>0)
		pos.increment();

	if (!selection->tracks.empty()) {

		r = find_next_region (pos, point, dir, selection->tracks, &ontrack);

	} else {

		r = find_next_region (pos, point, dir, track_views, &ontrack);
	}

	if (r == 0) {
		return;
	}

	switch (point) {
	case Start:
		pos = timepos_t (r->first_sample ());
		break;

	case End:
		pos = timepos_t (r->last_sample ());
		break;

	case SyncPoint:
		pos = r->adjust_to_sync (r->position());
		break;
	}

	loc->move_to (pos);
}

void
Editor::selected_marker_to_next_region_point (RegionPoint point)
{
	selected_marker_to_region_point (point, 1);
}

void
Editor::selected_marker_to_previous_region_point (RegionPoint point)
{
	selected_marker_to_region_point (point, -1);
}

void
Editor::selected_marker_to_selection_start ()
{
	timepos_t pos;
	Location* loc;
	bool ignored;

	if (!_session || selection->markers.empty()) {
		return;
	}

	if ((loc = find_location_from_marker (selection->markers.front(), ignored)) == 0) {
		return;
	}

	switch (mouse_mode) {
	case MouseObject:
		if (!selection->regions.empty()) {
			pos = selection->regions.start_time();
		}
		break;

	case MouseRange:
		if (!selection->time.empty()) {
			pos = selection->time.start_time ();
		}
		break;

	default:
		return;
	}

	loc->move_to (pos);
}

void
Editor::selected_marker_to_selection_end ()
{
	timepos_t pos;
	Location* loc;
	bool ignored;

	if (!_session || selection->markers.empty()) {
		return;
	}

	if ((loc = find_location_from_marker (selection->markers.front(), ignored)) == 0) {
		return;
	}

	switch (mouse_mode) {
	case MouseObject:
		if (!selection->regions.empty()) {
			pos = selection->regions.end_time();
		}
		break;

	case MouseRange:
		if (!selection->time.empty()) {
			pos = selection->time.end_time ();
		}
		break;

	default:
		return;
	}

	loc->move_to (pos);
}

void
Editor::scroll_playhead (bool forward)
{
	samplepos_t pos = _playhead_cursor->current_sample ();
	samplecnt_t delta = (samplecnt_t) floor (current_page_samples() / 0.8);

	if (forward) {
		if (pos == max_samplepos) {
			return;
		}

		if (pos < max_samplepos - delta) {
			pos += delta ;
		} else {
			pos = max_samplepos;
		}

	} else {

		if (pos == 0) {
			return;
		}

		if (pos > delta) {
			pos -= delta;
		} else {
			pos = 0;
		}
	}

	_session->request_locate (pos);
}

void
Editor::cursor_align (bool playhead_to_edit)
{
	if (!_session) {
		return;
	}

	if (playhead_to_edit) {

		if (selection->markers.empty()) {
			return;
		}

		_session->request_locate (selection->markers.front()->position().samples());

	} else {
		/* move selected markers to playhead */

		for (MarkerSelection::iterator i = selection->markers.begin(); i != selection->markers.end(); ++i) {
			bool ignored;

			Location* loc = find_location_from_marker (*i, ignored);

			if (loc->is_mark()) {
				loc->set_start (timepos_t (_playhead_cursor->current_sample ()), false);
			} else {
				loc->set (timepos_t (_playhead_cursor->current_sample ()),
				          timepos_t (_playhead_cursor->current_sample ()) + loc->length());
			}
		}
	}
}

void
Editor::scroll_backward (float pages)
{
	samplepos_t const one_page = (samplepos_t) rint (_visible_canvas_width * samples_per_pixel);
	samplepos_t const cnt = (samplepos_t) floor (pages * one_page);

	samplepos_t sample;
	if (_leftmost_sample < cnt) {
		sample = 0;
	} else {
		sample = _leftmost_sample - cnt;
	}

	reset_x_origin (sample);
}

void
Editor::scroll_forward (float pages)
{
	samplepos_t const one_page = (samplepos_t) rint (_visible_canvas_width * samples_per_pixel);
	samplepos_t const cnt = (samplepos_t) floor (pages * one_page);

	samplepos_t sample;
	if (max_samplepos - cnt < _leftmost_sample) {
		sample = max_samplepos - cnt;
	} else {
		sample = _leftmost_sample + cnt;
	}

	reset_x_origin (sample);
}

void
Editor::scroll_tracks_down ()
{
	double vert_value = vertical_adjustment.get_value() + vertical_adjustment.get_page_size();
	if (vert_value > vertical_adjustment.get_upper() - _visible_canvas_height) {
		vert_value = vertical_adjustment.get_upper() - _visible_canvas_height;
	}

	vertical_adjustment.set_value (vert_value);
}

void
Editor::scroll_tracks_up ()
{
	vertical_adjustment.set_value (vertical_adjustment.get_value() - vertical_adjustment.get_page_size());
}

void
Editor::scroll_tracks_down_line ()
{
	double vert_value = vertical_adjustment.get_value() + 60;

	if (vert_value > vertical_adjustment.get_upper() - _visible_canvas_height) {
		vert_value = vertical_adjustment.get_upper() - _visible_canvas_height;
	}

	vertical_adjustment.set_value (vert_value);
}

void
Editor::scroll_tracks_up_line ()
{
	reset_y_origin (vertical_adjustment.get_value() - 60);
}

void
Editor::select_topmost_track ()
{
	const double top_of_trackviews = vertical_adjustment.get_value();
	for (TrackViewList::iterator t = track_views.begin(); t != track_views.end(); ++t) {
		if ((*t)->hidden()) {
			continue;
		}
		std::pair<TimeAxisView*,double> res = (*t)->covers_y_position (top_of_trackviews);
		if (res.first) {
			selection->set (*t);
			break;
		}
	}
}

bool
Editor::scroll_down_one_track (bool skip_child_views)
{
	TrackViewList::reverse_iterator next = track_views.rend();
	const double top_of_trackviews = vertical_adjustment.get_value();

	for (TrackViewList::reverse_iterator t = track_views.rbegin(); t != track_views.rend(); ++t) {
		if ((*t)->hidden()) {
			continue;
		}

		/* If this is the upper-most visible trackview, we want to display
		 * the one above it (next)
		 *
		 * Note that covers_y_position() is recursive and includes child views
		 */
		std::pair<TimeAxisView*,double> res = (*t)->covers_y_position (top_of_trackviews);

		if (res.first) {
			if (skip_child_views) {
				break;
			}
			/* automation lane (one level, non-recursive)
			 *
			 * - if no automation lane exists -> move to next tack
			 * - if the first (here: bottom-most) matches -> move to next tack
			 * - if no y-axis match is found -> the current track is at the top
			 *     -> move to last (here: top-most) automation lane
			 */
			TimeAxisView::Children kids = (*t)->get_child_list();
			TimeAxisView::Children::reverse_iterator nkid = kids.rend();

			for (TimeAxisView::Children::reverse_iterator ci = kids.rbegin(); ci != kids.rend(); ++ci) {
				if ((*ci)->hidden()) {
					continue;
				}

				std::pair<TimeAxisView*,double> dev;
				dev = (*ci)->covers_y_position (top_of_trackviews);
				if (dev.first) {
					/* some automation lane is currently at the top */
					if (ci == kids.rbegin()) {
						/* first (bottom-most) autmation lane is at the top.
						 * -> move to next track
						 */
						nkid = kids.rend();
					}
					break;
				}
				nkid = ci;
			}

			if (nkid != kids.rend()) {
				ensure_time_axis_view_is_visible (**nkid, true);
				return true;
			}
			break;
		}
		next = t;
	}

	/* move to the track below the first one that covers the */

	if (next != track_views.rend()) {
		ensure_time_axis_view_is_visible (**next, true);
		return true;
	}

	return false;
}

bool
Editor::scroll_up_one_track (bool skip_child_views)
{
	TrackViewList::iterator prev = track_views.end();
	double top_of_trackviews = vertical_adjustment.get_value ();

	for (TrackViewList::iterator t = track_views.begin(); t != track_views.end(); ++t) {

		if ((*t)->hidden()) {
			continue;
		}

		/* find the trackview at the top of the trackview group
		 *
		 * Note that covers_y_position() is recursive and includes child views
		 */
		std::pair<TimeAxisView*,double> res = (*t)->covers_y_position (top_of_trackviews);

		if (res.first) {
			if (skip_child_views) {
				break;
			}
			/* automation lane (one level, non-recursive)
			 *
			 * - if no automation lane exists -> move to prev tack
			 * - if no y-axis match is found -> the current track is at the top -> move to prev track
			 *     (actually last automation lane of previous track, see below)
			 * - if first (top-most) lane is at the top -> move to this track
			 * - else move up one lane
			 */
			TimeAxisView::Children kids = (*t)->get_child_list();
			TimeAxisView::Children::iterator pkid = kids.end();

			for (TimeAxisView::Children::iterator ci = kids.begin(); ci != kids.end(); ++ci) {
				if ((*ci)->hidden()) {
					continue;
				}

				std::pair<TimeAxisView*,double> dev;
				dev = (*ci)->covers_y_position (top_of_trackviews);
				if (dev.first) {
					/* some automation lane is currently at the top */
					if (ci == kids.begin()) {
						/* first (top-most) autmation lane is at the top.
						 * jump directly to this track's top
						 */
						ensure_time_axis_view_is_visible (**t, true);
						return true;
					}
					else if (pkid != kids.end()) {
						/* some other automation lane is at the top.
						 * move up to prev automation lane.
						 */
						ensure_time_axis_view_is_visible (**pkid, true);
						return true;
					}
					assert(0); // not reached
					break;
				}
				pkid = ci;
			}
			break;
		}

		prev = t;
	}

	if (prev != track_views.end()) {
		// move to bottom-most automation-lane of the previous track
		TimeAxisView::Children kids = (*prev)->get_child_list();
		TimeAxisView::Children::reverse_iterator pkid = kids.rend();
		if (!skip_child_views) {
			// find the last visible lane
			for (TimeAxisView::Children::reverse_iterator ci = kids.rbegin(); ci != kids.rend(); ++ci) {
				if (!(*ci)->hidden()) {
					pkid = ci;
					break;
				}
			}
		}
		if (pkid != kids.rend()) {
			ensure_time_axis_view_is_visible (**pkid, true);
		} else  {
			ensure_time_axis_view_is_visible (**prev, true);
		}
		return true;
	}

	return false;
}

void
Editor::scroll_left_step ()
{
	samplepos_t xdelta = (current_page_samples() / 8);

	if (_leftmost_sample > xdelta) {
		reset_x_origin (_leftmost_sample - xdelta);
	} else {
		reset_x_origin (0);
	}
}


void
Editor::scroll_right_step ()
{
	samplepos_t xdelta = (current_page_samples() / 8);

	if (max_samplepos - xdelta > _leftmost_sample) {
		reset_x_origin (_leftmost_sample + xdelta);
	} else {
		reset_x_origin (max_samplepos - current_page_samples());
	}
}

void
Editor::scroll_left_half_page ()
{
	samplepos_t xdelta = (current_page_samples() / 2);
	if (_leftmost_sample > xdelta) {
		reset_x_origin (_leftmost_sample - xdelta);
	} else {
		reset_x_origin (0);
	}
}

void
Editor::scroll_right_half_page ()
{
	samplepos_t xdelta = (current_page_samples() / 2);
	if (max_samplepos - xdelta > _leftmost_sample) {
		reset_x_origin (_leftmost_sample + xdelta);
	} else {
		reset_x_origin (max_samplepos - current_page_samples());
	}
}

/* ZOOM */

void
Editor::tav_zoom_step (bool coarser)
{
	DisplaySuspender ds;

	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator i = ts->begin(); i != ts->end(); ++i) {
		TimeAxisView *tv = (static_cast<TimeAxisView*>(*i));
			tv->step_height (coarser);
	}
}

void
Editor::tav_zoom_smooth (bool coarser, bool force_all)
{
	DisplaySuspender ds;

	TrackViewList* ts;

	if (selection->tracks.empty() || force_all) {
		ts = &track_views;
	} else {
		ts = &selection->tracks;
	}

	for (TrackViewList::iterator i = ts->begin(); i != ts->end(); ++i) {
		TimeAxisView *tv = (static_cast<TimeAxisView*>(*i));
		uint32_t h = tv->current_height ();

		if (coarser) {
			if (h > 5) {
				h -= 5; // pixels
				if (h >= TimeAxisView::preset_height (HeightSmall)) {
					tv->set_height (h);
				}
			}
		} else {
			tv->set_height (h + 5);
		}
	}
}

void
Editor::temporal_zoom_step_mouse_focus_scale (bool zoom_out, double scale)
{
	PBD::Unwinder<Editing::ZoomFocus> zf (zoom_focus, Editing::ZoomFocusMouse);
	temporal_zoom_step_scale (zoom_out, scale);
}

void
Editor::temporal_zoom_step_mouse_focus (bool zoom_out)
{
	temporal_zoom_step_mouse_focus_scale (zoom_out, 2.0);
}

void
Editor::temporal_zoom_step (bool zoom_out)
{
	temporal_zoom_step_scale (zoom_out, 2.0);
}

void
Editor::temporal_zoom_step_scale (bool zoom_out, double scale)
{
	ENSURE_GUI_THREAD (*this, &Editor::temporal_zoom_step, zoom_out, scale)

	samplecnt_t nspp = samples_per_pixel;

	if (zoom_out) {
		nspp *= scale;
		if (nspp == samples_per_pixel) {
			nspp *= 2.0;
		}
	} else {
		nspp /= scale;
		if (nspp == samples_per_pixel) {
			nspp /= 2.0;
		}
	}

	//zoom-behavior-tweaks
	//limit our maximum zoom to the session gui extents value
	std::pair<timepos_t, timepos_t> ext = session_gui_extents();
	samplecnt_t session_extents_pp = (ext.second.samples() - ext.first.samples())  / _visible_canvas_width;
	if (nspp > session_extents_pp) {
		nspp = session_extents_pp;
	}

	temporal_zoom (nspp);
}

void
Editor::temporal_zoom (samplecnt_t spp)
{
	if (!_session) {
		return;
	}

	samplepos_t current_page = current_page_samples();
	samplepos_t current_leftmost = _leftmost_sample;
	samplepos_t current_rightmost;
	samplepos_t current_center;
	samplepos_t new_page_size;
	samplepos_t half_page_size;
	samplepos_t leftmost_after_zoom = 0;
	samplepos_t where;
	bool in_track_canvas;
	bool use_mouse_sample = true;
	samplecnt_t nspp;
	double l;

	if (spp == samples_per_pixel) {
		return;
	}

	// Imposing an arbitrary limit to zoom out as too much zoom out produces
	// segfaults for lack of memory. If somebody decides this is not high enough I
	// believe it can be raisen to higher values but some limit must be in place.
	//
	// This constant represents 1 day @ 48kHz on a 1600 pixel wide display
	// all of which is used for the editor track displays. The whole day
	// would be 4147200000 samples, so 2592000 samples per pixel.

	nspp = min (spp, (samplecnt_t) 2592000);
	nspp = max ((samplecnt_t) 1, nspp);

	new_page_size = (samplepos_t) floor (_visible_canvas_width * nspp);
	half_page_size = new_page_size / 2;

	Editing::ZoomFocus zf = zoom_focus;

	if (zf == ZoomFocusEdit && _edit_point == EditAtMouse) {
		zf = ZoomFocusMouse;
	}

	switch (zf) {
	case ZoomFocusLeft:
		leftmost_after_zoom = current_leftmost;
		break;

	case ZoomFocusRight:
		current_rightmost = _leftmost_sample + current_page;
		if (current_rightmost < new_page_size) {
			leftmost_after_zoom = 0;
		} else {
			leftmost_after_zoom = current_rightmost - new_page_size;
		}
		break;

	case ZoomFocusCenter:
		current_center = current_leftmost + (current_page/2);
		if (current_center < half_page_size) {
			leftmost_after_zoom = 0;
		} else {
			leftmost_after_zoom = current_center - half_page_size;
		}
		break;

	case ZoomFocusPlayhead:
		/* centre playhead */
		l = _playhead_cursor->current_sample () - (new_page_size * 0.5);

		if (l < 0) {
			leftmost_after_zoom = 0;
		} else if (l > max_samplepos) {
			leftmost_after_zoom = max_samplepos - new_page_size;
		} else {
			leftmost_after_zoom = (samplepos_t) l;
		}
		break;

	case ZoomFocusMouse:
		/* try to keep the mouse over the same point in the display */

		if (_drags->active()) {
			where = _drags->current_pointer_sample ();
		} else if (!mouse_sample (where, in_track_canvas)) {
			use_mouse_sample = false;
		}

		if (use_mouse_sample) {
			l = - ((new_page_size * ((where - current_leftmost)/(double)current_page)) - where);

			if (l < 0) {
				leftmost_after_zoom = 0;
			} else if (l > max_samplepos) {
				leftmost_after_zoom = max_samplepos - new_page_size;
			} else {
				leftmost_after_zoom = (samplepos_t) l;
			}
		} else {
			/* use playhead instead */
			where = _playhead_cursor->current_sample ();

			if (where < half_page_size) {
				leftmost_after_zoom = 0;
			} else {
				leftmost_after_zoom = where - half_page_size;
			}
		}
		break;

	case ZoomFocusEdit:
		/* try to keep the edit point in the same place */
		where = get_preferred_edit_position ().samples();
		{
			double l = - ((new_page_size * ((where - current_leftmost)/(double)current_page)) - where);

			if (l < 0) {
				leftmost_after_zoom = 0;
			} else if (l > max_samplepos) {
				leftmost_after_zoom = max_samplepos - new_page_size;
			} else {
				leftmost_after_zoom = (samplepos_t) l;
			}
		}
		break;

	}

	// leftmost_after_zoom = min (leftmost_after_zoom, _session->current_end_sample());

	reposition_and_zoom (leftmost_after_zoom, nspp);
}

void
Editor::calc_extra_zoom_edges(samplepos_t &start, samplepos_t &end)
{
	/* this func helps make sure we leave a little space
	   at each end of the editor so that the zoom doesn't fit the region
	   precisely to the screen.
	*/

	GdkScreen* screen = gdk_screen_get_default ();
	const gint pixwidth = gdk_screen_get_width (screen);
	const gint mmwidth = gdk_screen_get_width_mm (screen);
	const double pix_per_mm = (double) pixwidth/ (double) mmwidth;
	const double one_centimeter_in_pixels = pix_per_mm * 10.0;

	const samplepos_t range = end - start;
	const samplecnt_t new_fpp = (samplecnt_t) ceil ((double) range / (double) _visible_canvas_width);
	const samplepos_t extra_samples = (samplepos_t) floor (one_centimeter_in_pixels * new_fpp);

	if (start > extra_samples) {
		start -= extra_samples;
	} else {
		start = 0;
	}

	if (max_samplepos - extra_samples > end) {
		end += extra_samples;
	} else {
		end = max_samplepos;
	}
}

bool
Editor::get_selection_extents (timepos_t &start, timepos_t &end) const
{
	start = timepos_t::max (start.time_domain());
	end = timepos_t();
	bool ret = true;

	//ToDo:  if notes are selected, set extents to that selection

	//ToDo:  if control points are selected, set extents to that selection

	if (!selection->regions.empty()) {
		RegionSelection rs = get_regions_from_selection_and_entered ();

		for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {

			if ((*i)->region()->position() < start) {
				start = (*i)->region()->position();
			}

			if ((*i)->region()->end() > end) {
				end = (*i)->region()->end();
			}
		}

	} else if (!selection->time.empty()) {
		start = selection->time.start_time();
		end = selection->time.end_time();
	} else {
		ret = false;  //no selection found
	}

	//range check
	if ((start.is_zero () && end.is_zero ()) || end < start) {
		ret = false;
	}

	return ret;
}


void
Editor::temporal_zoom_selection (Editing::ZoomAxis axes)
{
	if (!selection) return;

	if (selection->regions.empty() && selection->time.empty()) {
		if (axes == Horizontal || axes == Both) {
			temporal_zoom_step(true);
		}
		if (axes == Vertical || axes == Both) {
			if (!track_views.empty()) {

				TrackViewList tvl;

				//implicit hack: by extending the top & bottom check outside the current view limits, we include the trackviews immediately above & below what is visible
				const double top = vertical_adjustment.get_value() - 10;
				const double btm = top + _visible_canvas_height + 10;

				for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
					if ((*iter)->covered_by_y_range (top, btm)) {
						tvl.push_back(*iter);
					}
				}

				fit_tracks (tvl);
			}
		}
		return;
	}

	//ToDo:  if notes are selected, zoom to that

	//ToDo:  if control points are selected, zoom to that

	if (axes == Horizontal || axes == Both) {

		timepos_t start, end;
		if (get_selection_extents (start, end)) {
			samplepos_t s = start.samples();
			samplepos_t e = end.samples();
			calc_extra_zoom_edges (s, e);
			temporal_zoom_by_sample (s, e);
		}
	}

	if (axes == Vertical || axes == Both) {
		fit_selection ();
	}

	//normally, we don't do anything "automatic" to the user's selection.
	//but in this case, we will clear the selection after a zoom-to-selection.
	selection->clear();
}

void
Editor::temporal_zoom_session ()
{
	ENSURE_GUI_THREAD (*this, &Editor::temporal_zoom_session)

	if (_session) {
		samplecnt_t start = _session->current_start_sample();
		samplecnt_t end = _session->current_end_sample();

		if (_session->actively_recording ()) {
			samplepos_t cur = _playhead_cursor->current_sample ();
			if (cur > end) {
				/* recording beyond the end marker; zoom out
				 * by 5 seconds more so that if 'follow
				 * playhead' is active we don't immediately
				 * scroll.
				 */
				end = cur + _session->sample_rate() * 5;
			}
		}

		if ((start == 0 && end == 0) || end < start) {
			return;
		}

		calc_extra_zoom_edges(start, end);

		temporal_zoom_by_sample (start, end);
	}
}

void
Editor::temporal_zoom_extents ()
{
	ENSURE_GUI_THREAD (*this, &Editor::temporal_zoom_extents)

	if (_session) {
		std::pair<timepos_t, timepos_t> ext = session_gui_extents (false);  //in this case we want to zoom to the extents explicitly; ignore the users prefs for extra padding

		samplecnt_t start = ext.first.samples();
		samplecnt_t end = ext.second.samples();

		if (_session->actively_recording ()) {
			samplepos_t cur = _playhead_cursor->current_sample ();
			if (cur > end) {
				/* recording beyond the end marker; zoom out
				 * by 5 seconds more so that if 'follow
				 * playhead' is active we don't immediately
				 * scroll.
				 */
				end = cur + _session->sample_rate() * 5;
			}
		}

		if ((start == 0 && end == 0) || end < start) {
			return;
		}

		calc_extra_zoom_edges(start, end);

		temporal_zoom_by_sample (start, end);
	}
}

void
Editor::temporal_zoom_by_sample (samplepos_t start, samplepos_t end)
{
	if (!_session) return;

	if ((start == 0 && end == 0) || end < start) {
		return;
	}

	samplepos_t range = end - start;

	const samplecnt_t new_fpp = (samplecnt_t) ceil ((double) range / (double) _visible_canvas_width);

	samplepos_t new_page = range;
	samplepos_t middle = (samplepos_t) floor ((double) start + ((double) range / 2.0f));
	samplepos_t new_leftmost = (samplepos_t) floor ((double) middle - ((double) new_page / 2.0f));

	if (new_leftmost > middle) {
		new_leftmost = 0;
	}

	if (new_leftmost < 0) {
		new_leftmost = 0;
	}

	reposition_and_zoom (new_leftmost, new_fpp);
}

void
Editor::temporal_zoom_to_sample (bool coarser, samplepos_t sample)
{
	if (!_session) {
		return;
	}

	samplecnt_t range_before = sample - _leftmost_sample;
	samplecnt_t new_spp;

	if (coarser) {
		if (samples_per_pixel <= 1) {
			new_spp = 2;
		} else {
			new_spp = samples_per_pixel + (samples_per_pixel/2);
		}
		range_before += range_before/2;
	} else {
		if (samples_per_pixel >= 1) {
			new_spp = samples_per_pixel - (samples_per_pixel/2);
		} else {
			/* could bail out here since we cannot zoom any finer,
			   but leave that to the equality test below
			*/
			new_spp = samples_per_pixel;
		}

		range_before -= range_before/2;
	}

	if (new_spp == samples_per_pixel)  {
		return;
	}

	/* zoom focus is automatically taken as @param sample when this
	   method is used.
	*/

	samplepos_t new_leftmost = sample - (samplepos_t)range_before;

	if (new_leftmost > sample) {
		new_leftmost = 0;
	}

	if (new_leftmost < 0) {
		new_leftmost = 0;
	}

	reposition_and_zoom (new_leftmost, new_spp);
}


bool
Editor::choose_new_marker_name(string &name, bool is_range) {

	if (!UIConfiguration::instance().get_name_new_markers()) {
		/* don't prompt user for a new name */
		return true;
	}

	Prompter dialog (true);

	dialog.set_prompt (_("New Name:"));

	if (is_range) {
		dialog.set_title(_("New Range"));
	} else {
		dialog.set_title (_("New Location Marker"));
	}

	dialog.set_name ("MarkNameWindow");
	dialog.set_size_request (250, -1);
	dialog.set_position (Gtk::WIN_POS_MOUSE);

	dialog.add_button (Stock::OK, RESPONSE_ACCEPT);
	dialog.set_initial_text (name);

	dialog.show ();

	switch (dialog.run ()) {
	case RESPONSE_ACCEPT:
		break;
	default:
		return false;
	}

	dialog.get_result(name);
	return true;

}


void
Editor::add_location_from_selection ()
{
	string rangename;

	if (selection->time.empty()) {
		return;
	}

	if (_session == 0 || clicked_axisview == 0) {
		return;
	}

	timepos_t start = selection->time[clicked_selection].start();
	timepos_t end = selection->time[clicked_selection].end();

	_session->locations()->next_available_name(rangename,"selection");
	if (!choose_new_marker_name(rangename, true)) {
		return;
	}
	Location *location = new Location (*_session, start, end, rangename, Location::IsRangeMarker);

	begin_reversible_command (_("add marker"));

	XMLNode &before = _session->locations()->get_state();
	_session->locations()->add (location, true);
	XMLNode &after = _session->locations()->get_state();
	_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));

	commit_reversible_command ();
}

void
Editor::add_location_mark (timepos_t const & where)
{
	if (_session->locations()->mark_at (where, timecnt_t (1))) {
		return;
	}

	string markername;

	select_new_marker = true;

	_session->locations()->next_available_name(markername,"mark");
	if (!choose_new_marker_name(markername)) {
		return;
	}
	Location *location = new Location (*_session, where, where, markername, Location::IsMark);
	begin_reversible_command (_("add marker"));

	XMLNode &before = _session->locations()->get_state();
	_session->locations()->add (location, true);
	XMLNode &after = _session->locations()->get_state();
	_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));

	commit_reversible_command ();
}

void
Editor::set_session_start_from_playhead ()
{
	if (!_session)
		return;

	Location* loc;
	if ((loc = _session->locations()->session_range_location()) == 0) {
		_session->set_session_extents (timepos_t (_session->audible_sample()), timepos_t (_session->audible_sample() + 3 * 60 * _session->sample_rate()));
	} else {
		XMLNode &before = loc->get_state();

		_session->set_session_extents (timepos_t (_session->audible_sample()), loc->end());

		XMLNode &after = loc->get_state();

		begin_reversible_command (_("Set session start"));

		_session->add_command (new MementoCommand<Location>(*loc, &before, &after));

		commit_reversible_command ();
	}

	_session->set_session_range_is_free (false);
}

void
Editor::set_session_end_from_playhead ()
{
	if (!_session)
		return;

	Location* loc;
	if ((loc = _session->locations()->session_range_location()) == 0) {  //should never happen
		_session->set_session_extents (timepos_t(), timepos_t (_session->audible_sample()));
	} else {
		XMLNode &before = loc->get_state();

		_session->set_session_extents (loc->start(), timepos_t (_session->audible_sample()));

		XMLNode &after = loc->get_state();

		begin_reversible_command (_("Set session start"));

		_session->add_command (new MementoCommand<Location>(*loc, &before, &after));

		commit_reversible_command ();
	}

	_session->set_session_range_is_free (false);
}


void
Editor::toggle_location_at_playhead_cursor ()
{
	if (!do_remove_location_at_playhead_cursor())
	{
		add_location_from_playhead_cursor();
	}
}

void
Editor::add_location_from_playhead_cursor ()
{
	add_location_mark (timepos_t (_session->audible_sample()));
}

bool
Editor::do_remove_location_at_playhead_cursor ()
{
	bool removed = false;
	if (_session) {
		//set up for undo
		XMLNode &before = _session->locations()->get_state();

		//find location(s) at this time
		Locations::LocationList locs;
		_session->locations()->find_all_between (timepos_t (_session->audible_sample()), timepos_t (_session->audible_sample()+1), locs, Location::Flags(0));
		for (Locations::LocationList::iterator i = locs.begin(); i != locs.end(); ++i) {
			if ((*i)->is_mark()) {
				_session->locations()->remove (*i);
				removed = true;
			}
		}

		//store undo
		if (removed) {
			begin_reversible_command (_("remove marker"));
			XMLNode &after = _session->locations()->get_state();
			_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
			commit_reversible_command ();
		}
	}
	return removed;
}

void
Editor::remove_location_at_playhead_cursor ()
{
	do_remove_location_at_playhead_cursor ();
}

/** Add a range marker around each selected region */
void
Editor::add_locations_from_region ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}
	bool commit = false;

	XMLNode &before = _session->locations()->get_state();

	for (RegionSelection::iterator i = rs.begin (); i != rs.end (); ++i) {

		boost::shared_ptr<Region> region = (*i)->region ();

		Location *location = new Location (*_session, region->position(), region->end(), region->name(), Location::IsRangeMarker);

		_session->locations()->add (location, true);
		commit = true;
	}

	if (commit) {
		begin_reversible_command (selection->regions.size () > 1 ? _("add markers") : _("add marker"));
		XMLNode &after = _session->locations()->get_state();
		_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
		commit_reversible_command ();
	}
}

/** Add a single range marker around all selected regions */
void
Editor::add_location_from_region ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	XMLNode &before = _session->locations()->get_state();

	string markername;

	if (rs.size() > 1) {
		_session->locations()->next_available_name(markername, "regions");
	} else {
		RegionView* rv = *(rs.begin());
		boost::shared_ptr<Region> region = rv->region();
		markername = region->name();
	}

	if (!choose_new_marker_name(markername)) {
		return;
	}

	// single range spanning all selected
	Location *location = new Location (*_session, selection->regions.start_time(), selection->regions.end_time(), markername, Location::IsRangeMarker);
	_session->locations()->add (location, true);

	begin_reversible_command (_("add marker"));
	XMLNode &after = _session->locations()->get_state();
	_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	commit_reversible_command ();
}

/* MARKS */

void
Editor::jump_forward_to_mark ()
{
	if (!_session) {
		return;
	}

	timepos_t pos = _session->locations()->first_mark_after (timepos_t (_playhead_cursor->current_sample()));

	if (pos == timepos_t::max (Temporal::AudioTime)) {
		return;
	}

	_session->request_locate (pos.samples());
}

void
Editor::jump_backward_to_mark ()
{
	if (!_session) {
		return;
	}

	timepos_t pos = _session->locations()->first_mark_before (timepos_t (_playhead_cursor->current_sample()));

	//handle the case where we are rolling, and we're less than one-half second past the mark, we want to go to the prior mark...
	if (_session->transport_rolling()) {
		if ((_playhead_cursor->current_sample() - pos.samples()) < _session->sample_rate()/2) {
			timepos_t prior = _session->locations()->first_mark_before (pos);
			pos = prior;
		}
	}

	if (pos == timepos_t::max (Temporal::AudioTime)) {
		return;
	}

	_session->request_locate (pos.samples());
}

void
Editor::set_mark ()
{
	samplepos_t const pos = _session->audible_sample ();

	string markername;
	_session->locations()->next_available_name (markername, "mark");

	if (!choose_new_marker_name (markername)) {
		return;
	}

	_session->locations()->add (new Location (*_session, timepos_t (pos), timepos_t(), markername, Location::IsMark), true);
}

void
Editor::clear_markers ()
{
	if (_session) {
		begin_reversible_command (_("clear markers"));

		XMLNode &before = _session->locations()->get_state();
		if (_session->locations()->clear_markers ()) {
			XMLNode &after = _session->locations()->get_state();
			_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
			commit_reversible_command ();
		}
	} else {
		abort_reversible_command ();
	}
}

void
Editor::clear_xrun_markers ()
{
	if (_session) {
		begin_reversible_command (_("clear xrun markers"));

		XMLNode &before = _session->locations()->get_state();
		if (_session->locations()->clear_xrun_markers ()) {
			XMLNode &after = _session->locations()->get_state();
			_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));

			commit_reversible_command ();
		}
	} else {
		abort_reversible_command ();
	}
}

void
Editor::clear_ranges ()
{
	if (_session) {
		begin_reversible_command (_("clear ranges"));

		XMLNode &before = _session->locations()->get_state();

		if (_session->locations()->clear_ranges ()) {

			XMLNode &after = _session->locations()->get_state();
			_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));

			commit_reversible_command ();
		}
	} else {
		abort_reversible_command ();
	}
}

void
Editor::clear_locations ()
{
	begin_reversible_command (_("clear locations"));

	XMLNode &before = _session->locations()->get_state();
	if (_session->locations()->clear ()) {
		XMLNode &after = _session->locations()->get_state();
		_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));

		commit_reversible_command ();
	} else {
		abort_reversible_command ();
	}
}

void
Editor::unhide_markers ()
{
	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		Location *l = (*i).first;
		if (l->is_hidden() && l->is_mark()) {
			l->set_hidden(false, this);
		}
	}
}

void
Editor::unhide_ranges ()
{
	for (LocationMarkerMap::iterator i = location_markers.begin(); i != location_markers.end(); ++i) {
		Location *l = (*i).first;
		if (l->is_hidden() && l->is_range_marker()) {
			l->set_hidden(false, this);
		}
	}
}

/* INSERT/REPLACE */

void
Editor::insert_source_list_selection (float times)
{
	RouteTimeAxisView *tv = 0;
	boost::shared_ptr<Playlist> playlist;

	if (clicked_routeview != 0) {
		tv = clicked_routeview;
	} else if (!selection->tracks.empty()) {
		if ((tv = dynamic_cast<RouteTimeAxisView*>(selection->tracks.front())) == 0) {
			return;
		}
	} else if (entered_track != 0) {
		if ((tv = dynamic_cast<RouteTimeAxisView*>(entered_track)) == 0) {
			return;
		}
	} else {
		return;
	}

	if ((playlist = tv->playlist()) == 0) {
		return;
	}

	boost::shared_ptr<Region> region = _sources->get_single_selection ();
	if (region == 0) {
		return;
	}

	begin_reversible_command (_("insert region"));
	playlist->clear_changes ();
	playlist->clear_owned_changes ();

	playlist->add_region ((RegionFactory::create (region, true)), get_preferred_edit_position(), times, _session->config.get_layered_record_mode());  //ToDo:  insert_mode ?

	if (should_ripple()) {
		do_ripple (playlist, get_preferred_edit_position(), region->length() * times, boost::shared_ptr<Region>(), true);
	} else {
		playlist->rdiff_and_add_command (_session);
	}

	commit_reversible_command ();
}

/* BUILT-IN EFFECTS */

void
Editor::reverse_selection ()
{

}

/* GAIN ENVELOPE EDITING */

void
Editor::edit_envelope ()
{
}

/* PLAYBACK */

void
Editor::transition_to_rolling (bool fwd)
{
	if (!_session) {
		return;
	}

	if (_session->config.get_external_sync()) {
		switch (TransportMasterManager::instance().current()->type()) {
		case Engine:
			break;
		default:
			/* transport controlled by the master */
			return;
		}
	}

	if (_session->is_auditioning()) {
		_session->cancel_audition ();
		return;
	}

	_session->request_transport_speed (fwd ? _session->default_play_speed() : -_session->default_play_speed());
	_session->request_roll ();
}

void
Editor::play_from_start ()
{
	_session->request_locate (_session->current_start_sample(), MustRoll);
}

void
Editor::play_from_edit_point ()
{
	_session->request_locate (get_preferred_edit_position().samples(), MustRoll);
}

void
Editor::play_from_edit_point_and_return ()
{
	samplepos_t start_sample;
	samplepos_t return_sample;

	start_sample = get_preferred_edit_position (EDIT_IGNORE_PHEAD).samples();

	if (_session->transport_rolling()) {
		_session->request_locate (start_sample, MustStop);
		return;
	}

	/* don't reset the return sample if its already set */

	if ((return_sample = _session->requested_return_sample()) < 0) {
		return_sample = _session->audible_sample();
	}

	if (start_sample >= 0) {
		_session->request_roll_at_and_return (start_sample, return_sample);
	}
}

void
Editor::play_selection ()
{
	timepos_t start, end;
	if (!get_selection_extents (start, end))
		return;

	TimelineRange ar (start, end, 0);
	list<TimelineRange> lar;
	lar.push_back (ar);

	_session->request_play_range (&lar, true);
}


void
Editor::maybe_locate_with_edit_preroll (samplepos_t location)
{
	if (_session->transport_rolling() || !UIConfiguration::instance().get_follow_edits() || _session->config.get_external_sync())
		return;

	location -= _session->preroll_samples (location);

	//don't try to locate before the beginning of time
	if (location < 0) {
		location = 0;
	}

	//if follow_playhead is on, keep the playhead on the screen
	if (_follow_playhead)
		if (location < _leftmost_sample)
			location = _leftmost_sample;

	_session->request_locate (location);
}

void
Editor::play_with_preroll ()
{
	timepos_t start, end;

	if (UIConfiguration::instance().get_follow_edits() && get_selection_extents (start, end)) {

		samplepos_t start_sample = start.samples();
		samplepos_t end_sample = end.samples();

		const samplepos_t preroll = _session->preroll_samples (start_sample);

		samplepos_t ret = start_sample;

		if (start_sample > preroll) {
			start_sample = start_sample - preroll;
		}

		end_sample = end_sample + preroll;  //"post-roll"

		TimelineRange ar (start, end, 0);
		list<TimelineRange> lar;
		lar.push_back (ar);

		_session->request_play_range (&lar, true);
		_session->set_requested_return_sample (ret);  //force auto-return to return to range start, without the preroll
	} else {
		samplepos_t ph = _playhead_cursor->current_sample ();
		const samplepos_t preroll = _session->preroll_samples (ph);
		samplepos_t start;
		if (ph > preroll) {
			start = ph - preroll;
		} else {
			start = 0;
		}
		_session->request_locate (start, MustRoll);
		_session->set_requested_return_sample (ph);  //force auto-return to return to playhead location, without the preroll
	}
}

void
Editor::rec_with_preroll ()
{
	samplepos_t ph = _playhead_cursor->current_sample ();
	samplepos_t preroll = _session->preroll_samples (ph);
	_session->request_preroll_record_trim (ph, preroll);
}

void
Editor::rec_with_count_in ()
{
	_session->request_count_in_record ();
}

void
Editor::play_location (Location& location)
{
	if (location.start() <= location.end()) {
		return;
	}

	_session->request_bounded_roll (location.start().samples(), location.end().samples());
}

void
Editor::loop_location (Location& location)
{
	if (location.start() <= location.end()) {
		return;
	}

	Location* tll;

	if ((tll = transport_loop_location()) != 0) {
		tll->set (location.start(), location.end());

		// enable looping, reposition and start rolling
		_session->request_locate (tll->start().samples(), MustRoll);
		_session->request_play_loop (true);
	}
}

void
Editor::do_layer_operation (LayerOperation op)
{
	if (selection->regions.empty ()) {
		return;
	}

	bool const multiple = selection->regions.size() > 1;
	switch (op) {
	case Raise:
		if (multiple) {
			begin_reversible_command (_("raise regions"));
		} else {
			begin_reversible_command (_("raise region"));
		}
		break;

	case RaiseToTop:
		if (multiple) {
			begin_reversible_command (_("raise regions to top"));
		} else {
			begin_reversible_command (_("raise region to top"));
		}
		break;

	case Lower:
		if (multiple) {
			begin_reversible_command (_("lower regions"));
		} else {
			begin_reversible_command (_("lower region"));
		}
		break;

	case LowerToBottom:
		if (multiple) {
			begin_reversible_command (_("lower regions to bottom"));
		} else {
			begin_reversible_command (_("lower region"));
		}
		break;
	}

	set<boost::shared_ptr<Playlist> > playlists = selection->regions.playlists ();
	for (set<boost::shared_ptr<Playlist> >::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		(*i)->clear_owned_changes ();
	}

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		boost::shared_ptr<Region> r = (*i)->region ();
		switch (op) {
		case Raise:
			r->raise ();
			break;
		case RaiseToTop:
			r->raise_to_top ();
			break;
		case Lower:
			r->lower ();
			break;
		case LowerToBottom:
			r->lower_to_bottom ();
		}
	}

	for (set<boost::shared_ptr<Playlist> >::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		vector<Command*> cmds;
		(*i)->rdiff (cmds);
		_session->add_commands (cmds);
	}

	commit_reversible_command ();
}

void
Editor::raise_region ()
{
	do_layer_operation (Raise);
}

void
Editor::raise_region_to_top ()
{
	do_layer_operation (RaiseToTop);
}

void
Editor::lower_region ()
{
	do_layer_operation (Lower);
}

void
Editor::lower_region_to_bottom ()
{
	do_layer_operation (LowerToBottom);
}

/** Show the region editor for the selected regions */
void
Editor::show_region_properties ()
{
	selection->foreach_regionview (&RegionView::show_region_editor);
}

/** Show the midi list editor for the selected MIDI regions */
void
Editor::show_midi_list_editor ()
{
	selection->foreach_midi_regionview (&MidiRegionView::show_list_editor);
}

void
Editor::rename_region ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	ArdourDialog d (_("Rename Region"), true, false);
	Entry entry;
	Label label (_("New name:"));
	HBox hbox;

	hbox.set_spacing (6);
	hbox.pack_start (label, false, false);
	hbox.pack_start (entry, true, true);

	d.get_vbox()->set_border_width (12);
	d.get_vbox()->pack_start (hbox, false, false);

	d.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	d.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	d.set_size_request (300, -1);

	entry.set_text (rs.front()->region()->name());
	entry.select_region (0, -1);

	entry.signal_activate().connect (sigc::bind (sigc::mem_fun (d, &Dialog::response), RESPONSE_OK));

	d.show_all ();

	entry.grab_focus();

	int const ret = d.run();

	d.hide ();

	if (ret != RESPONSE_OK) {
		return;
	}

	std::string str = entry.get_text();
	strip_whitespace_edges (str);
	if (!str.empty()) {
		if (!rs.front()->region()->set_name (str)) {
			ArdourMessageDialog msg (_("Rename failed. Check for characters such as '/' or ':'"));
			msg.run ();
		} else {
			_regions->redisplay ();
		}
	}
}

/** Start an audition of the first selected region */
void
Editor::play_edit_range ()
{
	timepos_t start, end;

	if (get_edit_op_range (start, end)) {
		_session->request_bounded_roll (start.samples(), end.samples());
	}
}

void
Editor::play_selected_region ()
{
	timepos_t start = timepos_t::max (Temporal::AudioTime);
	timepos_t end;

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		if ((*i)->region()->position() < start) {
			start = (*i)->region()->position();
		}
		if ((*i)->region()->end() > end) {
			end = (*i)->region()->end();
		}
	}

	_session->request_bounded_roll (start.samples(), end.samples());
}

void
Editor::audition_playlist_region_standalone (boost::shared_ptr<Region> region)
{
	_session->audition_region (region);
}

void
Editor::region_from_selection ()
{
	if (clicked_axisview == 0) {
		return;
	}

	if (selection->time.empty()) {
		return;
	}

	timepos_t start = selection->time[clicked_selection].start();
	timepos_t end = selection->time[clicked_selection].end();

	TrackViewList tracks = get_tracks_for_range_action ();

	timecnt_t selection_cnt = start.distance (end);

	for (TrackSelection::iterator i = tracks.begin(); i != tracks.end(); ++i) {
		boost::shared_ptr<Region> current;
		boost::shared_ptr<Playlist> pl;
		timecnt_t internal_start;
		string new_name;

		if ((pl = (*i)->playlist()) == 0) {
			continue;
		}

		if ((current = pl->top_region_at (start)) == 0) {
			continue;
		}

		internal_start = current->position().distance (start);
		RegionFactory::region_name (new_name, current->name(), true);

		PropertyList plist;

		plist.add (ARDOUR::Properties::start, current->start() + internal_start);
		plist.add (ARDOUR::Properties::length, selection_cnt);
		plist.add (ARDOUR::Properties::name, new_name);
		plist.add (ARDOUR::Properties::layer, 0);

		boost::shared_ptr<Region> region (RegionFactory::create (current, plist));
	}
}

void
Editor::create_region_from_selection (vector<boost::shared_ptr<Region> >& new_regions)
{
	if (selection->time.empty() || selection->tracks.empty()) {
		return;
	}

	timepos_t start, end;
	if (clicked_selection) {
		start = selection->time[clicked_selection].start();
		end = selection->time[clicked_selection].end();
	} else {
		start = selection->time.start_time();
		end = selection->time.end_time();
	}

	TrackViewList ts = selection->tracks.filter_to_unique_playlists ();
	sort_track_selection (ts);

	for (TrackSelection::iterator i = ts.begin(); i != ts.end(); ++i) {
		boost::shared_ptr<Region> current;
		boost::shared_ptr<Playlist> playlist;
		timecnt_t internal_start;
		string new_name;

		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}

		if ((current = playlist->top_region_at(start)) == 0) {
			continue;
		}

		internal_start = current->position().distance (start);
		RegionFactory::region_name (new_name, current->name(), true);

		PropertyList plist;

		plist.add (ARDOUR::Properties::start, current->start() + internal_start);
		plist.add (ARDOUR::Properties::length, start.distance (end));
		plist.add (ARDOUR::Properties::name, new_name);

		new_regions.push_back (RegionFactory::create (current, plist));
	}
}

void
Editor::split_multichannel_region ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	vector< boost::shared_ptr<Region> > v;

	for (list<RegionView*>::iterator x = rs.begin(); x != rs.end(); ++x) {
		(*x)->region()->separate_by_channel (v);
	}
}

void
Editor::new_region_from_selection ()
{
	region_from_selection ();
	cancel_selection ();
}

static void
add_if_covered (RegionView* rv, const TimelineRange* ar, RegionSelection* rs)
{
	switch (rv->region()->coverage (ar->start(), ar->end())) {
	case Temporal::OverlapNone:
		break;
	default:
		rs->push_back (rv);
	}
}

/** Return either:
 *    - selected tracks, or if there are none...
 *    - tracks containing selected regions, or if there are none...
 *    - all tracks
 * @return tracks.
 */
TrackViewList
Editor::get_tracks_for_range_action () const
{
	TrackViewList t;

	if (selection->tracks.empty()) {

		/* use tracks with selected regions */

		RegionSelection rs = selection->regions;

		for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
			TimeAxisView* tv = &(*i)->get_time_axis_view();

			if (!t.contains (tv)) {
				t.push_back (tv);
			}
		}

		if (t.empty()) {
			/* no regions and no tracks: use all tracks */
			t = track_views;
		}

	} else {

		t = selection->tracks;
	}

	return t.filter_to_unique_playlists();
}

void
Editor::separate_regions_between (const TimeSelection& ts)
{
	bool in_command = false;
	boost::shared_ptr<Playlist> playlist;
	RegionSelection new_selection;

	TrackViewList tmptracks = get_tracks_for_range_action ();
	sort_track_selection (tmptracks);

	for (TrackSelection::iterator i = tmptracks.begin(); i != tmptracks.end(); ++i) {

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);

		if (!rtv) {
			continue;
		}

		if (!rtv->is_track()) {
			continue;
		}

		if ((playlist = rtv->playlist()) != 0) {

			playlist->clear_changes ();

			/* XXX need to consider musical time selections here at some point */

			for (list<TimelineRange>::const_iterator t = ts.begin(); t != ts.end(); ++t) {

				if (!in_command) {
					begin_reversible_command (_("separate"));
					in_command = true;
				}

				sigc::connection c = rtv->view()->RegionViewAdded.connect (
					sigc::mem_fun(*this, &Editor::collect_new_region_view));

				latest_regionviews.clear ();

				playlist->partition ((*t).start(), (*t).end(), false);

				c.disconnect ();

				if (!latest_regionviews.empty()) {

					rtv->view()->foreach_regionview (sigc::bind (
					                                             sigc::ptr_fun (add_if_covered),
					                                             &(*t), &new_selection));

					/* pick up changes to existing regions */

					vector<Command*> cmds;
					playlist->rdiff (cmds);
					_session->add_commands (cmds);

					/* pick up changes to the playlist itself (adds/removes) */

					_session->add_command(new StatefulDiffCommand (playlist));
				}
			}
		}
	}

	if (in_command) {
		if (_session->abort_empty_reversible_command ()) {
			return;
		}

		RangeSelectionAfterSplit rsas = Config->get_range_selection_after_split();

		//if our config preference says to clear the selection, clear the Range selection
		if (rsas == ClearSel) {
			selection->clear_time();
			//but leave track selection intact
		} else if (rsas == ForceSel) {
			//note: forcing the regions to be selected *might* force a tool-change to Object here
			selection->set(new_selection);
		}

		commit_reversible_command ();
	}
}

struct PlaylistState {
	boost::shared_ptr<Playlist> playlist;
	XMLNode*  before;
};

/** Take tracks from get_tracks_for_range_action and cut any regions
 *  on those tracks so that the tracks are empty over the time
 *  selection.
 */
void
Editor::separate_region_from_selection ()
{
	/* preferentially use *all* ranges in the time selection if we're in range mode
	   to allow discontiguous operation, since get_edit_op_range() currently
	   returns a single range.
	*/

	if (!selection->time.empty()) {

		separate_regions_between (selection->time);

	} else {

		timepos_t start;
		timepos_t end;

		if (get_edit_op_range (start, end)) {

			TimelineRange ar (start, end, 1);
			TimeSelection ts;
			ts.push_back (ar);

			separate_regions_between (ts);
		}
	}
}

void
Editor::separate_region_from_punch ()
{
	Location* loc  = _session->locations()->auto_punch_location();
	if (loc) {
		separate_regions_using_location (*loc);
	}
}

void
Editor::separate_region_from_loop ()
{
	Location* loc  = _session->locations()->auto_loop_location();
	if (loc) {
		separate_regions_using_location (*loc);
	}
}

void
Editor::separate_regions_using_location (Location& loc)
{
	if (loc.is_mark()) {
		return;
	}

	TimelineRange ar (loc.start(), loc.end(), 1);
	TimeSelection ts;

	ts.push_back (ar);

	separate_regions_between (ts);
}

/** Separate regions under the selected region */
void
Editor::separate_under_selected_regions ()
{
	vector<PlaylistState> playlists;

	RegionSelection rs;

	rs = get_regions_from_selection_and_entered();

	if (!_session || rs.empty()) {
		return;
	}

	begin_reversible_command (_("separate region under"));

	list<boost::shared_ptr<Region> > regions_to_remove;

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		// we can't just remove the region(s) in this loop because
		// this removes them from the RegionSelection, and they thus
		// disappear from underneath the iterator, and the ++i above
		// SEGVs in a puzzling fashion.

		// so, first iterate over the regions to be removed from rs and
		// add them to the regions_to_remove list, and then
		// iterate over the list to actually remove them.

		regions_to_remove.push_back ((*i)->region());
	}

	for (list<boost::shared_ptr<Region> >::iterator rl = regions_to_remove.begin(); rl != regions_to_remove.end(); ++rl) {

		boost::shared_ptr<Playlist> playlist = (*rl)->playlist();

		if (!playlist) {
			// is this check necessary?
			continue;
		}

		vector<PlaylistState>::iterator i;

		//only take state if this is a new playlist.
		for (i = playlists.begin(); i != playlists.end(); ++i) {
			if ((*i).playlist == playlist) {
				break;
			}
		}

		if (i == playlists.end()) {

			PlaylistState before;
			before.playlist = playlist;
			before.before = &playlist->get_state();
			playlist->clear_changes ();
			playlist->freeze ();
			playlists.push_back(before);
		}

		//Partition on the region bounds
		playlist->partition ((*rl)->position().decrement(), (*rl)->end(), true);

		//Re-add region that was just removed due to the partition operation
		playlist->add_region ((*rl), (*rl)->position());
	}

	vector<PlaylistState>::iterator pl;

	for (pl = playlists.begin(); pl != playlists.end(); ++pl) {
		(*pl).playlist->thaw ();
		_session->add_command(new MementoCommand<Playlist>(*(*pl).playlist, (*pl).before, &(*pl).playlist->get_state()));
	}

	commit_reversible_command ();
}

void
Editor::crop_region_to_selection ()
{
	if (!selection->time.empty()) {

		begin_reversible_command (_("Crop Regions to Time Selection"));
		for (std::list<TimelineRange>::iterator i = selection->time.begin(); i != selection->time.end(); ++i) {
			crop_region_to ((*i).start(), (*i).end());
		}
		commit_reversible_command();
	} else {

		timepos_t start;
		timepos_t end;

		if (get_edit_op_range (start, end)) {
			begin_reversible_command (_("Crop Regions to Edit Range"));

			crop_region_to (start, end);

			commit_reversible_command();
		}
	}

}

void
Editor::crop_region_to (timepos_t const & start, timepos_t const & end)
{
	vector<boost::shared_ptr<Playlist> > playlists;
	boost::shared_ptr<Playlist> playlist;
	TrackViewList ts;

	if (selection->tracks.empty()) {
		ts = track_views.filter_to_unique_playlists();
	} else {
		ts = selection->tracks.filter_to_unique_playlists ();
	}

	sort_track_selection (ts);

	for (TrackSelection::iterator i = ts.begin(); i != ts.end(); ++i) {

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> ((*i));

		if (!rtv) {
			continue;
		}

		boost::shared_ptr<Track> t = rtv->track();

		if (t) {
			if ((playlist = rtv->playlist()) != 0) {
				playlists.push_back (playlist);
			}
		}
	}

	if (playlists.empty()) {
		return;
	}

	timepos_t pos;
	timepos_t new_start;
	timepos_t new_end;
	timecnt_t new_length;

	for (vector<boost::shared_ptr<Playlist> >::iterator i = playlists.begin(); i != playlists.end(); ++i) {

		/* Only the top regions at start and end have to be cropped */
		boost::shared_ptr<Region> region_at_start = (*i)->top_region_at(start);
		boost::shared_ptr<Region> region_at_end = (*i)->top_region_at(end);

		vector<boost::shared_ptr<Region> > regions;

		if (region_at_start != 0) {
			regions.push_back (region_at_start);
		}
		if (region_at_end != 0) {
			regions.push_back (region_at_end);
		}

		/* now adjust lengths */
		for (vector<boost::shared_ptr<Region> >::iterator i = regions.begin(); i != regions.end(); ++i) {

			pos = (*i)->position();
			new_start = max (start, pos);
			if (timepos_t::max (pos.time_domain()).earlier (pos) > (*i)->length()) {
				new_end = (*i)->end();
			} else {
				new_end = timepos_t::max (pos.time_domain());
			}
			new_end = min (end, new_end);
			new_length = new_start.distance (new_end);

			(*i)->clear_changes ();
			(*i)->trim_to (new_start, new_length);
			_session->add_command (new StatefulDiffCommand (*i));
		}
	}
}

void
Editor::region_fill_track ()
{
	boost::shared_ptr<Playlist> playlist;
	RegionSelection regions = get_regions_from_selection_and_entered ();
	RegionSelection foo;

	timepos_t const end (_session->current_end_sample ());

	if (regions.empty () || regions.end_time().increment() >= end) {
		return;
	}

	timepos_t const start_time = regions.start_time ();
	timepos_t const end_time = regions.end_time ();
	timecnt_t const gap = start_time.distance (end_time);

	begin_reversible_command (Operations::region_fill);

	selection->clear_regions ();

	for (RegionSelection::iterator i = regions.begin(); i != regions.end(); ++i) {

		boost::shared_ptr<Region> r ((*i)->region());

		TimeAxisView& tv = (*i)->get_time_axis_view();
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (&tv);
		latest_regionviews.clear ();
		sigc::connection c = rtv->view()->RegionViewAdded.connect (sigc::mem_fun(*this, &Editor::collect_new_region_view));

		/* XXX NUTEMPO this is non-const as the second arg of
		   Playlist::duplicate_until gets modified. Maybe change this to
		   be more consistent with other APIs
		*/
		timepos_t position = end_time + start_time.distance (r->position());
		playlist = (*i)->region()->playlist();
		playlist->clear_changes ();
		playlist->duplicate_until (r, position, gap, end);
		_session->add_command(new StatefulDiffCommand (playlist));

		c.disconnect ();

		foo.insert (foo.end(), latest_regionviews.begin(), latest_regionviews.end());
	}

	if (!foo.empty()) {
		selection->set (foo);
	}

	commit_reversible_command ();
}

void
Editor::set_region_sync_position ()
{
	set_sync_point (get_preferred_edit_position (), get_regions_from_selection_and_edit_point ());
}

void
Editor::set_sync_point (timepos_t const & where, const RegionSelection& rs)
{
	bool in_command = false;

	for (RegionSelection::const_iterator r = rs.begin(); r != rs.end(); ++r) {

		if (!(*r)->region()->covers (where)) {
			continue;
		}

		boost::shared_ptr<Region> region ((*r)->region());

		if (!in_command) {
			begin_reversible_command (_("set sync point"));
			in_command = true;
		}

		region->clear_changes ();
		region->set_sync_position (where);
		_session->add_command(new StatefulDiffCommand (region));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

/** Remove the sync positions of the selection */
void
Editor::remove_region_sync ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("remove region sync"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {

		(*i)->region()->clear_changes ();
		(*i)->region()->clear_sync_position ();
		_session->add_command(new StatefulDiffCommand ((*i)->region()));
	}

	commit_reversible_command ();
}

void
Editor::naturalize_region ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	if (rs.size() > 1) {
		begin_reversible_command (_("move regions to original position"));
	} else {
		begin_reversible_command (_("move region to original position"));
	}

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		(*i)->region()->clear_changes ();
		(*i)->region()->move_to_natural_position ();
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}

	commit_reversible_command ();
}

void
Editor::align_regions (RegionPoint what)
{
	RegionSelection const rs = get_regions_from_selection_and_edit_point ();

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("align selection"));

	timepos_t const position = get_preferred_edit_position ();

	for (RegionSelection::const_iterator i = rs.begin(); i != rs.end(); ++i) {
		align_region_internal ((*i)->region(), what, position);
	}

	commit_reversible_command ();
}

struct RegionSortByTime {
	bool operator() (const RegionView* a, const RegionView* b) {
		return a->region()->position() < b->region()->position();
	}
};

void
Editor::align_regions_relative (RegionPoint point)
{
	RegionSelection const rs = get_regions_from_selection_and_edit_point ();

	if (rs.empty()) {
		return;
	}

	timepos_t const position = get_preferred_edit_position ();

	timecnt_t distance (position.time_domain());
	timepos_t pos;
	int dir = 1;

	list<RegionView*> sorted;
	rs.by_position (sorted);

	boost::shared_ptr<Region> r ((*sorted.begin())->region());

	switch (point) {
	case Start:
		pos = position;
		if (position > r->position()) {
			distance = r->position().distance (position);
		} else {
			distance = position.distance (r->position());
			dir = -1;
		}
		break;

	case End:
		if (position > r->nt_last()) {
			distance = r->nt_last().distance (position);
			pos = r->position() + distance;
		} else {
			distance = position.distance (r->nt_last());
			pos = r->position().earlier (distance);
			dir = -1;
		}
		break;

	case SyncPoint:
		pos = r->adjust_to_sync (position);
		if (pos > r->position()) {
			distance = r->position().distance (pos);
		} else {
			distance = pos.distance (r->position());
			dir = -1;
		}
		break;
	}

	if (pos == r->position()) {
		return;
	}

	begin_reversible_command (_("align selection (relative)"));

	/* move first one specially */

	r->clear_changes ();
	r->set_position (pos);
	_session->add_command(new StatefulDiffCommand (r));

	/* move rest by the same amount */

	sorted.pop_front();

	for (list<RegionView*>::iterator i = sorted.begin(); i != sorted.end(); ++i) {

		boost::shared_ptr<Region> region ((*i)->region());

		region->clear_changes ();

		if (dir > 0) {
			region->set_position (region->position() + distance);
		} else {
			region->set_position (region->position().earlier (distance));
		}

		_session->add_command(new StatefulDiffCommand (region));

	}

	commit_reversible_command ();
}

void
Editor::align_region (boost::shared_ptr<Region> region, RegionPoint point, timepos_t const & position)
{
	begin_reversible_command (_("align region"));
	align_region_internal (region, point, position);
	commit_reversible_command ();
}

void
Editor::align_region_internal (boost::shared_ptr<Region> region, RegionPoint point, timepos_t const & position)
{
	region->clear_changes ();

	switch (point) {
	case SyncPoint:
		region->set_position (region->adjust_to_sync (position));
		break;

	case End:
		if (position > region->length()) {
			region->set_position (position.earlier (region->length()));
		}
		break;

	case Start:
		region->set_position (position);
		break;
	}

	_session->add_command(new StatefulDiffCommand (region));
}

void
Editor::trim_region_front ()
{
	trim_region (true);
}

void
Editor::trim_region_back ()
{
	trim_region (false);
}

void
Editor::trim_region (bool front)
{
	timepos_t where = get_preferred_edit_position();
	RegionSelection rs = get_regions_from_selection_and_edit_point ();

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (front ? _("trim front") : _("trim back"));

	for (list<RegionView*>::const_iterator i = rs.by_layer().begin(); i != rs.by_layer().end(); ++i) {
		if (!(*i)->region()->locked()) {

			(*i)->region()->clear_changes ();

			if (front) {
				(*i)->region()->trim_front (where);
			} else {
				(*i)->region()->trim_end (where);
			}

			_session->add_command (new StatefulDiffCommand ((*i)->region()));
		}
	}

	commit_reversible_command ();
}

/** Trim the end of the selected regions to the position of the edit cursor */
void
Editor::trim_region_to_loop ()
{
	Location* loc = _session->locations()->auto_loop_location();
	if (!loc) {
		return;
	}
	trim_region_to_location (*loc, _("trim to loop"));
}

void
Editor::trim_region_to_punch ()
{
	Location* loc = _session->locations()->auto_punch_location();
	if (!loc) {
		return;
	}
	trim_region_to_location (*loc, _("trim to punch"));
}

void
Editor::trim_region_to_location (const Location& loc, const char* str)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();
	bool in_command = false;

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		RegionView* rv = (*x);

		/* require region to span proposed trim */
		switch (rv->region()->coverage (loc.start(), loc.end())) {
		case Temporal::OverlapNone:
			break;
		default:
			continue;
		}

		RouteTimeAxisView* tav = dynamic_cast<RouteTimeAxisView*> (&rv->get_time_axis_view());
		if (!tav) {
			return;
		}

		timepos_t start;
		timepos_t end;

		start = max (loc.start(), rv->region()->position());
		end = min (loc.end(), rv->region()->position() + rv->region()->length());

		rv->region()->clear_changes ();
		rv->region()->trim_to (start, start.distance (end));

		if (!in_command) {
			begin_reversible_command (str);
			in_command = true;
		}
		_session->add_command(new StatefulDiffCommand (rv->region()));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::trim_region_to_previous_region_end ()
{
	return trim_to_region(false);
}

void
Editor::trim_region_to_next_region_start ()
{
	return trim_to_region(true);
}

void
Editor::trim_to_region(bool forward)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();
	bool in_command = false;

	boost::shared_ptr<Region> next_region;

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {

		AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*x);

		if (!arv) {
			continue;
		}

		AudioTimeAxisView* atav = dynamic_cast<AudioTimeAxisView*> (&arv->get_time_axis_view());

		if (!atav) {
			continue;
		}

		boost::shared_ptr<Region> region = arv->region();
		boost::shared_ptr<Playlist> playlist (region->playlist());

		region->clear_changes ();

		if (forward) {

			next_region = playlist->find_next_region (region->position(), Start, 1);

			if (!next_region) {
				continue;
			}

			region->trim_end (next_region->position().decrement());
			arv->region_changed (PropertyChange (ARDOUR::Properties::length));
		}
		else {

			next_region = playlist->find_next_region (region->position(), Start, 0);

			if (!next_region) {
				continue;
			}

			region->trim_front (next_region->end());
			arv->region_changed (ARDOUR::bounds_change);
		}

		if (!in_command) {
			begin_reversible_command (_("trim to region"));
			in_command = true;
		}
		_session->add_command(new StatefulDiffCommand (region));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::unfreeze_route ()
{
	if (clicked_routeview == 0 || !clicked_routeview->is_track()) {
		return;
	}

	clicked_routeview->track()->unfreeze ();
}

void*
Editor::_freeze_thread (void* arg)
{
	return static_cast<Editor*>(arg)->freeze_thread ();
}

void*
Editor::freeze_thread ()
{
	/* create event pool because we may need to talk to the session */
	SessionEvent::create_per_thread_pool ("freeze events", 64);
	/* create per-thread buffers for process() tree to use */
	clicked_routeview->audio_track()->freeze_me (*current_interthread_info);
	current_interthread_info->done = true;
	return 0;
}

void
Editor::freeze_route ()
{
	if (!_session) {
		return;
	}

	/* stop transport before we start. this is important */
	_session->request_stop();

	/* wait for just a little while, because the above call is asynchronous */
	int timeout = 10;
	do {
		Glib::usleep (_session->engine().usecs_per_cycle ());
	} while (!_session->transport_stopped() && --timeout > 0);

	if (timeout == 0) {
		ArdourMessageDialog d (
			_("Transport cannot be stopped, likely due to external timecode sync.\n"
			  "Freezing a track requires the transport to be stopped.")
			);
		d.set_title (_("Cannot freeze"));
		d.run ();
		return;
		return;
	}

	if (clicked_routeview == 0 || !clicked_routeview->is_audio_track()) {
		return;
	}

	if (!clicked_routeview->track()->bounceable (clicked_routeview->track()->main_outs(), true)) {
		ArdourMessageDialog d (
			_("This track/bus cannot be frozen because the signal adds or loses channels before reaching the outputs.\n"
			  "This is typically caused by plugins that generate stereo output from mono input or vice versa.")
			);
		d.set_title (_("Cannot freeze"));
		d.run ();
		return;
	}

	if (clicked_routeview->track()->has_external_redirects()) {
		ArdourMessageDialog d (string_compose (_("<b>%1</b>\n\nThis track has at least one send/insert/return/sidechain as part of its signal flow.\n\n"
		                                         "Freezing will only process the signal as far as the first send/insert/return/sidechain."),
		                                       clicked_routeview->track()->name()), true, MESSAGE_INFO, BUTTONS_NONE, true);

		d.add_button (_("Freeze anyway"), Gtk::RESPONSE_OK);
		d.add_button (_("Don't freeze"), Gtk::RESPONSE_CANCEL);
		d.set_title (_("Freeze Limits"));

		int response = d.run ();

		switch (response) {
			case Gtk::RESPONSE_OK:
				break;
			default:
				return;
		}
	}

	InterThreadInfo itt;
	current_interthread_info = &itt;

	InterthreadProgressWindow ipw (current_interthread_info, _("Freeze"), _("Cancel Freeze"));

	pthread_create_and_store (X_("freezer"), &itt.thread, _freeze_thread, this);

	CursorContext::Handle cursor_ctx = CursorContext::create(*this, _cursors->wait);

	while (!itt.done && !itt.cancel) {
		gtk_main_iteration ();
	}

	pthread_join (itt.thread, 0);
	current_interthread_info = 0;
}

void
Editor::bounce_range_selection (BounceTarget target, bool enable_processing)
{
	if (selection->time.empty()) {
		return;
	}

	/* you can't currently apply processing to a NewTrigger bounce */
	assert (!(enable_processing && (target == NewTrigger)));

	bool     copy_to_clip_library = false;
	uint32_t trigger_slot         = 0;

	string bounce_name;
	switch (target) {
		case NewSource:
			bounce_name = "Bounced";
			break;
		case ReplaceRange:
			bounce_name = "Consolidated";
			break;
		case NewTrigger:
			bounce_name = "Unnamed Clip";
			break;
	}

	TrackSelection views = selection->tracks;

	for (TrackViewList::iterator i = views.begin(); i != views.end(); ++i) {

		if (enable_processing) {

			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);

			if (rtv && rtv->track() && target == ReplaceRange && !rtv->track()->bounceable (rtv->track()->main_outs(), false)) {
				ArdourMessageDialog d (
					_("You can't perform this operation because the processing of the signal "
					  "will cause one or more of the tracks to end up with a region with more channels than this track has inputs.\n\n"
					  "You can do this without processing, which is a different operation.")
					);
				d.set_title (_("Cannot bounce"));
				d.run ();
				return;
			}
		}
	}

	/*prompt the user for a new name*/
	{
		Prompter dialog (true);
		ArdourDropdown* tslot = 0;
		Gtk::CheckButton* cliplib = 0;
		Gtk::Alignment *align = 0;

		switch (target) {
			case NewSource:
				dialog.set_prompt (_("Name for Bounced Region:"));
				dialog.add_button (_("Bounce"), RESPONSE_ACCEPT);
				break;
			case ReplaceRange:
				dialog.set_prompt (_("Name for Consolidated Region:"));
				dialog.add_button (_("Rename"), RESPONSE_ACCEPT);
				break;
			case NewTrigger:
				dialog.set_prompt (_("Name for Trigger Clip:"));
				dialog.add_button (_("Bounce"), RESPONSE_ACCEPT);
				break;
		}

		dialog.set_name ("BounceNameWindow");
		dialog.set_size_request (400, -1);
		dialog.set_position (Gtk::WIN_POS_MOUSE);

		dialog.set_initial_text (bounce_name);

		if (target != ReplaceRange) {
			cliplib = manage (new Gtk::CheckButton (_("Copy to Clip Libary")));
			align = manage (new Gtk::Alignment (0, .5, 0, 0));
			align->add (*cliplib);
			cliplib->show ();
		}

		if (target == NewSource) {
			Label* label = manage (new Label (_("Bounced Range will appear in the Source list.")));
			dialog.get_vbox()->set_spacing (8);
			dialog.get_vbox()->pack_start (*label);
			dialog.get_vbox()->pack_start (*align);
			label->show();
		} else if (target == NewTrigger) {
			Label* label = manage (new Label (_("Trigger Slot:")));
			label->set_alignment(1.0, 0.5);
			HBox*  tbox  = manage (new HBox);
			tslot        = manage (new ArdourDropdown ());

			for (int c = 0; c < default_triggers_per_box; ++c) {
				std::string lbl = cue_marker_name (c);
				tslot->AddMenuElem (Menu_Helpers::MenuElem (lbl, sigc::bind ([] (uint32_t* t, uint32_t v, ArdourDropdown* s, std::string l) {*t = v; s->set_text (l);}, &trigger_slot, c, tslot, lbl)));
			}
			tslot->set_active ("A");

			tbox->set_homogeneous (false);
			tbox->set_spacing (5);
			tbox->set_border_width (10);
			tbox->pack_start (*label, false, false);
			tbox->pack_start (*tslot, false, false);
			tbox->pack_start (*align, true, true);
			tbox->show_all ();
			dialog.get_vbox()->pack_start (*tbox);
		} else {
			dialog.get_vbox()->pack_start (*align);
		}

		dialog.show ();

		switch (dialog.run ()) {
		case RESPONSE_ACCEPT:
			break;
		default:
			return;
		}

		dialog.get_result (bounce_name);

		if (cliplib && cliplib->get_active ()) {
			copy_to_clip_library = true;
		}
	}

	timepos_t start = selection->time[clicked_selection].start();
	timepos_t end = selection->time[clicked_selection].end();
	timecnt_t cnt = start.distance (end);
	bool in_command = false;

	TempoMap::SharedPtr tmap (TempoMap::write_copy());
	double tempo = tmap->tempo_at(start).quarter_notes_per_minute();

	for (TrackViewList::iterator i = views.begin(); i != views.end(); ++i) {

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);

		if (!rtv) {
			continue;
		}

		boost::shared_ptr<Playlist> playlist;

		if ((playlist = rtv->playlist()) == 0) {
			continue;
		}

		InterThreadInfo itt;

		playlist->clear_changes ();
		playlist->clear_owned_changes ();

		boost::shared_ptr<Region> r;

		/*make the "source" (whole-file region)*/
		/*note: bounce_range() will append the playlist name to the resulting region and filename*/
		if (enable_processing) {
			r = rtv->track()->bounce_range (start.samples(), (start+cnt).samples(), itt, rtv->track()->main_outs(), false, bounce_name);
		} else {
			r = rtv->track()->bounce_range (start.samples(), (start+cnt).samples(), itt, boost::shared_ptr<Processor>(), false, bounce_name);
		}

		if (!r) {
			continue;
		}

		if (!in_command) {
			begin_reversible_command (_("bounce range"));
			in_command = true;
		}

		if (copy_to_clip_library) {
			export_to_clip_library (r);
		}

		if (target == ReplaceRange) {
			/*remove the edxisting regions under the edit range*/
			list<TimelineRange> ranges;
			ranges.push_back (TimelineRange (start, start+cnt, 0));
			playlist->cut (ranges); // discard result

			/*SPECIAL CASE:  we are bouncing to a new Source *AND* replacing the existing range on the timeline  (consolidate)*/
			/*we don't add the whole_file region here; we insert a discrete copy*/
			PropertyList plist;
			plist.add (ARDOUR::Properties::whole_file, false);
			boost::shared_ptr<Region> copy (RegionFactory::create (r, plist));
			playlist->add_region (copy, start);
		} else if (target == NewTrigger) {
			boost::shared_ptr<Trigger::UIState> state (new Trigger::UIState());
			state->name = bounce_name;
			state->tempo = tempo;
			rtv->track ()->triggerbox ()->enqueue_trigger_state_for_region(r, state);
			rtv->track ()->triggerbox ()->set_from_selection (trigger_slot, r);
			rtv->track ()->presentation_info ().set_trigger_track (true);
		}

		vector<Command*> cmds;
		playlist->rdiff (cmds);
		_session->add_commands (cmds);

		_session->add_command (new StatefulDiffCommand (playlist));
	}

	if (in_command && !_session->abort_empty_reversible_command ()) {
		commit_reversible_command ();
	}
}

/** Delete selected regions, automation points or a time range */
void
Editor::delete_ ()
{
	//special case: if the user is pointing in the editor/mixer strip, they may be trying to delete a plugin.
	//we need this because the editor-mixer strip is in the editor window, so it doesn't get the bindings from the mix window
	bool deleted = false;
	if (current_mixer_strip && current_mixer_strip == MixerStrip::entered_mixer_strip())
		deleted = current_mixer_strip->delete_processors ();

	if (!deleted)
		cut_copy (Delete);
}

/** Cut selected regions, automation points or a time range */
void
Editor::cut ()
{
	cut_copy (Cut);
}

/** Copy selected regions, automation points or a time range */
void
Editor::copy ()
{
	cut_copy (Copy);
}


/** @return true if a Cut, Copy or Clear is possible */
bool
Editor::can_cut_copy () const
{
	if (!selection->time.empty() || !selection->regions.empty() || !selection->points.empty())
		return true;

	return false;
}


/** Cut, copy or clear selected regions, automation points or a time range.
 * @param op Operation (Delete, Cut, Copy or Clear)
 */
void
Editor::cut_copy (CutCopyOp op)
{
	/* only cancel selection if cut/copy is successful.*/

	string opname;

	switch (op) {
	case Delete:
		opname = _("delete");
		break;
	case Cut:
		opname = _("cut");
		break;
	case Copy:
		opname = _("copy");
		break;
	case Clear:
		opname = _("clear");
		break;
	}

	/* if we're deleting something, and the mouse is still pressed,
	   the thing we started a drag for will be gone when we release
	   the mouse button(s). avoid this. see part 2 at the end of
	   this function.
	*/

	if (op == Delete || op == Cut || op == Clear) {
		if (_drags->active ()) {
			_drags->abort ();
		}
	}

	if (op != Delete) { //"Delete" doesn't change copy/paste buf
		cut_buffer->clear ();
	}

	if (entered_marker) {

		/* cut/delete op while pointing at a marker */

		remove_marker (entered_marker);
		entered_marker = NULL;
		_drags->abort ();
		return;
	}

	switch (mouse_mode) {
	case MouseDraw:
	case MouseContent:
		begin_reversible_command (opname + ' ' + X_("MIDI"));
		cut_copy_midi (op);
		commit_reversible_command ();
		return;
	default:
		break;
	}

	bool did_edit = false;

	if (!selection->regions.empty() || !selection->points.empty()) {
		begin_reversible_command (opname + ' ' + _("objects"));
		did_edit = true;

		if (!selection->regions.empty()) {
			cut_copy_regions (op, selection->regions);

			if (op == Cut || op == Delete) {
				selection->clear_regions ();
			}
		}

		if (!selection->points.empty()) {
			cut_copy_points (op, timepos_t (Temporal::AudioTime));

			if (op == Cut || op == Delete) {
				selection->clear_points ();
			}
		}
	} else if (selection->time.empty()) {
		timepos_t start, end;
		/* no time selection, see if we can get an edit range
		   and use that.
		*/
		if (get_edit_op_range (start, end)) {
			selection->set (start, end);
		}
	} else if (!selection->time.empty()) {
		begin_reversible_command (opname + ' ' + _("range"));

		did_edit = true;
		cut_copy_ranges (op);

		if (op == Cut || op == Delete) {
			selection->clear_time ();
		}
	}

	if (did_edit) {
		/* reset repeated paste state */
		paste_count    = 0;
		last_paste_pos = timepos_t::max (Temporal::AudioTime);
		commit_reversible_command ();
	}

	if (op == Delete || op == Cut || op == Clear) {
		_drags->abort ();
	}
}


struct AutomationRecord {
	AutomationRecord () : state (0) , line(NULL) {}
	AutomationRecord (XMLNode* s, const AutomationLine* l) : state (s) , line (l) {}

	XMLNode* state; ///< state before any operation
	const AutomationLine* line; ///< line this came from
	boost::shared_ptr<Evoral::ControlList> copy; ///< copied events for the cut buffer
};

struct PointsSelectionPositionSorter {
	bool operator() (ControlPoint* a, ControlPoint* b) {
		return (*(a->model()))->when < (*(b->model()))->when;
	}
};

/** Cut, copy or clear selected automation points.
 *  @param op Operation (Cut, Copy or Clear)
 */
void
Editor::cut_copy_points (Editing::CutCopyOp op, timepos_t const & earliest_time)
{
	if (selection->points.empty ()) {
		return;
	}

	timepos_t earliest (earliest_time);

	/* XXX: not ideal, as there may be more than one track involved in the point selection */
	_last_cut_copy_source_track = &selection->points.front()->line().trackview;

	/* Keep a record of the AutomationLists that we end up using in this operation */
	typedef std::map<boost::shared_ptr<AutomationList>, AutomationRecord> Lists;
	Lists lists;

	/* user could select points in any order */
	selection->points.sort(PointsSelectionPositionSorter ());

	/* Go through all selected points, making an AutomationRecord for each distinct AutomationList */
	for (PointSelection::iterator sel_point = selection->points.begin(); sel_point != selection->points.end(); ++sel_point) {
		const AutomationLine&                   line = (*sel_point)->line();
		const boost::shared_ptr<AutomationList> al   = line.the_list();
		if (lists.find (al) == lists.end ()) {
			/* We haven't seen this list yet, so make a record for it.  This includes
			   taking a copy of its current state, in case this is needed for undo later.
			*/
			lists[al] = AutomationRecord (&al->get_state (), &line);
		}
	}

	if (op == Cut || op == Copy) {
		/* This operation will involve putting things in the cut buffer, so create an empty
		   ControlList for each of our source lists to put the cut buffer data in.
		*/
		for (Lists::iterator i = lists.begin(); i != lists.end(); ++i) {
			i->second.copy = i->first->create (i->first->parameter (), i->first->descriptor(), i->first->time_domain());
		}

		/* Add all selected points to the relevant copy ControlLists */

		for (PointSelection::iterator sel_point = selection->points.begin(); sel_point != selection->points.end(); ++sel_point) {
			boost::shared_ptr<AutomationList>    al = (*sel_point)->line().the_list();
			AutomationList::const_iterator ctrl_evt = (*sel_point)->model ();

			lists[al].copy->fast_simple_add ((*ctrl_evt)->when, (*ctrl_evt)->value);
			earliest = std::min (earliest, (*ctrl_evt)->when);
		}

		/* Snap start time backwards, so copy/paste is snap aligned. */

		if (earliest != timepos_t::max (earliest.time_domain())) {
			if (earliest.time_domain() == Temporal::BeatTime) {
				/* always just round down to beat */
				earliest = timepos_t (earliest.beats().round_down_to_beat());
			} else {
				/* do actual snap */
				snap_to (earliest, Temporal::RoundDownMaybe);
			}
		}

		for (Lists::iterator i = lists.begin(); i != lists.end(); ++i) {
			/* Correct this copy list so that it is relative to the earliest
			   start time, so relative ordering between points is preserved
			   when copying from several lists and the paste starts at the
			   earliest copied piece of data.
			*/
			boost::shared_ptr<Evoral::ControlList> &al_cpy = i->second.copy;
			for (AutomationList::iterator ctrl_evt = al_cpy->begin(); ctrl_evt != al_cpy->end(); ++ctrl_evt) {
				(*ctrl_evt)->when.shift_earlier (earliest);
			}

			/* And add it to the cut buffer */
			cut_buffer->add (al_cpy);
		}
	}

	if (op == Delete || op == Cut) {
		/* This operation needs to remove things from the main AutomationList, so do that now */

		for (Lists::iterator i = lists.begin(); i != lists.end(); ++i) {
			i->first->freeze ();
		}

		/* Remove each selected point from its AutomationList */
		for (PointSelection::iterator sel_point = selection->points.begin(); sel_point != selection->points.end(); ++sel_point) {
			AutomationLine& line = (*sel_point)->line ();
			boost::shared_ptr<AutomationList> al = line.the_list();

			bool erase = true;

			if (dynamic_cast<AudioRegionGainLine*> (&line)) {
				/* removing of first and last gain point in region gain lines is prohibited*/
				if (line.is_last_point (*(*sel_point)) || line.is_first_point (*(*sel_point))) {
					erase = false;
				}
			}

			if(erase) {
				al->erase ((*sel_point)->model ());
			}
		}

		/* Thaw the lists and add undo records for them */
		for (Lists::iterator i = lists.begin(); i != lists.end(); ++i) {
			boost::shared_ptr<AutomationList> al = i->first;
			al->thaw ();
			_session->add_command (new MementoCommand<AutomationList> (*al.get(), i->second.state, &(al->get_state ())));
		}
	}
}

/** Cut, copy or clear selected automation points.
 * @param op Operation (Cut, Copy or Clear)
 */
void
Editor::cut_copy_midi (CutCopyOp op)
{
	Temporal::Beats earliest = timepos_t::max (Temporal::BeatTime).beats ();

	MidiRegionSelection ms = selection->midi_regions ();
	cerr << "CCM, mrv = " << ms.size() << endl;

	for (MidiRegionSelection::iterator i = ms.begin(); i != ms.end(); ++i) {

		MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (*i);

		if (!mrv->selection().empty()) {
			earliest = std::min (earliest, (*mrv->selection().begin())->note()->time());
		}
		mrv->cut_copy_clear (op);

		/* XXX: not ideal, as there may be more than one track involved in the selection */
		_last_cut_copy_source_track = &mrv->get_time_axis_view();
	}

	if (!selection->points.empty()) {
		cut_copy_points (op, timepos_t (earliest));
		if (op == Cut || op == Delete) {
			selection->clear_points ();
		}
	}
}

struct lt_playlist {
	bool operator () (const PlaylistState& a, const PlaylistState& b) {
		return a.playlist < b.playlist;
	}
};

struct PlaylistMapping {
	TimeAxisView* tv;
	boost::shared_ptr<Playlist> pl;

	PlaylistMapping (TimeAxisView* tvp) : tv (tvp) {}
};

/** Remove `clicked_regionview' */
void
Editor::remove_clicked_region ()
{
	if (clicked_routeview == 0 || clicked_regionview == 0) {
		return;
	}

	begin_reversible_command (_("remove region"));

	boost::shared_ptr<Playlist> playlist = clicked_routeview->playlist();
	boost::shared_ptr<Region> region = clicked_regionview->region();

	playlist->clear_changes ();
	playlist->clear_owned_changes ();
	playlist->remove_region (region);

	if (should_ripple()) {
		do_ripple (playlist, region->position(), - region->length(), boost::shared_ptr<Region>(), true);
	} else {
		playlist->rdiff_and_add_command (_session);
	}

	commit_reversible_command ();
}


void
Editor::recover_regions (ARDOUR::RegionList regions)
{
#ifdef RECOVER_REGIONS_IS_WORKING
	begin_reversible_command (_("recover regions"));

	for (RegionList::iterator i = regions.begin(); i != regions.end(); ++i) {
		boost::shared_ptr<ARDOUR::Source> source = (*i)->source();

		RouteList routes = _session->get_routelist();
		for (RouteList::iterator it = routes.begin(); it != routes.end(); ++it) {
			boost::shared_ptr<ARDOUR::Track> track = boost::dynamic_pointer_cast<Track>(*it);
			if (track) {
				//ToDo
				if (source->captured_for() == track->) {
					//_session->add_command(new StatefulDiffCommand (playlist));
				}
			}
		}
	}

	commit_reversible_command ();
#endif
}


/** This is an editor Action, called with no arguments */
void
Editor::remove_selected_regions ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	remove_regions (rs, true /*can_ripple*/, false /*as_part_of_other_command*/);
}

/** Remove region(s) from their associated playlists */
void
Editor::remove_regions (const RegionSelection& sel, bool can_ripple, bool as_part_of_other_command)
{
	if (!_session || sel.empty()) {
		return;
	}

	/* make a local copy */
	RegionSelection rs = sel;

	list<boost::shared_ptr<Region> > regions_to_remove;

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		// we can't just remove the region(s) in this loop because
		// this removes them from the RegionSelection, and they thus
		// disappear from underneath the iterator, and the ++i above
		// SEGVs in a puzzling fashion.

		// so, first iterate over the regions to be removed from rs and
		// add them to the regions_to_remove list, and then
		// iterate over the list to actually remove them.

		regions_to_remove.push_back ((*i)->region());
	}

	vector<boost::shared_ptr<Playlist> > playlists;

	for (list<boost::shared_ptr<Region> >::iterator rl = regions_to_remove.begin(); rl != regions_to_remove.end(); ++rl) {

		boost::shared_ptr<Playlist> playlist = (*rl)->playlist();

		if (!playlist) {
			// is this check necessary?
			continue;
		}

		/* get_regions_from_selection_and_entered() guarantees that
		   the playlists involved are unique, so there is no need
		   to check here.
		*/

		playlists.push_back (playlist);

		playlist->clear_changes ();
		playlist->clear_owned_changes ();
		playlist->freeze ();
		playlist->remove_region (*rl);

		if (can_ripple && should_ripple()) {
			do_ripple (playlist, (*rl)->position(), -(*rl)->length(), boost::shared_ptr<Region>(), false);
		}
	}

	vector<boost::shared_ptr<Playlist> >::iterator pl;
	bool in_command = false;

	for (pl = playlists.begin(); pl != playlists.end(); ++pl) {
		(*pl)->thaw ();

		/* We might have removed regions, which alters other regions' layering_index,
		   so we need to do a recursive diff here.
		*/

		if (!in_command && !as_part_of_other_command) {
			begin_reversible_command (_("remove region"));
			in_command = true;
		}
		vector<Command*> cmds;
		(*pl)->rdiff (cmds);
		_session->add_commands (cmds);

		_session->add_command(new StatefulDiffCommand (*pl));
	}

	if (in_command && !as_part_of_other_command) {
		commit_reversible_command ();
	}
}

/** Cut, copy or clear selected regions.
 * @param op Operation (Cut, Copy or Clear)
 */
void
Editor::cut_copy_regions (CutCopyOp op, RegionSelection& rs)
{
	/* we can't use a std::map here because the ordering is important, and we can't trivially sort
	   a map when we want ordered access to both elements. i think.
	*/

	vector<PlaylistMapping> pmap;

	timepos_t first_position = timepos_t::max (Temporal::AudioTime);

	typedef set<boost::shared_ptr<Playlist> > FreezeList;
	FreezeList freezelist;

	/* get ordering correct before we cut/copy */

	rs.sort_by_position_and_track ();

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {

		first_position = min ((*x)->region()->position(), first_position);

		if (op == Cut || op == Clear || op == Delete) {
			boost::shared_ptr<Playlist> pl = (*x)->region()->playlist();

			if (pl) {
				FreezeList::iterator fl;

				// only take state if this is a new playlist.
				for (fl = freezelist.begin(); fl != freezelist.end(); ++fl) {
					if ((*fl) == pl) {
						break;
					}
				}

				if (fl == freezelist.end()) {
					pl->clear_changes();
					pl->clear_owned_changes ();
					pl->freeze ();
					freezelist.insert (pl);
				}
			}
		}

		TimeAxisView* tv = &(*x)->get_time_axis_view();
		vector<PlaylistMapping>::iterator z;

		for (z = pmap.begin(); z != pmap.end(); ++z) {
			if ((*z).tv == tv) {
				break;
			}
		}

		if (z == pmap.end()) {
			pmap.push_back (PlaylistMapping (tv));
		}
	}

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ) {

		boost::shared_ptr<Playlist> pl = (*x)->region()->playlist();

		if (!pl) {
			/* region not yet associated with a playlist (e.g. unfinished
			   capture pass.
			*/
			++x;
			continue;
		}

		TimeAxisView& tv = (*x)->get_time_axis_view();
		boost::shared_ptr<Playlist> npl;
		RegionSelection::iterator tmp;

		tmp = x;
		++tmp;

		if (op != Delete) {

			vector<PlaylistMapping>::iterator z;

			for (z = pmap.begin(); z != pmap.end(); ++z) {
				if ((*z).tv == &tv) {
					break;
				}
			}

			assert (z != pmap.end());

			if (!(*z).pl) {
				npl = PlaylistFactory::create (pl->data_type(), *_session, "cutlist", true);
				npl->freeze();
				(*z).pl = npl;
			} else {
				npl = (*z).pl;
			}
		}

		boost::shared_ptr<Region> r = (*x)->region();
		boost::shared_ptr<Region> _xx;

		assert (r != 0);

		switch (op) {
		case Delete:
			pl->remove_region (r);
			if (should_ripple()) {
				do_ripple (pl, r->position(), -r->length(), boost::shared_ptr<Region>(), false);
			}
			break;

		case Cut:
			_xx = RegionFactory::create (r, false);
			npl->add_region (_xx, timepos_t (first_position.distance (r->position())));
			pl->remove_region (r);
			if (should_ripple()) {
				do_ripple (pl, r->position(), -r->length(), boost::shared_ptr<Region>(), false);
			}
			break;

		case Copy:
			/* copy region before adding, so we're not putting same object into two different playlists */
			npl->add_region (RegionFactory::create (r, false), timepos_t (first_position.distance (r->position())));
			break;

		case Clear:
			pl->remove_region (r);
			if (should_ripple()) {
				do_ripple (pl, r->position(), -r->length(), boost::shared_ptr<Region>(), false);
			}
			break;
		}

		x = tmp;
	}

	if (op != Delete) {

		list<boost::shared_ptr<Playlist> > foo;

		/* the pmap is in the same order as the tracks in which selected regions occurred */

		for (vector<PlaylistMapping>::iterator i = pmap.begin(); i != pmap.end(); ++i) {
			if ((*i).pl) {
				(*i).pl->thaw();
				foo.push_back ((*i).pl);
			}
		}

		if (!foo.empty()) {
			cut_buffer->set (foo);
		}

		if (pmap.empty()) {
			_last_cut_copy_source_track = 0;
		} else {
			_last_cut_copy_source_track = pmap.front().tv;
		}
	}

	for (FreezeList::iterator pl = freezelist.begin(); pl != freezelist.end(); ++pl) {
		(*pl)->thaw ();

		/* We might have removed regions, which alters other regions' layering_index,
		   so we need to do a recursive diff here.
		*/

		(*pl)->rdiff_and_add_command (_session);
	}
}

void
Editor::cut_copy_ranges (CutCopyOp op)
{
	TrackViewList ts = selection->tracks.filter_to_unique_playlists ();

	/* Sort the track selection now, so that it if is used, the playlists
	   selected by the calls below to cut_copy_clear are in the order that
	   their tracks appear in the editor.  This makes things like paste
	   of ranges work properly.
	*/

	sort_track_selection (ts);

	if (ts.empty()) {
		if (!entered_track) {
			return;
		}
		ts.push_back (entered_track);
	}

	for (TrackViewList::iterator i = ts.begin(); i != ts.end(); ++i) {
		(*i)->cut_copy_clear (*selection, op);
	}
}

void
Editor::paste (float times, bool from_context)
{
	DEBUG_TRACE (DEBUG::CutNPaste, "paste to preferred edit pos\n");
	timepos_t where (get_preferred_edit_position (EDIT_IGNORE_NONE, from_context));
	paste_internal (where, times);
}

void
Editor::mouse_paste ()
{
	samplepos_t sample;
	bool ignored;

	if (!mouse_sample (sample, ignored)) {
		return;
	}

	timepos_t where (sample);

	snap_to (where);
	paste_internal (where, 1);
}

void
Editor::paste_internal (timepos_t const & pos, float times)
{
	timepos_t position (pos);

	DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("apparent paste position is %1\n", position));

	if (cut_buffer->empty(internal_editing())) {
		return;
	}

	if (position == timepos_t::max (position.time_domain())) {
		position = get_preferred_edit_position();
		DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("preferred edit position is %1\n", position));
	}

	if (position != last_paste_pos) {
		/* paste in new location, reset repeated paste state */
		paste_count = 0;
		last_paste_pos = position;
	}

	/* get everything in the correct order */

	TrackViewList ts;
	if (!selection->tracks.empty()) {
		/* If there is a track selection, paste into exactly those tracks and
		 * only those tracks.  This allows the user to be explicit and override
		 * the below "do the reasonable thing" logic. */
		ts = selection->tracks.filter_to_unique_playlists ();
		sort_track_selection (ts);
	} else {
		/* Figure out which track to base the paste at. */
		TimeAxisView* base_track = NULL;
		if (_edit_point == Editing::EditAtMouse && entered_track) {
			/* With the mouse edit point, paste onto the track under the mouse. */
			base_track = entered_track;
		} else if (_edit_point == Editing::EditAtMouse && entered_regionview) {
			/* With the mouse edit point, paste onto the track of the region under the mouse. */
			base_track = &entered_regionview->get_time_axis_view();
		} else if (_last_cut_copy_source_track) {
			/* Paste to the track that the cut/copy came from (see mantis #333). */
			base_track = _last_cut_copy_source_track;
		} else {
			/* This is "impossible" since we've copied... well, do nothing. */
			return;
		}

		/* Walk up to parent if necessary, so base track is a route. */
		while (base_track->get_parent()) {
			base_track = base_track->get_parent();
		}

		/* Add base track and all tracks below it.  The paste logic will select
		   the appropriate object types from the cut buffer in relative order. */
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((*i)->order() >= base_track->order()) {
				ts.push_back(*i);
			}
		}

		/* Sort tracks so the nth track of type T will pick the nth object of type T. */
		sort_track_selection (ts);

		/* Add automation children of each track in order, for pasting several lines. */
		for (TrackViewList::iterator i = ts.begin(); i != ts.end();) {
			/* Add any automation children for pasting several lines */
			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*i++);
			if (!rtv) {
				continue;
			}

			typedef RouteTimeAxisView::AutomationTracks ATracks;
			const ATracks& atracks = rtv->automation_tracks();
			for (ATracks::const_iterator a = atracks.begin(); a != atracks.end(); ++a) {
				i = ts.insert(i, a->second.get());
				++i;
			}
		}

		/* We now have a list of trackviews starting at base_track, including
		   automation children, in the order shown in the editor, e.g. R1,
		   R1.A1, R1.A2, R2, R2.A1, ... */
	}

	begin_reversible_command (Operations::paste);

	if (ts.size() == 1 && cut_buffer->lines.size() == 1 &&
	    dynamic_cast<AutomationTimeAxisView*>(ts.front())) {
	    /* Only one line copied, and one automation track selected.  Do a
	       "greedy" paste from one automation type to another. */

		PasteContext ctx(paste_count, times, ItemCounts(), true);
		ts.front()->paste (position, *cut_buffer, ctx);

	} else {

		/* Paste into tracks */

		PasteContext ctx(paste_count, times, ItemCounts(), false);
		for (TrackViewList::iterator i = ts.begin(); i != ts.end(); ++i) {
			(*i)->paste (position, *cut_buffer, ctx);
		}
	}

	++paste_count;

	commit_reversible_command ();
}

void
Editor::duplicate_regions (float times)
{
	RegionSelection rs (get_regions_from_selection_and_entered());
	duplicate_some_regions (rs, times);
}

void
Editor::duplicate_some_regions (RegionSelection& regions, float times)
{
	if (regions.empty ()) {
		return;
	}

	boost::shared_ptr<Playlist> playlist;
	std::set<boost::shared_ptr<Playlist> > playlists; // list of unique playlists affected by duplication
	RegionSelection sel = regions; // clear (below) may  clear the argument list if its the current region selection
	RegionSelection foo;

	timepos_t const start_sample = regions.start_time ();
	timepos_t const end_sample = regions.end_time ();
	timecnt_t const span = start_sample.distance (end_sample);

	begin_reversible_command (Operations::duplicate_region);

	selection->clear_regions ();

	/* ripple first so that we don't move the duplicates that will be added */

	if (should_ripple()) {

		/* convert RegionSelection into RegionList so that we can pass it to ripple and exclude the regions we will duplicate */

		RegionList exclude;

		for (RegionSelection::iterator i = sel.begin(); i != sel.end(); ++i) {
			exclude.push_back ((*i)->region());
			playlist = (*i)->region()->playlist();
			if (playlists.insert (playlist).second) {
				/* successfully inserted into set, so it's the first time we've seen this playlist */
				playlist->clear_changes ();
				playlist->clear_owned_changes ();
			}
		}

		for (set<boost::shared_ptr<Playlist> >::iterator p = playlists.begin(); p != playlists.end(); ++p) {
			do_ripple ((*p), start_sample, span * times, &exclude, false);
		}
	}

	for (RegionSelection::iterator i = sel.begin(); i != sel.end(); ++i) {

		boost::shared_ptr<Region> r ((*i)->region());

		TimeAxisView& tv = (*i)->get_time_axis_view();
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (&tv);
		latest_regionviews.clear ();
		sigc::connection c = rtv->view()->RegionViewAdded.connect (sigc::mem_fun(*this, &Editor::collect_new_region_view));

		timepos_t position = end_sample;
		playlist = (*i)->region()->playlist();

		if (!should_ripple()) {
			if (playlists.insert (playlist).second) {
				playlist->clear_changes ();
				playlist->clear_owned_changes ();
			}
		}

		playlist->duplicate (r, position, span, times);

		c.disconnect ();

		foo.insert (foo.end(), latest_regionviews.begin(), latest_regionviews.end());
	}

	for (set<boost::shared_ptr<Playlist> >::iterator p = playlists.begin(); p != playlists.end(); ++p) {
		(*p)->rdiff_and_add_command (_session);
	}

	if (!foo.empty()) {
		selection->set (foo);
	}

	commit_reversible_command ();
}

void
Editor::duplicate_selection (float times)
{
	if (selection->time.empty() || selection->tracks.empty()) {
		return;
	}

	boost::shared_ptr<Playlist> playlist;

	TrackViewList ts = selection->tracks.filter_to_unique_playlists ();

	bool in_command = false;

	for (TrackViewList::iterator i = ts.begin(); i != ts.end(); ++i) {
		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}
		playlist->clear_changes ();

		if (clicked_selection) {
			playlist->duplicate_range (selection->time[clicked_selection], times);
		} else {
			playlist->duplicate_ranges (selection->time, times);
		}

		if (!in_command) {
			begin_reversible_command (_("duplicate range selection"));
			in_command = true;
		}
		_session->add_command (new StatefulDiffCommand (playlist));

	}

	if (in_command) {
		if (times == 1.0f) {
			// now "move" range selection to after the current range selection
			timecnt_t distance;

			if (clicked_selection) {
				distance = selection->time[clicked_selection].start().distance (selection->time[clicked_selection].end());
			} else {
				distance = selection->time.start_time().distance (selection->time.end_time ());
			}

			selection->move_time (distance);
		}
		commit_reversible_command ();
	}
}

/** Reset all selected points to the relevant default value */
void
Editor::reset_point_selection ()
{
	for (PointSelection::iterator i = selection->points.begin(); i != selection->points.end(); ++i) {
		ARDOUR::AutomationList::iterator j = (*i)->model ();
		(*j)->value = (*i)->line().the_list()->descriptor ().normal;
	}
}

void
Editor::center_playhead ()
{
	float const page = _visible_canvas_width * samples_per_pixel;
	center_screen_internal (_playhead_cursor->current_sample (), page);
}

void
Editor::center_edit_point ()
{
	float const page = _visible_canvas_width * samples_per_pixel;
	center_screen_internal (get_preferred_edit_position().samples(), page);
}

/** Caller must begin and commit a reversible command */
void
Editor::clear_playlist (boost::shared_ptr<Playlist> playlist)
{
	playlist->clear_changes ();
	playlist->clear ();
	_session->add_command (new StatefulDiffCommand (playlist));
}

void
Editor::nudge_track (bool use_edit, bool forwards)
{
	boost::shared_ptr<Playlist> playlist;
	timecnt_t distance;
	timecnt_t next_distance;
	timepos_t start;

	if (use_edit) {
		start = get_preferred_edit_position();
	}

	if ((distance = get_nudge_distance (start, next_distance)) == 0) {
		return;
	}

	if (selection->tracks.empty()) {
		return;
	}

	TrackViewList ts = selection->tracks.filter_to_unique_playlists ();
	bool in_command = false;

	for (TrackViewList::iterator i = ts.begin(); i != ts.end(); ++i) {

		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}

		playlist->clear_changes ();
		playlist->clear_owned_changes ();

		playlist->nudge_after (start, distance, forwards);

		if (!in_command) {
			begin_reversible_command (_("nudge track"));
			in_command = true;
		}
		vector<Command*> cmds;

		playlist->rdiff (cmds);
		_session->add_commands (cmds);

		_session->add_command (new StatefulDiffCommand (playlist));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::remove_last_capture ()
{
	vector<string> choices;
	string prompt;

	if (!_session) {
		return;
	}

	if (Config->get_verify_remove_last_capture()) {
		prompt  = _("Do you really want to destroy the last capture?"
		            "\n(This is destructive and cannot be undone)");

		choices.push_back (_("No, do nothing."));
		choices.push_back (_("Yes, destroy it."));

		Choice prompter (_("Destroy last capture"), prompt, choices);

		if (prompter.run () == 1) {
			_session->remove_last_capture ();
			_regions->redisplay ();
		}

	} else {
		_session->remove_last_capture();
		_regions->redisplay ();
	}
}

void
Editor::tag_regions (RegionList regions)
{
	ArdourDialog d (_("Tag Last Capture"), true, false);
	Entry entry;
	Label label (_("Tag:"));
	HBox hbox;

	hbox.set_spacing (6);
	hbox.pack_start (label, false, false);
	hbox.pack_start (entry, true, true);

	d.get_vbox()->set_border_width (12);
	d.get_vbox()->pack_start (hbox, false, false);

	d.add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	d.add_button(Gtk::Stock::OK, Gtk::RESPONSE_OK);

	d.set_size_request (300, -1);

	entry.set_text (_("Good"));
	entry.select_region (0, -1);

	entry.signal_activate().connect (sigc::bind (sigc::mem_fun (d, &Dialog::response), RESPONSE_OK));

	d.show_all ();

	entry.grab_focus();

	int const ret = d.run();

	d.hide ();

	if (ret != RESPONSE_OK) {
		return;
	}

	std::string tagstr = entry.get_text();
	strip_whitespace_edges (tagstr);

	if (!tagstr.empty()) {
		for (RegionList::iterator r = regions.begin(); r != regions.end(); r++) {
			(*r)->set_tags(tagstr);
		}

		_regions->redisplay ();
	}
}

void
Editor::tag_selected_region ()
{
	std::list<boost::shared_ptr<Region> > rlist;

	RegionSelection rs = get_regions_from_selection_and_entered ();
	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); r++) {
		rlist.push_back((*r)->region());
	}

	tag_regions(rlist);
}

void
Editor::tag_last_capture ()
{
	if (!_session) {
		return;
	}

	std::list<boost::shared_ptr<Region> > rlist;

	std::list<boost::shared_ptr<Source> > srcs;
	_session->get_last_capture_sources (srcs);
	for (std::list<boost::shared_ptr<Source> >::iterator i = srcs.begin(); i != srcs.end(); ++i) {
		boost::shared_ptr<ARDOUR::Source> source = (*i);
		if (source) {

			set<boost::shared_ptr<Region> > regions;
			RegionFactory::get_regions_using_source (source, regions);
			for (set<boost::shared_ptr<Region> >::iterator r = regions.begin(); r != regions.end(); r++) {
				rlist.push_back(*r);
			}

		}
	}

	tag_regions(rlist);
}

void
Editor::normalize_region ()
{
	if (!_session) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	NormalizeDialog dialog (rs.size() > 1);

	if (dialog.run () != RESPONSE_ACCEPT) {
		return;
	}

	CursorContext::Handle cursor_ctx = CursorContext::create(*this, _cursors->wait);
	gdk_flush ();

	/* XXX: should really only count audio regions here */
	int const regions = rs.size ();

	/* Make a list of the selected audio regions' maximum amplitudes, and also
	   obtain the maximum amplitude of them all.
	*/
	list<double> max_amps;
	list<double> rms_vals;
	list<float>  dbtp_vals;
	list<float>  lufs_vals;

	double max_amp   = 0;
	double max_rms   = 0;
	double max_tp    = 0;
	float max_lufs_i = -200;

	bool use_rms  = dialog.constrain_rms ();
	bool use_lufs = dialog.constrain_lufs ();
	bool use_dbtp = dialog.use_true_peak ();

	for (RegionSelection::const_iterator i = rs.begin(); i != rs.end(); ++i) {
		AudioRegionView const * arv = dynamic_cast<AudioRegionView const *> (*i);
		if (!arv) {
			continue;
		}
		dialog.descend (1.0 / regions);

		double const a = arv->audio_region()->maximum_amplitude (&dialog);
		if (use_rms) {
			double r = arv->audio_region()->rms (&dialog);
			max_rms = max (max_rms, r);
			rms_vals.push_back (r);
		}

		if ((use_dbtp || use_lufs) && !dialog.cancelled ()) {
			float true_peak, integrated, max_short, max_momentary;
			arv->audio_region()->loudness (true_peak, integrated, max_short, max_momentary, &dialog);
			float lufs = integrated;
			if (lufs == -200) {
				lufs = max_short;
			}
			if (lufs == -200) {
				lufs = max_momentary;
			}
			max_tp     = max<double> (max_tp, true_peak);
			max_lufs_i = max (max_lufs_i, lufs);
			dbtp_vals.push_back (true_peak);
			lufs_vals.push_back (lufs);
		}

		if (a == -1 || dialog.cancelled ()) {
			/* the user cancelled the operation */
			return;
		}

		max_amps.push_back (a);
		max_amp = max (max_amp, a);
		dialog.ascend ();
	}

	list<double>::const_iterator a = max_amps.begin ();
	list<double>::const_iterator l = rms_vals.begin ();
	list<float>::const_iterator  t = dbtp_vals.begin ();
	list<float>::const_iterator  i = lufs_vals.begin ();
	bool in_command = false;

	max_tp = max (max_tp, max_amp);

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*> (*r);
		if (!arv) {
			continue;
		}

		arv->region()->clear_changes ();
		double target = dialog.target_peak (); // dB

		double amp;
		if (use_dbtp) {
			amp = dialog.normalize_individually() ? *t : max_tp;
		} else {
			amp = dialog.normalize_individually() ? *a : max_amp;
		}

		if (use_rms) {
			double const amp_rms = dialog.normalize_individually() ? *l : max_rms;
			const double t_rms  = dialog.target_rms ();
			const gain_t c_peak = dB_to_coefficient (target);
			const gain_t c_rms  = dB_to_coefficient (t_rms);
			assert (c_peak >= GAIN_COEFF_SMALL && c_rms > GAIN_COEFF_SMALL);
			if ((amp_rms / c_rms) > (amp / c_peak)) {
				amp = amp_rms;
				target = t_rms;
			}
		}

		if (use_lufs) {
			double const tg_lufs = dialog.target_lufs ();
			double const db_lufs = dialog.normalize_individually() ? *i : max_lufs_i; // dB
			const gain_t ct_lufs = dB_to_coefficient (tg_lufs);
			const gain_t cv_lufs = dB_to_coefficient (db_lufs);
			const gain_t c_tgt   = dB_to_coefficient (target);

			if (db_lufs > -200 && (cv_lufs / ct_lufs) > (amp / c_tgt)) {
				amp = cv_lufs;
				target = tg_lufs;
			}
		}

		arv->audio_region()->normalize (amp, target);

		if (!in_command) {
			begin_reversible_command (_("normalize"));
			in_command = true;
		}
		_session->add_command (new StatefulDiffCommand (arv->region()));

		++a;
		++l;
		++i;
		++t;
	}

	if (in_command) {
		commit_reversible_command ();
	}
}


void
Editor::reset_region_scale_amplitude ()
{
	if (!_session) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	bool in_command = false;

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (!arv)
			continue;
		arv->region()->clear_changes ();
		arv->audio_region()->set_scale_amplitude (1.0f);

		if(!in_command) {
				begin_reversible_command ("reset gain");
				in_command = true;
		}
		_session->add_command (new StatefulDiffCommand (arv->region()));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::adjust_region_gain (bool up)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	bool in_command = false;
	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (arv) {
			arv->region()->playlist()->freeze ();
		}
	}

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (!arv) {
			continue;
		}

		arv->region()->clear_changes ();

		double dB = accurate_coefficient_to_dB (arv->audio_region()->scale_amplitude ());

		if (up) {
			dB += 1;
		} else {
			dB -= 1;
		}

		arv->audio_region()->set_scale_amplitude (dB_to_coefficient (dB));

		if (!in_command) {
				begin_reversible_command ("adjust region gain");
				in_command = true;
		}
		_session->add_command (new StatefulDiffCommand (arv->region()));
	}

	if (in_command) {
		commit_reversible_command ();
	}

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (arv) {
			arv->region()->playlist()->thaw ();
		}
	}
}

void
Editor::reset_region_gain ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	bool in_command = false;

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (!arv) {
			continue;
		}

		arv->region()->clear_changes ();

		arv->audio_region()->set_scale_amplitude (1.0f);

		if (!in_command) {
				begin_reversible_command ("reset region gain");
				in_command = true;
		}
		_session->add_command (new StatefulDiffCommand (arv->region()));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::reverse_region ()
{
	if (!_session) {
		return;
	}

	Reverse rev (*_session);
	apply_filter (rev, _("reverse regions"));
}

void
Editor::strip_region_silence ()
{
	if (!_session) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	std::list<RegionView*> audio_only;

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*> (*i);
		if (arv) {
			audio_only.push_back (arv);
		}
	}

	assert (!audio_only.empty());

	StripSilenceDialog d (_session, audio_only);
	int const r = d.run ();

	d.drop_rects ();

	if (r == Gtk::RESPONSE_OK) {
		ARDOUR::AudioIntervalMap silences;
		d.silences (silences);
		StripSilence s (*_session, silences, d.fade_length());

		apply_filter (s, _("strip silence"), &d);
	}
}

Command*
Editor::apply_midi_note_edit_op_to_region (MidiOperator& op, MidiRegionView& mrv)
{
	Evoral::Sequence<Temporal::Beats>::Notes selected;
	mrv.selection_as_notelist (selected, true);

	vector<Evoral::Sequence<Temporal::Beats>::Notes> v;
	v.push_back (selected);

	timepos_t pos = mrv.midi_region()->source_position();

	return op (mrv.midi_region()->model(), pos.beats(), v);
}

void
Editor::apply_midi_note_edit_op (MidiOperator& op, const RegionSelection& rs)
{
	if (rs.empty()) {
		return;
	}

	bool in_command = false;

	vector<MidiRegionView*> views = filter_to_unique_midi_region_views (rs);

	for (vector<MidiRegionView*>::iterator mrv = views.begin(); mrv != views.end(); ++mrv) {

		Command* cmd = apply_midi_note_edit_op_to_region (op, **mrv);
		if (cmd) {
			if (!in_command) {
				begin_reversible_command (op.name ());
				in_command = true;
			}
			(*cmd)();
			_session->add_command (cmd);
			}
	}

	if (in_command) {
		commit_reversible_command ();
		_session->set_dirty ();
	}
}

void
Editor::fork_region ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	CursorContext::Handle cursor_ctx = CursorContext::create(*this, _cursors->wait);
	bool in_command = false;

	gdk_flush ();

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ) {
		RegionSelection::iterator tmp = r;
		++tmp;

		MidiRegionView* const mrv = dynamic_cast<MidiRegionView*>(*r);

		if (mrv) {
			try {
				boost::shared_ptr<Playlist> playlist = mrv->region()->playlist();
				boost::shared_ptr<MidiSource> new_source = _session->create_midi_source_by_stealing_name (mrv->midi_view()->track());
				boost::shared_ptr<MidiRegion> newregion = mrv->midi_region()->clone (new_source);

				if (!in_command) {
					begin_reversible_command (_("Fork Region(s)"));
					in_command = true;
				}
				playlist->clear_changes ();
				playlist->replace_region (mrv->region(), newregion, mrv->region()->position());
				_session->add_command(new StatefulDiffCommand (playlist));
			} catch (...) {
				error << string_compose (_("Could not unlink %1"), mrv->region()->name()) << endmsg;
			}
		}

		r = tmp;
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::quantize_region ()
{
	if (_session) {
		quantize_regions(get_regions_from_selection_and_entered ());
	}
}

void
Editor::quantize_regions (const RegionSelection& rs)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	if (!quantize_dialog) {
		quantize_dialog = new QuantizeDialog (*this);
	}

	if (quantize_dialog->is_mapped()) {
		/* in progress already */
		return;
	}

	quantize_dialog->present ();
	const int r = quantize_dialog->run ();
	quantize_dialog->hide ();

	if (r == Gtk::RESPONSE_OK) {
		Quantize quant (quantize_dialog->snap_start(),
		                quantize_dialog->snap_end(),
				quantize_dialog->start_grid_size(),
		                quantize_dialog->end_grid_size(),
				quantize_dialog->strength(),
		                quantize_dialog->swing(),
		                quantize_dialog->threshold());

		apply_midi_note_edit_op (quant, rs);
	}
}

void
Editor::legatize_region (bool shrink_only)
{
	if (_session) {
		legatize_regions(get_regions_from_selection_and_entered (), shrink_only);
	}
}

void
Editor::deinterlace_midi_regions (const RegionSelection& rs)
{
	begin_reversible_command (_("de-interlace midi"));

	RegionSelection rcopy = rs;
	if (_session) {

		for (RegionSelection::iterator i = rcopy.begin (); i != rcopy.end(); i++) {
			MidiRegionView* const mrv = dynamic_cast<MidiRegionView*> (*i);
			if (mrv) {
				XMLNode& before (mrv->region()->playlist()->get_state());

				/* pass the regions to deinterlace_midi_region*/
				_session->deinterlace_midi_region(mrv->midi_region());

				XMLNode& after (mrv->region()->playlist()->get_state());
				_session->add_command (new MementoCommand<Playlist>(*(mrv->region()->playlist()), &before, &after));
			}
		}
	}

	/* Remove the original region(s) safely, without rippling, as part of this command */
	remove_regions(rs, false /*can_ripple*/, true /*as_part_of_other_command*/);

	commit_reversible_command ();
}

void
Editor::deinterlace_selected_midi_regions ()
{
	if (_session) {
		RegionSelection rs = get_regions_from_selection_and_entered ();
		deinterlace_midi_regions(rs);
	}
}

void
Editor::legatize_regions (const RegionSelection& rs, bool shrink_only)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	Legatize legatize(shrink_only);
	apply_midi_note_edit_op (legatize, rs);
}

void
Editor::transform_region ()
{
	if (_session) {
		transform_regions(get_regions_from_selection_and_entered ());
	}
}

void
Editor::transform_regions (const RegionSelection& rs)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	TransformDialog td;

	td.present();
	const int r = td.run();
	td.hide();

	if (r == Gtk::RESPONSE_OK) {
		Transform transform(td.get());
		apply_midi_note_edit_op(transform, rs);
	}
}

void
Editor::transpose_region ()
{
	if (_session) {
		transpose_regions(get_regions_from_selection_and_entered ());
	}
}

void
Editor::transpose_regions (const RegionSelection& rs)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	TransposeDialog d;
	int const r = d.run ();

	if (r == RESPONSE_ACCEPT) {
		Transpose transpose(d.semitones ());
		apply_midi_note_edit_op (transpose, rs);
	}
}

void
Editor::insert_patch_change (bool from_context)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty ()) {
		return;
	}

	const timepos_t p = get_preferred_edit_position (EDIT_IGNORE_NONE, from_context);

	/* XXX: bit of a hack; use the MIDNAM from the first selected region;
	   there may be more than one, but the PatchChangeDialog can only offer
	   one set of patch menus.
	*/
	MidiRegionView* first = dynamic_cast<MidiRegionView*> (rs.front ());

	Evoral::PatchChange<Temporal::Beats> empty (Temporal::Beats(), 0, 0, 0);
	PatchChangeDialog d (_session, empty, first->instrument_info(), Gtk::Stock::ADD);

	switch (d.run()) {
		case Gtk::RESPONSE_ACCEPT:
			break;
		default:
			return;
	}

	for (RegionSelection::iterator i = rs.begin (); i != rs.end(); ++i) {
		MidiRegionView* const mrv = dynamic_cast<MidiRegionView*> (*i);
		if (mrv) {
			if (p >= mrv->region()->position() && p <= mrv->region()->nt_last()) {
				mrv->add_patch_change (mrv->region()->position().distance (p), d.patch ());
			}
		}
	}
}

void
Editor::apply_filter (Filter& filter, string command, ProgressReporter* progress)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	CursorContext::Handle cursor_ctx = CursorContext::create(*this, _cursors->wait);
	bool in_command = false;

	gdk_flush ();

	int n = 0;
	int const N = rs.size ();

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ) {
		RegionSelection::iterator tmp = r;
		++tmp;

		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (arv) {
			boost::shared_ptr<Playlist> playlist = arv->region()->playlist();

			if (progress) {
				progress->descend (1.0 / N);
			}

			if (arv->audio_region()->apply (filter, progress) == 0) {

				playlist->clear_changes ();
				playlist->clear_owned_changes ();

				if (!in_command) {
					begin_reversible_command (command);
					in_command = true;
				}

				if (filter.results.empty ()) {

					/* no regions returned; remove the old one */
					playlist->remove_region (arv->region ());

				} else {

					std::vector<boost::shared_ptr<Region> >::iterator res = filter.results.begin ();

					/* first region replaces the old one */
					playlist->replace_region (arv->region(), *res, (*res)->position());
					++res;

					/* add the rest */
					while (res != filter.results.end()) {
						playlist->add_region (*res, (*res)->position());
						++res;
					}

				}

				/* We might have removed regions, which alters other regions' layering_index,
				   so we need to do a recursive diff here.
				*/
				vector<Command*> cmds;
				playlist->rdiff (cmds);
				_session->add_commands (cmds);

				_session->add_command(new StatefulDiffCommand (playlist));
			}

			if (progress) {
				progress->ascend ();
			}
		}

		r = tmp;
		++n;
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::external_edit_region ()
{
	/* more to come */
}

void
Editor::reset_region_gain_envelopes ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	bool in_command = false;

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			boost::shared_ptr<AutomationList> alist (arv->audio_region()->envelope());
			XMLNode& before (alist->get_state());

			arv->audio_region()->set_default_envelope ();

			if (!in_command) {
				begin_reversible_command (_("reset region gain"));
				in_command = true;
			}
			_session->add_command (new MementoCommand<AutomationList>(*arv->audio_region()->envelope().get(), &before, &alist->get_state()));
		}
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::set_region_gain_visibility (RegionView* rv)
{
	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (rv);
	if (arv) {
		arv->update_envelope_visibility();
	}
}

void
Editor::set_gain_envelope_visibility ()
{
	if (!_session) {
		return;
	}

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		AudioTimeAxisView* v = dynamic_cast<AudioTimeAxisView*>(*i);
		if (v) {
			v->audio_view()->foreach_regionview (sigc::mem_fun (this, &Editor::set_region_gain_visibility));
		}
	}
}

void
Editor::toggle_gain_envelope_active ()
{
	if (_ignore_region_action) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	bool in_command = false;

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			arv->region()->clear_changes ();
			arv->audio_region()->set_envelope_active (!arv->audio_region()->envelope_active());

			if (!in_command) {
				begin_reversible_command (_("region gain envelope active"));
				in_command = true;
			}
			_session->add_command (new StatefulDiffCommand (arv->region()));
		}
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::toggle_region_lock ()
{
	if (_ignore_region_action) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	begin_reversible_command (_("toggle region lock"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		(*i)->region()->clear_changes ();
		(*i)->region()->set_locked (!(*i)->region()->locked());
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}

	commit_reversible_command ();
}

void
Editor::toggle_region_video_lock ()
{
	if (_ignore_region_action) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	begin_reversible_command (_("Toggle Video Lock"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		(*i)->region()->clear_changes ();
		(*i)->region()->set_video_locked (!(*i)->region()->video_locked());
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}

	commit_reversible_command ();
}

void
Editor::toggle_region_lock_style ()
{
	if (_ignore_region_action) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	Glib::RefPtr<ToggleAction> a = Glib::RefPtr<ToggleAction>::cast_dynamic (_region_actions->get_action("toggle-region-lock-style"));
	vector<Widget*> proxies = a->get_proxies();
	Gtk::CheckMenuItem* cmi = dynamic_cast<Gtk::CheckMenuItem*> (proxies.front());

	assert (cmi);

	begin_reversible_command (_("toggle region lock style"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		(*i)->region()->clear_changes ();
		Temporal::TimeDomain const td = ((*i)->region()->position_time_domain() == Temporal::AudioTime && !cmi->get_inconsistent()) ? Temporal::BeatTime : Temporal::AudioTime;
		(*i)->region()->set_position_time_domain (td);
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}

	commit_reversible_command ();
}

void
Editor::toggle_opaque_region ()
{
	if (_ignore_region_action) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	begin_reversible_command (_("change region opacity"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		(*i)->region()->clear_changes ();
		(*i)->region()->set_opaque (!(*i)->region()->opaque());
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}

	commit_reversible_command ();
}

void
Editor::toggle_record_enable ()
{
	bool new_state = false;
	bool first = true;
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		RouteTimeAxisView *rtav = dynamic_cast<RouteTimeAxisView *>(*i);
		if (!rtav)
			continue;
		if (!rtav->is_track())
			continue;

		if (first) {
			new_state = !rtav->track()->rec_enable_control()->get_value();
			first = false;
		}

		rtav->track()->rec_enable_control()->set_value (new_state, Controllable::UseGroup);
	}
}

StripableList
tracklist_to_stripables (TrackViewList list)
{
	StripableList ret;

	for (TrackSelection::iterator i = list.begin(); i != list.end(); ++i) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> ((*i));

		if (rtv && rtv->is_track()) {
			ret.push_back (rtv->track());
		}
	}

	return ret;
}

void
Editor::play_solo_selection (bool restart)
{
	//note: session::solo_selection takes care of invalidating the region playlist

	if ((!selection->tracks.empty()) && selection->time.length() > 0) {  //a range is selected; solo the tracks and roll

		StripableList sl = tracklist_to_stripables (selection->tracks);
		_session->solo_selection (sl, true);

		if (restart) {
			samplepos_t start = selection->time.start_sample ();
			samplepos_t end = selection->time.end_sample ();
			_session->request_bounded_roll (start, end);
		}
	} else if (! selection->tracks.empty()) {  //no range is selected, but tracks are selected; solo the tracks and roll
		StripableList sl = tracklist_to_stripables (selection->tracks);
		_session->solo_selection (sl, true);
		_session->request_cancel_play_range();
		transition_to_rolling (true);

	} else if (! selection->triggers.empty()) {  //a Trigger is selected, so we solo its parent Stripable
		TriggerSelection ts = selection->triggers;
		TriggerEntry* entry = *ts.begin();
		TriggerPtr slot = entry->trigger();
		ARDOUR::SessionObject *obj = slot->box().owner();
		boost::shared_ptr<Stripable> shared_strip = _session->stripable_by_id(obj->id());
		StripableList sl;  sl.push_back(boost::shared_ptr<Stripable>(shared_strip));
		_session->solo_selection (sl, true);
		_session->request_cancel_play_range();
		slot->bang();  //ToDo:  how will this work with Gate+Repeat ?
	} else if (! selection->regions.empty()) {  //solo any tracks with selected regions, and roll
		StripableList sl = tracklist_to_stripables (get_tracks_for_range_action());
		_session->solo_selection (sl, true);
		_session->request_cancel_play_range();
		transition_to_rolling (true);
	} else {
		_session->request_cancel_play_range();
		transition_to_rolling (true);  //no selection.  just roll.
	}
}

void
Editor::toggle_solo ()
{
	bool new_state = false;
	bool first = true;
	boost::shared_ptr<ControlList> cl (new ControlList);

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		StripableTimeAxisView *stav = dynamic_cast<StripableTimeAxisView *>(*i);

		if (!stav || !stav->stripable()->solo_control()) {
			continue;
		}

		if (first) {
			new_state = !stav->stripable()->solo_control()->soloed ();
			first = false;
		}

		cl->push_back (stav->stripable()->solo_control());
	}

	_session->set_controls (cl, new_state ? 1.0 : 0.0, Controllable::UseGroup);
}

void
Editor::toggle_mute ()
{
	bool new_state = false;
	bool first = true;
	boost::shared_ptr<ControlList> cl (new ControlList);

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		StripableTimeAxisView *stav = dynamic_cast<StripableTimeAxisView *>(*i);

		if (!stav || !stav->stripable()->mute_control()) {
			continue;
		}

		if (first) {
			new_state = !stav->stripable()->mute_control()->muted();
			first = false;
		}

		boost::shared_ptr<MuteControl> mc = stav->stripable()->mute_control();
		cl->push_back (mc);
		mc->start_touch (timepos_t (_session->audible_sample ()));
	}

	_session->set_controls (cl, new_state, Controllable::UseGroup);
}

void
Editor::toggle_solo_isolate ()
{
}


void
Editor::fade_range ()
{
	TrackViewList ts = selection->tracks.filter_to_unique_playlists ();

	begin_reversible_command (_("fade range"));

	for (TrackViewList::iterator i = ts.begin(); i != ts.end(); ++i) {
		(*i)->fade_range (selection->time);
	}

	commit_reversible_command ();
}


void
Editor::set_fade_length (bool in)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	/* we need a region to measure the offset from the start */

	RegionView* rv = rs.front ();

	timepos_t pos = get_preferred_edit_position();
	timecnt_t len;
	char const * cmd;

	if (pos > rv->region()->last_sample() || pos < rv->region()->first_sample()) {
		/* edit point is outside the relevant region */
		return;
	}

	if (in) {
		if (pos <= rv->region()->position()) {
			/* can't do it */
			return;
		}
		len = rv->region()->position().distance (pos);
		cmd = _("set fade in length");
	} else {
		if (pos >= rv->region()->last_sample()) {
			/* can't do it */
			return;
		}
		len = pos.distance (rv->region()->nt_last());
		cmd = _("set fade out length");
	}

	bool in_command = false;

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			continue;
		}

		boost::shared_ptr<AutomationList> alist;
		if (in) {
			alist = tmp->audio_region()->fade_in();
		} else {
			alist = tmp->audio_region()->fade_out();
		}

		XMLNode &before = alist->get_state();

		if (in) {
			tmp->audio_region()->set_fade_in_length (len.samples());
			tmp->audio_region()->set_fade_in_active (true);
		} else {
			tmp->audio_region()->set_fade_out_length (len.samples());
			tmp->audio_region()->set_fade_out_active (true);
		}

		if (!in_command) {
			begin_reversible_command (cmd);
			in_command = true;
		}
		XMLNode &after = alist->get_state();
		_session->add_command(new MementoCommand<AutomationList>(*alist, &before, &after));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::set_fade_in_shape (FadeShape shape)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}
	bool in_command = false;

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			continue;
		}

		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_in();
		XMLNode &before = alist->get_state();

		tmp->audio_region()->set_fade_in_shape (shape);

		if (!in_command) {
			begin_reversible_command (_("set fade in shape"));
			in_command = true;
		}
		XMLNode &after = alist->get_state();
		_session->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::set_fade_out_shape (FadeShape shape)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}
	bool in_command = false;

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			continue;
		}

		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_out();
		XMLNode &before = alist->get_state();

		tmp->audio_region()->set_fade_out_shape (shape);

		if(!in_command) {
			begin_reversible_command (_("set fade out shape"));
			in_command = true;
		}
		XMLNode &after = alist->get_state();
		_session->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::set_fade_in_active (bool yn)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}
	bool in_command = false;

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			continue;
		}


		boost::shared_ptr<AudioRegion> ar (tmp->audio_region());

		ar->clear_changes ();
		ar->set_fade_in_active (yn);

		if (!in_command) {
			begin_reversible_command (_("set fade in active"));
			in_command = true;
		}
		_session->add_command (new StatefulDiffCommand (ar));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::set_fade_out_active (bool yn)
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}
	bool in_command = false;

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			continue;
		}

		boost::shared_ptr<AudioRegion> ar (tmp->audio_region());

		ar->clear_changes ();
		ar->set_fade_out_active (yn);

		if (!in_command) {
			begin_reversible_command (_("set fade out active"));
			in_command = true;
		}
		_session->add_command(new StatefulDiffCommand (ar));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::toggle_region_fades (int dir)
{
	if (_ignore_region_action) {
		return;
	}

	boost::shared_ptr<AudioRegion> ar;
	bool yn = false;

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty()) {
		return;
	}

	RegionSelection::iterator i;
	for (i = rs.begin(); i != rs.end(); ++i) {
		if ((ar = boost::dynamic_pointer_cast<AudioRegion>((*i)->region())) != 0) {
			if (dir == -1) {
				yn = ar->fade_out_active ();
			} else {
				yn = ar->fade_in_active ();
			}
			break;
		}
	}

	if (i == rs.end()) {
		return;
	}

	/* XXX should this undo-able? */
	bool in_command = false;

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		if ((ar = boost::dynamic_pointer_cast<AudioRegion>((*i)->region())) == 0) {
			continue;
		}
		ar->clear_changes ();

		if (dir == 1 || dir == 0) {
			ar->set_fade_in_active (!yn);
		}

		if (dir == -1 || dir == 0) {
			ar->set_fade_out_active (!yn);
		}
		if (!in_command) {
			begin_reversible_command (_("toggle fade active"));
			in_command = true;
		}
		_session->add_command(new StatefulDiffCommand (ar));
	}

	if (in_command) {
		commit_reversible_command ();
	}
}


/** Update region fade visibility after its configuration has been changed */
void
Editor::update_region_fade_visibility ()
{
	bool _fade_visibility = _session->config.get_show_region_fades ();

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		AudioTimeAxisView* v = dynamic_cast<AudioTimeAxisView*>(*i);
		if (v) {
			if (_fade_visibility) {
				v->audio_view()->show_all_fades ();
			} else {
				v->audio_view()->hide_all_fades ();
			}
		}
	}
}

void
Editor::set_edit_point ()
{
	bool ignored;
	samplepos_t sample;

	if (!mouse_sample (sample, ignored)) {
		return;
	}

	timepos_t where (sample);

	snap_to (where);

	if (selection->markers.empty()) {

		mouse_add_new_marker (where);

	} else {
		bool ignored;

		Location* loc = find_location_from_marker (selection->markers.front(), ignored);

		if (loc) {
			loc->move_to (where);
		}
	}
}

void
Editor::set_playhead_cursor ()
{
	if (entered_marker) {
		_session->request_locate (entered_marker->position().samples());
	} else {
		samplepos_t sample;
		bool ignored;

		if (!mouse_sample (sample, ignored)) {
			return;
		}

		timepos_t where (sample);

		snap_to (where);

		if (_session) {
			_session->request_locate (where.samples());
		}
	}

//not sure what this was for;  remove it for now.
//	if (UIConfiguration::instance().get_follow_edits() && (!_session || !_session->config.get_external_sync())) {
//		cancel_time_selection();
//	}

}

void
Editor::split_region ()
{
	if (_dragging_playhead) {
		/*continue*/
	} else if (_drags->active ()) {
		/*any other kind of drag, bail out so we avoid Undo snafu*/
		return;
	}

	//if a range is selected, separate it
	if (!selection->time.empty()) {
		separate_regions_between (selection->time);
		return;
	}

	//if no range was selected, try to find some regions to split
	if (current_mouse_mode() == MouseObject || current_mouse_mode() == MouseRange ) {  //don't try this for Internal Edit, Stretch, Draw, etc.

		RegionSelection rs;

		//new behavior:  the Split action will prioritize the entered_regionview rather than selected regions.
		//this fixes the unexpected case where you point at a region, but
		//  * nothing happens OR
		//  * some other region (maybe off-screen) is split.
		//NOTE:  if the entered_regionview is /part of the selection/ then we should operate on the selection as usual
		if (_edit_point == EditAtMouse && entered_regionview && !entered_regionview->selected()) {
			rs.add (entered_regionview);
		} else {
			rs = selection->regions;   //might be empty
		}

		if (rs.empty()) {
			TrackViewList tracks = selection->tracks;

			if (!tracks.empty()) {
				/* no region selected or entered, but some selected tracks:
				 * act on all regions on the selected tracks at the edit point
				 */
				timepos_t const where = get_preferred_edit_position (Editing::EDIT_IGNORE_NONE, false, false);
				get_regions_at (rs, where, tracks);
			}
		}

		const timepos_t pos = get_preferred_edit_position();

		if (rs.empty()) {
			return;
		}

		split_regions_at (pos, rs);
	}
}

void
Editor::select_next_stripable (bool routes_only)
{
	_session->selection().select_next_stripable (false, routes_only);
}

void
Editor::select_prev_stripable (bool routes_only)
{
	_session->selection().select_prev_stripable (false, routes_only);
}

void
Editor::set_loop_from_selection (bool play)
{
	if (_session == 0) {
		return;
	}

	timepos_t start, end;

	if (!get_selection_extents (start, end)) {
		return;
	}

	set_loop_range (start, end,  _("set loop range from selection"));

	if (play) {
		_session->request_play_loop (true, true);
	}
}

void
Editor::set_loop_from_region (bool play)
{
	timepos_t start, end;
	if (!get_selection_extents (start, end))
		return;

	set_loop_range (start, end, _("set loop range from region"));

	if (play) {
		_session->request_play_loop (true);
	}
}

void
Editor::set_punch_from_selection ()
{
	if (_session == 0) {
		return;
	}

	timepos_t start, end;
	if (!get_selection_extents (start, end))
		return;

	set_punch_range (start, end,  _("set punch range from selection"));
}

void
Editor::set_auto_punch_range ()
{
	// auto punch in/out button from a single button
	// If Punch In is unset, set punch range from playhead to end, enable punch in
	// If Punch In is set, the next punch sets Punch Out, unless the playhead has been
	//   rewound beyond the Punch In marker, in which case that marker will be moved back
	//   to the current playhead position.
	// If punch out is set, it clears the punch range and Punch In/Out buttons

	if (_session == 0) {
		return;
	}

	Location* tpl = transport_punch_location();
	timepos_t now (_playhead_cursor->current_sample());
	timepos_t begin = now;
	timepos_t end (_session->current_end_sample());

	if (!_session->config.get_punch_in()) {
		// First Press - set punch in and create range from here to eternity
		set_punch_range (begin, end, _("Auto Punch In"));
		_session->config.set_punch_in(true);
	} else if (tpl && !_session->config.get_punch_out()) {
		// Second press - update end range marker and set punch_out
		if (now < tpl->start()) {
			// playhead has been rewound - move start back  and pretend nothing happened
			begin = now;
			set_punch_range (begin, end, _("Auto Punch In/Out"));
		} else {
			// normal case for 2nd press - set the punch out
			end = timepos_t (_playhead_cursor->current_sample ());
			set_punch_range (tpl->start(), now, _("Auto Punch In/Out"));
			_session->config.set_punch_out(true);
		}
	} else {
		if (_session->config.get_punch_out()) {
			_session->config.set_punch_out(false);
		}

		if (_session->config.get_punch_in()) {
			_session->config.set_punch_in(false);
		}

		if (tpl)
		{
			// third press - unset punch in/out and remove range
			_session->locations()->remove(tpl);
		}
	}

}

void
Editor::set_session_extents_from_selection ()
{
	if (_session == 0) {
		return;
	}

	timepos_t start, end;
	if (!get_selection_extents (start, end))
		return;

	Location* loc;
	if ((loc = _session->locations()->session_range_location()) == 0) {
		_session->set_session_extents (start, end);  // this will create a new session range;  no need for UNDO
	} else {
		XMLNode &before = loc->get_state();

		_session->set_session_extents (start, end);

		XMLNode &after = loc->get_state();

		begin_reversible_command (_("set session start/end from selection"));

		_session->add_command (new MementoCommand<Location>(*loc, &before, &after));

		commit_reversible_command ();
	}

	_session->set_session_range_is_free (false);
}

void
Editor::set_punch_start_from_edit_point ()
{
	if (_session) {

		timepos_t start;
		timepos_t end = timepos_t::max (Temporal::AudioTime);

		//use the existing punch end, if any
		Location* tpl = transport_punch_location();
		if (tpl) {
			end = tpl->end();
		}

		if ((_edit_point == EditAtPlayhead) && _session->transport_rolling()) {
			start = timepos_t (_session->audible_sample());
		} else {
			start = get_preferred_edit_position();
		}

		//if there's not already a sensible selection endpoint, go "forever"
		if (start > end) {
			end = timepos_t::max (Temporal::AudioTime);
		}

		set_punch_range (start, end, _("set punch start from EP"));
	}
}

void
Editor::set_punch_end_from_edit_point ()
{
	if (_session) {

		timepos_t start;
		timepos_t end = timepos_t::max (Temporal::AudioTime);

		//use the existing punch start, if any
		Location* tpl = transport_punch_location();
		if (tpl) {
			start = tpl->start();
		}

		if ((_edit_point == EditAtPlayhead) && _session->transport_rolling()) {
			end = timepos_t (_session->audible_sample());
		} else {
			end = get_preferred_edit_position();
		}

		set_punch_range (start, end, _("set punch end from EP"));

	}
}

void
Editor::set_loop_start_from_edit_point ()
{
	if (_session) {

		timepos_t start;

		timepos_t end = timepos_t::max (Temporal::AudioTime);

		//use the existing loop end, if any
		Location* tpl = transport_loop_location();
		if (tpl) {
			end = tpl->end();
		}

		if ((_edit_point == EditAtPlayhead) && _session->transport_rolling()) {
			start = timepos_t (_session->audible_sample());
		} else {
			start = get_preferred_edit_position();
		}

		//if there's not already a sensible selection endpoint, go "forever"
		if (start > end) {
			end = timepos_t::max (Temporal::AudioTime);
		}

		set_loop_range (start, end, _("set loop start from EP"));
	}

}

void
Editor::set_loop_end_from_edit_point ()
{
	if (_session) {

		timepos_t start;
		timepos_t end = timepos_t::max (Temporal::AudioTime);

		//use the existing loop start, if any
		Location* tpl = transport_loop_location();
		if (tpl) {
			start = tpl->start();
		}

		if ((_edit_point == EditAtPlayhead) && _session->transport_rolling()) {
			end = timepos_t (_session->audible_sample());
		} else {
			end = get_preferred_edit_position();
		}

		set_loop_range (start, end, _("set loop end from EP"));
	}
}

void
Editor::set_punch_from_region ()
{
	timepos_t start, end;
	if (!get_selection_extents (start, end))
		return;

	set_punch_range (start, end, _("set punch range from region"));
}

void
Editor::pitch_shift_region ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	RegionSelection audio_rs;
	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		if (dynamic_cast<AudioRegionView*> (*i)) {
			audio_rs.push_back (*i);
		}
	}

	if (audio_rs.empty()) {
		return;
	}

	pitch_shift (audio_rs, 1.2);
}

void
Editor::set_tempo_from_region ()
{
	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	RegionView* rv = rs.front();

	define_one_bar (rv->region()->position(), rv->region()->end());
}

void
Editor::use_range_as_bar ()
{
	timepos_t start, end;
	if (get_edit_op_range (start, end)) {
		define_one_bar (start, end);
	}
}

void
Editor::define_one_bar (timepos_t const & start, timepos_t const & end)
{
	timecnt_t length = start.distance (end);

	TempoMap::SharedPtr tmap (TempoMap::write_copy());
	const Meter& m (tmap->meter_at (start));

	/* length = 1 bar */

	/* We're going to deliver a constant tempo here,
	   so we can use samples per beat to determine length.
	   now we want samples per beat.
	   we have samples per bar, and beats per bar, so ...
	*/

	/* XXXX METER MATH */
	double superclocks_per_beat = length.superclocks() / m.divisions_per_bar();

	/* beats per minute = */

	double beats_per_minute = (superclock_ticks_per_second * 60.0) / superclocks_per_beat;

	/* now decide whether to:

	    (a) set global tempo
	    (b) add a new tempo marker

	*/

	const TempoPoint& t (tmap->tempo_at (start));

	bool do_global = false;

	if ((tmap->n_tempos() == 1) && (tmap->n_meters() == 1)) {

		/* only 1 tempo & 1 meter: ask if the user wants to set the tempo
		   at the start, or create a new marker
		*/

		vector<string> options;
		options.push_back (_("Cancel"));
		options.push_back (_("Add new marker"));
		options.push_back (_("Set global tempo"));

		Choice c (
			_("Define one bar"),
			_("Do you want to set the global tempo or add a new tempo marker?"),
			options
			);

		c.set_default_response (2);

		switch (c.run()) {
		case 0:
			return;

		case 2:
			do_global = true;
			break;

		default:
			do_global = false;
		}

	} else {

		/* more than 1 tempo and/or meter section already, go ahead do the "usual":
		   if the marker is at the region starter, change it, otherwise add
		   a new tempo marker
		*/
	}

	begin_reversible_command (_("set tempo from region"));
	XMLNode& before (tmap->get_state());

	if (do_global) {
		tmap->set_tempo (Tempo (beats_per_minute, t.end_note_types_per_minute(), t.note_type()), timepos_t());
	} else if (t.time() == start) {
		tmap->set_tempo (Tempo (beats_per_minute, t.end_note_types_per_minute(), t.note_type()), start);
	} else {
		/* constant tempo */
		const Tempo tempo (beats_per_minute, t.note_type());
		tmap->set_tempo (tempo, start);
	}

	XMLNode& after (tmap->get_state());

	_session->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), &before, &after));
	commit_reversible_command ();

	TempoMap::update (tmap);
}

void
Editor::split_region_at_transients ()
{
	AnalysisFeatureList positions;

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	begin_reversible_command (_("split regions"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ) {

		RegionSelection::iterator tmp;

		tmp = i;
		++tmp;

		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> ((*i)->region());

		if (ar) {
			ar->transients (positions);
			split_region_at_points ((*i)->region(), positions, true);
			positions.clear ();
		}

		i = tmp;
	}

	commit_reversible_command ();

}

void
Editor::split_region_at_points (boost::shared_ptr<Region> r, AnalysisFeatureList& positions, bool can_ferret, bool select_new)
{
	bool use_rhythmic_rodent = false;

	boost::shared_ptr<Playlist> pl = r->playlist();

	list<boost::shared_ptr<Region> > new_regions;

	if (!pl) {
		return;
	}

	if (positions.empty()) {
		return;
	}

	if (positions.size() > 20 && can_ferret) {
		std::string msgstr = string_compose (_("You are about to split\n%1\ninto %2 pieces.\nThis could take a long time."), r->name(), positions.size() + 1);
		ArdourMessageDialog msg (msgstr,
		                         false,
		                         Gtk::MESSAGE_INFO,
		                         Gtk::BUTTONS_OK_CANCEL);

		if (can_ferret) {
			msg.add_button (_("Call for the Ferret!"), RESPONSE_APPLY);
			msg.set_secondary_text (_("Press OK to continue with this split operation\nor ask the Ferret dialog to tune the analysis"));
		} else {
			msg.set_secondary_text (_("Press OK to continue with this split operation"));
		}

		msg.set_title (_("Excessive split?"));
		int response = msg.run();
		msg.hide ();

		switch (response) {
		case RESPONSE_OK:
			break;
		case RESPONSE_APPLY:
			use_rhythmic_rodent = true;
			break;
		default:
			return;
		}
	}

	if (use_rhythmic_rodent) {
		show_rhythm_ferret ();
		return;
	}

	AnalysisFeatureList::const_iterator x;

	pl->clear_changes ();
	pl->clear_owned_changes ();

	x = positions.begin();

	if (x == positions.end()) {
		return;
	}

	pl->freeze ();
	pl->remove_region (r);

	timepos_t pos;

	const timepos_t rstart = r->position ();
	const samplepos_t start_sample = r->position_sample();
	const samplepos_t end_sample = r->last_sample() + 1;

	while (x != positions.end()) {

		/* deal with positions that are out of scope of present region bounds */

		if (*x < start_sample || *x >= end_sample) {
			++x;
			continue;
		}

		/* file start = original start + how far we from the initial position ?  */

		timepos_t file_start = r->start() + pos;

		/* length = next position - current position */

		timecnt_t len = pos.distance (timepos_t (*x)) - rstart;

		/* XXX we do we really want to allow even single-sample regions?
		 * shouldn't we have some kind of lower limit on region size?
		 */

		if (len.is_zero () || len.is_negative()) {
			break;
		}

		string new_name;

		if (RegionFactory::region_name (new_name, r->name())) {
			break;
		}

		/* do NOT announce new regions 1 by one, just wait till they are all done */

		PropertyList plist;

		plist.add (ARDOUR::Properties::start, file_start);
		plist.add (ARDOUR::Properties::length, len);
		plist.add (ARDOUR::Properties::name, new_name);
		plist.add (ARDOUR::Properties::layer, 0);
		// TODO set transients_offset

		boost::shared_ptr<Region> nr = RegionFactory::create (r->sources(), plist, false);
		/* because we set announce to false, manually add the new region to the
		 * RegionFactory map
		 */
		RegionFactory::map_add (nr);

		pl->add_region (nr, rstart + pos);

		if (select_new) {
			new_regions.push_front(nr);
		}

		pos += len;
		++x;
	}

	string new_name;

	RegionFactory::region_name (new_name, r->name());

	/* Add the final region */
	PropertyList plist;

	plist.add (ARDOUR::Properties::start, r->start() + pos);
	plist.add (ARDOUR::Properties::length, (r->position() + pos).distance (r->end()));
	plist.add (ARDOUR::Properties::name, new_name);
	plist.add (ARDOUR::Properties::layer, 0);

	boost::shared_ptr<Region> nr = RegionFactory::create (r->sources(), plist, false);
	/* because we set announce to false, manually add the new region to the
	   RegionFactory map
	*/
	RegionFactory::map_add (nr);
	pl->add_region (nr, r->position() + pos);

	if (select_new) {
		new_regions.push_front(nr);
	}

	pl->thaw ();

	/* We might have removed regions, which alters other regions' layering_index,
	   so we need to do a recursive diff here.
	*/
	vector<Command*> cmds;
	pl->rdiff (cmds);
	_session->add_commands (cmds);

	_session->add_command (new StatefulDiffCommand (pl));

	if (select_new) {

		for (list<boost::shared_ptr<Region> >::iterator i = new_regions.begin(); i != new_regions.end(); ++i){
			set_selected_regionview_from_region_list ((*i), Selection::Add);
		}
	}
}

void
Editor::place_transient()
{
	if (!_session) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_edit_point ();

	if (rs.empty()) {
		return;
	}

	timepos_t where = get_preferred_edit_position();

	begin_reversible_command (_("place transient"));

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		(*r)->region()->add_transient (where.samples());
	}

	commit_reversible_command ();
}

void
Editor::remove_transient(ArdourCanvas::Item* item)
{
	if (!_session) {
		return;
	}

	ArdourCanvas::Line* _line = reinterpret_cast<ArdourCanvas::Line*> (item);
	assert (_line);

	AudioRegionView* _arv = reinterpret_cast<AudioRegionView*> (item->get_data ("regionview"));
	_arv->remove_transient (*(float*) _line->get_data ("position"));
}

void
Editor::snap_regions_to_grid ()
{
	list <boost::shared_ptr<Playlist > > used_playlists;

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	begin_reversible_command (_("snap regions to grid"));

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {

		boost::shared_ptr<Playlist> pl = (*r)->region()->playlist();

		if (!pl->frozen()) {
			/* we haven't seen this playlist before */

			/* remember used playlists so we can thaw them later */
			used_playlists.push_back(pl);
			pl->freeze();
		}
		(*r)->region()->clear_changes ();

		timepos_t start ((*r)->region()->first_sample ());
		snap_to (start, Temporal::RoundNearest, SnapToGrid_Unscaled, true);
		(*r)->region()->set_position (start);
		_session->add_command(new StatefulDiffCommand ((*r)->region()));
	}

	while (used_playlists.size() > 0) {
		list <boost::shared_ptr<Playlist > >::iterator i = used_playlists.begin();
		(*i)->thaw();
		used_playlists.pop_front();
	}

	commit_reversible_command ();
}

void
Editor::close_region_gaps ()
{
	list <boost::shared_ptr<Playlist > > used_playlists;

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session || rs.empty()) {
		return;
	}

	Dialog dialog (_("Close Region Gaps"));

	Table table (2, 3);
	table.set_spacings (12);
	table.set_border_width (12);
	Label* l = manage (left_aligned_label (_("Crossfade length")));
	table.attach (*l, 0, 1, 0, 1);

	SpinButton spin_crossfade (1, 0);
	spin_crossfade.set_range (0, 15);
	spin_crossfade.set_increments (1, 1);
	spin_crossfade.set_value (5);
	table.attach (spin_crossfade, 1, 2, 0, 1);

	table.attach (*manage (new Label (_("ms"))), 2, 3, 0, 1);

	l = manage (left_aligned_label (_("Pull-back length")));
	table.attach (*l, 0, 1, 1, 2);

	SpinButton spin_pullback (1, 0);
	spin_pullback.set_range (0, 100);
	spin_pullback.set_increments (1, 1);
	spin_pullback.set_value(30);
	table.attach (spin_pullback, 1, 2, 1, 2);

	table.attach (*manage (new Label (_("ms"))), 2, 3, 1, 2);

	dialog.get_vbox()->pack_start (table);
	dialog.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	dialog.add_button (_("Ok"), RESPONSE_ACCEPT);
	dialog.show_all ();

	switch (dialog.run ()) {
		case Gtk::RESPONSE_ACCEPT:
		case Gtk::RESPONSE_OK:
			break;
		default:
			return;
	}

	samplepos_t crossfade_len = spin_crossfade.get_value();
	samplepos_t pull_back_samples = spin_pullback.get_value();

	crossfade_len = lrintf (crossfade_len * _session->sample_rate()/1000);
	pull_back_samples = lrintf (pull_back_samples * _session->sample_rate()/1000);

	/* Iterate over the region list and make adjacent regions overlap by crossfade_len_ms */

	begin_reversible_command (_("close region gaps"));

	int idx = 0;
	boost::shared_ptr<Region> last_region;

	rs.sort_by_position_and_track();

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {

		boost::shared_ptr<Playlist> pl = (*r)->region()->playlist();

		if (!pl->frozen()) {
			/* we haven't seen this playlist before */

			/* remember used playlists so we can thaw them later */
			used_playlists.push_back(pl);
			pl->freeze();
		}

		timepos_t position = (*r)->region()->position();

		if (idx == 0 || position < last_region->position()){
			last_region = (*r)->region();
			idx++;
			continue;
		}

		(*r)->region()->clear_changes ();
		(*r)->region()->trim_front (position.earlier (timecnt_t (pull_back_samples)));

		last_region->clear_changes ();
		last_region->trim_end (position.earlier (timecnt_t (pull_back_samples + crossfade_len)));

		_session->add_command (new StatefulDiffCommand ((*r)->region()));
		_session->add_command (new StatefulDiffCommand (last_region));

		last_region = (*r)->region();
		idx++;
	}

	while (used_playlists.size() > 0) {
		list <boost::shared_ptr<Playlist > >::iterator i = used_playlists.begin();
		(*i)->thaw();
		used_playlists.pop_front();
	}

	commit_reversible_command ();
}

void
Editor::tab_to_transient (bool forward)
{
	AnalysisFeatureList positions;

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (!_session) {
		return;
	}

	samplepos_t pos = _session->audible_sample ();

	if (!selection->tracks.empty()) {

		/* don't waste time searching for transients in duplicate playlists.
		 */

		TrackViewList ts = selection->tracks.filter_to_unique_playlists ();
		const timepos_t tpos (pos);

		for (TrackViewList::iterator t = ts.begin(); t != ts.end(); ++t) {

			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*t);

			if (rtv) {
				boost::shared_ptr<Track> tr = rtv->track();
				if (tr) {
					boost::shared_ptr<Playlist> pl = tr->playlist ();
					if (pl) {
						samplepos_t result = pl->find_next_transient (tpos, forward ? 1 : -1);

						if (result >= 0) {
							positions.push_back (result);
						}
					}
				}
			}
		}

	} else {

		if (rs.empty()) {
			return;
		}

		for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
			(*r)->region()->get_transients (positions);
		}
	}

	TransientDetector::cleanup_transients (positions, _session->sample_rate(), 3.0);

	if (forward) {
		AnalysisFeatureList::iterator x;

		for (x = positions.begin(); x != positions.end(); ++x) {
			if ((*x) > pos) {
				break;
			}
		}

		if (x != positions.end ()) {
			_session->request_locate (*x);
		}

	} else {
		AnalysisFeatureList::reverse_iterator x;

		for (x = positions.rbegin(); x != positions.rend(); ++x) {
			if ((*x) < pos) {
				break;
			}
		}

		if (x != positions.rend ()) {
			_session->request_locate (*x);
		}
	}
}

void
Editor::playhead_forward_to_grid ()
{
	if (!_session) {
		return;
	}

	timepos_t pos  (_playhead_cursor->current_sample ());

	if ( _grid_type == GridTypeNone) {
		if (pos < timepos_t::max (pos.time_domain()).earlier (timepos_t (samplepos_t (floor (current_page_samples()*0.1))))) {
			pos += timepos_t (samplepos_t (floor (current_page_samples()*0.1)));
			_session->request_locate (pos.samples());
		} else {
			_session->request_locate (0);
		}
	} else {

		if (pos < timepos_t::max (pos.time_domain()).earlier (timepos_t (samplepos_t (3)))) {
			pos += timepos_t (samplepos_t (2));
			pos = snap_to_grid (pos, Temporal::RoundUpAlways, SnapToGrid_Scaled);
			_session->request_locate (pos.samples());
		}
	}


	/* keep PH visible in window */
	if (pos > timepos_t (_leftmost_sample + current_page_samples() *0.9)) {
		reset_x_origin (pos.samples() -  (current_page_samples()*0.9));
	}
}


void
Editor::playhead_backward_to_grid ()
{
	if (!_session) {
		return;
	}

	timepos_t pos  (_playhead_cursor->current_sample ());

	if ( _grid_type == GridTypeNone) {
		if (pos.samples() > current_page_samples()*0.1 ) {
			pos.shift_earlier (timepos_t (samplepos_t (floor (current_page_samples()*0.1))));
			_session->request_locate (pos.samples());
		} else {
			_session->request_locate (0);
		}
	} else {

		if (pos.samples() > 2) {
			pos.shift_earlier (timepos_t (samplepos_t (2)));
			pos = snap_to_grid (pos, Temporal::RoundDownAlways, SnapToGrid_Scaled);
		}

		//handle the case where we are rolling, and we're less than one-half second past the mark, we want to go to the prior mark...
		//also see:  jump_backward_to_mark
		if (_session->transport_rolling()) {
			if ((_playhead_cursor->current_sample() - pos.samples()) < _session->sample_rate()/2) {
				pos = snap_to_grid (pos, Temporal::RoundDownAlways, SnapToGrid_Scaled);
			}
		}

		_session->request_locate (pos.samples());
	}

	/* keep PH visible in window */
	if (pos.samples() < (_leftmost_sample + current_page_samples() *0.1)) {
		reset_x_origin (pos.samples() - (current_page_samples()*0.1));
	}
}

void
Editor::set_track_height (Height h)
{
	TrackSelection& ts (selection->tracks);

	for (TrackSelection::iterator x = ts.begin(); x != ts.end(); ++x) {
		(*x)->set_height_enum (h);
	}
}

void
Editor::toggle_tracks_active ()
{
	TrackSelection& ts (selection->tracks);
	bool first = true;
	bool target = false;

	if (ts.empty()) {
		return;
	}

	for (TrackSelection::iterator x = ts.begin(); x != ts.end(); ++x) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(*x);

		if (rtv) {
			if (first) {
				target = !rtv->route()->active();
				first = false;
			}
			rtv->route()->set_active (target, this);
		}
	}
}

void
Editor::remove_tracks ()
{
	/* this will delete GUI objects that may be the subject of an event
	   handler in which this method is called. Defer actual deletion to the
	   next idle callback, when all event handling is finished.
	*/
	Glib::signal_idle().connect (sigc::mem_fun (*this, &Editor::idle_remove_tracks));
}

bool
Editor::idle_remove_tracks ()
{
	Session::StateProtector sp (_session);
	_remove_tracks ();
	return false; /* do not call again */
}

void
Editor::_remove_tracks ()
{
	TrackSelection& ts (selection->tracks);

	if (ts.empty()) {
		return;
	}

	if (!ARDOUR_UI_UTILS::engine_is_running ()) {
		return;
	}

	vector<string> choices;
	string prompt;
	int ntracks = 0;
	int nbusses = 0;
	int nvcas = 0;
	const char* trackstr;
	const char* busstr;
	const char* vcastr;
	vector<boost::shared_ptr<Route> > routes;
	vector<boost::shared_ptr<VCA> > vcas;
	bool special_bus = false;

	for (TrackSelection::iterator x = ts.begin(); x != ts.end(); ++x) {
		VCATimeAxisView* vtv = dynamic_cast<VCATimeAxisView*> (*x);
		if (vtv) {
			vcas.push_back (vtv->vca());
			++nvcas;
			continue;
		}
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*x);
		if (!rtv) {
			continue;
		}
		if (rtv->is_track()) {
			++ntracks;
		} else {
			++nbusses;
		}
		routes.push_back (rtv->route());

		if (rtv->route()->is_master() || rtv->route()->is_monitor()) {
			special_bus = true;
		}
	}

	if (special_bus && !Config->get_allow_special_bus_removal()) {
		ArdourMessageDialog msg (_("That would be bad news ...."),
		                         false,
		                         Gtk::MESSAGE_INFO,
		                         Gtk::BUTTONS_OK);
		msg.set_secondary_text (string_compose (_("Removing the master or monitor bus is such a bad idea\n\
that %1 is not going to allow it.\n\
\n\
If you really want to do this sort of thing\n\
edit your ardour.rc file to set the\n\
\"allow-special-bus-removal\" option to be \"yes\""), PROGRAM_NAME));

		msg.run ();
		return;
	}

	if (ntracks + nbusses + nvcas == 0) {
		return;
	}

	string title;

	trackstr = P_("track", "tracks", ntracks);
	busstr = P_("bus", "busses", nbusses);
	vcastr = P_("VCA", "VCAs", nvcas);

	if (ntracks > 0 && nbusses > 0 && nvcas > 0) {
		title = _("Remove various strips");
		prompt = string_compose (_("Do you really want to remove %1 %2, %3 %4 and %5 %6?"),
						  ntracks, trackstr, nbusses, busstr, nvcas, vcastr);
	}
	else if (ntracks > 0 && nbusses > 0) {
		title = string_compose (_("Remove %1 and %2"), trackstr, busstr);
		prompt = string_compose (_("Do you really want to remove %1 %2 and %3 %4?"),
				ntracks, trackstr, nbusses, busstr);
	}
	else if (ntracks > 0 && nvcas > 0) {
		title = string_compose (_("Remove %1 and %2"), trackstr, vcastr);
		prompt = string_compose (_("Do you really want to remove %1 %2 and %3 %4?"),
				ntracks, trackstr, nvcas, vcastr);
	}
	else if (nbusses > 0 && nvcas > 0) {
		title = string_compose (_("Remove %1 and %2"), busstr, vcastr);
		prompt = string_compose (_("Do you really want to remove %1 %2 and %3 %4?"),
				nbusses, busstr, nvcas, vcastr);
	}
	else if (ntracks > 0) {
		title = string_compose (_("Remove %1"), trackstr);
		prompt  = string_compose (_("Do you really want to remove %1 %2?"),
				ntracks, trackstr);
	}
	else if (nbusses > 0) {
		title = string_compose (_("Remove %1"), busstr);
		prompt  = string_compose (_("Do you really want to remove %1 %2?"),
				nbusses, busstr);
	}
	else if (nvcas > 0) {
		title = string_compose (_("Remove %1"), vcastr);
		prompt  = string_compose (_("Do you really want to remove %1 %2?"),
				nvcas, vcastr);
	}
	else {
		assert (0);
	}

	if (ntracks > 0) {
			prompt += "\n" + string_compose ("(You may also lose the playlists associated with the %1)", trackstr) + "\n";
	}

	prompt += "\n" + string(_("This action cannot be undone, and the session file will be overwritten!"));

	choices.push_back (_("No, do nothing."));
	if (ntracks + nbusses + nvcas > 1) {
		choices.push_back (_("Yes, remove them."));
	} else {
		choices.push_back (_("Yes, remove it."));
	}

	Choice prompter (title, prompt, choices);

	if (prompter.run () != 1) {
		return;
	}

	if (current_mixer_strip && routes.size () > 1 && std::find (routes.begin(), routes.end(), current_mixer_strip->route()) != routes.end ()) {
		/* Route deletion calls Editor::timeaxisview_deleted() iteratively (for each deleted
		 * route). If the deleted route is currently displayed in the Editor-Mixer (highly
		 * likely because deletion requires selection) this will call
		 * Editor::set_selected_mixer_strip () which is expensive (MixerStrip::set_route()).
		 * It's likewise likely that the route that has just been displayed in the
		 * Editor-Mixer will be next in line for deletion.
		 *
		 * So simply switch to the master-bus (if present)
		 */
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if ((*i)->stripable ()->is_master ()) {
				set_selected_mixer_strip (*(*i));
				break;
			}
		}
	}

	{
		PresentationInfo::ChangeSuspender cs;
		DisplaySuspender ds;

		boost::shared_ptr<RouteList> rl (new RouteList);
		for (vector<boost::shared_ptr<Route> >::iterator x = routes.begin(); x != routes.end(); ++x) {
			rl->push_back (*x);
		}
		_session->remove_routes (rl);

		for (vector<boost::shared_ptr<VCA> >::iterator x = vcas.begin(); x != vcas.end(); ++x) {
			_session->vca_manager().remove_vca (*x);
		}

	}
	/* TrackSelection and RouteList leave scope,
	 * destructors are called,
	 * diskstream drops references, save_state is called (again for every track)
	 */
}

void
Editor::do_insert_time ()
{
	if (selection->tracks.empty()) {
		ArdourMessageDialog msg (_("You must first select some tracks to Insert Time."),
	                           true, MESSAGE_INFO, BUTTONS_OK, true);
		msg.run ();
		return;
	}

	if (Config->get_edit_mode() == Lock) {
		ArdourMessageDialog msg (_("You cannot insert time in Lock Edit mode."),
		                         true, MESSAGE_INFO, BUTTONS_OK, true);
		msg.run ();
		return;
	}

	InsertRemoveTimeDialog d (*this);
	int response = d.run ();

	if (response != RESPONSE_OK) {
		return;
	}

	if (d.distance() == 0) {
		return;
	}

	insert_time (
		d.position(),
		d.distance(),
		d.intersected_region_action (),
		d.all_playlists(),
		d.move_glued(),
		d.move_markers(),
		d.move_glued_markers(),
		d.move_locked_markers(),
		d.move_tempos()
		);
}

void
Editor::insert_time (
	timepos_t const & pos, timecnt_t const & samples, InsertTimeOption opt,
	bool all_playlists, bool ignore_music_glue, bool markers_too, bool glued_markers_too, bool locked_markers_too, bool tempo_too
	)
{

	if (Config->get_edit_mode() == Lock) {
		return;
	}
	bool in_command = false;

	TrackViewList ts = selection->tracks.filter_to_unique_playlists ();

	for (TrackViewList::iterator x = ts.begin(); x != ts.end(); ++x) {

		/* regions */

		/* don't operate on any playlist more than once, which could
		 * happen if "all playlists" is enabled, but there is more
		 * than 1 track using playlists "from" a given track.
		 */

		set<boost::shared_ptr<Playlist> > pl;

		if (all_playlists) {
			RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*x);
			if (rtav && rtav->track ()) {
				vector<boost::shared_ptr<Playlist> > all = _session->playlists()->playlists_for_track (rtav->track ());
				for (vector<boost::shared_ptr<Playlist> >::iterator p = all.begin(); p != all.end(); ++p) {
					pl.insert (*p);
				}
			}
		} else {
			if ((*x)->playlist ()) {
				pl.insert ((*x)->playlist ());
			}
		}

		for (set<boost::shared_ptr<Playlist> >::iterator i = pl.begin(); i != pl.end(); ++i) {

			(*i)->clear_changes ();
			(*i)->clear_owned_changes ();

			if (!in_command) {
				begin_reversible_command (_("insert time"));
				in_command = true;
			}

			if (opt == SplitIntersected) {
				/* non musical split */
				(*i)->split (pos);
			}

			(*i)->shift (pos, samples, (opt == MoveIntersected), ignore_music_glue);

			vector<Command*> cmds;
			(*i)->rdiff (cmds);
			_session->add_commands (cmds);

			_session->add_command (new StatefulDiffCommand (*i));
		}

		/* automation */
		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*x);
		if (rtav) {
			if (!in_command) {
				begin_reversible_command (_("insert time"));
				in_command = true;
			}
			rtav->route ()->shift (pos, samples);
		}
	}

	/* markers */
	if (markers_too) {
		bool moved = false;
		XMLNode& before (_session->locations()->get_state());
		Locations::LocationList copy (_session->locations()->list());

		for (Locations::LocationList::iterator i = copy.begin(); i != copy.end(); ++i) {

			Locations::LocationList::const_iterator tmp;

			if ((*i)->position_time_domain() == Temporal::AudioTime || glued_markers_too) {
				bool const was_locked = (*i)->locked ();
				if (locked_markers_too) {
					(*i)->unlock ();
				}

				if ((*i)->start() >= pos) {
					// move end first, in case we're moving by more than the length of the range
					if (!(*i)->is_mark()) {
						(*i)->set_end ((*i)->end() + samples, false);
					}
					(*i)->set_start ((*i)->start() + samples, false);
					moved = true;
				}

				if (was_locked) {
					(*i)->lock ();
				}
			}
		}

		if (moved) {
			if (!in_command) {
				begin_reversible_command (_("insert time"));
				in_command = true;
			}
			XMLNode& after (_session->locations()->get_state());
			_session->add_command (new MementoCommand<Locations>(*_session->locations(), &before, &after));
		}
	}

	if (tempo_too) {
		if (!in_command) {
			begin_reversible_command (_("insert time"));
			in_command = true;
		}
		TempoMap::SharedPtr tmap (TempoMap::write_copy());

		XMLNode& before (tmap->get_state());
		tmap->insert_time (pos, samples);
		XMLNode& after (tmap->get_state());
		_session->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), &before, &after));

		TempoMap::update (tmap);
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::do_remove_time ()
{
	if (selection->tracks.empty()) {
		ArdourMessageDialog msg (_("You must first select some tracks to Remove Time."),
		                         true, MESSAGE_INFO, BUTTONS_OK, true);
		msg.run ();
		return;
	}

	if (Config->get_edit_mode() == Lock) {
		ArdourMessageDialog msg (_("You cannot remove time in Lock Edit mode."),
		                         true, MESSAGE_INFO, BUTTONS_OK, true);
		msg.run ();
		return;
	}

	InsertRemoveTimeDialog d (*this, true);

	int response = d.run ();

	if (response != RESPONSE_OK) {
		return;
	}

	timecnt_t distance = d.distance();

	if (distance.is_zero ()) {
		return;
	}

	remove_time (
		d.position(),
		distance,
		SplitIntersected,
		d.move_glued(),
		d.move_markers(),
		d.move_glued_markers(),
		d.move_locked_markers(),
		d.move_tempos()
	);
}

void
Editor::remove_time (timepos_t const & pos, timecnt_t const & duration, InsertTimeOption opt,
                     bool ignore_music_glue, bool markers_too, bool glued_markers_too, bool locked_markers_too, bool tempo_too)
{
	if (Config->get_edit_mode() == Lock) {
		error << (_("Cannot insert or delete time when in Lock edit.")) << endmsg;
		return;
	}
	bool in_command = false;

	for (TrackSelection::iterator x = selection->tracks.begin(); x != selection->tracks.end(); ++x) {
		/* regions */
		boost::shared_ptr<Playlist> pl = (*x)->playlist();

		if (pl) {

			XMLNode &before = pl->get_state();

			if (!in_command) {
				begin_reversible_command (_("remove time"));
				in_command = true;
			}

			std::list<TimelineRange> rl;
			TimelineRange ar(pos, pos+duration, 0);
			rl.push_back(ar);
			pl->cut (rl);
			pl->shift (pos, -duration, true, ignore_music_glue);

			XMLNode &after = pl->get_state();

			_session->add_command (new MementoCommand<Playlist> (*pl, &before, &after));
		}

		/* automation */
		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*x);
		if (rtav) {
			if (!in_command) {
				begin_reversible_command (_("remove time"));
				in_command = true;
			}
			rtav->route ()->shift (pos, -duration);
		}
	}

	std::list<Location*> loc_kill_list;

	/* markers */
	if (markers_too) {
		bool moved = false;
		XMLNode& before (_session->locations()->get_state());
		Locations::LocationList copy (_session->locations()->list());

		for (Locations::LocationList::iterator i = copy.begin(); i != copy.end(); ++i) {
			if ((*i)->position_time_domain() == Temporal::AudioTime || glued_markers_too) {

				bool const was_locked = (*i)->locked ();
				if (locked_markers_too) {
					(*i)->unlock ();
				}

				if (!(*i)->is_mark()) {  // it's a range;  have to handle both start and end
					if ((*i)->end() >= pos
					&& (*i)->end() < pos+duration
					&& (*i)->start() >= pos
					&& (*i)->end() < pos+duration) {  // range is completely enclosed;  kill it
						moved = true;
						loc_kill_list.push_back(*i);
					} else {  // only start or end is included, try to do the right thing
						// move start before moving end, to avoid trying to move the end to before the start
						// if we're removing more time than the length of the range
						if ((*i)->start() >= pos && (*i)->start() < pos+duration) {
							// start is within cut
							(*i)->set_start (pos, false);  // bring the start marker to the beginning of the cut
							moved = true;
						} else if ((*i)->start() >= pos+duration) {
							// start (and thus entire range) lies beyond end of cut
							(*i)->set_start ((*i)->start().earlier (duration), false); // slip the start marker back
							moved = true;
						}
						if ((*i)->end() >= pos && (*i)->end() < pos+duration) {
							// end is inside cut
							(*i)->set_end (pos, false);  // bring the end to the cut
							moved = true;
						} else if ((*i)->end() >= pos+duration) {
							// end is beyond end of cut
							(*i)->set_end ((*i)->end().earlier (duration), false); // slip the end marker back
							moved = true;
						}

					}
				} else if ((*i)->start() >= pos && (*i)->start() < pos+duration) {
					loc_kill_list.push_back(*i);
					moved = true;
				} else if ((*i)->start() >= pos) {
					(*i)->set_start ((*i)->start().earlier (duration), false);
					moved = true;
				}

				if (was_locked) {
					(*i)->lock ();
				}
			}
		}

		for (list<Location*>::iterator i = loc_kill_list.begin(); i != loc_kill_list.end(); ++i) {
			_session->locations()->remove (*i);
		}

		if (moved) {
			if (!in_command) {
				begin_reversible_command (_("remove time"));
				in_command = true;
			}
			XMLNode& after (_session->locations()->get_state());
			_session->add_command (new MementoCommand<Locations>(*_session->locations(), &before, &after));
		}
	}

	if (tempo_too) {
		TempoMap::SharedPtr tmap (TempoMap::write_copy());
		XMLNode& before (tmap->get_state());

		if (tmap->remove_time (pos, duration)) {
			if (!in_command) {
				begin_reversible_command (_("remove time"));
				in_command = true;
			}
			XMLNode& after (tmap->get_state());
			_session->add_command (new MementoCommand<Temporal::TempoMap> (new Temporal::TempoMap::MementoBinder(), &before, &after));

			TempoMap::update (tmap);
		}
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::fit_selection ()
{
	if (!selection->tracks.empty()) {
		fit_tracks (selection->tracks);
	} else {
		TrackViewList tvl;

		/* no selected tracks - use tracks with selected regions */

		if (!selection->regions.empty()) {
			for (RegionSelection::iterator r = selection->regions.begin(); r != selection->regions.end(); ++r) {
				tvl.push_back (&(*r)->get_time_axis_view ());
			}

			if (!tvl.empty()) {
				fit_tracks (tvl);
			}
		} else if (internal_editing()) {
			/* no selected tracks, or regions, but in internal edit mode, so follow the mouse and use
			 * the entered track
			 */
			if (entered_track) {
				tvl.push_back (entered_track);
				fit_tracks (tvl);
			}
		}
	}
}

void
Editor::fit_tracks (TrackViewList & tracks)
{
	if (tracks.empty()) {
		return;
	}

	uint32_t child_heights = 0;
	int visible_tracks = 0;

	for (TrackSelection::iterator t = tracks.begin(); t != tracks.end(); ++t) {

		if (!(*t)->marked_for_display()) {
			continue;
		}

		child_heights += (*t)->effective_height() - (*t)->current_height();
		++visible_tracks;
	}

	/* compute the per-track height from:
	 *
	 * total canvas visible height
	 *  - height that will be taken by visible children of selected tracks
	 *  - height of the ruler/hscroll area
	 */
	uint32_t h = (uint32_t) floor ((trackviews_height() - child_heights) / visible_tracks);
	double first_y_pos = DBL_MAX;

	if (h < TimeAxisView::preset_height (HeightSmall)) {
		ArdourMessageDialog msg (_("There are too many tracks to fit in the current window"));
		msg.run ();
		/* too small to be displayed, just use smallest possible */
		h = HeightSmall;
	}

	undo_visual_stack.push_back (current_visual_state (true));
	PBD::Unwinder<bool> nsv (no_save_visual, true);

	/* build a list of all tracks, including children */

	TrackViewList all;
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		all.push_back (*i);
		TimeAxisView::Children c = (*i)->get_child_list ();
		for (TimeAxisView::Children::iterator j = c.begin(); j != c.end(); ++j) {
			all.push_back (j->get());
		}
	}


	// find selection range.
	// if someone knows how to user TrackViewList::iterator for this
	// I'm all ears.
	int selected_top = -1;
	int selected_bottom = -1;
	int i = 0;
	for (TrackViewList::iterator t = all.begin(); t != all.end(); ++t, ++i) {
		if ((*t)->marked_for_display ()) {
			if (tracks.contains(*t)) {
				if (selected_top == -1) {
					selected_top = i;
				}
				selected_bottom = i;
			}
		}
	}

	i = 0;
	for (TrackViewList::iterator t = all.begin(); t != all.end(); ++t, ++i) {
		if ((*t)->marked_for_display ()) {
			if (tracks.contains(*t)) {
				(*t)->set_height (h);
				first_y_pos = std::min ((*t)->y_position (), first_y_pos);
			} else {
				if (i > selected_top && i < selected_bottom) {
					hide_track_in_display (*t);
				}
			}
		}
	}

	/* apply height changes, so that scrolling works */
	process_redisplay_track_views ();

	/*
	   set the controls_layout height now, because waiting for its size
	   request signal handler will cause the vertical adjustment setting to fail
	*/

	controls_layout.property_height () = _full_canvas_height;
	vertical_adjustment.set_value (first_y_pos);

	redo_visual_stack.push_back (current_visual_state (true));

	visible_tracks_selector.set_text (_("Sel"));
}

void
Editor::save_visual_state (uint32_t n)
{
	while (visual_states.size() <= n) {
		visual_states.push_back (0);
	}

	if (visual_states[n] != 0) {
		delete visual_states[n];
	}

	visual_states[n] = current_visual_state (true);
	gdk_beep ();
}

void
Editor::goto_visual_state (uint32_t n)
{
	if (visual_states.size() <= n) {
		return;
	}

	if (visual_states[n] == 0) {
		return;
	}

	use_visual_state (*visual_states[n]);
}

void
Editor::start_visual_state_op (uint32_t n)
{
	save_visual_state (n);

	PopUp* pup = new PopUp (WIN_POS_MOUSE, 1000, true);
	char buf[32];
	snprintf (buf, sizeof (buf), _("Saved view %u"), n+1);
	pup->set_text (buf);
	pup->touch();
}

void
Editor::cancel_visual_state_op (uint32_t n)
{
	goto_visual_state (n);
}

void
Editor::toggle_region_mute ()
{
	if (_ignore_region_action) {
		return;
	}

	RegionSelection rs = get_regions_from_selection_and_entered ();

	if (rs.empty ()) {
		return;
	}

	if (rs.size() > 1) {
		begin_reversible_command (_("mute regions"));
	} else {
		begin_reversible_command (_("mute region"));
	}

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {

		(*i)->region()->playlist()->clear_changes ();
		(*i)->region()->set_muted (!(*i)->region()->muted ());
		_session->add_command (new StatefulDiffCommand ((*i)->region()));

	}

	commit_reversible_command ();
}

void
Editor::combine_regions ()
{
	/* foreach track with selected regions, take all selected regions
	   and join them into a new region containing the subregions (as a
	   playlist)
	*/

	typedef set<RouteTimeAxisView*> RTVS;
	RTVS tracks;

	if (selection->regions.empty()) {
		return;
	}

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&(*i)->get_time_axis_view());

		if (rtv) {
			tracks.insert (rtv);
		}
	}

	begin_reversible_command (_("combine regions"));

	vector<RegionView*> new_selection;

	for (RTVS::iterator i = tracks.begin(); i != tracks.end(); ++i) {
		RegionView* rv;

		if ((rv = (*i)->combine_regions ()) != 0) {
			new_selection.push_back (rv);
		}
	}

	selection->clear_regions ();
	for (vector<RegionView*>::iterator i = new_selection.begin(); i != new_selection.end(); ++i) {
		selection->add (*i);
	}

	commit_reversible_command ();
}

void
Editor::uncombine_regions ()
{
	typedef set<RouteTimeAxisView*> RTVS;
	RTVS tracks;

	if (selection->regions.empty()) {
		return;
	}

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&(*i)->get_time_axis_view());

		if (rtv) {
			tracks.insert (rtv);
		}
	}

	begin_reversible_command (_("uncombine regions"));

	for (RTVS::iterator i = tracks.begin(); i != tracks.end(); ++i) {
		(*i)->uncombine_regions ();
	}

	commit_reversible_command ();
}

void
Editor::toggle_midi_input_active (bool flip_others)
{
	bool onoff = false;
	boost::shared_ptr<RouteList> rl (new RouteList);

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		RouteTimeAxisView *rtav = dynamic_cast<RouteTimeAxisView *>(*i);

		if (!rtav) {
			continue;
		}

		boost::shared_ptr<MidiTrack> mt = rtav->midi_track();

		if (mt) {
			rl->push_back (rtav->route());
			onoff = !mt->input_active();
		}
	}

	_session->set_exclusive_input_active (rl, onoff, flip_others);
}

static bool ok_fine (GdkEventAny*) { return true; }

void
Editor::lock ()
{
	if (!lock_dialog) {
		lock_dialog = new Gtk::Dialog (string_compose (_("%1: Locked"), PROGRAM_NAME), true);

		Gtk::Image* padlock = manage (new Gtk::Image (ARDOUR_UI_UTILS::get_icon ("padlock_closed")));
		lock_dialog->get_vbox()->pack_start (*padlock);
		lock_dialog->signal_delete_event ().connect (sigc::ptr_fun (ok_fine));

		ArdourButton* b = manage (new ArdourButton);
		b->set_name ("lock button");
		b->set_text (_("Click to unlock"));
		b->signal_clicked.connect (sigc::mem_fun (*this, &Editor::unlock));
		lock_dialog->get_vbox()->pack_start (*b);

		lock_dialog->get_vbox()->show_all ();
		lock_dialog->set_size_request (200, 200);
	}

	delete _main_menu_disabler;
	_main_menu_disabler = new MainMenuDisabler;

	lock_dialog->present ();

	lock_dialog->get_window()->set_decorations (Gdk::WMDecoration (0));
}

void
Editor::unlock ()
{
	lock_dialog->hide ();

	delete _main_menu_disabler;
	_main_menu_disabler = 0;

	if (UIConfiguration::instance().get_lock_gui_after_seconds()) {
		start_lock_event_timing ();
	}
}

void
Editor::bring_in_callback (Gtk::Label* label, uint32_t n, uint32_t total, string name)
{
	Gtkmm2ext::UI::instance()->call_slot (invalidator (*this), boost::bind (&Editor::update_bring_in_message, this, label, n, total, name));
}

void
Editor::update_bring_in_message (Gtk::Label* label, uint32_t n, uint32_t total, string name)
{
	Timers::TimerSuspender t;
	label->set_text (string_compose ("Copying %1, %2 of %3", name, n, total));
	Gtkmm2ext::UI::instance()->flush_pending (1);
}

void
Editor::bring_all_sources_into_session ()
{
	if (!_session) {
		return;
	}

	Gtk::Label msg;
	ArdourDialog w (_("Moving embedded files into session folder"));
	w.get_vbox()->pack_start (msg);
	w.present ();

	/* flush all pending GUI events because we're about to start copying
	 * files
	 */

	Timers::TimerSuspender t;
	Gtkmm2ext::UI::instance()->flush_pending (3);

	cerr << " Do it\n";

	_session->bring_all_sources_into_session (boost::bind (&Editor::bring_in_callback, this, &msg, _1, _2, _3));
}

void
Editor::toggle_all_existing_automation ()
{
	TrackViewList & tvl (selection->tracks.empty() ? track_views : selection->tracks);
	bool some_automation_shown = false;

	for (TrackViewList::const_iterator t = tvl.begin(); t != tvl.end(); ++t) {
		TimeAxisView::Children children = (*t)->get_child_list ();
		for (TimeAxisView::Children::const_iterator c = children.begin(); c != children.end(); ++c) {
			if (boost::dynamic_pointer_cast<AutomationTimeAxisView> (*c)) {
				some_automation_shown = true;
				break;
			}
		}

		if (some_automation_shown) {
			break;
		}
	}

	if (!some_automation_shown) {
		tvl.foreach_stripable_time_axis (boost::bind (&StripableTimeAxisView::show_existing_automation, _1, false));
	} else {
		tvl.foreach_stripable_time_axis (boost::bind (&StripableTimeAxisView::hide_all_automation, _1, false));
	}
}

void
Editor::toggle_layer_display ()
{
	TrackViewList & tvl (selection->tracks.empty() ? track_views : selection->tracks);
	bool seen_stacked = false;
	bool seen_overlaid = false;

	for (TrackViewList::const_iterator t = tvl.begin(); t != tvl.end(); ++t) {
		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*t);

		if (!rtav || !rtav->is_track()) {
			continue;
		}

		if (rtav->layer_display () == Stacked) {
			seen_stacked = true;
		} else if (rtav->layer_display() == Overlaid) {
			seen_overlaid = true;
		}
	}

	if (seen_stacked && seen_overlaid) {
		/* inconsistent current display - go to overlaid */
		tvl.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::set_layer_display, _1, Overlaid));

	} else {
		tvl.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::toggle_layer_display, _1));
	}

}

void
Editor::layer_display_overlaid ()
{
	TrackViewList & tvl (selection->tracks.empty() ? track_views : selection->tracks);
	tvl.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::set_layer_display, _1, Overlaid));
}

void
Editor::layer_display_stacked ()
{
	TrackViewList & tvl (selection->tracks.empty() ? track_views : selection->tracks);
	tvl.foreach_route_time_axis (boost::bind (&RouteTimeAxisView::set_layer_display, _1, Stacked));
}

void
Editor::launch_playlist_selector ()
{
	TrackViewList & tvl (selection->tracks);
	TrackViewList::const_iterator t = tvl.begin();
	if (t == tvl.end()) {
		return;
	}

	RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*t);

	if (rtav && rtav->is_track()) {
		rtav->show_playlist_selector ();
	}
}

vector<MidiRegionView*>
Editor::filter_to_unique_midi_region_views (RegionSelection const & ms) const
{
	typedef std::pair<boost::shared_ptr<MidiSource>,timepos_t> MapEntry;
	std::set<MapEntry> single_region_set;

	vector<MidiRegionView*> views;

	/* build a list of regions that are unique with respect to their source
	 * and start position. Note: this is non-exhaustive... if someone has a
	 * non-forked copy of a MIDI region and then suitably modifies it, this
	 * will still put both regions into the list of things to be acted
	 * upon.
	 *
	 * Solution: user should not select both regions, or should fork one of them.
	 */

	for (MidiRegionSelection::const_iterator i = ms.begin(); i != ms.end(); ++i) {

		MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (*i);

		if (!mrv) {
			continue;
		}

		MapEntry entry = make_pair (mrv->midi_region()->midi_source(), mrv->region()->start());

		if (single_region_set.insert (entry).second) {
			views.push_back (mrv);
		}
	}

	return views;
}


void
Editor::midi_action (void (MidiRegionView::*method)())
{
	MidiRegionSelection ms = selection->midi_regions();

	if (ms.empty()) {
		return;
	}

	if (ms.size() > 1) {

		vector<MidiRegionView*> views = filter_to_unique_midi_region_views (ms);

		for (vector<MidiRegionView*>::iterator mrv = views.begin(); mrv != views.end(); ++mrv) {
			((*mrv)->*method) ();
		}

	} else {

		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(ms.front());

		if (mrv) {
			(mrv->*method)();
		}
	}
}

void
Editor::add_region_marker ()
{
	if (!_session) {
		return;
	}

	/* get these before we display the dialog, since it will interfere if
	   the edit point is "mouse"
	*/
	RegionSelection rs = get_regions_from_selection_and_edit_point ();
	timepos_t position = get_preferred_edit_position ();

	cerr << "adding cue marker @ " << position << " in " << rs.size() << endl;

	ArdourDialog d (_("New Cue Marker Name"), true, false);
	Gtk::Entry e;
	d.get_vbox()->pack_start (e);
	e.show ();
	e.set_activates_default ();
	d.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	d.add_button (Stock::OK, RESPONSE_OK);
	d.set_default_response (RESPONSE_OK);

	int result = d.run();
	string str = e.get_text();

	if (result != RESPONSE_OK || str.empty()) {
		return;
	}


	bool in_command = false;

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {

		boost::shared_ptr<Region> region ((*r)->region());

		if (position < region->position() || position >= region->position() + region->length()) {
			cerr << "nope on that one\n";
			continue;
		}

		SourceList & sources = region->sources_for_edit ();

		CueMarker marker (str, region->start() + (region->position().distance (position)));

		for (SourceList::iterator s = sources.begin(); s != sources.end(); ++s) {

			XMLNode* before_cues = (*s)->get_state().child (X_("Cues"));

			if (!(*s)->add_cue_marker (marker)) {
				delete before_cues;
				continue;
			}

			if (!in_command) {
				begin_reversible_command (_("add cue marker"));
				in_command = true;
			}


			XMLNode* after_cues = (*s)->get_state().child (X_("Cues"));

			if (!before_cues) {
				before_cues = new XMLNode (X_("Cues"));
			}

			if (!after_cues) {
				after_cues = new XMLNode (X_("Cues"));
			}

			_session->add_command (new MementoCommand<Source> (**s, before_cues, after_cues));
		}
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::remove_region_marker (CueMarker& cm)
{
	RegionSelection rs = get_regions_from_selection_and_edit_point ();
	bool in_command = false;

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		SourceList & sources = (*r)->region()->sources_for_edit ();
		for (SourceList::iterator s = sources.begin(); s != sources.end(); ++s) {

			XMLNode* before_cues = (*s)->get_state().child (X_("Cues"));

			if (!(*s)->remove_cue_marker (cm)) {
				delete before_cues;
				continue;
			}

			if (!in_command) {
				begin_reversible_command (_("remove cue marker"));
				in_command = true;
			}

			XMLNode* after_cues = (*s)->get_state().child (X_("Cues"));

			if (!before_cues) {
				before_cues = new XMLNode (X_("Cues"));
			}

			if (!after_cues) {
				after_cues = new XMLNode (X_("Cues"));
			}

			_session->add_command (new MementoCommand<Source> (**s, before_cues, after_cues));
		}
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::clear_region_markers ()
{

	RegionSelection rs = get_regions_from_selection_and_edit_point ();
	bool in_command = false;

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {

		CueMarkers rcm;

		(*r)->region()->get_cue_markers (rcm, true);

		if (rcm.empty()) {
			continue;
		}

		SourceList & sources = (*r)->region()->sources_for_edit ();

		for (SourceList::iterator s = sources.begin(); s != sources.end(); ++s) {

			XMLNode* before_cues = (*s)->get_state().child (X_("Cues"));
			bool failed = false;

			for (CueMarkers::iterator c = rcm.begin(); c != rcm.end(); ++c) {
				if (!(*s)->remove_cue_marker (*c)) {
					failed = true;
					break;
				}
			}

			if (failed) {
				delete before_cues;
				continue;
			}

			if (!in_command) {
				begin_reversible_command (_("clear cue markers"));
				in_command = true;
			}
			XMLNode* after_cues = (*s)->get_state().child (X_("Cues"));

			if (!before_cues) {
				before_cues = new XMLNode (X_("Cues"));
			}

			if (!after_cues) {
				after_cues = new XMLNode (X_("Cues"));
			}

			_session->add_command (new MementoCommand<Source> (**s, before_cues, after_cues));
		}
	}

	if (in_command) {
		commit_reversible_command ();
	}
}

void
Editor::make_region_markers_global (bool as_cd_marker)
{
	RegionSelection rs = get_regions_from_selection_and_edit_point ();
	XMLNode& before (_session->locations()->get_state());
	bool in_command = false;

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		CueMarkers cues;

		(*r)->region()->get_cue_markers (cues, true);

		if (!cues.empty()) {
			if (!in_command) {
				in_command = true;
			}

			for (CueMarkers::iterator cm = cues.begin(); cm != cues.end(); ++cm) {
				/* marker position is absolute within source */
				const timepos_t absolute_pos = (*r)->region()->position() + (*r)->region()->start().distance (cm->position());
				Location* loc = new Location (*_session, absolute_pos, absolute_pos, cm->text(), as_cd_marker ? Location::Flags (Location::IsMark|Location::IsCDMarker) : Location::IsMark);
				_session->locations()->add (loc, false);
			}
		}

		cues.clear ();
	}

	if (in_command) {
		XMLNode& after (_session->locations()->get_state());
		begin_reversible_command (_("region markers -> global markers"));
		_session->add_command (new MementoCommand<Locations> (*(_session->locations()), &before, &after));
		commit_reversible_command ();
	}
}

void
Editor::do_remove_gaps ()
{
	ArdourDialog d (_("Remove Gaps"), true, false);

	Gtk::HBox hpacker1;
	Gtk::Label label1 (_("Smallest gap size to remove (seconds):"));
	Gtk::Entry e1;

	hpacker1.set_spacing (12);
	hpacker1.set_border_width (12);
	hpacker1.pack_start (label1, true, false);
	hpacker1.pack_start (e1, false, false);

	Gtk::HBox hpacker2;
	Gtk::Label label2 (_("Leave a gap of(seconds):"));
	Gtk::Entry e2;

	hpacker2.set_spacing (12);
	hpacker2.set_border_width (12);
	hpacker2.pack_start (label2, true, false);
	hpacker2.pack_start (e2, false, false);

	Gtk::CheckButton markers_too (_("Shift global markers too"));

	d.get_vbox()->pack_start (hpacker1);
	d.get_vbox()->pack_start (hpacker2);
	d.get_vbox()->pack_start (markers_too);
	d.get_vbox()->show_all ();

	e2.set_activates_default ();

	d.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	d.add_button (Stock::OK, RESPONSE_OK);
	d.set_default_response (RESPONSE_OK);

  again:
	int result = d.run ();

	if (result != RESPONSE_OK) {
		return;
	}

	float threshold_secs;

	if (sscanf (e1.get_text().c_str(), "%f", &threshold_secs) != 1) {
		ArdourMessageDialog msg (_("The threshold value you entered is not a number"));
		msg.run();
		goto again;
	}

	if (threshold_secs < 0) {
		ArdourMessageDialog msg (_("The threshold value must be larger than or equal to zero"));
		msg.run();
		goto again;
	}

	samplecnt_t threshold_samples = (samplecnt_t) floor (threshold_secs * _session->sample_rate());

	float leave_secs;

	if (sscanf (e2.get_text().c_str(), "%f", &leave_secs) != 1) {
		ArdourMessageDialog msg (_("The leave-gap value you entered is not a number"));
		msg.run();
		goto again;
	}

	if (leave_secs < 0) {
		ArdourMessageDialog msg (_("The threshold value must be larger than or equal to zero"));
		msg.run ();
		goto again;
	}

	samplecnt_t leave_samples = (samplecnt_t) floor (leave_secs * _session->sample_rate());

	d.hide ();

	remove_gaps (timecnt_t (threshold_samples, AudioTime), timecnt_t (leave_samples, AudioTime), markers_too.get_active());
}

/* one day, we can use an empty lambda for this */
static
void gap_marker_callback_relax (timepos_t, timecnt_t)
{
}

void
Editor::remove_gap_marker_callback (timepos_t at, timecnt_t distance)
{
	_session->locations()->ripple (at, -distance, false, false);
}

void
Editor::remove_gaps (timecnt_t const & gap_threshold, timecnt_t const & leave_gap, bool markers_too)
{
	bool in_command = false;
	TrackViewList ts = selection->tracks.filter_to_unique_playlists ();
	XMLNode* locations_before (0);

	if (markers_too) {
		locations_before = &_session->locations()->get_state();
	}

	set<boost::shared_ptr<Playlist> > pl;

	/* it will not be possible to infer this from the set<>, so keep track
	 * of it explicitly
	 */

	boost::shared_ptr<Playlist> first_selected_playlist;

	for (TrackSelection::iterator x = ts.begin(); x != ts.end(); ++x) {

		/* don't operate on any playlist more than once, which could
		 * happen if there is more than 1 track using the same
		 * playlist.
		 */

		if ((*x)->playlist ()) {
			if (!first_selected_playlist) {
				first_selected_playlist = (*x)->playlist();
			}
			pl.insert ((*x)->playlist ());
		}
	}

	for (set<boost::shared_ptr<Playlist> >::iterator i = pl.begin(); i != pl.end(); ++i) {

		(*i)->clear_changes ();
		(*i)->clear_owned_changes ();

		if (!in_command) {
			begin_reversible_command (_("remove gaps"));
			in_command = true;
		}

		/* only move markers when closing gaps on the first
		 * selected track/playlist
		 */

		if (markers_too && (*i == first_selected_playlist)) {
			boost::function<void (timepos_t, timecnt_t)> callback (boost::bind (&Editor::remove_gap_marker_callback, this, _1, _2));
			(*i)->remove_gaps (gap_threshold, leave_gap, callback);
		} else {
			boost::function<void (timepos_t, timecnt_t)> callback (boost::bind (gap_marker_callback_relax, _1, _2));
			(*i)->remove_gaps (gap_threshold, leave_gap, callback);
		}

		(*i)->rdiff_and_add_command (_session);
	}

	if (in_command) {
		if (markers_too) {
			XMLNode* locations_after = &_session->locations()->get_state();
			_session->add_command (new MementoCommand<Locations> (*_session->locations(), locations_before, locations_after));
		}
		commit_reversible_command ();
	} else {
		if (markers_too) {
			delete locations_before;
		}
	}

	cerr << "--- rc\n";
}

bool
Editor::should_ripple () const
{
	return (Config->get_edit_mode() == Ripple ||
	        (Config->get_edit_mode() == RippleAll && (selection->tracks.size() > 1 || !Config->get_interview_editing())));
}

void
Editor::do_ripple (boost::shared_ptr<ARDOUR::Playlist> target_playlist, timepos_t const & at, timecnt_t const & distance, boost::shared_ptr<ARDOUR::Region> exclude, bool add_to_command)
{
	RegionList el;
	if (exclude) {
		el.push_back (exclude);
	}
	do_ripple (target_playlist, at, distance, &el, add_to_command);
}

void
Editor::do_ripple (boost::shared_ptr<Playlist> target_playlist, timepos_t const & at, timecnt_t const & distance, RegionList* exclude, bool add_to_command)
{
	typedef std::set<boost::shared_ptr<Playlist> > UniquePlaylists;
	UniquePlaylists playlists;

	playlists.insert (target_playlist);

	if (Config->get_edit_mode() == RippleAll) {

		TrackViewList ts = track_views.filter_to_unique_playlists ();
		boost::shared_ptr<Playlist> pl;

		for (TrackSelection::iterator x = ts.begin(); x != ts.end(); ++x) {
			if ((pl = (*x)->playlist()) == 0) {
				continue;
			}
			playlists.insert (pl);
		}
	}

	if (add_to_command) {
		for (UniquePlaylists::iterator p = playlists.begin(); p != playlists.end(); ++p) {

			(*p)->clear_changes ();
			(*p)->clear_owned_changes ();
		}
	}

	for (UniquePlaylists::iterator p = playlists.begin(); p != playlists.end(); ++p) {
		(*p)->freeze ();
	}

	for (UniquePlaylists::iterator p = playlists.begin(); p != playlists.end(); ++p) {

		/* exclude list is only for the target */

		if ((*p) == target_playlist) {

			(*p)->clear_changes ();
			(*p)->clear_owned_changes ();

			(*p)->ripple (at, distance, exclude);

			/* caller may put the target playlist into the undo
			 * history, so only do this if asked
			 */

			if (add_to_command) {
				(*p)->rdiff_and_add_command (_session);
			}
		} else {
			/* all other playlists: do the ripple, and save to undo/redo */

			(*p)->clear_changes ();
			(*p)->clear_owned_changes ();
			(*p)->ripple (at, distance, 0);
			(*p)->rdiff_and_add_command (_session);
		}

	}

	for (UniquePlaylists::iterator p = playlists.begin(); p != playlists.end(); ++p) {
		(*p)->thaw ();
	}

	/* Ripple marks & ranges if appropriate */

	if (Config->get_edit_mode() != RippleAll) {
		cerr << "out here\n";
		return;
	}

	ripple_marks (target_playlist, at, distance);
}

timepos_t
Editor::effective_ripple_mark_start (boost::shared_ptr<Playlist> target_playlist, timepos_t pos)
{
	/* in the target playlist, find the region before the target
	 * (implicitly given by @param at. Allow all markers that occur between
	 * the end of the region and @param at to move too. This is
	 * desired/expected by many (most?) ripple-edit using folk.
	 */

	boost::shared_ptr<RegionList> rl = target_playlist->region_list();
	timepos_t last_region_end_before_at (pos.time_domain());

	for (RegionList::const_iterator r = rl->begin(); r != rl->end(); ++r) {
		timepos_t region_end = (*r)->end();
		if (region_end > last_region_end_before_at && region_end < pos) {
			last_region_end_before_at = region_end;
		}
	}

	if (last_region_end_before_at < pos) {
		pos = last_region_end_before_at.increment();
	}

	return pos;
}

void
Editor::ripple_marks (boost::shared_ptr<Playlist> target_playlist, timepos_t at, timecnt_t const & distance)
{
	at = effective_ripple_mark_start (target_playlist, at);

	XMLNode& before (_session->locations()->get_state());
	/* do not move locked markers, do notify */
	_session->locations()->ripple (at, distance, false, true);
	_session->add_command (new MementoCommand<Locations> (*_session->locations(), &before, &_session->locations()->get_state()));
}
