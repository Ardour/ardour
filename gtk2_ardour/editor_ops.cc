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

#include <unistd.h>

#include <cstdlib>
#include <cmath>
#include <string>
#include <map>

#include <pbd/error.h>
#include <pbd/basename.h>
#include <pbd/pthread_utils.h>
#include <pbd/memento_command.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>
#include <gtkmm2ext/window_title.h>

#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/audio_diskstream.h>
#include <ardour/utils.h>
#include <ardour/location.h>
#include <ardour/named_selection.h>
#include <ardour/audio_track.h>
#include <ardour/audioplaylist.h>
#include <ardour/region_factory.h>
#include <ardour/playlist_factory.h>
#include <ardour/reverse.h>

#include "ardour_ui.h"
#include "editor.h"
#include "time_axis_view.h"
#include "audio_time_axis.h"
#include "automation_time_axis.h"
#include "streamview.h"
#include "audio_region_view.h"
#include "rgb_macros.h"
#include "selection_templates.h"
#include "selection.h"
#include "editing.h"
#include "gtk-custom-hruler.h"
#include "gui_thread.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;

/***********************************************************************
  Editor operations
 ***********************************************************************/

void
Editor::undo (uint32_t n)
{
	if (session) {
		session->undo (n);
	}
}

void
Editor::redo (uint32_t n)
{
	if (session) {
		session->redo (n);
	}
}

int
Editor::ensure_cursor (nframes_t *pos)
{
	*pos = edit_cursor->current_frame;
	return 0;
}

void
Editor::split_region ()
{
	split_region_at (edit_cursor->current_frame);
}

void
Editor::split_region_at (nframes_t where)
{
	split_regions_at (where, selection->regions);
}

void
Editor::split_regions_at (nframes_t where, RegionSelection& regions)
{
	begin_reversible_command (_("split"));

	// if splitting a single region, and snap-to is using
	// region boundaries, don't pay attention to them

	if (regions.size() == 1) {
		switch (snap_type) {
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
		
		tmp = a;
		++tmp;

		boost::shared_ptr<Playlist> pl = (*a)->region()->playlist();

		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*a);
		if (arv)
			_new_regionviews_show_envelope = arv->envelope_visible();
		
		if (pl) {
                        XMLNode &before = pl->get_state();
			pl->split_region ((*a)->region(), where);
                        XMLNode &after = pl->get_state();
                        session->add_command(new MementoCommand<Playlist>(*pl, &before, &after));
		}

		a = tmp;
    }

	commit_reversible_command ();
	_new_regionviews_show_envelope = false;
}

void
Editor::remove_clicked_region ()
{
	if (clicked_audio_trackview == 0 || clicked_regionview == 0) {
		return;
	}

	boost::shared_ptr<Playlist> playlist = clicked_audio_trackview->playlist();
	
	begin_reversible_command (_("remove region"));
        XMLNode &before = playlist->get_state();
	playlist->remove_region (clicked_regionview->region());
        XMLNode &after = playlist->get_state();
	session->add_command(new MementoCommand<Playlist>(*playlist, &before, &after));
	commit_reversible_command ();
}

void
Editor::destroy_clicked_region ()
{
	uint32_t selected = selection->regions.size();

	if (!session || !selected) {
		return;
	}

	vector<string> choices;
	string prompt;
	
	prompt  = string_compose (_(" This is destructive, will possibly delete audio files\n\
It cannot be undone\n\
Do you really want to destroy %1 ?"),
			   (selected > 1 ? 
			    _("these regions") : _("this region")));

	choices.push_back (_("No, do nothing."));

	if (selected > 1) {
		choices.push_back (_("Yes, destroy them."));
	} else {
		choices.push_back (_("Yes, destroy it."));
	}

	Gtkmm2ext::Choice prompter (prompt, choices);
	
	if (prompter.run() == 0) { /* first choice */
		return;
	}

	if (selected) {
		list<boost::shared_ptr<Region> > r;

		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			r.push_back ((*i)->region());
		}

		session->destroy_regions (r);
	} 
}

boost::shared_ptr<Region>
Editor::select_region_for_operation (int dir, TimeAxisView **tv)
{
	RegionView* rv;
	boost::shared_ptr<Region> region;
	nframes_t start = 0;

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
	nframes_t start;

	if ((region = select_region_for_operation (next ? 1 : 0, &tv)) == 0) {
		return;
	}

	if (region && selection->time.start () == selection->time.end_frame ()) {
		start = region->position();
	} else {
		start = selection->time.start ();
	}

	/* Try to leave the selection with the same route if possible */

	if ((tv = selection->time.track) == 0) {
		return;
	}

	begin_reversible_command (_("extend selection"));
	selection->set (tv, start, region->position() + region->length());
	commit_reversible_command ();
}

void
Editor::extend_selection_to_start_of_region (bool previous)
{
	TimeAxisView *tv;
	boost::shared_ptr<Region> region;
	nframes_t end;

	if ((region = select_region_for_operation (previous ? -1 : 0, &tv)) == 0) {
		return;
	}

	if (region && selection->time.start () == selection->time.end_frame ()) {
		end = region->position() + region->length();
	} else {
		end = selection->time.end_frame ();
	}

	/* Try to leave the selection with the same route if possible */
	
	if ((tv = selection->time.track) == 0) {
		return;
	}

	begin_reversible_command (_("extend selection"));
	selection->set (tv, region->position(), end);
	commit_reversible_command ();
}


void
Editor::nudge_forward (bool next)
{
	nframes_t distance;
	nframes_t next_distance;

	if (!session) return;
	
	if (!selection->regions.empty()) {

		begin_reversible_command (_("nudge forward"));

		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());
			
			distance = get_nudge_distance (r->position(), next_distance);

			if (next) {
				distance = next_distance;
			}

                        XMLNode &before = r->playlist()->get_state();
			r->set_position (r->position() + distance, this);
                        XMLNode &after = r->playlist()->get_state();
			session->add_command (new MementoCommand<Playlist>(*(r->playlist()), &before, &after));
		}

		commit_reversible_command ();

	} else {
		distance = get_nudge_distance (playhead_cursor->current_frame, next_distance);
		session->request_locate (playhead_cursor->current_frame + distance);
	}
}
		
void
Editor::nudge_backward (bool next)
{
	nframes_t distance;
	nframes_t next_distance;

	if (!session) return;
	
	if (!selection->regions.empty()) {

		begin_reversible_command (_("nudge forward"));

		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());

			distance = get_nudge_distance (r->position(), next_distance);
			
			if (next) {
				distance = next_distance;
			}

                        XMLNode &before = r->playlist()->get_state();
			
			if (r->position() > distance) {
				r->set_position (r->position() - distance, this);
			} else {
				r->set_position (0, this);
			}
                        XMLNode &after = r->playlist()->get_state();
			session->add_command(new MementoCommand<Playlist>(*(r->playlist()), &before, &after));
		}

		commit_reversible_command ();

	} else {

		distance = get_nudge_distance (playhead_cursor->current_frame, next_distance);

		if (playhead_cursor->current_frame > distance) {
			session->request_locate (playhead_cursor->current_frame - distance);
		} else {
			session->goto_start();
		}
	}
}

void
Editor::nudge_forward_capture_offset ()
{
	nframes_t distance;

	if (!session) return;
	
	if (!selection->regions.empty()) {

		begin_reversible_command (_("nudge forward"));

		distance = session->worst_output_latency();

		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());
			
			XMLNode &before = r->playlist()->get_state();
			r->set_position (r->position() + distance, this);
			XMLNode &after = r->playlist()->get_state();
			session->add_command(new MementoCommand<Playlist>(*(r->playlist()), &before, &after));
		}

		commit_reversible_command ();

	} 
}
		
void
Editor::nudge_backward_capture_offset ()
{
	nframes_t distance;

	if (!session) return;
	
	if (!selection->regions.empty()) {

		begin_reversible_command (_("nudge forward"));

		distance = session->worst_output_latency();

		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			boost::shared_ptr<Region> r ((*i)->region());

                        XMLNode &before = r->playlist()->get_state();
			
			if (r->position() > distance) {
				r->set_position (r->position() - distance, this);
			} else {
				r->set_position (0, this);
			}
                        XMLNode &after = r->playlist()->get_state();
			session->add_command(new MementoCommand<Playlist>(*(r->playlist()), &before, &after));
		}

		commit_reversible_command ();
	}
}

/* DISPLAY MOTION */

void
Editor::move_to_start ()
{
	session->goto_start ();
}

void
Editor::move_to_end ()
{

	session->request_locate (session->current_end_frame());
}

