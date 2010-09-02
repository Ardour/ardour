/*
    Copyright (C) 2000-2004 Paul Davis

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

/* Note: public Editor methods are documented in public_editor.h */

#include <unistd.h>

#include <cstdlib>
#include <cmath>
#include <string>
#include <map>
#include <set>

#include "pbd/error.h"
#include "pbd/basename.h"
#include "pbd/pthread_utils.h"
#include "pbd/memento_command.h"
#include "pbd/whitespace.h"
#include "pbd/stateful_diff_command.h"

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/popup.h>

#include "ardour/audioengine.h"
#include "ardour/session.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/audio_diskstream.h"
#include "ardour/utils.h"
#include "ardour/location.h"
#include "ardour/audio_track.h"
#include "ardour/audioplaylist.h"
#include "ardour/region_factory.h"
#include "ardour/playlist_factory.h"
#include "ardour/reverse.h"
#include "ardour/transient_detector.h"
#include "ardour/dB.h"
#include "ardour/quantize.h"
#include "ardour/strip_silence.h"
#include "ardour/route_group.h"

#include "ardour_ui.h"
#include "editor.h"
#include "time_axis_view.h"
#include "route_time_axis.h"
#include "audio_time_axis.h"
#include "automation_time_axis.h"
#include "streamview.h"
#include "audio_streamview.h"
#include "audio_region_view.h"
#include "midi_region_view.h"
#include "rgb_macros.h"
#include "selection_templates.h"
#include "selection.h"
#include "editing.h"
#include "gtk-custom-hruler.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "utils.h"
#include "editor_drag.h"
#include "strip_silence_dialog.h"
#include "editor_routes.h"
#include "editor_regions.h"
#include "quantize_dialog.h"
#include "interthread_progress_window.h"
#include "insert_time_dialog.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using Gtkmm2ext::Keyboard;

/***********************************************************************
  Editor operations
 ***********************************************************************/

void
Editor::undo (uint32_t n)
{
	if (_session) {
		_session->undo (n);
	}
}

void
Editor::redo (uint32_t n)
{
	if (_session) {
		_session->redo (n);
	}
}

void
Editor::split_regions_at (nframes64_t where, RegionSelection& regions)
{
	list <boost::shared_ptr<Playlist > > used_playlists;

	if (regions.empty()) {
		return;
	}

	begin_reversible_command (_("split"));

	// if splitting a single region, and snap-to is using
	// region boundaries, don't pay attention to them

	if (regions.size() == 1) {
		switch (_snap_type) {
		case SnapToRegionStart:
		case SnapToRegionSync:
		case SnapToRegionEnd:
			break;
		default:
			snap_to (where);
		}
	} else {
		snap_to (where);
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
			pl->freeze();
		}

		if (pl) {
                        pl->clear_changes ();
			pl->split_region ((*a)->region(), where);
			_session->add_command (new StatefulDiffCommand (pl));
		}

		a = tmp;
	}

	while (used_playlists.size() > 0) {
		list <boost::shared_ptr<Playlist > >::iterator i = used_playlists.begin();
		(*i)->thaw();
		used_playlists.pop_front();
	}

	commit_reversible_command ();
}

boost::shared_ptr<Region>
Editor::select_region_for_operation (int /*dir*/, TimeAxisView **tv)
{
	RegionView* rv;
	boost::shared_ptr<Region> region;
	nframes64_t start = 0;

	if (selection->time.start () == selection->time.end_frame ()) {

		/* no current selection-> is there a selected regionview? */

		if (selection->regions.empty()) {
			return region;
		}

	}

	if (!selection->regions.empty()) {

		rv = *(selection->regions.begin());
		(*tv) = &rv->get_time_axis_view();
		region = rv->region();

	} else if (!selection->tracks.empty()) {

		(*tv) = selection->tracks.front();

		RouteTimeAxisView* rtv;

		if ((rtv = dynamic_cast<RouteTimeAxisView*> (*tv)) != 0) {
			boost::shared_ptr<Playlist> pl;

			if ((pl = rtv->playlist()) == 0) {
				return region;
			}

			region = pl->top_region_at (start);
		}
	}

	return region;
}

void
Editor::extend_selection_to_end_of_region (bool next)
{
	TimeAxisView *tv;
	boost::shared_ptr<Region> region;
	nframes64_t start;

	if ((region = select_region_for_operation (next ? 1 : 0, &tv)) == 0) {
		return;
	}

	if (region && selection->time.start () == selection->time.end_frame ()) {
		start = region->position();
	} else {
		start = selection->time.start ();
	}

	begin_reversible_command (_("extend selection"));
	selection->set (start, region->position() + region->length());
	commit_reversible_command ();
}

void
Editor::extend_selection_to_start_of_region (bool previous)
{
	TimeAxisView *tv;
	boost::shared_ptr<Region> region;
	nframes64_t end;

	if ((region = select_region_for_operation (previous ? -1 : 0, &tv)) == 0) {
		return;
	}

	if (region && selection->time.start () == selection->time.end_frame ()) {
		end = region->position() + region->length();
	} else {
		end = selection->time.end_frame ();
	}

	/* Try to leave the selection with the same route if possible */

	begin_reversible_command (_("extend selection"));
	selection->set (region->position(), end);
	commit_reversible_command ();
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
	nframes64_t distance;
	nframes64_t next_distance;
	RegionSelection rs;

	get_regions_for_action (rs);

	if (!_session) return;

	if (!force_playhead && !rs.empty()) {

		begin_reversible_command (_("nudge regions forward"));

		for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());

			distance = get_nudge_distance (r->position(), next_distance);

			if (next) {
				distance = next_distance;
			}

                        r->clear_changes ();
			r->set_position (r->position() + distance, this);
			_session->add_command (new StatefulDiffCommand (r));
		}

		commit_reversible_command ();


	} else if (!force_playhead && !selection->markers.empty()) {

		bool is_start;

		begin_reversible_command (_("nudge location forward"));

		for (MarkerSelection::iterator i = selection->markers.begin(); i != selection->markers.end(); ++i) {

			Location* loc = find_location_from_marker ((*i), is_start);

			if (loc) {

				XMLNode& before (loc->get_state());

				if (is_start) {
					distance = get_nudge_distance (loc->start(), next_distance);
					if (next) {
						distance = next_distance;
					}
					if (max_frames - distance > loc->start() + loc->length()) {
						loc->set_start (loc->start() + distance);
					} else {
						loc->set_start (max_frames - loc->length());
					}
				} else {
					distance = get_nudge_distance (loc->end(), next_distance);
					if (next) {
						distance = next_distance;
					}
					if (max_frames - distance > loc->end()) {
						loc->set_end (loc->end() + distance);
					} else {
						loc->set_end (max_frames);
					}
				}
				XMLNode& after (loc->get_state());
				_session->add_command (new MementoCommand<Location>(*loc, &before, &after));
			}
		}

		commit_reversible_command ();

	} else {
		distance = get_nudge_distance (playhead_cursor->current_frame, next_distance);
		_session->request_locate (playhead_cursor->current_frame + distance);
	}
}

void
Editor::nudge_backward (bool next, bool force_playhead)
{
	nframes64_t distance;
	nframes64_t next_distance;
	RegionSelection rs;

	get_regions_for_action (rs);

	if (!_session) return;

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
				r->set_position (r->position() - distance, this);
			} else {
				r->set_position (0, this);
			}
			_session->add_command (new StatefulDiffCommand (r));
		}

		commit_reversible_command ();

	} else if (!force_playhead && !selection->markers.empty()) {

		bool is_start;

		begin_reversible_command (_("nudge location forward"));

		for (MarkerSelection::iterator i = selection->markers.begin(); i != selection->markers.end(); ++i) {

			Location* loc = find_location_from_marker ((*i), is_start);

			if (loc) {

				XMLNode& before (loc->get_state());

				if (is_start) {
					distance = get_nudge_distance (loc->start(), next_distance);
					if (next) {
						distance = next_distance;
					}
					if (distance < loc->start()) {
						loc->set_start (loc->start() - distance);
					} else {
						loc->set_start (0);
					}
				} else {
					distance = get_nudge_distance (loc->end(), next_distance);

					if (next) {
						distance = next_distance;
					}

					if (distance < loc->end() - loc->length()) {
						loc->set_end (loc->end() - distance);
					} else {
						loc->set_end (loc->length());
					}
				}

				XMLNode& after (loc->get_state());
				_session->add_command (new MementoCommand<Location>(*loc, &before, &after));
			}
		}

		commit_reversible_command ();

	} else {

		distance = get_nudge_distance (playhead_cursor->current_frame, next_distance);

		if (playhead_cursor->current_frame > distance) {
			_session->request_locate (playhead_cursor->current_frame - distance);
		} else {
			_session->goto_start();
		}
	}
}

void
Editor::nudge_forward_capture_offset ()
{
	nframes64_t distance;
	RegionSelection rs;

	get_regions_for_action (rs);

	if (!_session) return;

	if (!rs.empty()) {

		begin_reversible_command (_("nudge forward"));

		distance = _session->worst_output_latency();

		for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());

                        r->clear_changes ();
			r->set_position (r->position() + distance, this);
			_session->add_command(new StatefulDiffCommand (r));
		}

		commit_reversible_command ();

	}
}

void
Editor::nudge_backward_capture_offset ()
{
	nframes64_t distance;
	RegionSelection rs;

	get_regions_for_action (rs);

	if (!_session) return;

	if (!rs.empty()) {

		begin_reversible_command (_("nudge forward"));

		distance = _session->worst_output_latency();

		for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());

                        r->clear_changes ();

			if (r->position() > distance) {
				r->set_position (r->position() - distance, this);
			} else {
				r->set_position (0, this);
			}
			_session->add_command(new StatefulDiffCommand (r));
		}

		commit_reversible_command ();
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

	_session->request_locate (_session->current_end_frame());
}

void
Editor::build_region_boundary_cache ()
{
	framepos_t pos = 0;
	vector<RegionPoint> interesting_points;
	boost::shared_ptr<Region> r;
	TrackViewList tracks;
	bool at_end = false;

	region_boundary_cache.clear ();

	if (_session == 0) {
		return;
	}

	switch (_snap_type) {
	case SnapToRegionStart:
		interesting_points.push_back (Start);
		break;
	case SnapToRegionEnd:
		interesting_points.push_back (End);
		break;
	case SnapToRegionSync:
		interesting_points.push_back (SyncPoint);
		break;
	case SnapToRegionBoundary:
		interesting_points.push_back (Start);
		interesting_points.push_back (End);
		break;
	default:
		fatal << string_compose (_("build_region_boundary_cache called with snap_type = %1"), _snap_type) << endmsg;
		/*NOTREACHED*/
		return;
	}

	TimeAxisView *ontrack = 0;
	TrackViewList tlist;

	if (!selection->tracks.empty()) {
		tlist = selection->tracks;
	} else {
		tlist = track_views;
	}

	while (pos < _session->current_end_frame() && !at_end) {

		framepos_t rpos;
		framepos_t lpos = max_frames;

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
				rpos = r->first_frame();
				break;

			case End:
				rpos = r->last_frame();
				break;

			case SyncPoint:
				rpos = r->sync_position ();
				break;

			default:
				break;
			}

			float speed = 1.0f;
			RouteTimeAxisView *rtav;

			if (ontrack != 0 && (rtav = dynamic_cast<RouteTimeAxisView*>(ontrack)) != 0 ) {
				if (rtav->track() != 0) {
					speed = rtav->track()->speed();
				}
			}

			rpos = track_frame_to_session_frame (rpos, speed);

			if (rpos < lpos) {
				lpos = rpos;
			}

			/* prevent duplicates, but we don't use set<> because we want to be able
			   to sort later.
			*/

			vector<framepos_t>::iterator ri;

			for (ri = region_boundary_cache.begin(); ri != region_boundary_cache.end(); ++ri) {
				if (*ri == rpos) {
					break;
				}
			}

			if (ri == region_boundary_cache.end()) {
				region_boundary_cache.push_back (rpos);
			}
		}

		pos = lpos + 1;
	}

	/* finally sort to be sure that the order is correct */

	sort (region_boundary_cache.begin(), region_boundary_cache.end());

        cerr << "RBC contains " << region_boundary_cache.size() << endl;

        for (vector<framepos_t>::iterator x = region_boundary_cache.begin(); x != region_boundary_cache.end(); ++x) {
                cerr << "Region boundary @ " << *x << endl;
        }
}

boost::shared_ptr<Region>
Editor::find_next_region (framepos_t frame, RegionPoint point, int32_t dir, TrackViewList& tracks, TimeAxisView **ontrack)
{
	TrackViewList::iterator i;
	nframes64_t closest = max_frames;
	boost::shared_ptr<Region> ret;
	framepos_t rpos = 0;

	float track_speed;
	framepos_t track_frame;
	RouteTimeAxisView *rtav;

	for (i = tracks.begin(); i != tracks.end(); ++i) {

		framecnt_t distance;
		boost::shared_ptr<Region> r;

		track_speed = 1.0f;
		if ( (rtav = dynamic_cast<RouteTimeAxisView*>(*i)) != 0 ) {
			if (rtav->track()!=0)
				track_speed = rtav->track()->speed();
		}

		track_frame = session_frame_to_track_frame(frame, track_speed);

		if ((r = (*i)->find_next_region (track_frame, point, dir)) == 0) {
			continue;
		}

		switch (point) {
		case Start:
			rpos = r->first_frame ();
			break;

		case End:
			rpos = r->last_frame ();
			break;

		case SyncPoint:
			rpos = r->sync_position ();
			break;
		}

		// rpos is a "track frame", converting it to "_session frame"
		rpos = track_frame_to_session_frame(rpos, track_speed);

		if (rpos > frame) {
			distance = rpos - frame;
		} else {
			distance = frame - rpos;
		}

		if (distance < closest) {
			closest = distance;
			if (ontrack != 0)
				*ontrack = (*i);
			ret = r;
		}
	}

	return ret;
}

framepos_t
Editor::find_next_region_boundary (framepos_t pos, int32_t dir, const TrackViewList& tracks)
{
	framecnt_t distance = max_frames;
	framepos_t current_nearest = -1;

	for (TrackViewList::const_iterator i = tracks.begin(); i != tracks.end(); ++i) {
		framepos_t contender;
		framecnt_t d;
                
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);

		if (!rtv) {
			continue;
		}

		if ((contender = rtv->find_next_region_boundary (pos, dir)) < 0) {
			continue;
		}

		d = ::llabs (pos - contender);

		if (d < distance) {
			current_nearest = contender;
			distance = d;
		}
	}

	return current_nearest;
}