void
Editor::build_region_boundary_cache ()
{
	nframes_t pos = 0;
	vector<RegionPoint> interesting_points;
	boost::shared_ptr<Region> r;
	TrackViewList tracks;
	bool at_end = false;

	region_boundary_cache.clear ();

	if (session == 0) {
		return;
	}
	
	switch (snap_type) {
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
		fatal << string_compose (_("build_region_boundary_cache called with snap_type = %1"), snap_type) << endmsg;
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

	while (pos < session->current_end_frame() && !at_end) {

		nframes_t rpos;
		nframes_t lpos = max_frames;

		for (vector<RegionPoint>::iterator p = interesting_points.begin(); p != interesting_points.end(); ++p) {

			if ((r = find_next_region (pos, *p, 1, tlist, &ontrack)) == 0) {
				at_end = true;
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
				rpos = r->adjust_to_sync (r->first_frame());
				break;
			default:
				break;
			}
			
			float speed = 1.0f;
			AudioTimeAxisView *atav;
			
			if (ontrack != 0 && (atav = dynamic_cast<AudioTimeAxisView*>(ontrack)) != 0 ) {
				if (atav->get_diskstream() != 0) {
					speed = atav->get_diskstream()->speed();
				}
			}
			
			rpos = track_frame_to_session_frame (rpos, speed);

			if (rpos < lpos) {
				lpos = rpos;
			}

			/* prevent duplicates, but we don't use set<> because we want to be able
			   to sort later.
			*/

			vector<nframes_t>::iterator ri; 
			
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
}

boost::shared_ptr<Region>
Editor::find_next_region (nframes_t frame, RegionPoint point, int32_t dir, TrackViewList& tracks, TimeAxisView **ontrack)
{
	TrackViewList::iterator i;
	nframes_t closest = max_frames;
	boost::shared_ptr<Region> ret;
	nframes_t rpos = 0;

	float track_speed;
	nframes_t track_frame;
	AudioTimeAxisView *atav;

	for (i = tracks.begin(); i != tracks.end(); ++i) {

		nframes_t distance;
		boost::shared_ptr<Region> r;
		
		track_speed = 1.0f;
		if ( (atav = dynamic_cast<AudioTimeAxisView*>(*i)) != 0 ) {
			if (atav->get_diskstream()!=0)
				track_speed = atav->get_diskstream()->speed();
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
			rpos = r->adjust_to_sync (r->first_frame());
			break;
		}
		// rpos is a "track frame", converting it to "session frame"
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

void
Editor::cursor_to_region_point (Cursor* cursor, RegionPoint point, int32_t dir)
{
	boost::shared_ptr<Region> r;
	nframes_t pos = cursor->current_frame;

	if (!session) {
		return;
	}

	TimeAxisView *ontrack = 0;

	// so we don't find the current region again..
	if (dir>0 || pos>0)
		pos+=dir;

	if (!selection->tracks.empty()) {
		
		r = find_next_region (pos, point, dir, selection->tracks, &ontrack);
		
	} else if (clicked_trackview) {
		
		TrackViewList t;
		t.push_back (clicked_trackview);
		
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
		pos = r->adjust_to_sync (r->first_frame());
		break;	
	}
	
	float speed = 1.0f;
	AudioTimeAxisView *atav;

	if ( ontrack != 0 && (atav = dynamic_cast<AudioTimeAxisView*>(ontrack)) != 0 ) {
		if (atav->get_diskstream() != 0) {
			speed = atav->get_diskstream()->speed();
		}
	}

	pos = track_frame_to_session_frame(pos, speed);
	
	if (cursor == playhead_cursor) {
		session->request_locate (pos);
	} else {
		cursor->set_position (pos);
	}
}

void
Editor::cursor_to_next_region_point (Cursor* cursor, RegionPoint point)
{
	cursor_to_region_point (cursor, point, 1);
}

void
Editor::cursor_to_previous_region_point (Cursor* cursor, RegionPoint point)
{
	cursor_to_region_point (cursor, point, -1);
}

void
Editor::cursor_to_selection_start (Cursor *cursor)
{
	nframes_t pos = 0;
	switch (mouse_mode) {
	case MouseObject:
		if (!selection->regions.empty()) {
			pos = selection->regions.start();
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
		session->request_locate (pos);
	} else {
		cursor->set_position (pos);
	}
}

void
Editor::cursor_to_selection_end (Cursor *cursor)
{
	nframes_t pos = 0;

	switch (mouse_mode) {
	case MouseObject:
		if (!selection->regions.empty()) {
			pos = selection->regions.end_frame();
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
		session->request_locate (pos);
	} else {
		cursor->set_position (pos);
	}
}

void
Editor::scroll_playhead (bool forward)
{
	nframes_t pos = playhead_cursor->current_frame;
	nframes_t delta = (nframes_t) floor (current_page_frames() / 0.8);

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

	session->request_locate (pos);
}

void
Editor::playhead_backward ()
{
	nframes_t pos;
	nframes_t cnt;
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (nframes_t) floor (prefix * session->frame_rate ());
		} else {
			cnt = (nframes_t) prefix;
		}
	}

	pos = playhead_cursor->current_frame;

	if ((nframes_t) pos < cnt) {
		pos = 0;
	} else {
		pos -= cnt;
	}
	
	/* XXX this is completely insane. with the current buffering
	   design, we'll force a complete track buffer flush and
	   reload, just to move 1 sample !!!
	*/

	session->request_locate (pos);
}

void
Editor::playhead_forward ()
{
	nframes_t pos;
	nframes_t cnt;
	bool was_floating;
	float prefix;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (nframes_t) floor (prefix * session->frame_rate ());
		} else {
			cnt = (nframes_t) floor (prefix);
		}
	}

	pos = playhead_cursor->current_frame;
	
	/* XXX this is completely insane. with the current buffering
	   design, we'll force a complete track buffer flush and
	   reload, just to move 1 sample !!!
	*/

	session->request_locate (pos+cnt);
}

void
Editor::cursor_align (bool playhead_to_edit)
{
	if (playhead_to_edit) {
		if (session) {
			session->request_locate (edit_cursor->current_frame);
		}
	} else {
		edit_cursor->set_position (playhead_cursor->current_frame);
	}
}

void
Editor::edit_cursor_backward ()
{
	nframes_t pos;
	nframes_t cnt;
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (nframes_t) floor (prefix * session->frame_rate ());
		} else {
			cnt = (nframes_t) prefix;
		}
	}

	pos = edit_cursor->current_frame;

	if ((nframes_t) pos < cnt) {
		pos = 0;
	} else {
		pos -= cnt;
	}
	
	edit_cursor->set_position (pos);
}

void
Editor::edit_cursor_forward ()
{
	nframes_t pos;
	nframes_t cnt;
	bool was_floating;
	float prefix;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (nframes_t) floor (prefix * session->frame_rate ());
		} else {
			cnt = (nframes_t) floor (prefix);
		}
	}

	pos = edit_cursor->current_frame;
	edit_cursor->set_position (pos+cnt);
}

void
Editor::goto_frame ()
{
	float prefix;
	bool was_floating;
	nframes_t frame;

	if (get_prefix (prefix, was_floating)) {
		return;
	}

	if (was_floating) {
		frame = (nframes_t) floor (prefix * session->frame_rate());
	} else {
		frame = (nframes_t) floor (prefix);
	}

	session->request_locate (frame);
}

void
Editor::scroll_backward (float pages)
{
	nframes_t frame;
	nframes_t one_page = (nframes_t) rint (canvas_width * frames_per_unit);
	bool was_floating;
	float prefix;
	nframes_t cnt;
	
	if (get_prefix (prefix, was_floating)) {
		cnt = (nframes_t) floor (pages * one_page);
	} else {
		if (was_floating) {
			cnt = (nframes_t) floor (prefix * session->frame_rate());
		} else {
			cnt = (nframes_t) floor (prefix * one_page);
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
	nframes_t frame;
	nframes_t one_page = (nframes_t) rint (canvas_width * frames_per_unit);
	bool was_floating;
	float prefix;
	nframes_t cnt;
	
	if (get_prefix (prefix, was_floating)) {
		cnt = (nframes_t) floor (pages * one_page);
	} else {
		if (was_floating) {
			cnt = (nframes_t) floor (prefix * session->frame_rate());
		} else {
			cnt = (nframes_t) floor (prefix * one_page);
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
	if (vert_value > vertical_adjustment.get_upper() - canvas_height) {
		vert_value = vertical_adjustment.get_upper() - canvas_height;
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

        Gtk::Adjustment* adj = edit_vscrollbar.get_adjustment();
	double vert_value = adj->get_value() + 20;

	if (vert_value>adj->get_upper() - canvas_height) {
		vert_value = adj->get_upper() - canvas_height;
	}
	adj->set_value (vert_value);
}

void
Editor::scroll_tracks_up_line ()
{
        Gtk::Adjustment* adj = edit_vscrollbar.get_adjustment();
	adj->set_value (adj->get_value() - 20);
}

/* ZOOM */

void
Editor::temporal_zoom_step (bool coarser)
{
	ENSURE_GUI_THREAD (bind (mem_fun (*this, &Editor::temporal_zoom_step), coarser));

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
	if (!session) return;
	
	nframes_t current_page = current_page_frames();
	nframes_t current_leftmost = leftmost_frame;
	nframes_t current_rightmost;
	nframes_t current_center;
	nframes_t new_page;
	nframes_t leftmost_after_zoom = 0;
	double nfpu;

	nfpu = fpu;
	
	new_page = (nframes_t) floor (canvas_width * nfpu);

	switch (zoom_focus) {
	case ZoomFocusLeft:
		leftmost_after_zoom = current_leftmost;
		break;
		
	case ZoomFocusRight:
		current_rightmost = leftmost_frame + current_page;
		if (current_rightmost > new_page) {
			leftmost_after_zoom = current_rightmost - new_page;
		} else {
			leftmost_after_zoom = 0;
		}
		break;
		
	case ZoomFocusCenter:
		current_center = current_leftmost + (current_page/2); 
		if (current_center > (new_page/2)) {
			leftmost_after_zoom = current_center - (new_page / 2);
		} else {
			leftmost_after_zoom = 0;
		}
		break;
		
	case ZoomFocusPlayhead:
		/* try to keep the playhead in the center */
		if (playhead_cursor->current_frame > new_page/2) {
			leftmost_after_zoom = playhead_cursor->current_frame - (new_page/2);
		} else {
			leftmost_after_zoom = 0;
		}
		break;

	case ZoomFocusEdit:
		/* try to keep the edit cursor in the center */
		if (edit_cursor->current_frame > new_page/2) {
			leftmost_after_zoom = edit_cursor->current_frame - (new_page/2);
		} else {
			leftmost_after_zoom = 0;
		}
		break;
		
	}
 
	// leftmost_after_zoom = min (leftmost_after_zoom, session->current_end_frame());

//	begin_reversible_command (_("zoom"));
//	session->add_undo (bind (mem_fun(*this, &Editor::reposition_and_zoom), current_leftmost, frames_per_unit));
//	session->add_redo (bind (mem_fun(*this, &Editor::reposition_and_zoom), leftmost_after_zoom, nfpu));
//	commit_reversible_command ();

	reposition_and_zoom (leftmost_after_zoom, nfpu);
}	

void
Editor::temporal_zoom_selection ()
{
	if (!selection) return;
	
	if (selection->time.empty()) {
		return;
	}

	nframes_t start = selection->time[clicked_selection].start;
	nframes_t end = selection->time[clicked_selection].end;

	temporal_zoom_by_frame (start, end, "zoom to selection");
}

void
Editor::temporal_zoom_session ()
{
	ENSURE_GUI_THREAD (mem_fun (*this, &Editor::temporal_zoom_session));

	if (session) {
		temporal_zoom_by_frame (session->current_start_frame(), session->current_end_frame(), "zoom to session");
	}
}

void
Editor::temporal_zoom_by_frame (nframes_t start, nframes_t end, const string & op)
{
	if (!session) return;

	if ((start == 0 && end == 0) || end < start) {
		return;
	}

	nframes_t range = end - start;

	double new_fpu = (double)range / (double)canvas_width;
// 	double p2 = 1.0;

// 	while (p2 < new_fpu) {
// 		p2 *= 2.0;
// 	}
// 	new_fpu = p2;
	
	nframes_t new_page = (nframes_t) floor (canvas_width * new_fpu);
	nframes_t middle = (nframes_t) floor( (double)start + ((double)range / 2.0f ));
	nframes_t new_leftmost = (nframes_t) floor( (double)middle - ((double)new_page/2.0f));

	if (new_leftmost > middle) new_leftmost = 0;

//	begin_reversible_command (op);
//	session->add_undo (bind (mem_fun(*this, &Editor::reposition_and_zoom), leftmost_frame, frames_per_unit));
//	session->add_redo (bind (mem_fun(*this, &Editor::reposition_and_zoom), new_leftmost, new_fpu));
//	commit_reversible_command ();

	reposition_and_zoom (new_leftmost, new_fpu);
}

void 
Editor::temporal_zoom_to_frame (bool coarser, nframes_t frame)
{
	if (!session) return;
	
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

	if (new_fpu == frames_per_unit) return;

	nframes_t new_leftmost = frame - (nframes_t)range_before;

	if (new_leftmost > frame) new_leftmost = 0;

//	begin_reversible_command (_("zoom to frame"));
//	session->add_undo (bind (mem_fun(*this, &Editor::reposition_and_zoom), leftmost_frame, frames_per_unit));
//	session->add_redo (bind (mem_fun(*this, &Editor::reposition_and_zoom), new_leftmost, new_fpu));
//	commit_reversible_command ();

	reposition_and_zoom (new_leftmost, new_fpu);
}

void
Editor::add_location_from_selection ()
{
	string rangename;

	if (selection->time.empty()) {
		return;
	}

	if (session == 0 || clicked_trackview == 0) {
		return;
	}

	nframes_t start = selection->time[clicked_selection].start;
	nframes_t end = selection->time[clicked_selection].end;

	session->locations()->next_available_name(rangename,"selection");
	Location *location = new Location (start, end, rangename, Location::IsRangeMarker);

	session->begin_reversible_command (_("add marker"));
        XMLNode &before = session->locations()->get_state();
	session->locations()->add (location, true);
        XMLNode &after = session->locations()->get_state();
	session->add_command(new MementoCommand<Locations>(*(session->locations()), &before, &after));
	session->commit_reversible_command ();
}

void
Editor::add_location_from_playhead_cursor ()
{
	string markername;

	nframes_t where = session->audible_frame();
	
	session->locations()->next_available_name(markername,"mark");
	Location *location = new Location (where, where, markername, Location::IsMark);
	session->begin_reversible_command (_("add marker"));
        XMLNode &before = session->locations()->get_state();
	session->locations()->add (location, true);
        XMLNode &after = session->locations()->get_state();
	session->add_command(new MementoCommand<Locations>(*(session->locations()), &before, &after));
	session->commit_reversible_command ();
}

void
Editor::add_location_from_audio_region ()
{
	if (selection->regions.empty()) {
		return;
	}

	RegionView* rv = *(selection->regions.begin());
	boost::shared_ptr<Region> region = rv->region();
	
	Location *location = new Location (region->position(), region->last_frame(), region->name(), Location::IsRangeMarker);
	session->begin_reversible_command (_("add marker"));
        XMLNode &before = session->locations()->get_state();
	session->locations()->add (location, true);
        XMLNode &after = session->locations()->get_state();
	session->add_command(new MementoCommand<Locations>(*(session->locations()), &before, &after));
	session->commit_reversible_command ();
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
	if (!session) {
		return;
	}
	
	Location *location = session->locations()->first_location_after (playhead_cursor->current_frame);

	if (location) {
		session->request_locate (location->start(), session->transport_rolling());
	} else {
		session->request_locate (session->current_end_frame());
	}
}

void
Editor::jump_backward_to_mark ()
{
	if (!session) {
		return;
	}

	Location *location = session->locations()->first_location_before (playhead_cursor->current_frame);
	
	if (location) {
		session->request_locate (location->start(), session->transport_rolling());
	} else {
		session->goto_start ();
	}
}

void
Editor::set_mark ()
{
	nframes_t pos;
	float prefix;
	bool was_floating;
	string markername;

	if (get_prefix (prefix, was_floating)) {
		pos = session->audible_frame ();
	} else {
		if (was_floating) {
			pos = (nframes_t) floor (prefix * session->frame_rate ());
		} else {
			pos = (nframes_t) floor (prefix);
		}
	}

	session->locations()->next_available_name(markername,"mark");
	session->locations()->add (new Location (pos, 0, markername, Location::IsMark), true);
}

void
Editor::clear_markers ()
{
	if (session) {
		session->begin_reversible_command (_("clear markers"));
                XMLNode &before = session->locations()->get_state();
		session->locations()->clear_markers ();
                XMLNode &after = session->locations()->get_state();
		session->add_command(new MementoCommand<Locations>(*(session->locations()), &before, &after));
		session->commit_reversible_command ();
	}
}

void
Editor::clear_ranges ()
{
	if (session) {
		session->begin_reversible_command (_("clear ranges"));
                XMLNode &before = session->locations()->get_state();
		
		Location * looploc = session->locations()->auto_loop_location();
		Location * punchloc = session->locations()->auto_punch_location();
		
		session->locations()->clear_ranges ();
		// re-add these
		if (looploc) session->locations()->add (looploc);
		if (punchloc) session->locations()->add (punchloc);
		
                XMLNode &after = session->locations()->get_state();
		session->add_command(new MementoCommand<Locations>(*(session->locations()), &before, &after));
		session->commit_reversible_command ();
	}
}

void
Editor::clear_locations ()
{
	session->begin_reversible_command (_("clear locations"));
        XMLNode &before = session->locations()->get_state();
	session->locations()->clear ();
        XMLNode &after = session->locations()->get_state();
	session->add_command(new MementoCommand<Locations>(*(session->locations()), &before, &after));
	session->commit_reversible_command ();
	session->locations()->clear ();
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
Editor::insert_region_list_drag (boost::shared_ptr<AudioRegion> region, int x, int y)
{
	double wx, wy;
	double cx, cy;
	TimeAxisView *tv;
	nframes_t where;
	AudioTimeAxisView *atv = 0;
	boost::shared_ptr<Playlist> playlist;
	
	track_canvas.window_to_world (x, y, wx, wy);
	wx += horizontal_adjustment.get_value();
	wy += vertical_adjustment.get_value();

	GdkEvent event;
	event.type = GDK_BUTTON_RELEASE;
	event.button.x = wx;
	event.button.y = wy;
	
	where = event_frame (&event, &cx, &cy);

	if (where < leftmost_frame || where > leftmost_frame + current_page_frames()) {
		/* clearly outside canvas area */
		return;
	}
	
	if ((tv = trackview_by_y_position (cy)) == 0) {
		return;
	}
	
	if ((atv = dynamic_cast<AudioTimeAxisView*>(tv)) == 0) {
		return;
	}

	if ((playlist = atv->playlist()) == 0) {
		return;
	}
	
	snap_to (where);
	
	begin_reversible_command (_("insert dragged region"));
        XMLNode &before = playlist->get_state();
	playlist->add_region (RegionFactory::create (region), where, 1.0);
	session->add_command(new MementoCommand<Playlist>(*playlist, &before, &playlist->get_state()));
	commit_reversible_command ();
}

void
Editor::insert_region_list_selection (float times)
{
	RouteTimeAxisView *tv = 0;
	boost::shared_ptr<Playlist> playlist;

	if (clicked_audio_trackview != 0) {
		tv = clicked_audio_trackview;
	} else if (!selection->tracks.empty()) {
		if ((tv = dynamic_cast<RouteTimeAxisView*>(selection->tracks.front())) == 0) {
			return;
		}
	} else {
		return;
	}

	if ((playlist = tv->playlist()) == 0) {
		return;
	}
	
	Glib::RefPtr<TreeSelection> selected = region_list_display.get_selection();
	
	if (selected->count_selected_rows() != 1) {
		return;
	}
	
	TreeView::Selection::ListHandle_Path rows = selected->get_selected_rows ();

	/* only one row selected, so rows.begin() is it */

	TreeIter iter;

	if ((iter = region_list_model->get_iter (*rows.begin()))) {

		boost::shared_ptr<Region> region = (*iter)[region_list_columns.region];
		
		begin_reversible_command (_("insert region"));
		XMLNode &before = playlist->get_state();
		playlist->add_region ((RegionFactory::create (region)), edit_cursor->current_frame, times);
		session->add_command(new MementoCommand<Playlist>(*playlist, &before, &playlist->get_state()));
		commit_reversible_command ();
	} 
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
	if (!session) {
		return;
	}

	switch (Config->get_slave_source()) {
	case None:
	case JACK:
		break;
	default:
		/* transport controlled by the master */
		return;
	}

	if (session->is_auditioning()) {
		session->cancel_audition ();
		return;
	}
	
	session->request_transport_speed (fwd ? 1.0f : -1.0f);
}

void
Editor::toggle_playback (bool with_abort)
{
	if (!session) {
		return;
	}

	switch (Config->get_slave_source()) {
	case None:
	case JACK:
		break;
	default:
		/* transport controlled by the master */
		return;
	}

	if (session->is_auditioning()) {
		session->cancel_audition ();
		return;
	}
	
	if (session->transport_rolling()) {
		session->request_stop (with_abort);
		if (session->get_play_loop()) {
			session->request_play_loop (false);
		}
	} else {
		session->request_transport_speed (1.0f);
	}
}

void
Editor::play_from_start ()
{
	session->request_locate (session->current_start_frame(), true);
}

void
Editor::play_from_edit_cursor ()
{
       session->request_locate (edit_cursor->current_frame, true);
}

void
Editor::play_selection ()
{
	if (selection->time.empty()) {
		return;
	}

	session->request_play_range (true);
}

void
Editor::play_selected_region ()
{
	if (!selection->regions.empty()) {
		RegionView *rv = *(selection->regions.begin());

		session->request_bounded_roll (rv->region()->position(), rv->region()->last_frame());	
	}
}

void
Editor::loop_selected_region ()
{
	if (!selection->regions.empty()) {
		RegionView *rv = *(selection->regions.begin());
		Location* tll;

		if ((tll = transport_loop_location()) != 0)  {

			tll->set (rv->region()->position(), rv->region()->last_frame());
			
			// enable looping, reposition and start rolling

			session->request_play_loop (true);
			session->request_locate (tll->start(), false);
			session->request_transport_speed (1.0f);
		}
	}
}

void
Editor::play_location (Location& location)
{
	if (location.start() <= location.end()) {
		return;
	}

	session->request_bounded_roll (location.start(), location.end());
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
		session->request_play_loop (true);
		session->request_locate (tll->start(), true);
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

void
Editor::edit_region ()
{
	if (clicked_regionview == 0) {
		return;
	}
	
	clicked_regionview->show_region_editor ();
}

void
Editor::rename_region ()
{
	Dialog dialog;
	Entry  entry;
	Button ok_button (_("OK"));
	Button cancel_button (_("Cancel"));

	if (selection->regions.empty()) {
		return;
	}

	WindowTitle title(Glib::get_application_name());
	title += _("Rename Region");

	dialog.set_title (title.get_string());
	dialog.set_name ("RegionRenameWindow");
	dialog.set_size_request (300, -1);
	dialog.set_position (Gtk::WIN_POS_MOUSE);
	dialog.set_modal (true);

	dialog.get_vbox()->set_border_width (10);
	dialog.get_vbox()->pack_start (entry);
	dialog.get_action_area()->pack_start (ok_button);
	dialog.get_action_area()->pack_start (cancel_button);

	entry.set_name ("RegionNameDisplay");
	ok_button.set_name ("EditorGTKButton");
	cancel_button.set_name ("EditorGTKButton");

	region_renamed = false;

	entry.signal_activate().connect (bind (mem_fun(*this, &Editor::rename_region_finished), true));
	ok_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::rename_region_finished), true));
	cancel_button.signal_clicked().connect (bind (mem_fun(*this, &Editor::rename_region_finished), false));

	/* recurse */

	dialog.show_all ();
	Main::run ();

	if (region_renamed) {
		(*selection->regions.begin())->region()->set_name (entry.get_text());
		redisplay_regions ();
	}
}

void
Editor::rename_region_finished (bool status)

{
	region_renamed = status;
	Main::quit ();
}

void
Editor::audition_playlist_region_via_route (boost::shared_ptr<Region> region, Route& route)
{
	if (session->is_auditioning()) {
		session->cancel_audition ();
	} 

	// note: some potential for creativity here, because region doesn't
	// have to belong to the playlist that Route is handling

	// bool was_soloed = route.soloed();

	route.set_solo (true, this);
	
	session->request_bounded_roll (region->position(), region->position() + region->length());
	
	/* XXX how to unset the solo state ? */
}

void
Editor::audition_selected_region ()
{
	if (!selection->regions.empty()) {
		RegionView* rv = *(selection->regions.begin());
		session->audition_region (rv->region());
	}
}

void
Editor::audition_playlist_region_standalone (boost::shared_ptr<Region> region)
{
	session->audition_region (region);
}

void
Editor::build_interthread_progress_window ()
{
	interthread_progress_window = new ArdourDialog (X_("interthread progress"), true);

	interthread_progress_bar.set_orientation (Gtk::PROGRESS_LEFT_TO_RIGHT);
	
	interthread_progress_window->get_vbox()->pack_start (interthread_progress_label, false, false);
	interthread_progress_window->get_vbox()->pack_start (interthread_progress_bar,false, false);

	// GTK2FIX: this button needs a modifiable label

	Button* b = interthread_progress_window->add_button (Stock::CANCEL, RESPONSE_CANCEL);
	b->signal_clicked().connect (mem_fun(*this, &Editor::interthread_cancel_clicked));

	interthread_cancel_button.add (interthread_cancel_label);

	interthread_progress_window->set_default_size (200, 100);
}

void
Editor::interthread_cancel_clicked ()
{
	if (current_interthread_info) {
		current_interthread_info->cancel = true;
	}
}

void
Editor::region_from_selection ()
{
	if (clicked_trackview == 0) {
		return;
	}

	if (selection->time.empty()) {
		return;
	}

	nframes_t start = selection->time[clicked_selection].start;
	nframes_t end = selection->time[clicked_selection].end;

	nframes_t selection_cnt = end - start + 1;
	
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		boost::shared_ptr<AudioRegion> current;
		boost::shared_ptr<Region> current_r;
		boost::shared_ptr<Playlist> pl;

		nframes_t internal_start;
		string new_name;

		if ((pl = (*i)->playlist()) == 0) {
			continue;
		}

		if ((current_r = pl->top_region_at (start)) == 0) {
			continue;
		}

		current = boost::dynamic_pointer_cast<AudioRegion> (current_r);
		// FIXME: audio only
		if (current != 0) {
			internal_start = start - current->position();
			session->region_name (new_name, current->name(), true);
			boost::shared_ptr<Region> region (RegionFactory::create (current, internal_start, selection_cnt, new_name));
		}
	}
}	

void
Editor::create_region_from_selection (vector<boost::shared_ptr<AudioRegion> >& new_regions)
{
	if (selection->time.empty() || selection->tracks.empty()) {
		return;
	}

	nframes_t start = selection->time[clicked_selection].start;
	nframes_t end = selection->time[clicked_selection].end;
	
	sort_track_selection ();

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		boost::shared_ptr<AudioRegion> current;
		boost::shared_ptr<Region> current_r;
		boost::shared_ptr<Playlist> playlist;
		nframes_t internal_start;
		string new_name;

		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}

		if ((current_r = playlist->top_region_at(start)) == 0) {
			continue;
		}

		if ((current = boost::dynamic_pointer_cast<AudioRegion>(current_r)) == 0) {
			continue;
		}
	
		internal_start = start - current->position();
		session->region_name (new_name, current->name(), true);
		
		new_regions.push_back (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (current, internal_start, end - start + 1, new_name)));
	}
}