framepos_t
Editor::get_region_boundary (framepos_t pos, int32_t dir, bool with_selection, bool only_onscreen)
{
	framepos_t target;
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
	nframes64_t pos = playhead_cursor->current_frame;
	nframes64_t target;

	if (!_session) {
		return;
	}

	// so we don't find the current region again..
	if (dir > 0 || pos > 0) {
		pos += dir;
	}

	if ((target = get_region_boundary (pos, dir, with_selection, false)) < 0) {
		return;
	}

	_session->request_locate (target);
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
	nframes64_t pos = cursor->current_frame;

	if (!_session) {
		return;
	}

	TimeAxisView *ontrack = 0;

	// so we don't find the current region again..
	if (dir>0 || pos>0)
		pos+=dir;

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

	switch (point){
	case Start:
		pos = r->first_frame ();
		break;

	case End:
		pos = r->last_frame ();
		break;

	case SyncPoint:
		pos = r->sync_position ();
		break;
	}

	float speed = 1.0f;
	RouteTimeAxisView *rtav;

	if ( ontrack != 0 && (rtav = dynamic_cast<RouteTimeAxisView*>(ontrack)) != 0 ) {
		if (rtav->track() != 0) {
			speed = rtav->track()->speed();
		}
	}

	pos = track_frame_to_session_frame(pos, speed);

	if (cursor == playhead_cursor) {
		_session->request_locate (pos);
	} else {
		cursor->set_position (pos);
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
	nframes64_t pos = 0;
	RegionSelection rs;

	get_regions_for_action (rs);

	switch (mouse_mode) {
	case MouseObject:
		if (!rs.empty()) {
			pos = rs.start();
		}
		break;

	case MouseRange:
		if (!selection->time.empty()) {
			pos = selection->time.start ();
		}
		break;

	default:
		return;
	}

	if (cursor == playhead_cursor) {
		_session->request_locate (pos);
	} else {
		cursor->set_position (pos);
	}
}

void
Editor::cursor_to_selection_end (EditorCursor *cursor)
{
	nframes64_t pos = 0;
	RegionSelection rs;

	get_regions_for_action (rs);

	switch (mouse_mode) {
	case MouseObject:
		if (!rs.empty()) {
			pos = rs.end_frame();
		}
		break;

	case MouseRange:
		if (!selection->time.empty()) {
			pos = selection->time.end_frame ();
		}
		break;

	default:
		return;
	}

	if (cursor == playhead_cursor) {
		_session->request_locate (pos);
	} else {
		cursor->set_position (pos);
	}
}

void
Editor::selected_marker_to_region_boundary (bool with_selection, int32_t dir)
{
	nframes64_t target;
	Location* loc;
	bool ignored;

	if (!_session) {
		return;
	}

	if (selection->markers.empty()) {
		nframes64_t mouse;
		bool ignored;

		if (!mouse_frame (mouse, ignored)) {
			return;
		}

		add_location_mark (mouse);
	}

	if ((loc = find_location_from_marker (selection->markers.front(), ignored)) == 0) {
		return;
	}

	nframes64_t pos = loc->start();

	// so we don't find the current region again..
	if (dir > 0 || pos > 0) {
		pos += dir;
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
	nframes64_t pos;
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
		pos+=dir;

	if (!selection->tracks.empty()) {

		r = find_next_region (pos, point, dir, selection->tracks, &ontrack);

	} else {

		r = find_next_region (pos, point, dir, track_views, &ontrack);
	}

	if (r == 0) {
		return;
	}

	switch (point){
	case Start:
		pos = r->first_frame ();
		break;

	case End:
		pos = r->last_frame ();
		break;

	case SyncPoint:
		pos = r->adjust_to_sync (r->first_frame());
		break;
	}

	float speed = 1.0f;
	RouteTimeAxisView *rtav;

	if (ontrack != 0 && (rtav = dynamic_cast<RouteTimeAxisView*>(ontrack)) != 0) {
		if (rtav->track() != 0) {
			speed = rtav->track()->speed();
		}
	}

	pos = track_frame_to_session_frame(pos, speed);

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
	nframes64_t pos = 0;
	Location* loc;
	bool ignored;

	if (!_session || selection->markers.empty()) {
		return;
	}

	if ((loc = find_location_from_marker (selection->markers.front(), ignored)) == 0) {
		return;
	}

	RegionSelection rs;

	get_regions_for_action (rs);

	switch (mouse_mode) {
	case MouseObject:
		if (!rs.empty()) {
			pos = rs.start();
		}
		break;

	case MouseRange:
		if (!selection->time.empty()) {
			pos = selection->time.start ();
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
	nframes64_t pos = 0;
	Location* loc;
	bool ignored;

	if (!_session || selection->markers.empty()) {
		return;
	}

	if ((loc = find_location_from_marker (selection->markers.front(), ignored)) == 0) {
		return;
	}

	RegionSelection rs;

	get_regions_for_action (rs);

	switch (mouse_mode) {
	case MouseObject:
		if (!rs.empty()) {
			pos = rs.end_frame();
		}
		break;

	case MouseRange:
		if (!selection->time.empty()) {
			pos = selection->time.end_frame ();
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
	nframes64_t pos = playhead_cursor->current_frame;
	nframes64_t delta = (nframes64_t) floor (current_page_frames() / 0.8);

	if (forward) {
		if (pos == max_frames) {
			return;
		}

		if (pos < max_frames - delta) {
			pos += delta ;
		} else {
			pos = max_frames;
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
Editor::playhead_backward ()
{
	nframes64_t pos;
	nframes64_t cnt;
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (nframes64_t) floor (prefix * _session->frame_rate ());
		} else {
			cnt = (nframes64_t) prefix;
		}
	}

	pos = playhead_cursor->current_frame;

	if ((nframes64_t) pos < cnt) {
		pos = 0;
	} else {
		pos -= cnt;
	}

	/* XXX this is completely insane. with the current buffering
	   design, we'll force a complete track buffer flush and
	   reload, just to move 1 sample !!!
	*/

	_session->request_locate (pos);
}

void
Editor::playhead_forward ()
{
	nframes64_t pos;
	nframes64_t cnt;
	bool was_floating;
	float prefix;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (nframes64_t) floor (prefix * _session->frame_rate ());
		} else {
			cnt = (nframes64_t) floor (prefix);
		}
	}

	pos = playhead_cursor->current_frame;

	/* XXX this is completely insane. with the current buffering
	   design, we'll force a complete track buffer flush and
	   reload, just to move 1 sample !!!
	*/

	_session->request_locate (pos+cnt);
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

		_session->request_locate (selection->markers.front()->position(), _session->transport_rolling());

	} else {
		/* move selected markers to playhead */

		for (MarkerSelection::iterator i = selection->markers.begin(); i != selection->markers.end(); ++i) {
			bool ignored;

			Location* loc = find_location_from_marker (*i, ignored);

			if (loc->is_mark()) {
				loc->set_start (playhead_cursor->current_frame);
			} else {
				loc->set (playhead_cursor->current_frame,
					  playhead_cursor->current_frame + loc->length());
			}
		}
	}
}

void
Editor::edit_cursor_backward ()
{
	nframes64_t pos;
	nframes64_t cnt;
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (nframes64_t) floor (prefix * _session->frame_rate ());
		} else {
			cnt = (nframes64_t) prefix;
		}
	}

	if ((pos = get_preferred_edit_position()) < 0) {
		return;
	}

	if (pos < cnt) {
		pos = 0;
	} else {
		pos -= cnt;
	}

	// EDIT CURSOR edit_cursor->set_position (pos);
}

void
Editor::edit_cursor_forward ()
{
	//nframes64_t pos;
	nframes64_t cnt;
	bool was_floating;
	float prefix;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (nframes64_t) floor (prefix * _session->frame_rate ());
		} else {
			cnt = (nframes64_t) floor (prefix);
		}
	}

	// pos = edit_cursor->current_frame;
	// EDIT CURSOR edit_cursor->set_position (pos+cnt);
}

void
Editor::goto_frame ()
{
	float prefix;
	bool was_floating;
	nframes64_t frame;

	if (get_prefix (prefix, was_floating)) {
		return;
	}

	if (was_floating) {
		frame = (nframes64_t) floor (prefix * _session->frame_rate());
	} else {
		frame = (nframes64_t) floor (prefix);
	}

	_session->request_locate (frame);
}

void
Editor::scroll_backward (float pages)
{
	nframes64_t frame;
	nframes64_t one_page = (nframes64_t) rint (_canvas_width * frames_per_unit);
	bool was_floating;
	float prefix;
	nframes64_t cnt;

	if (get_prefix (prefix, was_floating)) {
		cnt = (nframes64_t) floor (pages * one_page);
	} else {
		if (was_floating) {
			cnt = (nframes64_t) floor (prefix * _session->frame_rate());
		} else {
			cnt = (nframes64_t) floor (prefix * one_page);
		}
	}

	if (leftmost_frame < cnt) {
		frame = 0;
	} else {
		frame = leftmost_frame - cnt;
	}

	reset_x_origin (frame);
}

void
Editor::scroll_forward (float pages)
{
	nframes64_t frame;
	nframes64_t one_page = (nframes64_t) rint (_canvas_width * frames_per_unit);
	bool was_floating;
	float prefix;
	nframes64_t cnt;

	if (get_prefix (prefix, was_floating)) {
		cnt = (nframes64_t) floor (pages * one_page);
	} else {
		if (was_floating) {
			cnt = (nframes64_t) floor (prefix * _session->frame_rate());
		} else {
			cnt = (nframes64_t) floor (prefix * one_page);
		}
	}

	if (max_frames - cnt < leftmost_frame) {
		frame = max_frames - cnt;
	} else {
		frame = leftmost_frame + cnt;
	}

	reset_x_origin (frame);
}

void
Editor::scroll_tracks_down ()
{
	float prefix;
	bool was_floating;
	int cnt;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		cnt = (int) floor (prefix);
	}

	double vert_value = vertical_adjustment.get_value() + (cnt *
		vertical_adjustment.get_page_size());
	if (vert_value > vertical_adjustment.get_upper() - _canvas_height) {
		vert_value = vertical_adjustment.get_upper() - _canvas_height;
	}
	vertical_adjustment.set_value (vert_value);
}

void
Editor::scroll_tracks_up ()
{
	float prefix;
	bool was_floating;
	int cnt;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		cnt = (int) floor (prefix);
	}

	vertical_adjustment.set_value (vertical_adjustment.get_value() - (cnt * vertical_adjustment.get_page_size()));
}

void
Editor::scroll_tracks_down_line ()
{
	double vert_value = vertical_adjustment.get_value() + 60;

	if (vert_value > vertical_adjustment.get_upper() - _canvas_height) {
		vert_value = vertical_adjustment.get_upper() - _canvas_height;
	}
	
	vertical_adjustment.set_value (vert_value);
}

void
Editor::scroll_tracks_up_line ()
{
	reset_y_origin (vertical_adjustment.get_value() - 60);
}

/* ZOOM */

void
Editor::tav_zoom_step (bool coarser)
{
	ENSURE_GUI_THREAD (*this, &Editor::temporal_zoom_step, coarser)

	_routes->suspend_redisplay ();

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		TimeAxisView *tv = (static_cast<TimeAxisView*>(*i));
			tv->step_height (coarser);
	}

	_routes->resume_redisplay ();
}

void
Editor::temporal_zoom_step (bool coarser)
{
	ENSURE_GUI_THREAD (*this, &Editor::temporal_zoom_step, coarser)

	double nfpu;

	nfpu = frames_per_unit;

	if (coarser) {
		nfpu *= 1.61803399;
	} else {
		nfpu = max(1.0,(nfpu/1.61803399));
	}

	temporal_zoom (nfpu);
}

void
Editor::temporal_zoom (gdouble fpu)
{
	if (!_session) return;

	nframes64_t current_page = current_page_frames();
	nframes64_t current_leftmost = leftmost_frame;
	nframes64_t current_rightmost;
	nframes64_t current_center;
	nframes64_t new_page_size;
	nframes64_t half_page_size;
	nframes64_t leftmost_after_zoom = 0;
	nframes64_t where;
	bool in_track_canvas;
	double nfpu;
	double l;

	/* XXX this limit is also in ::set_frames_per_unit() */

	if (frames_per_unit <= 1.0 && fpu <= frames_per_unit) {
		return;
	}

	nfpu = fpu;

	new_page_size = (nframes64_t) floor (_canvas_width * nfpu);
	half_page_size = new_page_size / 2;

	switch (zoom_focus) {
	case ZoomFocusLeft:
		leftmost_after_zoom = current_leftmost;
		break;

	case ZoomFocusRight:
		current_rightmost = leftmost_frame + current_page;
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
		l = playhead_cursor->current_frame - (new_page_size * 0.5);

		if (l < 0) {
			leftmost_after_zoom = 0;
		} else if (l > max_frames) {
			leftmost_after_zoom = max_frames - new_page_size;
		} else {
			leftmost_after_zoom = (nframes64_t) l;
		}
		break;

	case ZoomFocusMouse:
		/* try to keep the mouse over the same point in the display */

		if (!mouse_frame (where, in_track_canvas)) {
			/* use playhead instead */
			where = playhead_cursor->current_frame;

			if (where < half_page_size) {
				leftmost_after_zoom = 0;
			} else {
				leftmost_after_zoom = where - half_page_size;
			}

		} else {

			l = - ((new_page_size * ((where - current_leftmost)/(double)current_page)) - where);

			if (l < 0) {
				leftmost_after_zoom = 0;
			} else if (l > max_frames) {
				leftmost_after_zoom = max_frames - new_page_size;
			} else {
				leftmost_after_zoom = (nframes64_t) l;
			}
		}

		break;

	case ZoomFocusEdit:
		/* try to keep the edit point in the same place */
		where = get_preferred_edit_position ();

		if (where > 0) {

			double l = - ((new_page_size * ((where - current_leftmost)/(double)current_page)) - where);

			if (l < 0) {
				leftmost_after_zoom = 0;
			} else if (l > max_frames) {
				leftmost_after_zoom = max_frames - new_page_size;
			} else {
				leftmost_after_zoom = (nframes64_t) l;
			}

		} else {
			/* edit point not defined */
			return;
		}
		break;

	}

	// leftmost_after_zoom = min (leftmost_after_zoom, _session->current_end_frame());

	reposition_and_zoom (leftmost_after_zoom, nfpu);
}

void
Editor::temporal_zoom_region (bool both_axes)
{

	nframes64_t start = max_frames;
	nframes64_t end = 0;
	RegionSelection rs;
	set<TimeAxisView*> tracks;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {

		if ((*i)->region()->position() < start) {
			start = (*i)->region()->position();
		}

		if ((*i)->region()->last_frame() + 1 > end) {
			end = (*i)->region()->last_frame() + 1;
		}

		tracks.insert (&((*i)->get_time_axis_view()));
	}

	/* now comes an "interesting" hack ... make sure we leave a little space
	   at each end of the editor so that the zoom doesn't fit the region
	   precisely to the screen.
	*/

	GdkScreen* screen = gdk_screen_get_default ();
	gint pixwidth = gdk_screen_get_width (screen);
	gint mmwidth = gdk_screen_get_width_mm (screen);
	double pix_per_mm = (double) pixwidth/ (double) mmwidth;
	double one_centimeter_in_pixels = pix_per_mm * 10.0;

	if ((start == 0 && end == 0) || end < start) {
		return;
	}

	nframes64_t range = end - start;
	double new_fpu = (double)range / (double)_canvas_width;
	nframes64_t extra_samples = (nframes64_t) floor (one_centimeter_in_pixels * new_fpu);

	if (start > extra_samples) {
		start -= extra_samples;
	} else {
		start = 0;
	}

	if (max_frames - extra_samples > end) {
		end += extra_samples;
	} else {
		end = max_frames;
	}

	if (both_axes) {
		/* save visual state with track states included, and prevent
		   set_frames_per_unit() from doing it again.
		*/
		undo_visual_stack.push_back (current_visual_state(true));
		no_save_visual = true;
	}

	temporal_zoom_by_frame (start, end, "zoom to region");

	if (both_axes) {
		uint32_t per_track_height = (uint32_t) floor ((_canvas_height - canvas_timebars_vsize - 10.0) / tracks.size());

		/* set visible track heights appropriately */

		for (set<TimeAxisView*>::iterator t = tracks.begin(); t != tracks.end(); ++t) {
			(*t)->set_height (per_track_height);
		}

		/* hide irrelevant tracks */

		_routes->suspend_redisplay ();

		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if (find (tracks.begin(), tracks.end(), (*i)) == tracks.end()) {
				hide_track_in_display (*i, true);
			}
		}

		_routes->resume_redisplay ();

		vertical_adjustment.set_value (0.0);
		no_save_visual = false;
	}

	redo_visual_stack.push_back (current_visual_state());
}

void
Editor::zoom_to_region (bool both_axes)
{
	temporal_zoom_region (both_axes);
}

void
Editor::temporal_zoom_selection ()
{
	if (!selection) return;

	if (selection->time.empty()) {
		return;
	}

	nframes64_t start = selection->time[clicked_selection].start;
	nframes64_t end = selection->time[clicked_selection].end;

	temporal_zoom_by_frame (start, end, "zoom to selection");
}

void
Editor::temporal_zoom_session ()
{
	ENSURE_GUI_THREAD (*this, &Editor::temporal_zoom_session)

	if (_session) {
		nframes_t const l = _session->current_end_frame() - _session->current_start_frame();
		double s = _session->current_start_frame() - l * 0.01;
		if (s < 0) {
			s = 0;
		}
		nframes_t const e = _session->current_end_frame() + l * 0.01;
		temporal_zoom_by_frame (nframes_t (s), e, "zoom to _session");
	}
}

void
Editor::temporal_zoom_by_frame (nframes64_t start, nframes64_t end, const string & /*op*/)
{
	if (!_session) return;

	if ((start == 0 && end == 0) || end < start) {
		return;
	}

	nframes64_t range = end - start;

	double new_fpu = (double)range / (double)_canvas_width;

	nframes64_t new_page = (nframes64_t) floor (_canvas_width * new_fpu);
	nframes64_t middle = (nframes64_t) floor( (double)start + ((double)range / 2.0f ));
	nframes64_t new_leftmost = (nframes64_t) floor( (double)middle - ((double)new_page/2.0f));

	if (new_leftmost > middle) {
		new_leftmost = 0;
	}

	reposition_and_zoom (new_leftmost, new_fpu);
}

void
Editor::temporal_zoom_to_frame (bool coarser, nframes64_t frame)
{
	if (!_session) {
		return;
	}
	double range_before = frame - leftmost_frame;
	double new_fpu;

	new_fpu = frames_per_unit;

	if (coarser) {
		new_fpu *= 1.61803399;
		range_before *= 1.61803399;
	} else {
		new_fpu = max(1.0,(new_fpu/1.61803399));
		range_before /= 1.61803399;
	}

	if (new_fpu == frames_per_unit)  {
		return;
	}

	nframes64_t new_leftmost = frame - (nframes64_t)range_before;

	if (new_leftmost > frame) {
		new_leftmost = 0;
	}
//	begin_reversible_command (_("zoom to frame"));
//	_session->add_undo (sigc::bind (sigc::mem_fun(*this, &Editor::reposition_and_zoom), leftmost_frame, frames_per_unit));
//	_session->add_redo (sigc::bind (sigc::mem_fun(*this, &Editor::reposition_and_zoom), new_leftmost, new_fpu));
//	commit_reversible_command ();

	reposition_and_zoom (new_leftmost, new_fpu);
}


bool
Editor::choose_new_marker_name(string &name) {

	if (!Config->get_name_new_markers()) {
		/* don't prompt user for a new name */
		return true;
	}

	ArdourPrompter dialog (true);

	dialog.set_prompt (_("New Name:"));

	dialog.set_title (_("New Location Marker"));

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

	nframes64_t start = selection->time[clicked_selection].start;
	nframes64_t end = selection->time[clicked_selection].end;

	_session->locations()->next_available_name(rangename,"selection");
	Location *location = new Location (*_session, start, end, rangename, Location::IsRangeMarker);

	_session->begin_reversible_command (_("add marker"));
        XMLNode &before = _session->locations()->get_state();
	_session->locations()->add (location, true);
        XMLNode &after = _session->locations()->get_state();
	_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	_session->commit_reversible_command ();
}

void
Editor::add_location_mark (nframes64_t where)
{
	string markername;

	select_new_marker = true;

	_session->locations()->next_available_name(markername,"mark");
	if (!choose_new_marker_name(markername)) {
		return;
	}
	Location *location = new Location (*_session, where, where, markername, Location::IsMark);
	_session->begin_reversible_command (_("add marker"));
        XMLNode &before = _session->locations()->get_state();
	_session->locations()->add (location, true);
        XMLNode &after = _session->locations()->get_state();
	_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	_session->commit_reversible_command ();
}

void
Editor::add_location_from_playhead_cursor ()
{
	add_location_mark (_session->audible_frame());
}

void
Editor::add_locations_from_audio_region ()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	_session->begin_reversible_command (rs.size () > 1 ? _("add markers") : _("add marker"));
	XMLNode &before = _session->locations()->get_state();

	for (RegionSelection::iterator i = rs.begin (); i != rs.end (); ++i) {

		boost::shared_ptr<Region> region = (*i)->region ();

		Location *location = new Location (*_session, region->position(), region->last_frame(), region->name(), Location::IsRangeMarker);

		_session->locations()->add (location, true);
	}

	XMLNode &after = _session->locations()->get_state();
	_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	_session->commit_reversible_command ();
}

void
Editor::add_location_from_audio_region ()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	_session->begin_reversible_command (_("add marker"));
	XMLNode &before = _session->locations()->get_state();

	string markername;

	if (rs.size() > 1) {		// more than one region selected
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
	Location *location = new Location (*_session, rs.start(), rs.end_frame(), markername, Location::IsRangeMarker);
	_session->locations()->add (location, true);

	XMLNode &after = _session->locations()->get_state();
	_session->add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	_session->commit_reversible_command ();
}

void
Editor::amplitude_zoom_step (bool in)
{
	gdouble zoom = 1.0;

	if (in) {
		zoom *= 2.0;
	} else {
		if (zoom > 2.0) {
			zoom /= 2.0;
		} else {
			zoom = 1.0;
		}
	}

#ifdef FIX_FOR_CANVAS
	/* XXX DO SOMETHING */
#endif
}


/* DELETION */


void
Editor::delete_sample_forward ()
{
}

void
Editor::delete_sample_backward ()
{
}

void
Editor::delete_screen ()
{
}

/* SEARCH */

void
Editor::search_backwards ()
{
	/* what ? */
}

void
Editor::search_forwards ()
{
	/* what ? */
}

/* MARKS */

void
Editor::jump_forward_to_mark ()
{
	if (!_session) {
		return;
	}

	Location *location = _session->locations()->first_location_after (playhead_cursor->current_frame);

	if (location) {
		_session->request_locate (location->start(), _session->transport_rolling());
	} else {
		_session->request_locate (_session->current_end_frame());
	}
}

void
Editor::jump_backward_to_mark ()
{
	if (!_session) {
		return;
	}

	Location *location = _session->locations()->first_location_before (playhead_cursor->current_frame);

	if (location) {
		_session->request_locate (location->start(), _session->transport_rolling());
	} else {
		_session->goto_start ();
	}
}

void
Editor::set_mark ()
{
	nframes64_t pos;
	float prefix;
	bool was_floating;
	string markername;

	if (get_prefix (prefix, was_floating)) {
		pos = _session->audible_frame ();
	} else {
		if (was_floating) {
			pos = (nframes64_t) floor (prefix * _session->frame_rate ());
		} else {
			pos = (nframes64_t) floor (prefix);
		}
	}

	_session->locations()->next_available_name(markername,"mark");
	if (!choose_new_marker_name(markername)) {
		return;
	}
	_session->locations()->add (new Location (*_session, pos, 0, markername, Location::IsMark), true);
}

void
Editor::clear_markers ()
{
	if (_session) {
		_session->begin_reversible_command (_("clear markers"));
                XMLNode &before = _session->locations()->get_state();
		_session->locations()->clear_markers ();
                XMLNode &after = _session->locations()->get_state();
		_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
		_session->commit_reversible_command ();
	}
}

void
Editor::clear_ranges ()
{
	if (_session) {
		_session->begin_reversible_command (_("clear ranges"));
                XMLNode &before = _session->locations()->get_state();

		Location * looploc = _session->locations()->auto_loop_location();
		Location * punchloc = _session->locations()->auto_punch_location();

		_session->locations()->clear_ranges ();
		// re-add these
		if (looploc) _session->locations()->add (looploc);
		if (punchloc) _session->locations()->add (punchloc);

                XMLNode &after = _session->locations()->get_state();
		_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
		_session->commit_reversible_command ();
	}
}

void
Editor::clear_locations ()
{
	_session->begin_reversible_command (_("clear locations"));
        XMLNode &before = _session->locations()->get_state();
	_session->locations()->clear ();
        XMLNode &after = _session->locations()->get_state();
	_session->add_command(new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	_session->commit_reversible_command ();
	_session->locations()->clear ();
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
Editor::insert_region_list_drag (boost::shared_ptr<Region> region, int x, int y)
{
	double wx, wy;
	double cx, cy;
	nframes64_t where;
	RouteTimeAxisView *rtv = 0;
	boost::shared_ptr<Playlist> playlist;

	track_canvas->window_to_world (x, y, wx, wy);

	GdkEvent event;
	event.type = GDK_BUTTON_RELEASE;
	event.button.x = wx;
	event.button.y = wy;

	where = event_frame (&event, &cx, &cy);

	if (where < leftmost_frame || where > leftmost_frame + current_page_frames()) {
		/* clearly outside canvas area */
		return;
	}

	std::pair<TimeAxisView*, int> tv = trackview_by_y_position (cy);
	if (tv.first == 0) {
		return;
	}

	if ((rtv = dynamic_cast<RouteTimeAxisView*> (tv.first)) == 0) {
		return;
	}

	if ((playlist = rtv->playlist()) == 0) {
		return;
	}

	snap_to (where);

	begin_reversible_command (_("insert dragged region"));
        playlist->clear_changes ();
	playlist->add_region (RegionFactory::create (region), where, 1.0);
	_session->add_command(new StatefulDiffCommand (playlist));
	commit_reversible_command ();
}

void
Editor::insert_route_list_drag (boost::shared_ptr<Route> route, int x, int y)
{
	double wx, wy;
	double cx, cy;
	nframes_t where;
	RouteTimeAxisView *dest_rtv = 0;
	RouteTimeAxisView *source_rtv = 0;

	track_canvas->window_to_world (x, y, wx, wy);
	wx += horizontal_position ();
	wy += vertical_adjustment.get_value();

	GdkEvent event;
	event.type = GDK_BUTTON_RELEASE;
	event.button.x = wx;
	event.button.y = wy;

	where = event_frame (&event, &cx, &cy);

	std::pair<TimeAxisView*, int> const tv = trackview_by_y_position (cy);
	if (tv.first == 0) {
		return;
	}

	if ((dest_rtv = dynamic_cast<RouteTimeAxisView*> (tv.first)) == 0) {
		return;
	}

	/* use this drag source to add underlay to a track. But we really don't care
	   about the Route, only the view of the route, so find it first */
	for(TrackViewList::iterator it = track_views.begin(); it != track_views.end(); ++it) {
		if((source_rtv = dynamic_cast<RouteTimeAxisView*>(*it)) == 0) {
			continue;
		}

		if(source_rtv->route() == route && source_rtv != dest_rtv) {
			dest_rtv->add_underlay(source_rtv->view());
			break;
		}
	}
}

void
Editor::insert_region_list_selection (float times)
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

	boost::shared_ptr<Region> region = _regions->get_single_selection ();
	if (region == 0) {
		return;
	}

	begin_reversible_command (_("insert region"));
        playlist->clear_changes ();
	playlist->add_region ((RegionFactory::create (region)), get_preferred_edit_position(), times);
	_session->add_command(new StatefulDiffCommand (playlist));
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
		switch (_session->config.get_sync_source()) {
		case JACK:
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

	_session->request_transport_speed (fwd ? 1.0f : -1.0f);
}

void
Editor::play_from_start ()
{
	_session->request_locate (_session->current_start_frame(), true);
}

void
Editor::play_from_edit_point ()
{
	_session->request_locate (get_preferred_edit_position(), true);
}

void
Editor::play_from_edit_point_and_return ()
{
	nframes64_t start_frame;
	nframes64_t return_frame;

	start_frame = get_preferred_edit_position (true);

	if (_session->transport_rolling()) {
		_session->request_locate (start_frame, false);
		return;
	}

	/* don't reset the return frame if its already set */

	if ((return_frame = _session->requested_return_frame()) < 0) {
		return_frame = _session->audible_frame();
	}

	if (start_frame >= 0) {
		_session->request_roll_at_and_return (start_frame, return_frame);
	}
}

void
Editor::play_selection ()
{
	if (selection->time.empty()) {
		return;
	}

	_session->request_play_range (&selection->time, true);
}

void
Editor::loop_selected_region ()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (!rs.empty()) {
		RegionView *rv = *(rs.begin());
		Location* tll;

		if ((tll = transport_loop_location()) != 0)  {

			tll->set (rv->region()->position(), rv->region()->last_frame());

			// enable looping, reposition and start rolling

			_session->request_play_loop (true);
			_session->request_locate (tll->start(), false);
			_session->request_transport_speed (1.0f);
		}
	}
}

void
Editor::play_location (Location& location)
{
	if (location.start() <= location.end()) {
		return;
	}

	_session->request_bounded_roll (location.start(), location.end());
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
		_session->request_play_loop (true);
		_session->request_locate (tll->start(), true);
	}
}

void
Editor::raise_region ()
{
	selection->foreach_region (&Region::raise);
}

void
Editor::raise_region_to_top ()
{
	selection->foreach_region (&Region::raise_to_top);
}

void
Editor::lower_region ()
{
	selection->foreach_region (&Region::lower);
}

void
Editor::lower_region_to_bottom ()
{
	selection->foreach_region (&Region::lower_to_bottom);
}

/** Show the region editor for the selected regions */
void
Editor::edit_region ()
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
Editor::rename_region()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	ArdourDialog d (*this, _("Rename Region"), true, false);
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
	d.set_position (Gtk::WIN_POS_MOUSE);

	entry.set_text (rs.front()->region()->name());
	entry.select_region (0, -1);

	entry.signal_activate().connect (sigc::bind (sigc::mem_fun (d, &Dialog::response), RESPONSE_OK));

	d.show_all ();

	entry.grab_focus();

	int ret = d.run();

	d.hide ();

	if (ret == RESPONSE_OK) {
		std::string str = entry.get_text();
		strip_whitespace_edges (str);
		if (!str.empty()) {
			rs.front()->region()->set_name (str);
			_regions->redisplay ();
		}
	}
}