void
Editor::split_multichannel_region ()
{
	if (selection->regions.empty()) {
		return;
	}

	vector<boost::shared_ptr<AudioRegion> > v;

	for (list<RegionView*>::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {

		AudioRegionView* arv = dynamic_cast<AudioRegionView*>(*x);
		
		if (!arv || arv->audio_region()->n_channels() < 2) {
			continue;
		}

		(arv)->audio_region()->separate_by_channel (*session, v);
	}
}

void
Editor::new_region_from_selection ()
{
	region_from_selection ();
	cancel_selection ();
}

void
Editor::separate_region_from_selection ()
{
	bool doing_undo = false;

	if (selection->time.empty()) {
		return;
	}

	boost::shared_ptr<Playlist> playlist;
		
	sort_track_selection ();

	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*> ((*i))) != 0) {

			if (atv->is_audio_track()) {

				/* no edits to destructive tracks */

				if (atv->audio_track()->audio_diskstream()->destructive()) {
					continue;
				}
					
				if ((playlist = atv->playlist()) != 0) {
					if (!doing_undo) {
						begin_reversible_command (_("separate"));
						doing_undo = true;
					}
                                        XMLNode *before;
					if (doing_undo) 
                                            before = &(playlist->get_state());
			
					/* XXX need to consider musical time selections here at some point */

					double speed = atv->get_diskstream()->speed();

					for (list<AudioRange>::iterator t = selection->time.begin(); t != selection->time.end(); ++t) {
						playlist->partition ((nframes_t)((*t).start * speed), (nframes_t)((*t).end * speed), true);
					}

					if (doing_undo) 
                                            session->add_command(new MementoCommand<Playlist>(*playlist, before, &playlist->get_state()));
				}
			}
		}
	}

	if (doing_undo)	commit_reversible_command ();
}

void
Editor::separate_regions_using_location (Location& loc)
{
	bool doing_undo = false;

	if (loc.is_mark()) {
		return;
	}

	boost::shared_ptr<Playlist> playlist;

	/* XXX i'm unsure as to whether this should operate on selected tracks only 
	   or the entire enchillada. uncomment the below line to correct the behaviour 
	   (currently set for all tracks)
	*/

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {	
	//for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*> ((*i))) != 0) {

			if (atv->is_audio_track()) {
					
				/* no edits to destructive tracks */

				if (atv->audio_track()->audio_diskstream()->destructive()) {
					continue;
				}

				if ((playlist = atv->playlist()) != 0) {
                                        XMLNode *before;
					if (!doing_undo) {
						begin_reversible_command (_("separate"));
						doing_undo = true;
					}
					if (doing_undo) 
                                            before = &(playlist->get_state());
                                            
			
					/* XXX need to consider musical time selections here at some point */

					double speed = atv->get_diskstream()->speed();


					playlist->partition ((nframes_t)(loc.start() * speed), (nframes_t)(loc.end() * speed), true);
					if (doing_undo) 
                                            session->add_command(new MementoCommand<Playlist>(*playlist, before, &playlist->get_state()));
				}
			}
		}
	}

	if (doing_undo)	commit_reversible_command ();
}

void
Editor::crop_region_to_selection ()
{
	if (selection->time.empty() || selection->tracks.empty()) {
		return;
	}

	vector<boost::shared_ptr<Playlist> > playlists;
	boost::shared_ptr<Playlist> playlist;

	sort_track_selection ();
	
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		
		AudioTimeAxisView* atv;
		
		if ((atv = dynamic_cast<AudioTimeAxisView*> ((*i))) != 0) {
			
			if (atv->is_audio_track()) {
				
				/* no edits to destructive tracks */

				if (atv->audio_track()->audio_diskstream()->destructive()) {
					continue;
				}

				if ((playlist = atv->playlist()) != 0) {
					playlists.push_back (playlist);
				}
			}
		}
	}

	if (playlists.empty()) {
		return;
	}
		
	nframes_t start;
	nframes_t end;
	nframes_t cnt;
	
	begin_reversible_command (_("trim to selection"));
	
	for (vector<boost::shared_ptr<Playlist> >::iterator i = playlists.begin(); i != playlists.end(); ++i) {
		
		boost::shared_ptr<Region> region;
		
		start = selection->time.start();
		
		if ((region = (*i)->top_region_at(start)) == 0) {
			continue;
		}
		
		/* now adjust lengths to that we do the right thing
		   if the selection extends beyond the region
		*/
		
		start = max (start, region->position());
		if (max_frames - start < region->length()) {
			end = start + region->length() - 1;
		} else {
			end = max_frames;
		}
		end = min (selection->time.end_frame(), end);
		cnt = end - start + 1;
		
		XMLNode &before = (*i)->get_state();
		region->trim_to (start, cnt, this);
		XMLNode &after = (*i)->get_state();
		session->add_command (new MementoCommand<Playlist>(*(*i), &before, &after));
	}
	
	commit_reversible_command ();
}		

void
Editor::region_fill_track ()
{
	nframes_t end;

	if (!session || selection->regions.empty()) {
		return;
	}

	end = session->current_end_frame ();

	begin_reversible_command (_("region fill"));

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

		boost::shared_ptr<Region> region ((*i)->region());
		
		// FIXME
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>(region);
		if (!ar)
			continue;

		boost::shared_ptr<Playlist> pl = region->playlist();

		if (end <= region->last_frame()) {
			return;
		}

		double times = (double) (end - region->last_frame()) / (double) region->length();

		if (times == 0) {
			return;
		}

                XMLNode &before = pl->get_state();
		pl->add_region (RegionFactory::create (ar), ar->last_frame(), times);
		session->add_command (new MementoCommand<Playlist>(*pl, &before, &pl->get_state()));
	}

	commit_reversible_command ();
}

void
Editor::region_fill_selection ()
{
       	if (clicked_audio_trackview == 0 || !clicked_audio_trackview->is_audio_track()) {
		return;
	}

	if (selection->time.empty()) {
		return;
	}


	Glib::RefPtr<TreeSelection> selected = region_list_display.get_selection();

	if (selected->count_selected_rows() != 1) {
		return;
	}

	TreeModel::iterator i = region_list_display.get_selection()->get_selected();
	boost::shared_ptr<Region> region = (*i)[region_list_columns.region];

	nframes_t start = selection->time[clicked_selection].start;
	nframes_t end = selection->time[clicked_selection].end;

	boost::shared_ptr<Playlist> playlist; 

	if (selection->tracks.empty()) {
		return;
	}

	nframes_t selection_length = end - start;
	float times = (float)selection_length / region->length();
	
	begin_reversible_command (_("fill selection"));
	
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}		
		
                XMLNode &before = playlist->get_state();
		playlist->add_region (RegionFactory::create (region), start, times);
		session->add_command (new MementoCommand<Playlist>(*playlist, &before, &playlist->get_state()));
	}
	
	commit_reversible_command ();			
}