void
Editor::audition_playlist_region_via_route (boost::shared_ptr<Region> region, Route& route)
{
	if (_session->is_auditioning()) {
		_session->cancel_audition ();
	}

	// note: some potential for creativity here, because region doesn't
	// have to belong to the playlist that Route is handling

	// bool was_soloed = route.soloed();

	route.set_solo (true, this);

	_session->request_bounded_roll (region->position(), region->position() + region->length());

	/* XXX how to unset the solo state ? */
}

/** Start an audition of the first selected region */
void
Editor::play_edit_range ()
{
	nframes64_t start, end;

	if (get_edit_op_range (start, end)) {
		_session->request_bounded_roll (start, end);
	}
}

void
Editor::play_selected_region ()
{
	nframes64_t start = max_frames;
	nframes64_t end = 0;
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		if ((*i)->region()->position() < start) {
			start = (*i)->region()->position();
		}
		if ((*i)->region()->last_frame() + 1 > end) {
			end = (*i)->region()->last_frame() + 1;
		}
	}

	_session->request_bounded_roll (start, end);
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

	nframes64_t start = selection->time[clicked_selection].start;
	nframes64_t end = selection->time[clicked_selection].end;

	TrackViewList tracks = get_tracks_for_range_action ();

	nframes64_t selection_cnt = end - start + 1;

	for (TrackSelection::iterator i = tracks.begin(); i != tracks.end(); ++i) {
		boost::shared_ptr<Region> current;
		boost::shared_ptr<Playlist> pl;
		nframes64_t internal_start;
		string new_name;

		if ((pl = (*i)->playlist()) == 0) {
			continue;
		}

		if ((current = pl->top_region_at (start)) == 0) {
			continue;
		}

		internal_start = start - current->position();
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

	nframes64_t start = selection->time[clicked_selection].start;
	nframes64_t end = selection->time[clicked_selection].end;

	sort_track_selection ();

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		boost::shared_ptr<Region> current;
		boost::shared_ptr<Playlist> playlist;
		nframes64_t internal_start;
		string new_name;

		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}

		if ((current = playlist->top_region_at(start)) == 0) {
			continue;
		}

		internal_start = start - current->position();
		RegionFactory::region_name (new_name, current->name(), true);

		PropertyList plist; 
		
		plist.add (ARDOUR::Properties::start, current->start() + internal_start);
		plist.add (ARDOUR::Properties::length, end - start + 1);
		plist.add (ARDOUR::Properties::name, new_name);

		new_regions.push_back (RegionFactory::create (current, plist));
	}
}

void
Editor::split_multichannel_region ()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	vector< boost::shared_ptr<Region> > v;

	for (list<RegionView*>::iterator x = rs.begin(); x != rs.end(); ++x) {
		(*x)->region()->separate_by_channel (*_session, v);
	}
}

void
Editor::new_region_from_selection ()
{
	region_from_selection ();
	cancel_selection ();
}

static void
add_if_covered (RegionView* rv, const AudioRange* ar, RegionSelection* rs)
{
	switch (rv->region()->coverage (ar->start, ar->end - 1)) {
	case OverlapNone:
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

	return t;
}

void
Editor::separate_regions_between (const TimeSelection& ts)
{
	bool in_command = false;
	boost::shared_ptr<Playlist> playlist;
	RegionSelection new_selection;

	TrackViewList tmptracks = get_tracks_for_range_action ();
	sort_track_selection (&tmptracks);

	for (TrackSelection::iterator i = tmptracks.begin(); i != tmptracks.end(); ++i) {

		RouteTimeAxisView* rtv;

		if ((rtv = dynamic_cast<RouteTimeAxisView*> ((*i))) != 0) {

			if (rtv->is_track()) {

				/* no edits to destructive tracks */

				if (rtv->track()->destructive()) {
					continue;
				}

				if ((playlist = rtv->playlist()) != 0) {

					playlist->clear_changes ();

					/* XXX need to consider musical time selections here at some point */

					double speed = rtv->track()->speed();


					for (list<AudioRange>::const_iterator t = ts.begin(); t != ts.end(); ++t) {

						sigc::connection c = rtv->view()->RegionViewAdded.connect (
								sigc::mem_fun(*this, &Editor::collect_new_region_view));

						latest_regionviews.clear ();

						playlist->partition ((nframes64_t)((*t).start * speed),
								(nframes64_t)((*t).end * speed), false);

						c.disconnect ();

						if (!latest_regionviews.empty()) {

							rtv->view()->foreach_regionview (sigc::bind (
										sigc::ptr_fun (add_if_covered),
										&(*t), &new_selection));
                                                        
							if (!in_command) {
								begin_reversible_command (_("separate"));
								in_command = true;
							}
                                                        
                                                        /* pick up changes to existing regions */

                                                        vector<Command*> cmds;
                                                        playlist->rdiff (cmds);
							_session->add_commands (cmds);

                                                        /* pick up changes to the playlist itself (adds/removes)
                                                         */

							_session->add_command(new StatefulDiffCommand (playlist));
						}
					}
				}
			}
		}
	}

	if (in_command)	{
		selection->set (new_selection);
		set_mouse_mode (MouseObject);

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

	if (mouse_mode == MouseRange && !selection->time.empty()) {

		separate_regions_between (selection->time);

	} else {

		nframes64_t start;
		nframes64_t end;

		if (get_edit_op_range (start, end)) {

			AudioRange ar (start, end, 1);
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

	AudioRange ar (loc.start(), loc.end(), 1);
	TimeSelection ts;

	ts.push_back (ar);

	separate_regions_between (ts);
}

/** Separate regions under the selected region */
void
Editor::separate_under_selected_regions ()
{
	RegionSelection rs;
	get_regions_for_action (rs);
	
	vector<PlaylistState> playlists;

	if (!_session) {
		return;
	}

	if (rs.empty()) {
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

			playlist->freeze ();
			playlists.push_back(before);
		}

		//Partition on the region bounds
		playlist->partition ((*rl)->first_frame() - 1, (*rl)->last_frame() + 1, true);
		
		//Re-add region that was just removed due to the partition operation
		playlist->add_region( (*rl), (*rl)->first_frame() );
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

		crop_region_to (selection->time.start(), selection->time.end_frame());

	} else {

		nframes64_t start;
		nframes64_t end;

		if (get_edit_op_range (start, end)) {
			crop_region_to (start, end);
		}
	}

}

void
Editor::crop_region_to (nframes64_t start, nframes64_t end)
{
	vector<boost::shared_ptr<Playlist> > playlists;
	boost::shared_ptr<Playlist> playlist;
	TrackViewList* ts;

	if (selection->tracks.empty()) {
		ts = &track_views;
	} else {
		sort_track_selection ();
		ts = &selection->tracks;
	}

	for (TrackSelection::iterator i = ts->begin(); i != ts->end(); ++i) {

		RouteTimeAxisView* rtv;

		if ((rtv = dynamic_cast<RouteTimeAxisView*> ((*i))) != 0) {

			boost::shared_ptr<Track> t = rtv->track();

			if (t != 0 && ! t->destructive()) {

				if ((playlist = rtv->playlist()) != 0) {
					playlists.push_back (playlist);
				}
			}
		}
	}

	if (playlists.empty()) {
		return;
	}

	nframes64_t the_start;
	nframes64_t the_end;
	nframes64_t cnt;

	begin_reversible_command (_("trim to selection"));

	for (vector<boost::shared_ptr<Playlist> >::iterator i = playlists.begin(); i != playlists.end(); ++i) {

		boost::shared_ptr<Region> region;

		the_start = start;

		if ((region = (*i)->top_region_at(the_start)) == 0) {
			continue;
		}

		/* now adjust lengths to that we do the right thing
		   if the selection extends beyond the region
		*/

		the_start = max (the_start, (nframes64_t) region->position());
		if (max_frames - the_start < region->length()) {
			the_end = the_start + region->length() - 1;
		} else {
			the_end = max_frames;
		}
		the_end = min (end, the_end);
		cnt = the_end - the_start + 1;

                region->clear_changes ();
		region->trim_to (the_start, cnt, this);
		_session->add_command (new StatefulDiffCommand (region));
	}

	commit_reversible_command ();
}

void
Editor::region_fill_track ()
{
	nframes64_t end;
	RegionSelection rs;

	get_regions_for_action (rs);

	if (!_session || rs.empty()) {
		return;
	}

	end = _session->current_end_frame ();

	begin_reversible_command (_("region fill"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {

		boost::shared_ptr<Region> region ((*i)->region());

		boost::shared_ptr<Playlist> pl = region->playlist();

		if (end <= region->last_frame()) {
			return;
		}

		double times = (double) (end - region->last_frame()) / (double) region->length();

		if (times == 0) {
			return;
		}

                pl->clear_changes ();
		pl->add_region (RegionFactory::create (region), region->last_frame(), times);
		_session->add_command (new StatefulDiffCommand (pl));
	}

	commit_reversible_command ();
}

void
Editor::region_fill_selection ()
{
	if (clicked_routeview == 0 || !clicked_routeview->is_audio_track()) {
		return;
	}

	if (selection->time.empty()) {
		return;
	}

	boost::shared_ptr<Region> region = _regions->get_single_selection ();
	if (region == 0) {
		return;
	}

	nframes64_t start = selection->time[clicked_selection].start;
	nframes64_t end = selection->time[clicked_selection].end;

	boost::shared_ptr<Playlist> playlist;

	if (selection->tracks.empty()) {
		return;
	}

	nframes64_t selection_length = end - start;
	float times = (float)selection_length / region->length();

	begin_reversible_command (_("fill selection"));

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}

                playlist->clear_changes ();
		playlist->add_region (RegionFactory::create (region), start, times);
		_session->add_command (new StatefulDiffCommand (playlist));
	}

	commit_reversible_command ();
}

void
Editor::set_region_sync_from_edit_point ()
{
	nframes64_t where = get_preferred_edit_position ();
	RegionSelection rs;
	get_regions_for_action (rs);
	set_sync_point (where, rs);
}

void
Editor::set_sync_point (nframes64_t where, const RegionSelection& rs)
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
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("remove sync"));
	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {

                (*i)->region()->clear_changes ();
		(*i)->region()->clear_sync_position ();
		_session->add_command(new StatefulDiffCommand ((*i)->region()));
	}
	commit_reversible_command ();
}

void
Editor::naturalize ()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("naturalize"));
	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		(*i)->region()->clear_changes ();
		(*i)->region()->move_to_natural_position (this);
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}
	commit_reversible_command ();
}

void
Editor::align (RegionPoint what)
{
	RegionSelection rs;

	get_regions_for_action (rs);
	nframes64_t where = get_preferred_edit_position();

	if (!rs.empty()) {
		align_selection (what, where, rs);
	} else {

		RegionSelection rs;
		get_regions_at (rs, where, selection->tracks);
		align_selection (what, where, rs);
	}
}

void
Editor::align_relative (RegionPoint what)
{
	nframes64_t where = get_preferred_edit_position();
	RegionSelection rs;

	get_regions_for_action (rs);

	if (!rs.empty()) {
		align_selection_relative (what, where, rs);
	}
}

struct RegionSortByTime {
    bool operator() (const RegionView* a, const RegionView* b) {
	    return a->region()->position() < b->region()->position();
    }
};

void
Editor::align_selection_relative (RegionPoint point, nframes64_t position, const RegionSelection& rs)
{
	if (rs.empty()) {
		return;
	}

	nframes64_t distance = 0;
	nframes64_t pos = 0;
	int dir = 1;

	list<RegionView*> sorted;
	rs.by_position (sorted);

	boost::shared_ptr<Region> r ((*sorted.begin())->region());

	switch (point) {
	case Start:
		pos = position;
		if (position > r->position()) {
			distance = position - r->position();
		} else {
			distance = r->position() - position;
			dir = -1;
		}
		break;

	case End:
		if (position > r->last_frame()) {
			distance = position - r->last_frame();
			pos = r->position() + distance;
		} else {
			distance = r->last_frame() - position;
			pos = r->position() - distance;
			dir = -1;
		}
		break;

	case SyncPoint:
		pos = r->adjust_to_sync (position);
		if (pos > r->position()) {
			distance = pos - r->position();
		} else {
			distance = r->position() - pos;
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
	r->set_position (pos, this);
	_session->add_command(new StatefulDiffCommand (r));

	/* move rest by the same amount */

	sorted.pop_front();

	for (list<RegionView*>::iterator i = sorted.begin(); i != sorted.end(); ++i) {

		boost::shared_ptr<Region> region ((*i)->region());

                region->clear_changes ();

		if (dir > 0) {
			region->set_position (region->position() + distance, this);
		} else {
			region->set_position (region->position() - distance, this);
		}
                
		_session->add_command(new StatefulDiffCommand (region));

	}

	commit_reversible_command ();
}

void
Editor::align_selection (RegionPoint point, nframes64_t position, const RegionSelection& rs)
{
	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("align selection"));

	for (RegionSelection::const_iterator i = rs.begin(); i != rs.end(); ++i) {
		align_region_internal ((*i)->region(), point, position);
	}

	commit_reversible_command ();
}

void
Editor::align_region (boost::shared_ptr<Region> region, RegionPoint point, nframes64_t position)
{
	begin_reversible_command (_("align region"));
	align_region_internal (region, point, position);
	commit_reversible_command ();
}

void
Editor::align_region_internal (boost::shared_ptr<Region> region, RegionPoint point, nframes64_t position)
{
	region->clear_changes ();

	switch (point) {
	case SyncPoint:
		region->set_position (region->adjust_to_sync (position), this);
		break;

	case End:
		if (position > region->length()) {
			region->set_position (position - region->length(), this);
		}
		break;

	case Start:
		region->set_position (position, this);
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
	nframes64_t where = get_preferred_edit_position();
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (front ? _("trim front") : _("trim back"));

	for (list<RegionView*>::const_iterator i = rs.by_layer().begin(); i != rs.by_layer().end(); ++i) {
		if (!(*i)->region()->locked()) {
                        
			(*i)->region()->clear_changes ();
			
			if (front) {
				(*i)->region()->trim_front (where, this);
			} else {
				(*i)->region()->trim_end (where, this);
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
	RegionSelection rs;

	get_regions_for_action (rs);

	begin_reversible_command (str);

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		RegionView* rv = (*x);

		/* require region to span proposed trim */
		switch (rv->region()->coverage (loc.start(), loc.end())) {
		case OverlapInternal:
			break;
		default:
			continue;
		}

		RouteTimeAxisView* tav = dynamic_cast<RouteTimeAxisView*> (&rv->get_time_axis_view());
		if (!tav) {
			return;
		}

		float speed = 1.0;
		nframes64_t start;
		nframes64_t end;

		if (tav->track() != 0) {
			speed = tav->track()->speed();
		}

		start = session_frame_to_track_frame (loc.start(), speed);
		end = session_frame_to_track_frame (loc.end(), speed);
                
		rv->region()->clear_changes ();
		rv->region()->trim_to (start, (end - start), this);
		_session->add_command(new StatefulDiffCommand (rv->region()));
	}

	commit_reversible_command ();
}

void
Editor::trim_region_to_edit_point ()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	nframes64_t where = get_preferred_edit_position();

	begin_reversible_command (_("trim region start to edit point"));

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		RegionView* rv = (*x);

		/* require region to cover trim */
		if (!rv->region()->covers (where)) {
			continue;
		}

		RouteTimeAxisView* tav = dynamic_cast<RouteTimeAxisView*> (&rv->get_time_axis_view());
		if (!tav) {
			return;
		}

		float speed = 1.0;

		if (tav->track() != 0) {
			speed = tav->track()->speed();
		}

                rv->region()->clear_changes ();
		rv->region()->trim_end (session_frame_to_track_frame(where, speed), this);
		_session->add_command(new StatefulDiffCommand (rv->region()));
	}

	commit_reversible_command ();
}

void
Editor::trim_region_from_edit_point ()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	nframes64_t where = get_preferred_edit_position();

	begin_reversible_command (_("trim region end to edit point"));

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		RegionView* rv = (*x);

		/* require region to cover trim */
		if (!rv->region()->covers (where)) {
			continue;
		}

		RouteTimeAxisView* tav = dynamic_cast<RouteTimeAxisView*> (&rv->get_time_axis_view());
		if (!tav) {
			return;
		}

		float speed = 1.0;

		if (tav->track() != 0) {
			speed = tav->track()->speed();
		}

                rv->region()->clear_changes ();
		rv->region()->trim_front (session_frame_to_track_frame(where, speed), this);
		_session->add_command(new StatefulDiffCommand (rv->region()));
	}

	commit_reversible_command ();
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
	RegionSelection rs;

	get_regions_for_action (rs);

	begin_reversible_command (_("trim to region"));

	boost::shared_ptr<Region> next_region;

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {

		AudioRegionView* arv = dynamic_cast<AudioRegionView*> (*x);

		if (!arv) {
			continue;
		}

		AudioTimeAxisView* atav = dynamic_cast<AudioTimeAxisView*> (&arv->get_time_axis_view());

		if (!atav) {
			return;
		}

		float speed = 1.0;

		if (atav->track() != 0) {
			speed = atav->track()->speed();
		}


		boost::shared_ptr<Region> region = arv->region();
		boost::shared_ptr<Playlist> playlist (region->playlist());

                region->clear_changes ();

		if(forward){

		    next_region = playlist->find_next_region (region->first_frame(), Start, 1);

		    if(!next_region){
			continue;
		    }

		    region->trim_end((nframes64_t) ( (next_region->first_frame() - 1) * speed), this);
		    arv->region_changed (PropertyChange (ARDOUR::Properties::length));
		}
		else {

		    next_region = playlist->find_next_region (region->first_frame(), Start, 0);

		    if(!next_region){
			continue;
		    }

		    region->trim_front((nframes64_t) ((next_region->last_frame() + 1) * speed), this);

		    arv->region_changed (ARDOUR::bounds_change);
		}

		_session->add_command(new StatefulDiffCommand (region));
	}

	commit_reversible_command ();
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
	SessionEvent::create_per_thread_pool ("freeze events", 64);

	return static_cast<Editor*>(arg)->freeze_thread ();
}