void
Editor::set_a_regions_sync_position (boost::shared_ptr<Region> region, nframes_t position)
{

	if (!region->covers (position)) {
	  error << _("Programming error. that region doesn't cover that position") << __FILE__ << " +" << __LINE__ << endmsg;
		return;
	}
	begin_reversible_command (_("set region sync position"));
        XMLNode &before = region->playlist()->get_state();
	region->set_sync_position (position);
        XMLNode &after = region->playlist()->get_state();
	session->add_command(new MementoCommand<Playlist>(*(region->playlist()), &before, &after));
	commit_reversible_command ();
}

void
Editor::set_region_sync_from_edit_cursor ()
{
	if (clicked_regionview == 0) {
		return;
	}

	if (!clicked_regionview->region()->covers (edit_cursor->current_frame)) {
		error << _("Place the edit cursor at the desired sync point") << endmsg;
		return;
	}

	boost::shared_ptr<Region> region (clicked_regionview->region());
	begin_reversible_command (_("set sync from edit cursor"));
        XMLNode &before = region->playlist()->get_state();
	region->set_sync_position (edit_cursor->current_frame);
        XMLNode &after = region->playlist()->get_state();
	session->add_command(new MementoCommand<Playlist>(*(region->playlist()), &before, &after));
	commit_reversible_command ();
}

void
Editor::remove_region_sync ()
{
	if (clicked_regionview) {
		boost::shared_ptr<Region> region (clicked_regionview->region());
		begin_reversible_command (_("remove sync"));
                XMLNode &before = region->playlist()->get_state();
		region->clear_sync_position ();
                XMLNode &after = region->playlist()->get_state();
		session->add_command(new MementoCommand<Playlist>(*(region->playlist()), &before, &after));
		commit_reversible_command ();
	}
}

void
Editor::naturalize ()
{
	if (selection->regions.empty()) {
		return;
	}
	begin_reversible_command (_("naturalize"));
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
                XMLNode &before = (*i)->region()->get_state();
		(*i)->region()->move_to_natural_position (this);
                XMLNode &after = (*i)->region()->get_state();
		session->add_command (new MementoCommand<Region>(*((*i)->region().get()), &before, &after));
	}
	commit_reversible_command ();
}

void
Editor::align (RegionPoint what)
{
	align_selection (what, edit_cursor->current_frame);
}

void
Editor::align_relative (RegionPoint what)
{
	align_selection_relative (what, edit_cursor->current_frame);
}

struct RegionSortByTime {
    bool operator() (const AudioRegionView* a, const AudioRegionView* b) {
	    return a->region()->position() < b->region()->position();
    }
};

void
Editor::align_selection_relative (RegionPoint point, nframes_t position)
{
	if (selection->regions.empty()) {
		return;
	}

	nframes_t distance;
	nframes_t pos = 0;
	int dir;

	list<RegionView*> sorted;
	selection->regions.by_position (sorted);
	boost::shared_ptr<Region> r ((*sorted.begin())->region());

	switch (point) {
	case Start:
		pos = r->first_frame ();
		break;

	case End:
		pos = r->last_frame();
		break;

	case SyncPoint:
		pos = r->adjust_to_sync (r->first_frame());
		break;	
	}

	if (pos > position) {
		distance = pos - position;
		dir = -1;
	} else {
		distance = position - pos;
		dir = 1;
	}

	begin_reversible_command (_("align selection (relative)"));

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

		boost::shared_ptr<Region> region ((*i)->region());

                XMLNode &before = region->playlist()->get_state();
		
		if (dir > 0) {
			region->set_position (region->position() + distance, this);
		} else {
			region->set_position (region->position() - distance, this);
		}

                XMLNode &after = region->playlist()->get_state();
		session->add_command(new MementoCommand<Playlist>(*(region->playlist()), &before, &after));

	}

	commit_reversible_command ();
}

void
Editor::align_selection (RegionPoint point, nframes_t position)
{
	if (selection->regions.empty()) {
		return;
	}

	begin_reversible_command (_("align selection"));

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		align_region_internal ((*i)->region(), point, position);
	}

	commit_reversible_command ();
}

void
Editor::align_region (boost::shared_ptr<Region> region, RegionPoint point, nframes_t position)
{
	begin_reversible_command (_("align region"));
	align_region_internal (region, point, position);
	commit_reversible_command ();
}

void
Editor::align_region_internal (boost::shared_ptr<Region> region, RegionPoint point, nframes_t position)
{
	XMLNode &before = region->playlist()->get_state();

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

	XMLNode &after = region->playlist()->get_state();
	session->add_command(new MementoCommand<Playlist>(*(region->playlist()), &before, &after));
}	

void
Editor::trim_region_to_edit_cursor ()
{
	if (clicked_regionview == 0) {
		return;
	}

	boost::shared_ptr<Region> region (clicked_regionview->region());

	float speed = 1.0f;
	AudioTimeAxisView *atav;

	if ( clicked_trackview != 0 && (atav = dynamic_cast<AudioTimeAxisView*>(clicked_trackview)) != 0 ) {
		if (atav->get_diskstream() != 0) {
			speed = atav->get_diskstream()->speed();
		}
	}

	begin_reversible_command (_("trim to edit"));
        XMLNode &before = region->playlist()->get_state();
	region->trim_end( session_frame_to_track_frame(edit_cursor->current_frame, speed), this);
        XMLNode &after = region->playlist()->get_state();
	session->add_command(new MementoCommand<Playlist>(*(region->playlist()), &before, &after));
	commit_reversible_command ();
}

void
Editor::trim_region_from_edit_cursor ()
{
	if (clicked_regionview == 0) {
		return;
	}

	boost::shared_ptr<Region> region (clicked_regionview->region());

	float speed = 1.0f;
	AudioTimeAxisView *atav;

	if ( clicked_trackview != 0 && (atav = dynamic_cast<AudioTimeAxisView*>(clicked_trackview)) != 0 ) {
		if (atav->get_diskstream() != 0) {
			speed = atav->get_diskstream()->speed();
		}
	}

	begin_reversible_command (_("trim to edit"));
        XMLNode &before = region->playlist()->get_state();
	region->trim_front ( session_frame_to_track_frame(edit_cursor->current_frame, speed), this);
        XMLNode &after = region->playlist()->get_state();
	session->add_command(new MementoCommand<Playlist>(*(region->playlist()), &before, &after));
	commit_reversible_command ();
}

void
Editor::unfreeze_route ()
{
	if (clicked_audio_trackview == 0 || !clicked_audio_trackview->is_audio_track()) {
		return;
	}
	
	clicked_audio_trackview->audio_track()->unfreeze ();
}

void*
Editor::_freeze_thread (void* arg)
{
	PBD::ThreadCreated (pthread_self(), X_("Freeze"));
	return static_cast<Editor*>(arg)->freeze_thread ();
}

void*
Editor::freeze_thread ()
{
	clicked_audio_trackview->audio_track()->freeze (*current_interthread_info);
	return 0;
}

gint
Editor::freeze_progress_timeout (void *arg)
{
	interthread_progress_bar.set_fraction (current_interthread_info->progress/100);
	return !(current_interthread_info->done || current_interthread_info->cancel);
}

void
Editor::freeze_route ()
{
	if (clicked_audio_trackview == 0 || !clicked_audio_trackview->is_audio_track()) {
		return;
	}
	
	InterThreadInfo itt;

	if (interthread_progress_window == 0) {
		build_interthread_progress_window ();
	}

	WindowTitle title(Glib::get_application_name());
	title += _("Freeze");
	interthread_progress_window->set_title (title.get_string());
	interthread_progress_window->set_position (Gtk::WIN_POS_MOUSE);
	interthread_progress_window->show_all ();
	interthread_progress_bar.set_fraction (0.0f);
	interthread_progress_label.set_text ("");
	interthread_cancel_label.set_text (_("Cancel Freeze"));
	current_interthread_info = &itt;

	interthread_progress_connection = 
	  Glib::signal_timeout().connect (bind (mem_fun(*this, &Editor::freeze_progress_timeout), (gpointer) 0), 100);

	itt.done = false;
	itt.cancel = false;
	itt.progress = 0.0f;
	
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setstacksize(&attr, 500000);

	pthread_create (&itt.thread, &attr, _freeze_thread, this);

	pthread_attr_destroy(&attr);

	track_canvas.get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));

	while (!itt.done && !itt.cancel) {
		gtk_main_iteration ();
	}

	interthread_progress_connection.disconnect ();
	interthread_progress_window->hide_all ();
	current_interthread_info = 0;
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
}

void
Editor::bounce_range_selection ()
{
	if (selection->time.empty()) {
		return;
	}

	TrackSelection views = selection->tracks;

	nframes_t start = selection->time[clicked_selection].start;
	nframes_t end = selection->time[clicked_selection].end;
	nframes_t cnt = end - start + 1;

	begin_reversible_command (_("bounce range"));

	for (TrackViewList::iterator i = views.begin(); i != views.end(); ++i) {

		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*> (*i)) == 0) {
			continue;
		}
		
		boost::shared_ptr<Playlist> playlist;
		
		if ((playlist = atv->playlist()) == 0) {
			return;
		}

		InterThreadInfo itt;
		
		itt.done = false;
		itt.cancel = false;
		itt.progress = false;

                XMLNode &before = playlist->get_state();
		atv->audio_track()->bounce_range (start, cnt, itt);
                XMLNode &after = playlist->get_state();
		session->add_command (new MementoCommand<Playlist> (*playlist, &before, &after));
	}
	
	commit_reversible_command ();
}