void*
Editor::freeze_thread ()
{
	clicked_routeview->audio_track()->freeze_me (*current_interthread_info);
	current_interthread_info->done = true;
	return 0;
}

void
Editor::freeze_route ()
{
	if (clicked_routeview == 0 || !clicked_routeview->is_audio_track()) {
		return;
	}

	InterThreadInfo itt;
	current_interthread_info = &itt;

	InterthreadProgressWindow ipw (current_interthread_info, _("Freeze"), _("Cancel Freeze"));

	pthread_create_and_store (X_("freezer"), &itt.thread, _freeze_thread, this);

	track_canvas->get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));

	while (!itt.done && !itt.cancel) {
		gtk_main_iteration ();
	}

	current_interthread_info = 0;
	track_canvas->get_window()->set_cursor (*current_canvas_cursor);
}

void
Editor::bounce_range_selection (bool replace, bool enable_processing)
{
	if (selection->time.empty()) {
		return;
	}

	TrackSelection views = selection->tracks;

	nframes64_t start = selection->time[clicked_selection].start;
	nframes64_t end = selection->time[clicked_selection].end;
	nframes64_t cnt = end - start + 1;

	begin_reversible_command (_("bounce range"));

	for (TrackViewList::iterator i = views.begin(); i != views.end(); ++i) {

		RouteTimeAxisView* rtv;

		if ((rtv = dynamic_cast<RouteTimeAxisView*> (*i)) == 0) {
			continue;
		}

		boost::shared_ptr<Playlist> playlist;

		if ((playlist = rtv->playlist()) == 0) {
			return;
		}

		InterThreadInfo itt;

                playlist->clear_changes ();
		playlist->clear_owned_changes ();
		
		boost::shared_ptr<Region> r = rtv->track()->bounce_range (start, start+cnt, itt, enable_processing);

		if (replace) {
			list<AudioRange> ranges;
			ranges.push_back (AudioRange (start, start+cnt, 0));
			playlist->cut (ranges); // discard result
			playlist->add_region (r, start);
		}

		vector<Command*> cmds;
		playlist->rdiff (cmds);
		_session->add_commands (cmds);

                _session->add_command (new StatefulDiffCommand (playlist));
	}

	commit_reversible_command ();
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
	switch (current_mouse_mode()) {

	case MouseObject:
		if (!selection->regions.empty() || !selection->points.empty()) {
			return true;
		}
		break;

	case MouseRange:
		if (!selection->time.empty()) {
			return true;
		}
		break;

	default:
		break;
	}

	return false;
}


/** Cut, copy or clear selected regions, automation points or a time range.
 * @param op Operation (Cut, Copy or Clear)
 */
void
Editor::cut_copy (CutCopyOp op)
{
	/* only cancel selection if cut/copy is successful.*/

	string opname;

	switch (op) {
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

	if (op == Cut || op == Clear) {
		if (_drags->active ()) {
			_drags->abort ();
		}
	}

	cut_buffer->clear ();

	if (entered_marker) {

		/* cut/delete op while pointing at a marker */

		bool ignored;
		Location* loc = find_location_from_marker (entered_marker, ignored);

		if (_session && loc) {
			Glib::signal_idle().connect (sigc::bind (sigc::mem_fun(*this, &Editor::really_remove_marker), loc));
		}

		_drags->abort ();
		return;
	}

	if (internal_editing()) {

		switch (current_mouse_mode()) {
		case MouseObject:
		case MouseRange:
			cut_copy_midi (op);
			break;
		default:
			break;
		}

	} else {

		RegionSelection rs;

		/* we only want to cut regions if some are selected */

		if (!selection->regions.empty()) {
			get_regions_for_action (rs, false, false);
		}

		switch (current_mouse_mode()) {
		case MouseObject:
			if (!rs.empty() || !selection->points.empty()) {

				begin_reversible_command (opname + _(" objects"));

				if (!rs.empty()) {
					cut_copy_regions (op, rs);

					if (op == Cut) {
						selection->clear_regions ();
					}
				}

				if (!selection->points.empty()) {
					cut_copy_points (op);

					if (op == Cut) {
						selection->clear_points ();
					}
				}

				commit_reversible_command ();
				break; // terminate case statement here
			}
			if (!selection->time.empty()) {
				/* don't cause suprises */
				break;
			}
			// fall thru if there was nothing selected

		case MouseRange:
			if (selection->time.empty()) {
				nframes64_t start, end;
				if (!get_edit_op_range (start, end)) {
					return;
				}
				selection->set (start, end);
			}

			begin_reversible_command (opname + _(" range"));
			cut_copy_ranges (op);
			commit_reversible_command ();

			if (op == Cut) {
				selection->clear_time ();
			}

			break;

		default:
			break;
		}
	}

	if (op == Cut || op == Clear) {
		_drags->abort ();
	}
}

/** Cut, copy or clear selected automation points.
 * @param op Operation (Cut, Copy or Clear)
 */
void
Editor::cut_copy_points (CutCopyOp op)
{
	for (PointSelection::iterator i = selection->points.begin(); i != selection->points.end(); ++i) {

		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*>((*i).track);

		if (atv) {
			atv->cut_copy_clear_objects (selection->points, op);
		}
	}
}

/** Cut, copy or clear selected automation points.
 * @param op Operation (Cut, Copy or Clear)
 */
void
Editor::cut_copy_midi (CutCopyOp op)
{
	for (MidiRegionSelection::iterator i = selection->midi_regions.begin(); i != selection->midi_regions.end(); ++i) {
		MidiRegionView* mrv = *i;
		mrv->cut_copy_clear (op);
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

	boost::shared_ptr<Playlist> playlist = clicked_routeview->playlist();

	begin_reversible_command (_("remove region"));
        playlist->clear_changes ();
	playlist->remove_region (clicked_regionview->region());
	_session->add_command(new StatefulDiffCommand (playlist));
	commit_reversible_command ();
}


/** Remove the selected regions */
void
Editor::remove_selected_regions ()
{
	RegionSelection rs;
	get_regions_for_action (rs);

	if (!_session) {
		return;
	}

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("remove region"));

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

		vector<boost::shared_ptr<Playlist> >::iterator i;

		//only prep history if this is a new playlist.
		for (i = playlists.begin(); i != playlists.end(); ++i) {
			if ((*i) == playlist) {
				break;
			}
		}

		if (i == playlists.end()) {

                        playlist->clear_changes ();
			playlist->freeze ();

			playlists.push_back (playlist);
		}

		playlist->remove_region (*rl);
	}

	vector<boost::shared_ptr<Playlist> >::iterator pl;

	for (pl = playlists.begin(); pl != playlists.end(); ++pl) {
		(*pl)->thaw ();
		_session->add_command(new StatefulDiffCommand (*pl));
	}

	commit_reversible_command ();
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

	nframes64_t first_position = max_frames;

	typedef set<boost::shared_ptr<Playlist> > FreezeList;
        FreezeList freezelist;

	/* get ordering correct before we cut/copy */

	rs.sort_by_position_and_track ();

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {

		first_position = min ((nframes64_t) (*x)->region()->position(), first_position);

		if (op == Cut || op == Clear) {
			boost::shared_ptr<Playlist> pl = (*x)->region()->playlist();

			if (pl) {
				FreezeList::iterator fl;

				//only take state if this is a new playlist.
				for (fl = freezelist.begin(); fl != freezelist.end(); ++fl) {
					if ((*fl) == pl) {
						break;
					}
				}

				if (fl == freezelist.end()) {
                                        pl->clear_changes();
					pl->freeze ();
					freezelist.insert (pl);
				}
			}
		}

		TimeAxisView* tv = &(*x)->get_trackview();
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
			/* impossible, but this handles it for the future */
			continue;
		}

		TimeAxisView& tv = (*x)->get_trackview();
		boost::shared_ptr<Playlist> npl;
		RegionSelection::iterator tmp;

		tmp = x;
		++tmp;

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

		boost::shared_ptr<Region> r = (*x)->region();
		boost::shared_ptr<Region> _xx;

		assert (r != 0);

		switch (op) {
		case Cut:
			_xx = RegionFactory::create (r);
			npl->add_region (_xx, r->position() - first_position);
			pl->remove_region (r);
			break;

		case Copy:
			/* copy region before adding, so we're not putting same object into two different playlists */
			npl->add_region (RegionFactory::create (r), r->position() - first_position);
			break;

		case Clear:
			pl->remove_region (r);
			break;
		}

		x = tmp;
	}

	list<boost::shared_ptr<Playlist> > foo;

	/* the pmap is in the same order as the tracks in which selected regions occured */

	for (vector<PlaylistMapping>::iterator i = pmap.begin(); i != pmap.end(); ++i) {
		(*i).pl->thaw();
		foo.push_back ((*i).pl);
	}

	if (!foo.empty()) {
		cut_buffer->set (foo);
	}

	if (pmap.empty()) {
		_last_cut_copy_source_track = 0;
	} else {
		_last_cut_copy_source_track = pmap.front().tv;
	}
        
	for (FreezeList::iterator pl = freezelist.begin(); pl != freezelist.end(); ++pl) {
		(*pl)->thaw ();
		_session->add_command (new StatefulDiffCommand (*pl));
	}
}

void
Editor::cut_copy_ranges (CutCopyOp op)
{
	TrackViewList* ts;
	TrackViewList entered;

	if (selection->tracks.empty()) {
		if (!entered_track) {
			return;
		}
		entered.push_back (entered_track);
		ts = &entered;
	} else {
		ts = &selection->tracks;
	}

	for (TrackSelection::iterator i = ts->begin(); i != ts->end(); ++i) {
		(*i)->cut_copy_clear (*selection, op);
	}
}

void
Editor::paste (float times)
{
	paste_internal (get_preferred_edit_position(), times);
}

void
Editor::mouse_paste ()
{
	nframes64_t where;
	bool ignored;

	if (!mouse_frame (where, ignored)) {
		return;
	}

	snap_to (where);
	paste_internal (where, 1);
}

void
Editor::paste_internal (nframes64_t position, float times)
{
	bool commit = false;

	if (internal_editing()) {
		if (cut_buffer->midi_notes.empty()) {
			return;
		}
	} else {
		if (cut_buffer->empty()) {
			return;
		}
	}

	if (position == max_frames) {
		position = get_preferred_edit_position();
	}

	begin_reversible_command (_("paste"));

	TrackViewList ts;
	TrackViewList::iterator i;
	size_t nth;

	/* get everything in the correct order */

	if (!selection->tracks.empty()) {
		/* there are some selected tracks, so paste to them */
		sort_track_selection ();
		ts = selection->tracks;
	} else if (_last_cut_copy_source_track) {
		/* otherwise paste to the track that the cut/copy came from;
		   see discussion in mants #3333.
		*/
		ts.push_back (_last_cut_copy_source_track);
	}

	for (nth = 0, i = ts.begin(); i != ts.end(); ++i, ++nth) {

		/* undo/redo is handled by individual tracks/regions */

		if (internal_editing()) {

			RegionSelection rs;
			RegionSelection::iterator r;
			MidiNoteSelection::iterator cb;

			get_regions_at (rs, position, ts);

			for (cb = cut_buffer->midi_notes.begin(), r = rs.begin();
			     cb != cut_buffer->midi_notes.end() && r != rs.end(); ++r) {
				MidiRegionView* mrv = dynamic_cast<MidiRegionView*> (*r);
				if (mrv) {
					mrv->paste (position, times, **cb);
					++cb;
				}
			}

		} else {

			if ((*i)->paste (position, times, *cut_buffer, nth)) {
				commit = true;
			}
		}
	}

	if (commit) {
		commit_reversible_command ();
	}
}

void
Editor::duplicate_some_regions (RegionSelection& regions, float times)
{
	boost::shared_ptr<Playlist> playlist;
	RegionSelection sel = regions; // clear (below) may  clear the argument list if its the current region selection
	RegionSelection foo;

	nframes_t const start_frame = regions.start ();
	nframes_t const end_frame = regions.end_frame ();

	begin_reversible_command (_("duplicate region"));

	selection->clear_regions ();

	for (RegionSelection::iterator i = sel.begin(); i != sel.end(); ++i) {

		boost::shared_ptr<Region> r ((*i)->region());

		TimeAxisView& tv = (*i)->get_time_axis_view();
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (&tv);
		latest_regionviews.clear ();
		sigc::connection c = rtv->view()->RegionViewAdded.connect (sigc::mem_fun(*this, &Editor::collect_new_region_view));

 		playlist = (*i)->region()->playlist();
                playlist->clear_changes ();
		playlist->duplicate (r, end_frame + (r->first_frame() - start_frame), times);
		_session->add_command(new StatefulDiffCommand (playlist));

		c.disconnect ();

		foo.insert (foo.end(), latest_regionviews.begin(), latest_regionviews.end());
	}

	commit_reversible_command ();

	if (!foo.empty()) {
		selection->set (foo);
	}
}

void
Editor::duplicate_selection (float times)
{
	if (selection->time.empty() || selection->tracks.empty()) {
		return;
	}

	boost::shared_ptr<Playlist> playlist;
	vector<boost::shared_ptr<Region> > new_regions;
	vector<boost::shared_ptr<Region> >::iterator ri;

	create_region_from_selection (new_regions);

	if (new_regions.empty()) {
		return;
	}

	begin_reversible_command (_("duplicate selection"));

	ri = new_regions.begin();

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}
                playlist->clear_changes ();
		playlist->duplicate (*ri, selection->time[clicked_selection].end, times);
		_session->add_command (new StatefulDiffCommand (playlist));

		++ri;
		if (ri == new_regions.end()) {
			--ri;
		}
	}

	commit_reversible_command ();
}

void
Editor::reset_point_selection ()
{
	/* reset all selected points to the relevant default value */

	for (PointSelection::iterator i = selection->points.begin(); i != selection->points.end(); ++i) {

		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*>((*i).track);

		if (atv) {
			atv->reset_objects (selection->points);
		}
	}
}

void
Editor::center_playhead ()
{
	float page = _canvas_width * frames_per_unit;
	center_screen_internal (playhead_cursor->current_frame, page);
}

void
Editor::center_edit_point ()
{
	float page = _canvas_width * frames_per_unit;
	center_screen_internal (get_preferred_edit_position(), page);
}

void
Editor::clear_playlist (boost::shared_ptr<Playlist> playlist)
{
	begin_reversible_command (_("clear playlist"));
        playlist->clear_changes ();
	playlist->clear ();
	_session->add_command (new StatefulDiffCommand (playlist));
	commit_reversible_command ();
}

void
Editor::nudge_track (bool use_edit, bool forwards)
{
	boost::shared_ptr<Playlist> playlist;
	nframes64_t distance;
	nframes64_t next_distance;
	nframes64_t start;

	if (use_edit) {
		start = get_preferred_edit_position();
	} else {
		start = 0;
	}

	if ((distance = get_nudge_distance (start, next_distance)) == 0) {
		return;
	}

	if (selection->tracks.empty()) {
		return;
	}

	begin_reversible_command (_("nudge track"));

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}

                playlist->clear_changes ();
                playlist->clear_owned_changes ();

		playlist->nudge_after (start, distance, forwards);
                
                vector<Command*> cmds;

                playlist->rdiff (cmds);
		_session->add_commands (cmds);

                _session->add_command (new StatefulDiffCommand (playlist));
	}

	commit_reversible_command ();
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

		Gtkmm2ext::Choice prompter (_("Destroy last capture"), prompt, choices);

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
Editor::normalize_region ()
{
	if (!_session) {
		return;
	}

	RegionSelection rs;
	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	Dialog dialog (rs.size() > 1 ? _("Normalize regions") : _("Normalize region"));

	VBox vbox;
	vbox.set_spacing (6);
	vbox.set_border_width (6);
	
	HBox hbox;
	hbox.set_spacing (6);
	hbox.set_border_width (6);
	hbox.pack_start (*manage (new Label (_("Normalize to:"))));
	SpinButton spin (0.2, 2);
	spin.set_range (-112, 0);
	spin.set_increments (0.1, 1);
	spin.set_value (0);
	hbox.pack_start (spin);
	spin.set_value (_last_normalization_value);
	hbox.pack_start (*manage (new Label (_("dbFS"))));
	vbox.pack_start (hbox);

	CheckButton* normalize_across_all = manage (new CheckButton (_("Normalize across all selected regions")));
	vbox.pack_start (*normalize_across_all);
	if (rs.size() <= 1) {
		normalize_across_all->set_sensitive (false);
	}

	vbox.show_all ();
	
	dialog.get_vbox()->set_spacing (12);
	dialog.get_vbox()->pack_start (vbox);
	dialog.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	dialog.add_button (_("Normalize"), RESPONSE_ACCEPT);

	if (dialog.run () == RESPONSE_CANCEL) {
		return;
	}

	begin_reversible_command (_("normalize"));

	track_canvas->get_window()->set_cursor (*wait_cursor);
	gdk_flush ();

	double maxamp = 0;
	if (normalize_across_all->get_active ()) {
		for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
			AudioRegionView* const arv = dynamic_cast<AudioRegionView*> (*r);
			if (!arv) {
				continue;
			}
			maxamp = max (maxamp, arv->audio_region()->maximum_amplitude ());
		}
	}
	
	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*> (*r);
		if (!arv) {
			continue;
		}
		arv->region()->clear_changes ();

		double const amp = normalize_across_all->get_active() ? maxamp : arv->audio_region()->maximum_amplitude ();
		
		arv->audio_region()->normalize (amp, spin.get_value ());
		_session->add_command (new StatefulDiffCommand (arv->region()));
	}

	commit_reversible_command ();
	track_canvas->get_window()->set_cursor (*current_canvas_cursor);

	_last_normalization_value = spin.get_value ();
}


void
Editor::reset_region_scale_amplitude ()
{
	if (!_session) {
		return;
	}

	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command ("reset gain");

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (!arv)
			continue;
                arv->region()->clear_changes ();
		arv->audio_region()->set_scale_amplitude (1.0f);
		_session->add_command (new StatefulDiffCommand (arv->region()));
	}

	commit_reversible_command ();
}

void
Editor::adjust_region_scale_amplitude (bool up)
{
	if (!_session) {
		return;
	}

	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command ("denormalize");

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (!arv) {
			continue;
                }

                arv->region()->clear_changes ();
                
		double fraction = gain_to_slider_position (arv->audio_region()->scale_amplitude ());

		if (up) {
			fraction += 0.05;
			fraction = min (fraction, 1.0);
		} else {
			fraction -= 0.05;
			fraction = max (fraction, 0.0);
		}

		if (!up && fraction <= 0) {
			continue;
		}

		fraction = slider_position_to_gain (fraction);

		if (up && fraction >= 2.0) {
			continue;
		}

		arv->audio_region()->set_scale_amplitude (fraction);
		_session->add_command (new StatefulDiffCommand (arv->region()));
	}

	commit_reversible_command ();
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

	RegionSelection rs;
	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	std::list<boost::shared_ptr<AudioRegion> > ar;

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*> (*i);
	        if (arv) {
			ar.push_back (arv->audio_region ());
		}
	}

	StripSilenceDialog d (_session, ar);
	int const r = d.run ();

	if (r == Gtk::RESPONSE_OK) {
		StripSilence s (*_session, d.threshold (), d.minimum_length (), d.fade_length ());
		apply_filter (s, _("strip silence"));
	}
}

Command*
Editor::apply_midi_note_edit_op_to_region (MidiOperator& op, MidiRegionView& mrv)
{
	Evoral::Sequence<Evoral::MusicalTime>::Notes selected;
	mrv.selection_as_notelist (selected, true);

	vector<Evoral::Sequence<Evoral::MusicalTime>::Notes> v;
	v.push_back (selected);

	return op (mrv.midi_region()->model(), v);
}

void
Editor::apply_midi_note_edit_op (MidiOperator& op)
{
	RegionSelection rs;
	Command* cmd;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (op.name ());

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ) {
		RegionSelection::iterator tmp = r;
		++tmp;

		MidiRegionView* const mrv = dynamic_cast<MidiRegionView*> (*r);

		if (mrv) {
			cmd = apply_midi_note_edit_op_to_region (op, *mrv);
			if (cmd) {
				(*cmd)();
				_session->add_command (cmd);
			}
		}

		r = tmp;
	}

	commit_reversible_command ();
	rs.clear ();
}

void
Editor::fork_region ()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("Fork Region(s)"));

	track_canvas->get_window()->set_cursor (*wait_cursor);
	gdk_flush ();

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ) {
		RegionSelection::iterator tmp = r;
		++tmp;

		MidiRegionView* const mrv = dynamic_cast<MidiRegionView*>(*r);

		if (mrv) {
			boost::shared_ptr<Playlist> playlist = mrv->region()->playlist();
                        boost::shared_ptr<MidiRegion> newregion = mrv->midi_region()->clone ();
                        
                        playlist->clear_changes ();
                        playlist->replace_region (mrv->region(), newregion, mrv->region()->position());
                        _session->add_command(new StatefulDiffCommand (playlist));
		}

		r = tmp;
	}

	commit_reversible_command ();
	rs.clear ();

	track_canvas->get_window()->set_cursor (*current_canvas_cursor);
}

void
Editor::quantize_region ()
{
	if (!_session) {
		return;
	}

	QuantizeDialog* qd = new QuantizeDialog (*this);

	qd->present ();
	const int r = qd->run ();
	qd->hide ();

	if (r == Gtk::RESPONSE_OK) {
		Quantize quant (*_session, Plain,
				qd->snap_start(), qd->snap_end(),
				qd->start_grid_size(), qd->end_grid_size(),
				qd->strength(), qd->swing(), qd->threshold());

		apply_midi_note_edit_op (quant);
	}
}

void
Editor::apply_filter (Filter& filter, string command)
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (command);

	track_canvas->get_window()->set_cursor (*wait_cursor);
	gdk_flush ();

	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ) {
		RegionSelection::iterator tmp = r;
		++tmp;

		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (arv) {
			boost::shared_ptr<Playlist> playlist = arv->region()->playlist();

			if (arv->audio_region()->apply (filter) == 0) {

				playlist->clear_changes ();
                                
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

				_session->add_command(new StatefulDiffCommand (playlist));
			} else {
				goto out;
			}
		}

		r = tmp;
	}

	commit_reversible_command ();
	rs.clear ();

  out:
	track_canvas->get_window()->set_cursor (*current_canvas_cursor);
}

void
Editor::region_selection_op (void (Region::*pmf)(void))
{
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		Region* region = (*i)->region().get();
		(region->*pmf)();
	}
}


void
Editor::region_selection_op (void (Region::*pmf)(void*), void *arg)
{
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		Region* region = (*i)->region().get();
		(region->*pmf)(arg);
	}
}

void
Editor::region_selection_op (void (Region::*pmf)(bool), bool yn)
{
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		Region* region = (*i)->region().get();
		(region->*pmf)(yn);
	}
}

void
Editor::external_edit_region ()
{
	/* more to come */
}

void
Editor::brush (nframes64_t pos)
{
	RegionSelection sel;
	RegionSelection rs;

	get_regions_for_action (rs);

	snap_to (pos);

	if (rs.empty()) {
		return;
	}

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		mouse_brush_insert_region ((*i), pos);
	}
}

void
Editor::reset_region_gain_envelopes ()
{
	RegionSelection rs = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);

	if (!_session || rs.empty()) {
		return;
	}

	_session->begin_reversible_command (_("reset region gain"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			boost::shared_ptr<AutomationList> alist (arv->audio_region()->envelope());
			XMLNode& before (alist->get_state());

			arv->audio_region()->set_default_envelope ();
			_session->add_command (new MementoCommand<AutomationList>(*arv->audio_region()->envelope().get(), &before, &alist->get_state()));
		}
	}

	_session->commit_reversible_command ();
}

void
Editor::toggle_gain_envelope_visibility ()
{
	RegionSelection rs = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);

	if (!_session || rs.empty()) {
		return;
	}

	_session->begin_reversible_command (_("region gain envelope visible"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
                        arv->region()->clear_changes ();
			arv->set_envelope_visible (!arv->envelope_visible());
			_session->add_command (new StatefulDiffCommand (arv->region()));
		}
	}

	_session->commit_reversible_command ();
}

void
Editor::toggle_gain_envelope_active ()
{
	RegionSelection rs = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);

	if (!_session || rs.empty()) {
		return;
	}

	_session->begin_reversible_command (_("region gain envelope active"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			arv->region()->clear_changes ();
			arv->audio_region()->set_envelope_active (!arv->audio_region()->envelope_active());
			_session->add_command (new StatefulDiffCommand (arv->region()));
		}
	}

	_session->commit_reversible_command ();
}

void
Editor::toggle_region_lock ()
{
	RegionSelection rs = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);

	if (!_session || rs.empty()) {
		return;
	}

	_session->begin_reversible_command (_("region lock"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		(*i)->region()->clear_changes ();
		(*i)->region()->set_locked (!(*i)->region()->locked());
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}

	_session->commit_reversible_command ();
}

void
Editor::toggle_region_lock_style ()
{
	RegionSelection rs = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);

	if (!_session || rs.empty()) {
		return;
	}

	_session->begin_reversible_command (_("region lock style"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
                (*i)->region()->clear_changes ();
		PositionLockStyle const ns = (*i)->region()->position_lock_style() == AudioTime ? MusicTime : AudioTime;
		(*i)->region()->set_position_lock_style (ns);
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}

	_session->commit_reversible_command ();
}


void
Editor::toggle_region_mute ()
{
	RegionSelection rs = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);

	if (!_session || rs.empty()) {
		return;
	}

	_session->begin_reversible_command (_("region mute"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		(*i)->region()->clear_changes ();
		(*i)->region()->set_muted (!(*i)->region()->muted());
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}

	_session->commit_reversible_command ();
}

void
Editor::toggle_region_opaque ()
{
	RegionSelection rs = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);

	if (!_session || rs.empty()) {
		return;
	}

	_session->begin_reversible_command (_("region opacity"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		(*i)->region()->clear_changes ();
		(*i)->region()->set_opaque (!(*i)->region()->opaque());
		_session->add_command (new StatefulDiffCommand ((*i)->region()));
	}

	_session->commit_reversible_command ();
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
			new_state = !rtav->track()->record_enabled();
			first = false;
		}

		rtav->track()->set_record_enabled (new_state, this);
	}
}