void
Editor::cut ()
{
	cut_copy (Cut);
}

void
Editor::copy ()
{
	cut_copy (Copy);
}

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
	
	cut_buffer->clear ();

	switch (current_mouse_mode()) {
	case MouseObject: 
		if (!selection->regions.empty() || !selection->points.empty()) {

			begin_reversible_command (opname + _(" objects"));

			if (!selection->regions.empty()) {
				
				cut_copy_regions (op);
				
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
		}
		break;
		
	case MouseRange:
		if (!selection->time.empty()) {

			begin_reversible_command (opname + _(" range"));
			cut_copy_ranges (op);
			commit_reversible_command ();

			if (op == Cut) {
				selection->clear_time ();
			}
			
		}
		break;
		
	default:
		break;
	}
}

void
Editor::cut_copy_points (CutCopyOp op)
{
	for (PointSelection::iterator i = selection->points.begin(); i != selection->points.end(); ++i) {

		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*>(&(*i).track);

		if (atv) {
			atv->cut_copy_clear_objects (selection->points, op);
		} 
	}
}

struct PlaylistState {
    boost::shared_ptr<Playlist> playlist;
    XMLNode*  before;
};

struct lt_playlist {
    bool operator () (const PlaylistState& a, const PlaylistState& b) {
	    return a.playlist < b.playlist;
    }
};
	
struct PlaylistMapping { 
    TimeAxisView* tv;
    boost::shared_ptr<AudioPlaylist> pl;

    PlaylistMapping (TimeAxisView* tvp) : tv (tvp) {}
};

void
Editor::cut_copy_regions (CutCopyOp op)
{
	/* we can't use a std::map here because the ordering is important, and we can't trivially sort
	   a map when we want ordered access to both elements. i think.
	*/

	vector<PlaylistMapping> pmap;

	nframes_t first_position = max_frames;
	
	set<PlaylistState, lt_playlist> freezelist;
	pair<set<PlaylistState, lt_playlist>::iterator,bool> insert_result;
	
	/* get ordering correct before we cut/copy */
	
	selection->regions.sort_by_position_and_track ();

	for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {

		first_position = min ((*x)->region()->position(), first_position);

		if (op == Cut || op == Clear) {
			boost::shared_ptr<AudioPlaylist> pl = boost::dynamic_pointer_cast<AudioPlaylist>((*x)->region()->playlist());

			if (pl) {

				PlaylistState before;
				before.playlist = pl;
				before.before = &pl->get_state();
				
				insert_result = freezelist.insert (before);
				
				if (insert_result.second) {
					pl->freeze ();
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

	for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ) {

		boost::shared_ptr<AudioPlaylist> pl = boost::dynamic_pointer_cast<AudioPlaylist>((*x)->region()->playlist());
		
		if (!pl) {
			/* impossible, but this handles it for the future */
			continue;
		}

		TimeAxisView& tv = (*x)->get_trackview();
		boost::shared_ptr<AudioPlaylist> npl;
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
			npl = boost::dynamic_pointer_cast<AudioPlaylist> (PlaylistFactory::create (*session, "cutlist", true));
			npl->freeze();
			(*z).pl = npl;
		} else {
			npl = (*z).pl;
		}
		
		boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion>((*x)->region());
		boost::shared_ptr<Region> _xx;
		
		switch (op) {
		case Cut:
			if (!ar) break;
			
			_xx = RegionFactory::create ((*x)->region());
			npl->add_region (_xx, (*x)->region()->position() - first_position);
			pl->remove_region (((*x)->region()));
			break;
			
		case Copy:
			if (!ar) break;

			/* copy region before adding, so we're not putting same object into two different playlists */
			npl->add_region (RegionFactory::create ((*x)->region()), (*x)->region()->position() - first_position);
			break;
			
		case Clear:
			pl->remove_region (((*x)->region()));
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

	for (set<PlaylistState, lt_playlist>::iterator pl = freezelist.begin(); pl != freezelist.end(); ++pl) {
		(*pl).playlist->thaw ();
		session->add_command (new MementoCommand<Playlist>(*(*pl).playlist, (*pl).before, &(*pl).playlist->get_state()));
	}
}

void
Editor::cut_copy_ranges (CutCopyOp op)
{
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
		(*i)->cut_copy_clear (*selection, op);
	}
}

void
Editor::paste (float times)
{
	paste_internal (edit_cursor->current_frame, times);
}

void
Editor::mouse_paste ()
{
	int x, y;
	double wx, wy;

	track_canvas.get_pointer (x, y);
	track_canvas.window_to_world (x, y, wx, wy);
	wx += horizontal_adjustment.get_value();
	wy += vertical_adjustment.get_value();

	GdkEvent event;
	event.type = GDK_BUTTON_RELEASE;
	event.button.x = wx;
	event.button.y = wy;
	
	nframes_t where = event_frame (&event, 0, 0);
	snap_to (where);
	paste_internal (where, 1);
}

void
Editor::paste_internal (nframes_t position, float times)
{
	bool commit = false;

	if (cut_buffer->empty() || selection->tracks.empty()) {
		return;
	}

	if (position == max_frames) {
		position = edit_cursor->current_frame;
	}

	begin_reversible_command (_("paste"));

	TrackSelection::iterator i;
	size_t nth;

	/* get everything in the correct order */

	sort_track_selection ();

	for (nth = 0, i = selection->tracks.begin(); i != selection->tracks.end(); ++i, ++nth) {

		/* undo/redo is handled by individual tracks */

		if ((*i)->paste (position, times, *cut_buffer, nth)) {
			commit = true;
		}
	}
	
	if (commit) {
		commit_reversible_command ();
	}
}

void
Editor::paste_named_selection (float times)
{
	TrackSelection::iterator t;

	Glib::RefPtr<TreeSelection> selected = named_selection_display.get_selection();

	if (selected->count_selected_rows() != 1 || selection->tracks.empty()) {
		return;
	}

	TreeModel::iterator i = selected->get_selected();
	NamedSelection* ns = (*i)[named_selection_columns.selection];

	list<boost::shared_ptr<Playlist> >::iterator chunk;
	list<boost::shared_ptr<Playlist> >::iterator tmp;

	chunk = ns->playlists.begin();
		
	begin_reversible_command (_("paste chunk"));
	
	sort_track_selection ();

	for (t = selection->tracks.begin(); t != selection->tracks.end(); ++t) {
		
		AudioTimeAxisView* atv;
		boost::shared_ptr<Playlist> pl;
		boost::shared_ptr<AudioPlaylist> apl;

		if ((atv = dynamic_cast<AudioTimeAxisView*> (*t)) == 0) {
			continue;
		}

		if ((pl = atv->playlist()) == 0) {
			continue;
		}
		
		if ((apl = boost::dynamic_pointer_cast<AudioPlaylist> (pl)) == 0) {
			continue;
		}

		tmp = chunk;
		++tmp;

                XMLNode &before = apl->get_state();
		apl->paste (*chunk, edit_cursor->current_frame, times);
		session->add_command(new MementoCommand<AudioPlaylist>(*apl, &before, &apl->get_state()));

		if (tmp != ns->playlists.end()) {
			chunk = tmp;
		}
	}

	commit_reversible_command();
}

void
Editor::duplicate_some_regions (RegionSelection& regions, float times)
{
	boost::shared_ptr<Playlist> playlist; 
	RegionSelection sel = regions; // clear (below) will clear the argument list
		
	begin_reversible_command (_("duplicate region"));

	selection->clear_regions ();

	for (RegionSelection::iterator i = sel.begin(); i != sel.end(); ++i) {

		boost::shared_ptr<Region> r ((*i)->region());

		TimeAxisView& tv = (*i)->get_time_axis_view();
		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (&tv);
		sigc::connection c = atv->view()->RegionViewAdded.connect (mem_fun(*this, &Editor::collect_new_region_view));
		
 		playlist = (*i)->region()->playlist();
                XMLNode &before = playlist->get_state();
		playlist->duplicate (r, r->last_frame(), times);
		session->add_command(new MementoCommand<Playlist>(*playlist, &before, &playlist->get_state()));

		c.disconnect ();

		if (latest_regionview) {
			selection->add (latest_regionview);
		}
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
	vector<boost::shared_ptr<AudioRegion> > new_regions;
	vector<boost::shared_ptr<AudioRegion> >::iterator ri;
		
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
                XMLNode &before = playlist->get_state();
		playlist->duplicate (*ri, selection->time[clicked_selection].end, times);
                XMLNode &after = playlist->get_state();
		session->add_command (new MementoCommand<Playlist>(*playlist, &before, &after));

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
		
		AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*>(&(*i).track);
		
		if (atv) {
			atv->reset_objects (selection->points);
		} 
	}
}

void
Editor::center_playhead ()
{
	float page = canvas_width * frames_per_unit;

	center_screen_internal (playhead_cursor->current_frame, page);
}

void
Editor::center_edit_cursor ()
{
	float page = canvas_width * frames_per_unit;

	center_screen_internal (edit_cursor->current_frame, page);
}

void
Editor::clear_playlist (boost::shared_ptr<Playlist> playlist)
{
	begin_reversible_command (_("clear playlist"));
        XMLNode &before = playlist->get_state();
	playlist->clear ();
        XMLNode &after = playlist->get_state();
	session->add_command (new MementoCommand<Playlist>(*playlist.get(), &before, &after));
	commit_reversible_command ();
}

void
Editor::nudge_track (bool use_edit_cursor, bool forwards)
{
	boost::shared_ptr<Playlist> playlist; 
	nframes_t distance;
	nframes_t next_distance;
	nframes_t start;

	if (use_edit_cursor) {
		start = edit_cursor->current_frame;
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
		
                XMLNode &before = playlist->get_state();
		playlist->nudge_after (start, distance, forwards);
                XMLNode &after = playlist->get_state();
		session->add_command (new MementoCommand<Playlist>(*playlist, &before, &after));
	}
	
	commit_reversible_command ();			
}

void
Editor::remove_last_capture ()
{
	vector<string> choices;
	string prompt;
	
	if (!session) {
		return;
	}

	if (Config->get_verify_remove_last_capture()) {
		prompt  = _("Do you really want to destroy the last capture?"
			    "\n(This is destructive and cannot be undone)");

		choices.push_back (_("No, do nothing."));
		choices.push_back (_("Yes, destroy it."));
		
		Gtkmm2ext::Choice prompter (prompt, choices);
		
		if (prompter.run () == 1) {
			session->remove_last_capture ();
		}

	} else {
		session->remove_last_capture();
	}
}

void
Editor::normalize_region ()
{
	if (!session) {
		return;
	}

	if (selection->regions.empty()) {
		return;
	}

	begin_reversible_command (_("normalize"));

	track_canvas.get_window()->set_cursor (*wait_cursor);
	gdk_flush ();

	for (RegionSelection::iterator r = selection->regions.begin(); r != selection->regions.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (!arv)
			continue;
 		XMLNode &before = arv->region()->get_state();
		arv->audio_region()->normalize_to (0.0f);
		session->add_command (new MementoCommand<Region>(*(arv->region().get()), &before, &arv->region()->get_state()));
	}

	commit_reversible_command ();
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
}


void
Editor::denormalize_region ()
{
	if (!session) {
		return;
	}

	if (selection->regions.empty()) {
		return;
	}

	begin_reversible_command ("denormalize");

	for (RegionSelection::iterator r = selection->regions.begin(); r != selection->regions.end(); ++r) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (!arv)
			continue;
		XMLNode &before = arv->region()->get_state();
		arv->audio_region()->set_scale_amplitude (1.0f);
		session->add_command (new MementoCommand<Region>(*(arv->region().get()), &before, &arv->region()->get_state()));
	}

	commit_reversible_command ();
}


void
Editor::reverse_region ()
{
	if (!session) {
		return;
	}

	Reverse rev (*session);
	apply_filter (rev, _("reverse regions"));
}

void
Editor::apply_filter (AudioFilter& filter, string command)
{
	if (selection->regions.empty()) {
		return;
	}

	begin_reversible_command (command);

	track_canvas.get_window()->set_cursor (*wait_cursor);
	gdk_flush ();

	for (RegionSelection::iterator r = selection->regions.begin(); r != selection->regions.end(); ) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*r);
		if (!arv)
			continue;

		boost::shared_ptr<Playlist> playlist = arv->region()->playlist();

		RegionSelection::iterator tmp;
		
		tmp = r;
		++tmp;

		if (arv->audio_region()->apply (filter) == 0) {

                        XMLNode &before = playlist->get_state();
			playlist->replace_region (arv->region(), filter.results.front(), arv->region()->position());
                        XMLNode &after = playlist->get_state();
			session->add_command(new MementoCommand<Playlist>(*playlist, &before, &after));
		} else {
			goto out;
		}

		r = tmp;
	}

	commit_reversible_command ();
	selection->regions.clear ();

  out:
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
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
	if (!clicked_regionview) {
		return;
	}

	/* more to come */
}

void
Editor::brush (nframes_t pos)
{
	RegionSelection sel;
	snap_to (pos);

	if (selection->regions.empty()) {
		/* XXX get selection from region list */
	} else { 
		sel = selection->regions;
	}

	if (sel.empty()) {
		return;
	}

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		mouse_brush_insert_region ((*i), pos);
	}
}

void
Editor::reset_region_gain_envelopes ()
{
	if (!session || selection->regions.empty()) {
		return;
	}

	session->begin_reversible_command (_("reset region gain"));

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			AutomationList& alist (arv->audio_region()->envelope());
			XMLNode& before (alist.get_state());

			arv->audio_region()->set_default_envelope ();
			session->add_command (new MementoCommand<AutomationList>(arv->audio_region()->envelope(), &before, &alist.get_state()));
		}
	}

	session->commit_reversible_command ();
}

void
Editor::toggle_gain_envelope_visibility ()
{
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			bool x = region_envelope_visible_item->get_active();
			if (x != arv->envelope_visible()) {
				arv->set_envelope_visible (x);
			}
		}
	}
}

void
Editor::toggle_gain_envelope_active ()
{
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			bool x = region_envelope_active_item->get_active();
			if (x != arv->audio_region()->envelope_active()) {
				arv->audio_region()->set_envelope_active (x);
			}
		}
	}
}

void
Editor::toggle_region_lock ()
{
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			bool x = region_lock_item->get_active();
			if (x != arv->audio_region()->locked()) {
				arv->audio_region()->set_locked (x);
			}
		}
	}
}

void
Editor::toggle_region_mute ()
{
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			bool x = region_mute_item->get_active();
			if (x != arv->audio_region()->muted()) {
				arv->audio_region()->set_muted (x);
			}
		}
	}
}

void
Editor::toggle_region_opaque ()
{
	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
		AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
		if (arv) {
			bool x = region_opaque_item->get_active();
			if (x != arv->audio_region()->opaque()) {
				arv->audio_region()->set_opaque (x);
			}
		}
	}
}

void
Editor::set_fade_in_shape (AudioRegion::FadeShape shape)
{
	begin_reversible_command (_("set fade in shape"));

	for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}

		AutomationList& alist = tmp->audio_region()->fade_in();
		XMLNode &before = alist.get_state();

		tmp->audio_region()->set_fade_in_shape (shape);
		
		XMLNode &after = alist.get_state();
		session->add_command(new MementoCommand<AutomationList>(alist, &before, &after));
	}

	commit_reversible_command ();
}

void
Editor::set_fade_out_shape (AudioRegion::FadeShape shape)
{
	begin_reversible_command (_("set fade out shape"));

	for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}

		AutomationList& alist = tmp->audio_region()->fade_out();
		XMLNode &before = alist.get_state();

		tmp->audio_region()->set_fade_out_shape (shape);
		
		XMLNode &after = alist.get_state();
		session->add_command(new MementoCommand<AutomationList>(alist, &before, &after));
	}

	commit_reversible_command ();
}

void
Editor::set_fade_in_active (bool yn)
{
	begin_reversible_command (_("set fade in active"));

	for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}


		boost::shared_ptr<AudioRegion> ar (tmp->audio_region());

		XMLNode &before = ar->get_state();

		ar->set_fade_in_active (yn);
		
		XMLNode &after = ar->get_state();
		session->add_command(new MementoCommand<AudioRegion>(*ar, &before, &after));
	}
}

void
Editor::set_fade_out_active (bool yn)
{
	begin_reversible_command (_("set fade out active"));

	for (RegionSelection::iterator x = selection->regions.begin(); x != selection->regions.end(); ++x) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*x);

		if (!tmp) {
			return;
		}

		boost::shared_ptr<AudioRegion> ar (tmp->audio_region());

		XMLNode &before = ar->get_state();

		ar->set_fade_out_active (yn);
		
		XMLNode &after = ar->get_state();
		session->add_command(new MementoCommand<AudioRegion>(*ar, &before, &after));
	}
}


/** Update crossfade visibility after its configuration has been changed */
void
Editor::update_xfade_visibility ()
{
	_xfade_visibility = Config->get_xfades_visible ();
	
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