void
Editor::set_fade_length (bool in)
{
	RegionSelection rs;

	get_regions_for_action (rs, true);

	if (rs.empty()) {
		return;
	}

	/* we need a region to measure the offset from the start */

	RegionView* rv = rs.front ();

	nframes64_t pos = get_preferred_edit_position();
	nframes64_t len;
	char const * cmd;

	if (pos > rv->region()->last_frame() || pos < rv->region()->first_frame()) {
		/* edit point is outside the relevant region */
		return;
	}

	if (in) {
		if (pos <= rv->region()->position()) {
			/* can't do it */
			return;
		}
		len = pos - rv->region()->position();
		cmd = _("set fade in length");
	} else {
		if (pos >= rv->region()->last_frame()) {
			/* can't do it */
			return;
		}
		len = rv->region()->last_frame() - pos;
		cmd = _("set fade out length");
	}

	begin_reversible_command (cmd);

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}

		boost::shared_ptr<AutomationList> alist;
		if (in) {
			alist = tmp->audio_region()->fade_in();
		} else {
			alist = tmp->audio_region()->fade_out();
		}

		XMLNode &before = alist->get_state();

		if (in) {
			tmp->audio_region()->set_fade_in_length (len);
			tmp->audio_region()->set_fade_in_active (true);
		} else {
			tmp->audio_region()->set_fade_out_length (len);
			tmp->audio_region()->set_fade_out_active (true);
		}

		XMLNode &after = alist->get_state();
		_session->add_command(new MementoCommand<AutomationList>(*alist, &before, &after));
	}

	commit_reversible_command ();
}

void
Editor::toggle_fade_active (bool in)
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	const char* cmd = (in ? _("toggle fade in active") : _("toggle fade out active"));
	bool have_switch = false;
	bool yn = false;

	begin_reversible_command (cmd);

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}

		boost::shared_ptr<AudioRegion> region (tmp->audio_region());

		/* make the behaviour consistent across all regions */

		if (!have_switch) {
			if (in) {
				yn = region->fade_in_active();
			} else {
				yn = region->fade_out_active();
			}
			have_switch = true;
		}

		region->clear_changes ();

		if (in) {
			region->set_fade_in_active (!yn);
		} else {
			region->set_fade_out_active (!yn);
		}
		
		_session->add_command(new StatefulDiffCommand (region));
	}

	commit_reversible_command ();
}

void
Editor::set_fade_in_shape (AudioRegion::FadeShape shape)
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("set fade in shape"));

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}

		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_in();
		XMLNode &before = alist->get_state();

		tmp->audio_region()->set_fade_in_shape (shape);

		XMLNode &after = alist->get_state();
		_session->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	commit_reversible_command ();

}

void
Editor::set_fade_out_shape (AudioRegion::FadeShape shape)
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("set fade out shape"));

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}

		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_out();
		XMLNode &before = alist->get_state();

		tmp->audio_region()->set_fade_out_shape (shape);

		XMLNode &after = alist->get_state();
		_session->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	commit_reversible_command ();
}

void
Editor::set_fade_in_active (bool yn)
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("set fade in active"));

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}


		boost::shared_ptr<AudioRegion> ar (tmp->audio_region());
		
                ar->clear_changes ();
		ar->set_fade_in_active (yn);
		_session->add_command (new StatefulDiffCommand (ar));
	}

	commit_reversible_command ();
}

void
Editor::set_fade_out_active (bool yn)
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	begin_reversible_command (_("set fade out active"));

	for (RegionSelection::iterator x = rs.begin(); x != rs.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}

		boost::shared_ptr<AudioRegion> ar (tmp->audio_region());

                ar->clear_changes ();
		ar->set_fade_out_active (yn);
		_session->add_command(new StatefulDiffCommand (ar));
	}

	commit_reversible_command ();
}

void
Editor::toggle_selected_region_fades (int dir)
{
	RegionSelection rs;
	RegionSelection::iterator i;
	boost::shared_ptr<AudioRegion> ar;
	bool yn;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

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

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		if ((ar = boost::dynamic_pointer_cast<AudioRegion>((*i)->region())) == 0) {
			continue;
		}
		if (dir == 1 || dir == 0) {
			ar->set_fade_in_active (!yn);
		}

		if (dir == -1 || dir == 0) {
			ar->set_fade_out_active (!yn);
		}
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

/** Update crossfade visibility after its configuration has been changed */
void
Editor::update_xfade_visibility ()
{
	_xfade_visibility = _session->config.get_xfades_visible ();

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		AudioTimeAxisView* v = dynamic_cast<AudioTimeAxisView*>(*i);
		if (v) {
			if (_xfade_visibility) {
				v->show_all_xfades ();
			} else {
				v->hide_all_xfades ();
			}
		}
	}
}

void
Editor::set_edit_point ()
{
	nframes64_t where;
	bool ignored;

	if (!mouse_frame (where, ignored)) {
		return;
	}

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
		_session->request_locate (entered_marker->position(), _session->transport_rolling());
	} else {
		nframes64_t where;
		bool ignored;

		if (!mouse_frame (where, ignored)) {
			return;
		}

		snap_to (where);

		if (_session) {
			_session->request_locate (where, _session->transport_rolling());
		}
	}
}

void
Editor::split ()
{
        if (((mouse_mode == MouseRange) || 
             (mouse_mode != MouseObject && _join_object_range_state == JOIN_OBJECT_RANGE_RANGE)) && 
            !selection->time.empty()) {
                separate_regions_between (selection->time);
                return;
        } 

	RegionSelection rs;

	get_regions_for_action (rs, true);

	nframes64_t where = get_preferred_edit_position();

	if (rs.empty()) {
		return;
	}

	split_regions_at (where, rs);
}

void
Editor::ensure_entered_track_selected (bool op_really_wants_one_track_if_none_are_selected)
{
	if (entered_track && mouse_mode == MouseObject) {
		if (!selection->tracks.empty()) {
			if (!selection->selected (entered_track)) {
				selection->add (entered_track);
			}
		} else {
			/* there is no selection, but this operation requires/prefers selected objects */

			if (op_really_wants_one_track_if_none_are_selected) {
				selection->set (entered_track);
			}
		}
	}
}

struct EditorOrderRouteSorter {
    bool operator() (boost::shared_ptr<Route> a, boost::shared_ptr<Route> b) {
	    /* use of ">" forces the correct sort order */
	    return a->order_key ("editor") < b->order_key ("editor");
    }
};

void
Editor::select_next_route()
{
	if (selection->tracks.empty()) {
		selection->set (track_views.front());
		return;
	}

	TimeAxisView* current = selection->tracks.front();

	RouteUI *rui;
	do {
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			if (*i == current) {
				++i;
				if (i != track_views.end()) {
					current = (*i);
				} else {
					current = (*(track_views.begin()));
					//selection->set (*(track_views.begin()));
				}
				break;
			}
		}
		rui = dynamic_cast<RouteUI *>(current);
	} while ( current->hidden() || (rui != NULL && !rui->route()->active()));

	selection->set(current);

	ensure_track_visible(current);
}

void
Editor::select_prev_route()
{
	if (selection->tracks.empty()) {
		selection->set (track_views.front());
		return;
	}

	TimeAxisView* current = selection->tracks.front();

	RouteUI *rui;
	do {
		for (TrackViewList::reverse_iterator i = track_views.rbegin(); i != track_views.rend(); ++i) {
			if (*i == current) {
				++i;
				if (i != track_views.rend()) {
					current = (*i);
				} else {
					current = *(track_views.rbegin());
				}
				break;
			}
		}
		rui = dynamic_cast<RouteUI *>(current);
	} while ( current->hidden() || (rui != NULL && !rui->route()->active()));

	selection->set (current);

	ensure_track_visible(current);
}

void
Editor::ensure_track_visible(TimeAxisView *track)
{
	if (track->hidden())
		return;

	double const current_view_min_y = vertical_adjustment.get_value();
	double const current_view_max_y = vertical_adjustment.get_value() + vertical_adjustment.get_page_size() - canvas_timebars_vsize;

	double const track_min_y = track->y_position ();
	double const track_max_y = track->y_position () + track->effective_height ();

	if (track_min_y >= current_view_min_y &&
            track_max_y <= current_view_max_y) {
		return;
	}

	double new_value;

	if (track_min_y < current_view_min_y) {
		// Track is above the current view
		new_value = track_min_y;
	} else {
		// Track is below the current view
		new_value = track->y_position () + track->effective_height() + canvas_timebars_vsize - vertical_adjustment.get_page_size();
	}

	vertical_adjustment.set_value(new_value);
}

void
Editor::set_loop_from_selection (bool play)
{
	if (_session == 0 || selection->time.empty()) {
		return;
	}

	nframes64_t start = selection->time[clicked_selection].start;
	nframes64_t end = selection->time[clicked_selection].end;

	set_loop_range (start, end,  _("set loop range from selection"));

	if (play) {
		_session->request_play_loop (true);
		_session->request_locate (start, true);
	}
}

void
Editor::set_loop_from_edit_range (bool play)
{
	if (_session == 0) {
		return;
	}

	nframes64_t start;
	nframes64_t end;

	if (!get_edit_op_range (start, end)) {
		return;
	}

	set_loop_range (start, end,  _("set loop range from edit range"));

	if (play) {
		_session->request_play_loop (true);
		_session->request_locate (start, true);
	}
}

void
Editor::set_loop_from_region (bool play)
{
	nframes64_t start = max_frames;
	nframes64_t end = 0;

	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		if ((*i)->region()->position() < start) {
			start = (*i)->region()->position();
		}
		if ((*i)->region()->last_frame() + 1 > end) {
			end = (*i)->region()->last_frame() + 1;
		}
	}

	set_loop_range (start, end, _("set loop range from region"));

	if (play) {
		_session->request_play_loop (true);
		_session->request_locate (start, true);
	}
}

void
Editor::set_punch_from_selection ()
{
	if (_session == 0 || selection->time.empty()) {
		return;
	}

	nframes64_t start = selection->time[clicked_selection].start;
	nframes64_t end = selection->time[clicked_selection].end;

	set_punch_range (start, end,  _("set punch range from selection"));
}

void
Editor::set_punch_from_edit_range ()
{
	if (_session == 0) {
		return;
	}

	nframes64_t start;
	nframes64_t end;

	if (!get_edit_op_range (start, end)) {
		return;
	}

	set_punch_range (start, end,  _("set punch range from edit range"));
}

void
Editor::set_punch_from_region ()
{
	nframes64_t start = max_frames;
	nframes64_t end = 0;

	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ++i) {
		if ((*i)->region()->position() < start) {
			start = (*i)->region()->position();
		}
		if ((*i)->region()->last_frame() + 1 > end) {
			end = (*i)->region()->last_frame() + 1;
		}
	}

	set_punch_range (start, end, _("set punch range from region"));
}

void
Editor::pitch_shift_regions ()
{
	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	pitch_shift (rs, 1.2);
}

void
Editor::use_region_as_bar ()
{
	if (!_session) {
		return;
	}

	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	RegionView* rv = rs.front();

	define_one_bar (rv->region()->position(), rv->region()->last_frame() + 1);
}

void
Editor::use_range_as_bar ()
{
	nframes64_t start, end;
	if (get_edit_op_range (start, end)) {
		define_one_bar (start, end);
	}
}

void
Editor::define_one_bar (nframes64_t start, nframes64_t end)
{
	nframes64_t length = end - start;

	const Meter& m (_session->tempo_map().meter_at (start));

	/* length = 1 bar */

	/* now we want frames per beat.
	   we have frames per bar, and beats per bar, so ...
	*/

	double frames_per_beat = length / m.beats_per_bar();

	/* beats per minute = */

	double beats_per_minute = (_session->frame_rate() * 60.0) / frames_per_beat;

	/* now decide whether to:

	    (a) set global tempo
	    (b) add a new tempo marker

	*/

	const TempoSection& t (_session->tempo_map().tempo_section_at (start));

	bool do_global = false;

	if ((_session->tempo_map().n_tempos() == 1) && (_session->tempo_map().n_meters() == 1)) {

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
	XMLNode& before (_session->tempo_map().get_state());

	if (do_global) {
		_session->tempo_map().change_initial_tempo (beats_per_minute, t.note_type());
	} else if (t.frame() == start) {
		_session->tempo_map().change_existing_tempo_at (start, beats_per_minute, t.note_type());
	} else {
		_session->tempo_map().add_tempo (Tempo (beats_per_minute, t.note_type()), start);
	}

	XMLNode& after (_session->tempo_map().get_state());

	_session->add_command (new MementoCommand<TempoMap>(_session->tempo_map(), &before, &after));
	commit_reversible_command ();
}

void
Editor::split_region_at_transients ()
{
	AnalysisFeatureList positions;

	if (!_session) {
		return;
	}

	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}

	_session->begin_reversible_command (_("split regions"));

	for (RegionSelection::iterator i = rs.begin(); i != rs.end(); ) {

		RegionSelection::iterator tmp;

		tmp = i;
		++tmp;

		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> ((*i)->region());

		if (ar && (ar->get_transients (positions) == 0)) {
			split_region_at_points ((*i)->region(), positions, true);
			positions.clear ();
		}

		i = tmp;
	}

	_session->commit_reversible_command ();

}

void
Editor::split_region_at_points (boost::shared_ptr<Region> r, AnalysisFeatureList& positions, bool can_ferret)
{
	bool use_rhythmic_rodent = false;

	boost::shared_ptr<Playlist> pl = r->playlist();

	if (!pl) {
		return;
	}

	if (positions.empty()) {
		return;
	}


	if (positions.size() > 20) {
		Glib::ustring msgstr = string_compose (_("You are about to split\n%1\ninto %2 pieces.\nThis could take a long time."), r->name(), positions.size() + 1);
		MessageDialog msg (msgstr,
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
		msg.present ();

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

	x = positions.begin();

	if (x == positions.end()) {
		return;
	}

	pl->freeze ();
	pl->remove_region (r);

	nframes64_t pos = 0;

	while (x != positions.end()) {
	  
		/* deal with positons that are out of scope of present region bounds */
		if (*x <= 0 || *x > r->length()){
			++x;
			continue;
		}

		/* file start = original start + how far we from the initial position ?
		 */

		nframes64_t file_start = r->start() + pos;

		/* length = next position - current position
		 */

		nframes64_t len = (*x) - pos;
		
		/* XXX we do we really want to allow even single-sample regions?
		   shouldn't we have some kind of lower limit on region size?
		*/

		if (len <= 0) {
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

		boost::shared_ptr<Region> nr = RegionFactory::create (r->sources(), plist, false);
		pl->add_region (nr, r->position() + pos);

		pos += len;
		++x;
	}

	string new_name;

	RegionFactory::region_name (new_name, r->name());
	
	/* Add the final region */
	PropertyList plist; 
		
	plist.add (ARDOUR::Properties::start, r->start() + pos);
	plist.add (ARDOUR::Properties::length, r->last_frame() - (r->position() + pos) + 1);
	plist.add (ARDOUR::Properties::name, new_name);
	plist.add (ARDOUR::Properties::layer, 0);

	boost::shared_ptr<Region> nr = RegionFactory::create (r->sources(), plist, false);
	pl->add_region (nr, r->position() + pos);

	
	pl->thaw ();

	_session->add_command (new StatefulDiffCommand (pl));
}

void
Editor::place_transient()
{
	if (!_session) {
		return;
	}

	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}
	
	nframes64_t where = get_preferred_edit_position();

	_session->begin_reversible_command (_("place transient"));
	
	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		framepos_t position = (*r)->region()->position();
		(*r)->region()->add_transient(where - position);
	}
	
	_session->commit_reversible_command ();
}

void
Editor::remove_transient(ArdourCanvas::Item* item)
{
	if (!_session) {
		return;
	}

	ArdourCanvas::SimpleLine* _line = reinterpret_cast<ArdourCanvas::SimpleLine*> (item);
	assert (_line);

	AudioRegionView* _arv = reinterpret_cast<AudioRegionView*> (item->get_data ("regionview"));
	_arv->remove_transient(_line->property_x1());
}

void
Editor::snap_regions_to_grid()
{
	if (!_session) {
		return;
	}

	RegionSelection rs;

	get_regions_for_action (rs);

	if (rs.empty()) {
		return;
	}
	
	_session->begin_reversible_command (_("snap regions to grid"));
	
	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
		framepos_t start_frame = (*r)->region()->first_frame ();
		snap_to (start_frame);
		(*r)->region()->set_position (start_frame, this);
	}
	
	_session->commit_reversible_command ();
}

void
Editor::close_region_gaps()
{	
	if (!_session) {
		return;
	}

	RegionSelection rs;

	get_regions_for_action (rs);
	
	if (rs.empty()) {
		return;
	}
	
	Dialog dialog (rs.size() > 1 ? _("Conform regions") : _("Conform region"));
	
	HBox hbox_crossfade;
	hbox_crossfade.set_spacing (10);
	//hbox_crossfade.set_border_width (3);
	hbox_crossfade.pack_start (*manage (new Label (_("Crossfade length:"))));
	
	SpinButton spin_crossfade (1, 0);
	spin_crossfade.set_range (0, 15);
	spin_crossfade.set_increments (1, 1);
	spin_crossfade.set_value (3);
	
	hbox_crossfade.pack_start (spin_crossfade);
	hbox_crossfade.pack_start (*manage (new Label (_("ms"))));
	hbox_crossfade.show_all ();

	HBox hbox_pullback;
	
	hbox_pullback.set_spacing (10);
	//hbox_pullback.set_border_width (3);
	hbox_pullback.pack_start (*manage (new Label (_("Pull-back length:"))));
	
	SpinButton spin_pullback (1, 0);
	spin_pullback.set_range (0, 15);
	spin_pullback.set_increments (1, 1);
	spin_pullback.set_value (5);
	
	hbox_pullback.pack_start (spin_pullback);
	hbox_pullback.pack_start (*manage (new Label (_("ms"))));
	hbox_pullback.show_all ();
	
	dialog.get_vbox()->set_spacing (6);
	dialog.get_vbox()->pack_start (hbox_crossfade);
	dialog.get_vbox()->pack_start (hbox_pullback);
	dialog.add_button (Stock::CANCEL, RESPONSE_CANCEL);
	dialog.add_button (_("Ok"), RESPONSE_ACCEPT);

	if (dialog.run () == RESPONSE_CANCEL) {
		return;
	}

	nframes64_t crossfade_len  = spin_crossfade.get_value(); 
	nframes64_t pull_back_frames = spin_pullback.get_value();

	crossfade_len = lrintf (crossfade_len * _session->frame_rate()/1000);
	pull_back_frames = lrintf (pull_back_frames * _session->frame_rate()/1000);

	/* Iterate over the region list and make adjacent regions overlap by crossfade_len_ms */
	
	_session->begin_reversible_command (_("close region gaps"));
		
	int idx = 0;
	boost::shared_ptr<Region> last_region;
	
	rs.sort_by_position_and_track();
	
	for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {

		nframes64_t position = (*r)->region()->position();
	  
		if (idx == 0 || position < last_region->position()){
			last_region = (*r)->region();
			idx++;
			continue;
		}
		
		(*r)->region()->trim_front( (position - pull_back_frames), this );
		last_region->trim_end( (position - pull_back_frames + crossfade_len), this );
		
		last_region = (*r)->region();
		
		idx++;
	}
	
	_session->commit_reversible_command ();
}

void
Editor::tab_to_transient (bool forward)
{
	AnalysisFeatureList positions;

	if (!_session) {
		return;
	}

	nframes64_t pos = _session->audible_frame ();

	if (!selection->tracks.empty()) {

		for (TrackSelection::iterator t = selection->tracks.begin(); t != selection->tracks.end(); ++t) {

			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*t);

			if (rtv) {
				boost::shared_ptr<Track> tr = rtv->track();
				if (tr) {
					boost::shared_ptr<Playlist> pl = tr->playlist ();
					if (pl) {
						nframes64_t result = pl->find_next_transient (pos, forward ? 1 : -1);

						if (result >= 0) {
							positions.push_back (result);
						}
					}
				}
			}
		}

	} else {

		RegionSelection rs;

		get_regions_for_action (rs);

		if (rs.empty()) {
			return;
		}

		for (RegionSelection::iterator r = rs.begin(); r != rs.end(); ++r) {
			(*r)->region()->get_transients (positions);
		}
	}

	TransientDetector::cleanup_transients (positions, _session->frame_rate(), 3.0);

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
	if (!_session) return;
	nframes64_t pos = playhead_cursor->current_frame;
	if (pos < max_frames - 1) {
		pos += 2;
		snap_to_internal (pos, 1, false);
		_session->request_locate (pos);
	}
}


void
Editor::playhead_backward_to_grid ()
{
	if (!_session) return;
	nframes64_t pos = playhead_cursor->current_frame;
	if (pos > 2) {
		pos -= 2;
		snap_to_internal (pos, -1, false);
		_session->request_locate (pos);
	}
}

void
Editor::set_track_height (Height h)
{
	TrackSelection& ts (selection->tracks);

	for (TrackSelection::iterator x = ts.begin(); x != ts.end(); ++x) {
		(*x)->set_height (h);
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
				target = !rtv->_route->active();
				first = false;
			}
			rtv->_route->set_active (target);
		}
	}
}

void
Editor::remove_tracks ()
{
	TrackSelection& ts (selection->tracks);

	if (ts.empty()) {
		return;
	}

	vector<string> choices;
	string prompt;
	int ntracks = 0;
	int nbusses = 0;
	const char* trackstr;
	const char* busstr;
	vector<boost::shared_ptr<Route> > routes;
        bool special_bus = false;

	for (TrackSelection::iterator x = ts.begin(); x != ts.end(); ++x) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*x);
		if (rtv) {
			if (rtv->is_track()) {
				ntracks++;
			} else {
				nbusses++;
			}
		}
		routes.push_back (rtv->_route);

                if (rtv->route()->is_master() || rtv->route()->is_monitor()) {
                        special_bus = true;
                }
        }

        if (special_bus && !Config->get_allow_special_bus_removal()) {
                MessageDialog msg (_("That would be bad news ...."),
                                   false,
                                   Gtk::MESSAGE_INFO,
                                   Gtk::BUTTONS_OK);
                msg.set_secondary_text (string_compose (_(
"Removing the master or monitor bus is such a bad idea\n\
that %1 is not going to allow it.\n\
\n\
If you really want to do this sort of thing\n\
edit your ardour.rc file to set the\n\
\"allow-special-bus-removal\" option to be \"yes\""), PROGRAM_NAME));

                msg.present ();
                msg.run ();
                return;
        }
                
	if (ntracks + nbusses == 0) {
 		return;
	}

	if (ntracks > 1) {
		trackstr = _("tracks");
	} else {
		trackstr = _("track");
	}

	if (nbusses > 1) {
		busstr = _("busses");
	} else {
		busstr = _("bus");
	}

	if (ntracks) {
		if (nbusses) {
			prompt  = string_compose (_("Do you really want to remove %1 %2 and %3 %4?\n"
						    "(You may also lose the playlists associated with the %2)\n\n"
						    "This action cannot be undone, and the session file will be overwritten!"),
						  ntracks, trackstr, nbusses, busstr);
		} else {
			prompt  = string_compose (_("Do you really want to remove %1 %2?\n"
						    "(You may also lose the playlists associated with the %2)\n\n"
						    "This action cannot be undone, and the session file will be overwritten!"),
						  ntracks, trackstr);
		}
	} else if (nbusses) {
		prompt  = string_compose (_("Do you really want to remove %1 %2?\n\n"
                                            "This action cannot be undon, and the session file will be overwritten"),
					  nbusses, busstr);
	}

	choices.push_back (_("No, do nothing."));
	if (ntracks + nbusses > 1) {
		choices.push_back (_("Yes, remove them."));
	} else {
		choices.push_back (_("Yes, remove it."));
	}

	string title;
	if (ntracks) {
		title = string_compose (_("Remove %1"), trackstr);
	} else {
		title = string_compose (_("Remove %1"), busstr);
	}

	Choice prompter (title, prompt, choices);

	if (prompter.run () != 1) {
		return;
	}

	for (vector<boost::shared_ptr<Route> >::iterator x = routes.begin(); x != routes.end(); ++x) {
		_session->remove_route (*x);
	}
}

void
Editor::do_insert_time ()
{
	if (selection->tracks.empty()) {
		return;
	}

	InsertTimeDialog d (*this);
	int response = d.run ();

	if (response != RESPONSE_OK) {
		return;
	}

	if (d.distance() == 0) {
		return;
	}

	InsertTimeOption opt = d.intersected_region_action ();

	insert_time (
		get_preferred_edit_position(),
		d.distance(),
		opt,
		d.move_glued(),
		d.move_markers(),
		d.move_glued_markers(),
		d.move_locked_markers(),
		d.move_tempos()
		);
}

void
Editor::insert_time (nframes64_t pos, nframes64_t frames, InsertTimeOption opt,
		     bool ignore_music_glue, bool markers_too, bool glued_markers_too, bool locked_markers_too, bool tempo_too)
{
	bool commit = false;

	if (Config->get_edit_mode() == Lock) {
		return;
	}

	begin_reversible_command (_("insert time"));

	for (TrackSelection::iterator x = selection->tracks.begin(); x != selection->tracks.end(); ++x) {
		/* regions */
		boost::shared_ptr<Playlist> pl = (*x)->playlist();

		if (pl) {

			pl->clear_changes ();
                        pl->clear_owned_changes ();

			if (opt == SplitIntersected) {
				pl->split (pos);
			}

			pl->shift (pos, frames, (opt == MoveIntersected), ignore_music_glue);

                        vector<Command*> cmds;
                        pl->rdiff (cmds);
			_session->add_commands (cmds);
			
			_session->add_command (new StatefulDiffCommand (pl));
			commit = true;
		}

		/* automation */
		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (*x);
		if (rtav) {
			rtav->route ()->shift (pos, frames);
			commit = true;
		}
	}

	/* markers */
	if (markers_too) {
		bool moved = false;
		XMLNode& before (_session->locations()->get_state());
		Locations::LocationList copy (_session->locations()->list());

		for (Locations::LocationList::iterator i = copy.begin(); i != copy.end(); ++i) {

			Locations::LocationList::const_iterator tmp;

			bool const was_locked = (*i)->locked ();
			if (locked_markers_too) {
				(*i)->unlock ();
			}

			if ((*i)->position_lock_style() == AudioTime || glued_markers_too) {

				if ((*i)->start() >= pos) {
					(*i)->set_start ((*i)->start() + frames);
					if (!(*i)->is_mark()) {
						(*i)->set_end ((*i)->end() + frames);
					}
					moved = true;
				}
				
			}

			if (was_locked) {
				(*i)->lock ();
			}
		}

		if (moved) {
			XMLNode& after (_session->locations()->get_state());
			_session->add_command (new MementoCommand<Locations>(*_session->locations(), &before, &after));
		}
	}

	if (tempo_too) {
		_session->tempo_map().insert_time (pos, frames);
	}

	if (commit) {
		commit_reversible_command ();
	}
}

void
Editor::fit_selected_tracks ()
{
	fit_tracks (selection->tracks);
}

void
Editor::fit_tracks (TrackViewList & tracks)
{
	if (tracks.empty()) {
		return;
	}

	uint32_t child_heights = 0;

	for (TrackSelection::iterator t = tracks.begin(); t != tracks.end(); ++t) {

		if (!(*t)->marked_for_display()) {
			continue;
		}

		child_heights += (*t)->effective_height() - (*t)->current_height();
	}

	uint32_t h = (uint32_t) floor ((_canvas_height - child_heights - canvas_timebars_vsize) / tracks.size());
	double first_y_pos = DBL_MAX;

	if (h < TimeAxisView::preset_height (HeightSmall)) {
		MessageDialog msg (*this, _("There are too many tracks to fit in the current window"));
		/* too small to be displayed */
		return;
	}

	undo_visual_stack.push_back (current_visual_state());

	/* build a list of all tracks, including children */

	TrackViewList all;
	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		all.push_back (*i);
		TimeAxisView::Children c = (*i)->get_child_list ();
		for (TimeAxisView::Children::iterator j = c.begin(); j != c.end(); ++j) {
			all.push_back (j->get());
		}
	}

	/* operate on all tracks, hide unselected ones that are in the middle of selected ones */

	bool prev_was_selected = false;
	bool is_selected = tracks.contains (all.front());
	bool next_is_selected;

	for (TrackViewList::iterator t = all.begin(); t != all.end(); ++t) {

		TrackViewList::iterator next;

		next = t;
		++next;

		if (next != all.end()) {
			next_is_selected = tracks.contains (*next);
		} else {
			next_is_selected = false;
		}

		if (is_selected) {
			(*t)->set_height (h);
			first_y_pos = std::min ((*t)->y_position (), first_y_pos);
		} else {
			if (prev_was_selected && next_is_selected) {
				hide_track_in_display (*t);
			}
		}

		prev_was_selected = is_selected;
		is_selected = next_is_selected;
	}

	/*
	   set the controls_layout height now, because waiting for its size
	   request signal handler will cause the vertical adjustment setting to fail
	*/

	controls_layout.property_height () = full_canvas_height - canvas_timebars_vsize;
	vertical_adjustment.set_value (first_y_pos);

	redo_visual_stack.push_back (current_visual_state());
}

void
Editor::save_visual_state (uint32_t n)
{
	while (visual_states.size() <= n) {
		visual_states.push_back (0);
	}

	delete visual_states[n];

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
	if (visual_state_op_connection.empty()) {
		visual_state_op_connection = Glib::signal_timeout().connect (sigc::bind (sigc::mem_fun (*this, &Editor::end_visual_state_op), n), 1000);
	}
}

void
Editor::cancel_visual_state_op (uint32_t n)
{
	if (!visual_state_op_connection.empty()) {
		visual_state_op_connection.disconnect();
		goto_visual_state (n);
	}  else {
		//we land here if called from the menu OR if end_visual_state_op has been called
		//so check if we are already in visual state n
		// XXX not yet checking it at all, but redoing does not hurt
		goto_visual_state (n);
	}
}

bool
Editor::end_visual_state_op (uint32_t n)
{
	visual_state_op_connection.disconnect();
	save_visual_state (n);

	PopUp* pup = new PopUp (WIN_POS_MOUSE, 1000, true);
	char buf[32];
	snprintf (buf, sizeof (buf), _("Saved view %u"), n+1);
	pup->set_text (buf);
	pup->touch();

	return false; // do not call again
}

