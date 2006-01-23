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

    $Id$
*/

#include <unistd.h>

#include <cstdlib>
#include <cmath>
#include <string>
#include <map>

#include <sndfile.h>

#include <pbd/error.h>
#include <pbd/basename.h>
#include <pbd/pthread_utils.h>

#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/choice.h>

#include <ardour/audioengine.h>
#include <ardour/session.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/diskstream.h>
#include <ardour/filesource.h>
#include <ardour/sndfilesource.h>
#include <ardour/utils.h>
#include <ardour/location.h>
#include <ardour/named_selection.h>
#include <ardour/audio_track.h>
#include <ardour/audioplaylist.h>
#include <ardour/region_factory.h>
#include <ardour/reverse.h>

#include "ardour_ui.h"
#include "editor.h"
#include "time_axis_view.h"
#include "audio_time_axis.h"
#include "automation_time_axis.h"
#include "streamview.h"
#include "regionview.h"
#include "rgb_macros.h"
#include "selection_templates.h"
#include "selection.h"
#include "sfdb_ui.h"
#include "editing.h"
#include "gtk-custom-hruler.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace sigc;
using namespace Gtk;
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

void
Editor::set_meter_hold (int32_t cnt)
{
	if (session) {
		session->set_meter_hold (cnt);
	}
}

void
Editor::set_meter_falloff (float val)
{
	if (session) {
		session->set_meter_falloff (val);
	}
}


int
Editor::ensure_cursor (jack_nframes_t *pos)
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
Editor::split_region_at (jack_nframes_t where)
{
	split_regions_at (where, selection->audio_regions);
}

void
Editor::split_regions_at (jack_nframes_t where, AudioRegionSelection& regions)
{
	begin_reversible_command (_("split"));

	snap_to (where);
	for (AudioRegionSelection::iterator a = regions.begin(); a != regions.end(); ) {

		AudioRegionSelection::iterator tmp;
		
		tmp = a;
		++tmp;

		Playlist* pl = (*a)->region.playlist();

		_new_regionviews_show_envelope = (*a)->envelope_visible();
		
		if (pl) {
			session->add_undo (pl->get_memento());
			pl->split_region ((*a)->region, where);
			session->add_redo_no_execute (pl->get_memento());
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

	Playlist* playlist = clicked_audio_trackview->playlist();
	
	begin_reversible_command (_("remove region"));
	session->add_undo (playlist->get_memento());
	playlist->remove_region (&clicked_regionview->region);
	session->add_redo_no_execute (playlist->get_memento());
	commit_reversible_command ();
}

void
Editor::destroy_clicked_region ()
{
	int32_t selected = selection->audio_regions.size();

	if (!session || clicked_regionview == 0 && selected == 0) {
		return;
	}

	vector<string> choices;
	string prompt;
	
	prompt  = string_compose (_(" This is destructive, will possibly delete audio files\n\
It cannot be undone\n\
Do you really want to destroy %1 ?"),
			   (selected > 1 ? 
			    _("these regions") : _("this region")));

	if (selected > 1) {
		choices.push_back (_("Yes, destroy them."));
	} else {
		choices.push_back (_("Yes, destroy it."));
	}

	choices.push_back (_("No, do nothing."));

	Gtkmm2ext::Choice prompter (prompt, choices);

	prompter.chosen.connect (ptr_fun (Main::quit));
	prompter.show_all ();

	Main::run ();
		
	if (prompter.get_choice() != 0) {
		return;
	}

	if (selected > 0) {
		list<Region*> r;

		for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
			r.push_back (&(*i)->region);
		}

		session->destroy_regions (r);

	} else if (clicked_regionview) {
		session->destroy_region (&clicked_regionview->region);
	} 
}

AudioRegion *
Editor::select_region_for_operation (int dir, TimeAxisView **tv)
{
	AudioRegionView* rv;
	AudioRegion *region;
	jack_nframes_t start = 0;

	if (selection->time.start () == selection->time.end_frame ()) {
		
		/* no current selection-> is there a selected regionview? */

		if (selection->audio_regions.empty()) {
			return 0;
		}

	} 

	region = 0;

	if (!selection->audio_regions.empty()) {

		rv = *(selection->audio_regions.begin());
		(*tv) = &rv->get_time_axis_view();
		region = &rv->region;

	} else if (!selection->tracks.empty()) {

		(*tv) = selection->tracks.front();

		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*> (*tv)) != 0) {
			Playlist *pl;
			
			if ((pl = atv->playlist()) == 0) {
				return 0;
			}
			
			region = dynamic_cast<AudioRegion*> (pl->top_region_at (start));
		}
	} 
	
	return region;
}
	
void
Editor::extend_selection_to_end_of_region (bool next)
{
	TimeAxisView *tv;
	Region *region;
	jack_nframes_t start;

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
	Region *region;
	jack_nframes_t end;

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
	jack_nframes_t distance;
	jack_nframes_t next_distance;

	if (!session) return;
	
	if (!selection->audio_regions.empty()) {

		begin_reversible_command (_("nudge forward"));

		for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
			AudioRegion& r ((*i)->region);
			
			distance = get_nudge_distance (r.position(), next_distance);

			if (next) {
				distance = next_distance;
			}

			session->add_undo (r.playlist()->get_memento());
			r.set_position (r.position() + distance, this);
			session->add_redo_no_execute (r.playlist()->get_memento());
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
	jack_nframes_t distance;
	jack_nframes_t next_distance;

	if (!session) return;
	
	if (!selection->audio_regions.empty()) {

		begin_reversible_command (_("nudge forward"));

		for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
			AudioRegion& r ((*i)->region);

			distance = get_nudge_distance (r.position(), next_distance);
			
			if (next) {
				distance = next_distance;
			}

			session->add_undo (r.playlist()->get_memento());
			
			if (r.position() > distance) {
				r.set_position (r.position() - distance, this);
			} else {
				r.set_position (0, this);
			}
			session->add_redo_no_execute (r.playlist()->get_memento());
		}

		commit_reversible_command ();

	} else {

		distance = get_nudge_distance (playhead_cursor->current_frame, next_distance);

		if (playhead_cursor->current_frame > distance) {
			session->request_locate (playhead_cursor->current_frame - distance);
		} else {
			session->request_locate (0);
		}
	}
}

void
Editor::nudge_forward_capture_offset ()
{
	jack_nframes_t distance;

	if (!session) return;
	
	if (!selection->audio_regions.empty()) {

		begin_reversible_command (_("nudge forward"));

		distance = session->worst_output_latency();

		for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
			AudioRegion& r ((*i)->region);
			
			session->add_undo (r.playlist()->get_memento());
			r.set_position (r.position() + distance, this);
			session->add_redo_no_execute (r.playlist()->get_memento());
		}

		commit_reversible_command ();

	} 
}
		
void
Editor::nudge_backward_capture_offset ()
{
	jack_nframes_t distance;

	if (!session) return;
	
	if (!selection->audio_regions.empty()) {

		begin_reversible_command (_("nudge forward"));

		distance = session->worst_output_latency();

		for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
			AudioRegion& r ((*i)->region);

			session->add_undo (r.playlist()->get_memento());
			
			if (r.position() > distance) {
				r.set_position (r.position() - distance, this);
			} else {
				r.set_position (0, this);
			}
			session->add_redo_no_execute (r.playlist()->get_memento());
		}

		commit_reversible_command ();
	}
}

/* DISPLAY MOTION */

void
Editor::move_to_start ()
{
	session->request_locate (0);
}

void
Editor::move_to_end ()
{

	session->request_locate (session->current_end_frame());
}

void
Editor::build_region_boundary_cache ()
{
	jack_nframes_t pos = 0;
	RegionPoint point;
	Region *r;
	TrackViewList tracks;

	region_boundary_cache.clear ();

	if (session == 0) {
		return;
	}
	
	switch (snap_type) {
	case SnapToRegionStart:
		point = Start;
		break;
	case SnapToRegionEnd:
		point = End;
		break;	
	case SnapToRegionSync:
		point = SyncPoint;
		break;	
	case SnapToRegionBoundary:
		point = Start;
		break;	
	default:
		fatal << string_compose (_("build_region_boundary_cache called with snap_type = %1"), snap_type) << endmsg;
		/*NOTREACHED*/
		return;
	}
	
	TimeAxisView *ontrack = 0;

	while (pos < session->current_end_frame()) {

		if (!selection->tracks.empty()) {

			if ((r = find_next_region (pos, point, 1, selection->tracks, &ontrack)) == 0) {
				break;
			}

		} else if (clicked_trackview) {

			TrackViewList t;
			t.push_back (clicked_trackview);

			if ((r = find_next_region (pos, point, 1, t, &ontrack)) == 0) {
				break;
			}

		} else {

			if ((r = find_next_region (pos, point, 1, track_views, &ontrack)) == 0) {
				break;
			}
		}

		jack_nframes_t rpos;
		
		switch (snap_type) {
		case SnapToRegionStart:
			rpos = r->first_frame();
			break;
		case SnapToRegionEnd:
			rpos = r->last_frame();
			break;	
		case SnapToRegionSync:
			rpos = r->adjust_to_sync (r->first_frame());
			break;

		case SnapToRegionBoundary:
			rpos = r->last_frame();
			break;	
		default:
			break;
		}
		
		float speed = 1.0f;
		AudioTimeAxisView *atav;

		if ( ontrack != 0 && (atav = dynamic_cast<AudioTimeAxisView*>(ontrack)) != 0 ) {
			if (atav->get_diskstream() != 0) {
				speed = atav->get_diskstream()->speed();
			}
		}

		rpos = track_frame_to_session_frame(rpos, speed);

		if (region_boundary_cache.empty() || rpos != region_boundary_cache.back()) {
			if (snap_type == SnapToRegionBoundary) {
				region_boundary_cache.push_back (r->first_frame());
			}
			region_boundary_cache.push_back (rpos);
		}

		pos = rpos + 1;
	}
}

Region*
Editor::find_next_region (jack_nframes_t frame, RegionPoint point, int32_t dir, TrackViewList& tracks, TimeAxisView **ontrack)
{
	TrackViewList::iterator i;
	jack_nframes_t closest = max_frames;
	Region* ret = 0;
	jack_nframes_t rpos = 0;

	float track_speed;
	jack_nframes_t track_frame;
	AudioTimeAxisView *atav;

	for (i = tracks.begin(); i != tracks.end(); ++i) {

		jack_nframes_t distance;
		Region* r;

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
	Region* r;
	jack_nframes_t pos = cursor->current_frame;

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
	jack_nframes_t pos = 0;
	switch (mouse_mode) {
	case MouseObject:
		if (!selection->audio_regions.empty()) {
			pos = selection->audio_regions.start();
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
	jack_nframes_t pos = 0;

	switch (mouse_mode) {
	case MouseObject:
		if (!selection->audio_regions.empty()) {
			pos = selection->audio_regions.end_frame();
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
Editor::playhead_backward ()
{
	jack_nframes_t pos;
	jack_nframes_t cnt;
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (jack_nframes_t) floor (prefix * session->frame_rate ());
		} else {
			cnt = (jack_nframes_t) prefix;
		}
	}

	pos = playhead_cursor->current_frame;

	if ((jack_nframes_t) pos < cnt) {
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
	jack_nframes_t pos;
	jack_nframes_t cnt;
	bool was_floating;
	float prefix;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (jack_nframes_t) floor (prefix * session->frame_rate ());
		} else {
			cnt = (jack_nframes_t) floor (prefix);
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
	jack_nframes_t pos;
	jack_nframes_t cnt;
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (jack_nframes_t) floor (prefix * session->frame_rate ());
		} else {
			cnt = (jack_nframes_t) prefix;
		}
	}

	pos = edit_cursor->current_frame;

	if ((jack_nframes_t) pos < cnt) {
		pos = 0;
	} else {
		pos -= cnt;
	}
	
	edit_cursor->set_position (pos);
}

void
Editor::edit_cursor_forward ()
{
	jack_nframes_t pos;
	jack_nframes_t cnt;
	bool was_floating;
	float prefix;

	if (get_prefix (prefix, was_floating)) {
		cnt = 1;
	} else {
		if (was_floating) {
			cnt = (jack_nframes_t) floor (prefix * session->frame_rate ());
		} else {
			cnt = (jack_nframes_t) floor (prefix);
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
	jack_nframes_t frame;

	if (get_prefix (prefix, was_floating)) {
		return;
	}

	if (was_floating) {
		frame = (jack_nframes_t) floor (prefix * session->frame_rate());
	} else {
		frame = (jack_nframes_t) floor (prefix);
	}

	session->request_locate (frame);
}

void
Editor::scroll_backward (float pages)
{
	jack_nframes_t frame;
	jack_nframes_t one_page = (jack_nframes_t) rint (canvas_width * frames_per_unit);
	bool was_floating;
	float prefix;
	jack_nframes_t cnt;
	
	if (get_prefix (prefix, was_floating)) {
		cnt = (jack_nframes_t) floor (pages * one_page);
	} else {
		if (was_floating) {
			cnt = (jack_nframes_t) floor (prefix * session->frame_rate());
		} else {
			cnt = (jack_nframes_t) floor (prefix * one_page);
		}
	}

	if (leftmost_frame < cnt) {
		frame = 0;
	} else {
		frame = leftmost_frame - cnt;
	}

	reposition_x_origin (frame);
}

void
Editor::scroll_forward (float pages)
{
	jack_nframes_t frame;
	jack_nframes_t one_page = (jack_nframes_t) rint (canvas_width * frames_per_unit);
	bool was_floating;
	float prefix;
	jack_nframes_t cnt;
	
	if (get_prefix (prefix, was_floating)) {
		cnt = (jack_nframes_t) floor (pages * one_page);
	} else {
		if (was_floating) {
			cnt = (jack_nframes_t) floor (prefix * session->frame_rate());
		} else {
			cnt = (jack_nframes_t) floor (prefix * one_page);
		}
	}

	if (ULONG_MAX - cnt < leftmost_frame) {
		frame = ULONG_MAX - cnt;
	} else {
		frame = leftmost_frame + cnt;
	}

	reposition_x_origin (frame);
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

	vertical_adjustment.set_value (vertical_adjustment.get_value() + (cnt * vertical_adjustment.get_page_size()));
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
	adj->set_value (adj->get_value() + 10);
}

void
Editor::scroll_tracks_up_line ()
{
        Gtk::Adjustment* adj = edit_vscrollbar.get_adjustment();
	adj->set_value (adj->get_value() - 10);
}

/* ZOOM */

void
Editor::temporal_zoom_step (bool coarser)
{
	double nfpu;

	nfpu = frames_per_unit;
	
	if (coarser) { 
		nfpu *= 2.0;
	} else { 
		nfpu = max(1.0,(nfpu/2.0));
	}

	temporal_zoom (nfpu);
}	

void
Editor::temporal_zoom (gdouble fpu)
{
	if (!session) return;
	
	jack_nframes_t current_page = current_page_frames();
	jack_nframes_t current_leftmost = leftmost_frame;
	jack_nframes_t current_rightmost;
	jack_nframes_t current_center;
	jack_nframes_t new_page;
	jack_nframes_t leftmost_after_zoom = 0;
	double nfpu;

	nfpu = fpu;
	
	new_page = (jack_nframes_t) floor (canvas_width * nfpu);

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
		if (edit_cursor->current_frame > leftmost_frame + (new_page/2)) {
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

	jack_nframes_t start = selection->time[clicked_selection].start;
	jack_nframes_t end = selection->time[clicked_selection].end;

	temporal_zoom_by_frame (start, end, "zoom to selection");
}

void
Editor::temporal_zoom_session ()
{
	if (session) {
		temporal_zoom_by_frame (0, session->current_end_frame(), "zoom to session");
	}
}

void
Editor::temporal_zoom_by_frame (jack_nframes_t start, jack_nframes_t end, const string & op)
{
	if (!session) return;

	if ((start == 0 && end == 0) || end < start) {
		return;
	}

	jack_nframes_t range = end - start;

	double new_fpu = (double)range / (double)canvas_width;
// 	double p2 = 1.0;

// 	while (p2 < new_fpu) {
// 		p2 *= 2.0;
// 	}
// 	new_fpu = p2;
	
	jack_nframes_t new_page = (jack_nframes_t) floor (canvas_width * new_fpu);
	jack_nframes_t middle = (jack_nframes_t) floor( (double)start + ((double)range / 2.0f ));
	jack_nframes_t new_leftmost = (jack_nframes_t) floor( (double)middle - ((double)new_page/2.0f));

	if (new_leftmost > middle) new_leftmost = 0;

//	begin_reversible_command (op);
//	session->add_undo (bind (mem_fun(*this, &Editor::reposition_and_zoom), leftmost_frame, frames_per_unit));
//	session->add_redo (bind (mem_fun(*this, &Editor::reposition_and_zoom), new_leftmost, new_fpu));
//	commit_reversible_command ();

	reposition_and_zoom (new_leftmost, new_fpu);
}

void 
Editor::temporal_zoom_to_frame (bool coarser, jack_nframes_t frame)
{
	if (!session) return;
	
	jack_nframes_t range_before = frame - leftmost_frame;
	double new_fpu;
	
	new_fpu = frames_per_unit;
	
	if (coarser) { 
		new_fpu *= 2.0;
		range_before *= 2;
	} else { 
		new_fpu = max(1.0,(new_fpu/2.0));
		range_before /= 2;
	}

	if (new_fpu == frames_per_unit) return;

	jack_nframes_t new_leftmost = frame - range_before;

	if (new_leftmost > frame) new_leftmost = 0;

//	begin_reversible_command (_("zoom to frame"));
//	session->add_undo (bind (mem_fun(*this, &Editor::reposition_and_zoom), leftmost_frame, frames_per_unit));
//	session->add_redo (bind (mem_fun(*this, &Editor::reposition_and_zoom), new_leftmost, new_fpu));
//	commit_reversible_command ();

	reposition_and_zoom (new_leftmost, new_fpu);
}

void
Editor::select_all_in_track (bool add)
{
	list<Selectable *> touched;

	if (!clicked_trackview) {
		return;
	}
	
	clicked_trackview->get_selectables (0, max_frames, 0, DBL_MAX, touched);

	if (add) {
		selection->add (touched);
	} else {
		selection->set (touched);
	}
}

void
Editor::select_all (bool add)
{
	list<Selectable *> touched;
	
	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (0, max_frames, 0, DBL_MAX, touched);
	}

	if (add) {
		selection->add (touched);
	} else {
		selection->set (touched);
	}

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
Editor::select_all_within (jack_nframes_t start, jack_nframes_t end, double top, double bot, bool add)
{
	list<Selectable *> touched;
	
	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, top, bot, touched);
	}

	if (add) {
		selection->add (touched);
	} else {
		selection->set (touched);
	}

	return !touched.empty();
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
Editor::select_all_from_punch()
{
	Location* location;
	list<Selectable *> touched;
	if ((location = session->locations()->auto_punch_location()) == 0)  {
		return;
	}

	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (location->start(), location->end(), 0, DBL_MAX, touched);
	}
		selection->set (touched);

}

void
Editor::select_all_from_loop()
{
	Location* location;
	list<Selectable *> touched;

	if ((location = session->locations()->auto_loop_location()) == 0)  {
		return;
	}
	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (location->start(), location->end(), 0, DBL_MAX, touched);
	}
		selection->set (touched);

}

void
Editor::set_selection_from_range (Location& range)
{
	if (clicked_trackview == 0) {
		return;
	}
	
	begin_reversible_command (_("set selection from range"));
	selection->set (0, range.start(), range.end());
	commit_reversible_command ();
}

void
Editor::select_all_after_cursor (Cursor *cursor, bool after)
{
        jack_nframes_t start;
	jack_nframes_t end;
	list<Selectable *> touched;

	if (after) {
	  start = cursor->current_frame ;
	  end = session->current_end_frame();
	} else {
	  start = 0;
	  end = cursor->current_frame ;
	}
	for (TrackViewList::iterator iter = track_views.begin(); iter != track_views.end(); ++iter) {
		if ((*iter)->hidden()) {
			continue;
		}
		(*iter)->get_selectables (start, end, 0, DBL_MAX, touched);
	}
	selection->set (touched);
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
		session->request_locate (0);
	}
}

void
Editor::set_mark ()
{
	jack_nframes_t pos;
	float prefix;
	bool was_floating;

	if (get_prefix (prefix, was_floating)) {
		pos = session->audible_frame ();
	} else {
		if (was_floating) {
			pos = (jack_nframes_t) floor (prefix * session->frame_rate ());
		} else {
			pos = (jack_nframes_t) floor (prefix);
		}
	}

	session->locations()->add (new Location (pos, 0, "mark", Location::IsMark), true);
}

void
Editor::clear_markers ()
{
	if (session) {
		session->begin_reversible_command (_("clear markers"));
		session->add_undo (session->locations()->get_memento());
		session->locations()->clear_markers ();
		session->add_redo_no_execute (session->locations()->get_memento());
		session->commit_reversible_command ();
	}
}

void
Editor::clear_ranges ()
{
	if (session) {
		session->begin_reversible_command (_("clear ranges"));
		session->add_undo (session->locations()->get_memento());
		
		Location * looploc = session->locations()->auto_loop_location();
		Location * punchloc = session->locations()->auto_punch_location();
		
		session->locations()->clear_ranges ();
		// re-add these
		if (looploc) session->locations()->add (looploc);
		if (punchloc) session->locations()->add (punchloc);
		
		session->add_redo_no_execute (session->locations()->get_memento());
		session->commit_reversible_command ();
	}
}

void
Editor::clear_locations ()
{
	session->begin_reversible_command (_("clear locations"));
	session->add_undo (session->locations()->get_memento());
	session->locations()->clear ();
	session->add_redo_no_execute (session->locations()->get_memento());
	session->commit_reversible_command ();
	session->locations()->clear ();
}

/* INSERT/REPLACE */

void
Editor::insert_region_list_drag (AudioRegion& region, int x, int y)
{
	double wx, wy;
	double cx, cy;
	TimeAxisView *tv;
	jack_nframes_t where;
	AudioTimeAxisView *atv = 0;
	Playlist *playlist;
	
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
	session->add_undo (playlist->get_memento());
	playlist->add_region (*(new AudioRegion (region)), where, 1.0);
	session->add_redo_no_execute (playlist->get_memento());
	commit_reversible_command ();
}

void
Editor::insert_region_list_selection (float times)
{
	AudioTimeAxisView *tv = 0;
	Playlist *playlist;

	if (clicked_audio_trackview != 0) {
		tv = clicked_audio_trackview;
	} else if (!selection->tracks.empty()) {
		if ((tv = dynamic_cast<AudioTimeAxisView*>(selection->tracks.front())) == 0) {
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
	
	TreeModel::iterator i = region_list_display.get_selection()->get_selected();
	Region* region = (*i)[region_list_columns.region];

	begin_reversible_command (_("insert region"));
	session->add_undo (playlist->get_memento());
	playlist->add_region (*(createRegion (*region)), edit_cursor->current_frame, times);
	session->add_redo_no_execute (playlist->get_memento());
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
Editor::toggle_playback (bool with_abort)
{
	if (!session) {
		return;
	}

	switch (session->slave_source()) {
	case Session::None:
	case Session::JACK:
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
		if (session->get_auto_loop()) {
			session->request_auto_loop (false);
		}
	} else {
		session->request_transport_speed (1.0f);
	}
}

void
Editor::play_from_start ()
{
	session->request_locate (0, true);
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
	if (!selection->audio_regions.empty()) {
		AudioRegionView *rv = *(selection->audio_regions.begin());

		session->request_bounded_roll (rv->region.position(), rv->region.last_frame());	
	}
}

void
Editor::loop_selected_region ()
{
	if (!selection->audio_regions.empty()) {
		AudioRegionView *rv = *(selection->audio_regions.begin());
		Location* tll;

		if ((tll = transport_loop_location()) != 0)  {

			tll->set (rv->region.position(), rv->region.last_frame());
			
			// enable looping, reposition and start rolling

			session->request_auto_loop (true);
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
		session->request_auto_loop (true);
		session->request_locate (tll->start(), true);
	}
}

void 
Editor::toggle_region_mute ()
{
	if (clicked_regionview) {
		clicked_regionview->region.set_muted (!clicked_regionview->region.muted());
	} else if (!selection->audio_regions.empty()) {
		bool yn = ! (*selection->audio_regions.begin())->region.muted();
		selection->foreach_audio_region (&AudioRegion::set_muted, yn);
	}
}

void
Editor::toggle_region_opaque ()
{
	if (clicked_regionview) {
		clicked_regionview->region.set_opaque (!clicked_regionview->region.opaque());
	} else if (!selection->audio_regions.empty()) {
		bool yn = ! (*selection->audio_regions.begin())->region.opaque();
		selection->foreach_audio_region (&Region::set_opaque, yn);
	}
}

void
Editor::raise_region ()
{
	selection->foreach_audio_region (&Region::raise);
}

void
Editor::raise_region_to_top ()
{
	selection->foreach_audio_region (&Region::raise_to_top);
}

void
Editor::lower_region ()
{
	selection->foreach_audio_region (&Region::lower);
}

void
Editor::lower_region_to_bottom ()
{
	selection->foreach_audio_region (&Region::lower_to_bottom);
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

	if (selection->audio_regions.empty()) {
		return;
	}

	dialog.set_title (_("ardour: rename region"));
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
		(*selection->audio_regions.begin())->region.set_name (entry.get_text());
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
Editor::audition_playlist_region_via_route (AudioRegion& region, Route& route)
{
	if (session->is_auditioning()) {
		session->cancel_audition ();
	} 

	// note: some potential for creativity here, because region doesn't
	// have to belong to the playlist that Route is handling

	// bool was_soloed = route.soloed();

	route.set_solo (true, this);
	
	session->request_bounded_roll (region.position(), region.position() + region.length());
	
	/* XXX how to unset the solo state ? */
}

void
Editor::audition_selected_region ()
{
	if (!selection->audio_regions.empty()) {
		AudioRegionView* rv = *(selection->audio_regions.begin());
		session->audition_region (rv->region);
	}
}

void
Editor::audition_playlist_region_standalone (AudioRegion& region)
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

void *
Editor::_import_thread (void *arg)
{
	PBD::ThreadCreated (pthread_self(), X_("Import"));

	Editor *ed = (Editor *) arg;
	return ed->import_thread ();
}

void *
Editor::import_thread ()
{
	session->import_audiofile (import_status);
	return 0;
}

gint
Editor::import_progress_timeout (void *arg)
{
	interthread_progress_label.set_text (import_status.doing_what);

	if (import_status.freeze) {
		interthread_cancel_button.set_sensitive(false);
	} else {
		interthread_cancel_button.set_sensitive(true);
	}

	if (import_status.doing_what == "building peak files") {
		interthread_progress_bar.pulse ();
		return FALSE;
	} else {
		interthread_progress_bar.set_fraction (import_status.progress/100);
	}

	return !(import_status.done || import_status.cancel);
}

void
Editor::import_audio (bool as_tracks)
{
	if (session == 0) {
		warning << _("You can't import an audiofile until you have a session loaded.") << endmsg;
		return;
	}

	string str;

	if (as_tracks) {
		str =_("Import selected as tracks");
	} else {
		str = _("Import selected to region list");
	}

	SoundFileOmega sfdb (str);
	sfdb.Imported.connect (bind (mem_fun (*this, &Editor::do_import), as_tracks));

	sfdb.run();
}

void
Editor::catch_new_audio_region (AudioRegion* ar)
{
	last_audio_region = ar;
}

void
Editor::do_import (vector<string> paths, bool split, bool as_tracks)
{
	sigc::connection c;
	
	/* SFDB sets "multichan" to true to indicate "split channels"
	   so reverse the setting to match the way libardour
	   interprets it.
	*/
	
	import_status.multichan = !split;

	if (interthread_progress_window == 0) {
		build_interthread_progress_window ();
	}
	
	interthread_progress_window->set_title (_("ardour: audio import in progress"));
	interthread_progress_window->set_position (Gtk::WIN_POS_MOUSE);
	interthread_progress_window->show_all ();
	interthread_progress_bar.set_fraction (0.0f);
	interthread_cancel_label.set_text (_("Cancel Import"));
	current_interthread_info = &import_status;

	c = session->AudioRegionAdded.connect (mem_fun(*this, &Editor::catch_new_audio_region));

	for (vector<string>::iterator i = paths.begin(); i != paths.end(); ++i ) {

		interthread_progress_window->set_title (string_compose (_("ardour: importing %1"), (*i)));
	
		import_status.pathname = (*i);
		import_status.done = false;
		import_status.cancel = false;
		import_status.freeze = false;
		import_status.done = 0.0;
		
		interthread_progress_connection = 
		  Glib::signal_timeout().connect (bind (mem_fun(*this, &Editor::import_progress_timeout), (gpointer) 0), 100);
		
		last_audio_region = 0;
		
		pthread_create_and_store ("import", &import_status.thread, 0, _import_thread, this);
		pthread_detach (import_status.thread);
		
		while (!(import_status.done || import_status.cancel)) {
			gtk_main_iteration ();
		}
		
		import_status.done = true;
		interthread_progress_connection.disconnect ();

		if (as_tracks && last_audio_region != 0) {
			uint32_t channels = last_audio_region->n_channels();

			AudioTrack* at = session->new_audio_track (channels, channels);
			AudioRegion* copy = new AudioRegion (*last_audio_region);
			at->disk_stream().playlist()->add_region (*copy, 0);
		}
	}

	c.disconnect ();
	interthread_progress_window->hide_all ();
}

int
Editor::reject_because_rate_differs (const string & path, SF_INFO& finfo, const string & action, bool multiple_pending)
{
	if (!session) {
		return 1;
	}

	if (finfo.samplerate != (int) session->frame_rate()) {
		vector<string> choices;

		choices.push_back (string_compose (_("%1 it anyway"), action));

		if (multiple_pending) {
			/* XXX assumptions about sentence structure
			   here for translators. Sorry.
			*/
			choices.push_back (string_compose (_("Don't %1 it"), action));
			choices.push_back (string_compose (_("%1 all without questions"), action));
			choices.push_back (_("Cancel entire import"));
		} else {
			choices.push_back (_("Cancel"));
		}

		Gtkmm2ext::Choice rate_choice (
			string_compose (_("%1\nThis audiofile's sample rate doesn't match the session sample rate!"), path),
			choices);

		rate_choice.chosen.connect (ptr_fun (Main::quit));
		rate_choice.show_all ();

		Main::run ();

		switch (rate_choice.get_choice()) {
		case 0: /* do it anyway */
			return 0;
		case 1: /* don't import this one */
			return 1;
		case 2: /* do the rest without asking */
			return -1;
		case 3: /* stop a multi-file import */
		default:
			return -2;
		}
	}

	return 0;
}

void 
Editor::embed_audio ()
{
	if (session == 0) {
		warning << _("You can't embed an audiofile until you have a session loaded.") << endmsg;
		return;
	}

	SoundFileOmega sfdb (_("Add to External Region list"));
	sfdb.Embedded.connect (mem_fun (*this, &Editor::do_embed_sndfiles));

	sfdb.run ();
}

void
Editor::do_embed_sndfiles (vector<string> paths, bool split)
{
	bool multiple_files = paths.size() > 1;
	bool check_sample_rate = true;

	for (vector<string>::iterator i = paths.begin(); i != paths.end(); ++i) {
		embed_sndfile (*i, split, multiple_files, check_sample_rate);
	}

	session->save_state ("");
}

void
Editor::embed_sndfile (string path, bool split, bool multiple_files, bool& check_sample_rate)
{
	SndFileSource *source = 0; /* keep g++ quiet */
	AudioRegion::SourceList sources;
	string idspec;
	string linked_path;
	SNDFILE *sf;
	SF_INFO finfo;

	/* lets see if we can link it into the session */
	
	linked_path = session->sound_dir();
	linked_path += PBD::basename (path);

	if (link (path.c_str(), linked_path.c_str()) == 0) {

		/* there are many reasons why link(2) might have failed.
		   but if it succeeds, we now have a link in the
		   session sound dir that will protect against
		   unlinking of the original path. nice.
		*/

		path = linked_path;
	}

	memset (&finfo, 0, sizeof(finfo));

	/* note that we temporarily truncated _id at the colon */
	
	if ((sf = sf_open (path.c_str(), SFM_READ, &finfo)) == 0) {
		char errbuf[256];
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		error << string_compose(_("Editor: cannot open file \"%1\" (%2)"), selection, errbuf) << endmsg;
		return;
	}
	sf_close (sf);
	sf = 0;
	
	if (check_sample_rate) {
		switch (reject_because_rate_differs (path, finfo, "Embed", multiple_files)) {
		case 0:
			break;
		case 1:
			return;
		case -1:
			check_sample_rate = false;
			break;
			
		case -2:
		default:
			return;
		}
	}

	track_canvas.get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));
	ARDOUR_UI::instance()->flush_pending ();

	/* make the proper number of channels in the region */

	for (int n=0; n < finfo.channels; ++n)
	{
		idspec = path;
		idspec += string_compose(":%1", n);
		
		try {
			source = new SndFileSource (idspec.c_str());
			sources.push_back(source);
		} 

		catch (failed_constructor& err) {
			error << string_compose(_("could not open %1"), path) << endmsg;
			goto out;
		}

		ARDOUR_UI::instance()->flush_pending ();
	}

	if (sources.size() > 0) {

		string region_name = PBD::basename_nosuffix (path);
		region_name += "-0";

		/* The created region isn't dropped.  It emits a signal
		   that is picked up by the session. 
		*/

		new AudioRegion (sources, 0, sources[0]->length(), region_name, 0,
				 Region::Flag (Region::DefaultFlags|Region::WholeFile|Region::External));
		
		/* make sure we can see it in the list */

                /* its the second node, always */

		// GTK2FIX ?? is it still always the 2nd node

		TreeModel::Path path ("2");
		region_list_display.expand_row (path, true);

		ARDOUR_UI::instance()->flush_pending ();
	}

  out:
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
}

void
Editor::insert_sndfile (bool as_tracks)
{
//	SoundFileSelector& sfdb (ARDOUR_UI::instance()->get_sfdb_window());
	sigc::connection c;
	string str;

	if (as_tracks) {

//		c = sfdb.Action.connect (mem_fun(*this, &Editor::insert_paths_as_new_tracks));
		str = _("Insert selected as new tracks");

	} else {

		jack_nframes_t pos;

		if (clicked_audio_trackview == 0) {
			return;
		}

		if (ensure_cursor (&pos)) {
			return;
		}

//		c = sfdb.Action.connect (bind (mem_fun(*this, &Editor::do_insert_sndfile), pos));
		str = _("Insert selected");
	}

//	sfdb.run (str, false);
//	c.disconnect ();
}

void
Editor::insert_paths_as_new_tracks (vector<string> paths, bool split)
{
	SNDFILE *sf;
	SF_INFO finfo;
	bool multiple_files;
	bool check_sample_rate = true;

	multiple_files = paths.size() > 1;	

	for (vector<string>::iterator p = paths.begin(); p != paths.end(); ++p) {
		
		memset (&finfo, 0, sizeof(finfo));
		
		if ((sf = sf_open ((*p).c_str(), SFM_READ, &finfo)) == 0) {
			char errbuf[256];
			sf_error_str (0, errbuf, sizeof (errbuf) - 1);
			error << string_compose(_("Editor: cannot open file \"%1\" (%2)"), (*p), errbuf) << endmsg;
			continue;
		}
		
		sf_close (sf);
		sf = 0;
		
		/* add a new track */
		
		if (check_sample_rate) {
			switch (reject_because_rate_differs (*p, finfo, "Insert", multiple_files)) {
			case 0:
				break;
			case 1:
				continue;
			case -1:
				check_sample_rate = false;
				break;
				
			case -2:
				return;
			}
		}
		
		uint32_t input_chan = finfo.channels;
		uint32_t output_chan;
		
		if (session->get_output_auto_connect() & Session::AutoConnectMaster) {
			output_chan = (session->master_out() ? session->master_out()->n_inputs() : input_chan);
		} else {
			output_chan = input_chan;
		}
		
		(void) session->new_audio_track (input_chan, output_chan);


		/* get the last (most recently added) track view */
	
		AudioTimeAxisView* tv;
	
		if ((tv = dynamic_cast<AudioTimeAxisView*>(track_views.back())) == 0) {
			fatal << _("programming error: ")
			      << X_("last trackview after new_audio_track is not an audio track!")
			      << endmsg;
			/*NOTREACHED*/
		}
		
		jack_nframes_t pos = 0;
		insert_sndfile_into (*p, true, tv, pos, false);
	}
}

void
Editor::do_insert_sndfile (vector<string> paths, bool split, jack_nframes_t pos)
{
	for (vector<string>::iterator x = paths.begin(); x != paths.end(); ++x) {
		insert_sndfile_into (*x, !split, clicked_audio_trackview, pos);
	}
}

void
Editor::insert_sndfile_into (const string & path, bool multi, AudioTimeAxisView* tv, jack_nframes_t& pos, bool prompt)
{
	SndFileSource *source = 0; /* keep g++ quiet */
	AudioRegion::SourceList sources;
	string idspec;
	SNDFILE *sf;
	SF_INFO finfo;

	memset (&finfo, 0, sizeof(finfo));

	/* note that we temporarily truncated _id at the colon */
	
	if ((sf = sf_open (path.c_str(), SFM_READ, &finfo)) == 0) {
		char errbuf[256];
		sf_error_str (0, errbuf, sizeof (errbuf) - 1);
		error << string_compose(_("Editor: cannot open file \"%1\" (%2)"), path, errbuf) << endmsg;
		return;
	}
	sf_close (sf);
	sf = 0;
	
	if (prompt && (reject_because_rate_differs (path, finfo, "Insert", false) != 0)) {
		return;
	}

	track_canvas.get_window()->set_cursor (Gdk::Cursor (Gdk::WATCH));
	ARDOUR_UI::instance()->flush_pending ();

	/* make the proper number of channels in the region */

	for (int n=0; n < finfo.channels; ++n)
	{
		idspec = path;
		idspec += string_compose(":%1", n);

		try {
			source = new SndFileSource (idspec.c_str());
			sources.push_back(source);
		} 

		catch (failed_constructor& err) {
			error << string_compose(_("could not open %1"), path) << endmsg;
			goto out;
		}

		ARDOUR_UI::instance()->flush_pending ();
	}

	if (sources.size() > 0) {

		string region_name = region_name_from_path (PBD::basename (path));
		
		AudioRegion *region = new AudioRegion (sources, 0, sources[0]->length(), region_name, 
						       0, /* irrelevant these days */
						       Region::Flag (Region::DefaultFlags|Region::WholeFile|Region::External));

		begin_reversible_command (_("insert sndfile"));
		session->add_undo (tv->playlist()->get_memento());
		tv->playlist()->add_region (*region, pos);
		session->add_redo_no_execute (tv->playlist()->get_memento());
		commit_reversible_command ();
		
		pos += sources[0]->length();

		ARDOUR_UI::instance()->flush_pending ();
	}

  out:
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
	return;
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

	jack_nframes_t start = selection->time[clicked_selection].start;
	jack_nframes_t end = selection->time[clicked_selection].end;

	jack_nframes_t selection_cnt = end - start + 1;
	
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		AudioRegion *region;
		AudioRegion *current;
		Region* current_r;
		Playlist *pl;

		jack_nframes_t internal_start;
		string new_name;

		if ((pl = (*i)->playlist()) == 0) {
			continue;
		}

		if ((current_r = pl->top_region_at (start)) == 0) {
			continue;
		}

		if ((current = dynamic_cast<AudioRegion*> (current_r)) != 0) {
			internal_start = start - current->position();
			session->region_name (new_name, current->name(), true);
			region = new AudioRegion (*current, internal_start, selection_cnt, new_name);
		}
	}
}	

void
Editor::create_region_from_selection (vector<AudioRegion *>& new_regions)
{
	if (selection->time.empty() || selection->tracks.empty()) {
		return;
	}

	jack_nframes_t start = selection->time[clicked_selection].start;
	jack_nframes_t end = selection->time[clicked_selection].end;
	
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		AudioRegion* current;
		Region* current_r;
		Playlist* playlist;
		jack_nframes_t internal_start;
		string new_name;

		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}

		if ((current_r = playlist->top_region_at(start)) == 0) {
			continue;
		}

		if ((current = dynamic_cast<AudioRegion*>(current_r)) == 0) {
			continue;
		}
	
		internal_start = start - current->position();
		session->region_name (new_name, current->name(), true);
		
		new_regions.push_back (new AudioRegion (*current, internal_start, end - start + 1, new_name));
	}
}

void
Editor::split_multichannel_region ()
{
	vector<AudioRegion*> v;

	if (!clicked_regionview || clicked_regionview->region.n_channels() < 2) {
		return;
	}

	clicked_regionview->region.separate_by_channel (*session, v);

	/* nothing else to do, really */
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

	Playlist *playlist;
		
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*> ((*i))) != 0) {

			if (atv->is_audio_track()) {
					
				if ((playlist = atv->playlist()) != 0) {
					if (!doing_undo) {
						begin_reversible_command (_("separate"));
						doing_undo = true;
					}
					if (doing_undo) session->add_undo ((playlist)->get_memento());
			
					/* XXX need to consider musical time selections here at some point */

					double speed = atv->get_diskstream()->speed();

					for (list<AudioRange>::iterator t = selection->time.begin(); t != selection->time.end(); ++t) {
						playlist->partition ((jack_nframes_t)((*t).start * speed), (jack_nframes_t)((*t).end * speed), true);
					}

					if (doing_undo) session->add_redo_no_execute (playlist->get_memento());
				}
			}
		}
	}

	if (doing_undo)	commit_reversible_command ();
}

void
Editor::crop_region_to_selection ()
{
	if (selection->time.empty()) {
		return;
	}

	vector<Playlist*> playlists;
	Playlist *playlist;

	if (clicked_trackview != 0) {

		if ((playlist = clicked_trackview->playlist()) == 0) {
			return;
		}

		playlists.push_back (playlist);

	} else {
		
		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

			AudioTimeAxisView* atv;

			if ((atv = dynamic_cast<AudioTimeAxisView*> ((*i))) != 0) {

				if (atv->is_audio_track()) {
					
					if ((playlist = atv->playlist()) != 0) {
						playlists.push_back (playlist);
					}
				}
			}
		}
	}

	if (!playlists.empty()) {

		jack_nframes_t start;
		jack_nframes_t end;
		jack_nframes_t cnt;

		begin_reversible_command (_("trim to selection"));

		for (vector<Playlist*>::iterator i = playlists.begin(); i != playlists.end(); ++i) {
			
			Region *region;
			
			start = selection->time.start();

			if ((region = (*i)->top_region_at(start)) == 0) {
				continue;
			}
			
			/* now adjust lengths to that we do the right thing
			   if the selection extends beyond the region
			*/
			
			start = max (start, region->position());
			end = min (selection->time.end_frame(), start + region->length() - 1);
			cnt = end - start + 1;

			session->add_undo ((*i)->get_memento());
			region->trim_to (start, cnt, this);
			session->add_redo_no_execute ((*i)->get_memento());
		}

		commit_reversible_command ();
	}
}		

void
Editor::region_fill_track ()
{
	jack_nframes_t end;

	if (!session || selection->audio_regions.empty()) {
		return;
	}

	end = session->current_end_frame ();

	begin_reversible_command (_("region fill"));

	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {

		AudioRegion& region ((*i)->region);
		Playlist* pl = region.playlist();

		if (end <= region.last_frame()) {
			return;
		}

		double times = (double) (end - region.last_frame()) / (double) region.length();

		if (times == 0) {
			return;
		}

		session->add_undo (pl->get_memento());
		pl->add_region (*(new AudioRegion (region)), region.last_frame(), times);
		session->add_redo_no_execute (pl->get_memento());
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

	Region *region;

	Glib::RefPtr<TreeSelection> selected = region_list_display.get_selection();

	if (selected->count_selected_rows() != 1) {
		return;
	}

	TreeModel::iterator i = region_list_display.get_selection()->get_selected();
	region = (*i)[region_list_columns.region];

	jack_nframes_t start = selection->time[clicked_selection].start;
	jack_nframes_t end = selection->time[clicked_selection].end;

	Playlist *playlist; 

	if (selection->tracks.empty()) {
		return;
	}

	jack_nframes_t selection_length = end - start;
	float times = (float)selection_length / region->length();
	
	begin_reversible_command (_("fill selection"));
	
	for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {

		if ((playlist = (*i)->playlist()) == 0) {
			continue;
		}		
		
		session->add_undo (playlist->get_memento());
		playlist->add_region (*(createRegion (*region)), start, times);
		session->add_redo_no_execute (playlist->get_memento());
	}
	
	commit_reversible_command ();			
}
	
void
Editor::set_region_sync_from_edit_cursor ()
{
	if (clicked_regionview == 0) {
		return;
	}

	if (!clicked_regionview->region.covers (edit_cursor->current_frame)) {
		error << _("Place the edit cursor at the desired sync point") << endmsg;
		return;
	}

	Region& region (clicked_regionview->region);
	begin_reversible_command (_("set sync from edit cursor"));
	session->add_undo (region.playlist()->get_memento());
	region.set_sync_position (edit_cursor->current_frame);
	session->add_redo_no_execute (region.playlist()->get_memento());
	commit_reversible_command ();
}

void
Editor::remove_region_sync ()
{
	if (clicked_regionview) {
		Region& region (clicked_regionview->region);
		begin_reversible_command (_("remove sync"));
		session->add_undo (region.playlist()->get_memento());
		region.clear_sync_position ();
		session->add_redo_no_execute (region.playlist()->get_memento());
		commit_reversible_command ();
	}
}

void
Editor::naturalize ()
{
	if (selection->audio_regions.empty()) {
		return;
	}
	begin_reversible_command (_("naturalize"));
	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
		session->add_undo ((*i)->region.get_memento());
		(*i)->region.move_to_natural_position (this);
		session->add_redo_no_execute ((*i)->region.get_memento());
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
	    return a->region.position() < b->region.position();
    }
};

void
Editor::align_selection_relative (RegionPoint point, jack_nframes_t position)
{
	if (selection->audio_regions.empty()) {
		return;
	}

	jack_nframes_t distance;
	jack_nframes_t pos = 0;
	int dir;

	list<AudioRegionView*> sorted;
	selection->audio_regions.by_position (sorted);
	Region& r ((*sorted.begin())->region);

	switch (point) {
	case Start:
		pos = r.first_frame ();
		break;

	case End:
		pos = r.last_frame();
		break;

	case SyncPoint:
		pos = r.adjust_to_sync (r.first_frame());
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

	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {

		Region& region ((*i)->region);

		session->add_undo (region.playlist()->get_memento());
		
		if (dir > 0) {
			region.set_position (region.position() + distance, this);
		} else {
			region.set_position (region.position() - distance, this);
		}

		session->add_redo_no_execute (region.playlist()->get_memento());

	}

	commit_reversible_command ();
}

void
Editor::align_selection (RegionPoint point, jack_nframes_t position)
{
	if (selection->audio_regions.empty()) {
		return;
	}

	begin_reversible_command (_("align selection"));

	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
		align_region_internal ((*i)->region, point, position);
	}

	commit_reversible_command ();
}

void
Editor::align_region (Region& region, RegionPoint point, jack_nframes_t position)
{
	begin_reversible_command (_("align region"));
	align_region_internal (region, point, position);
	commit_reversible_command ();
}

void
Editor::align_region_internal (Region& region, RegionPoint point, jack_nframes_t position)
{
	session->add_undo (region.playlist()->get_memento());

	switch (point) {
	case SyncPoint:
		region.set_position (region.adjust_to_sync (position), this);
		break;

	case End:
		if (position > region.length()) {
			region.set_position (position - region.length(), this);
		}
		break;

	case Start:
		region.set_position (position, this);
		break;
	}

	session->add_redo_no_execute (region.playlist()->get_memento());
}	

void
Editor::trim_region_to_edit_cursor ()
{
	if (clicked_regionview == 0) {
		return;
	}

	Region& region (clicked_regionview->region);

	float speed = 1.0f;
	AudioTimeAxisView *atav;

	if ( clicked_trackview != 0 && (atav = dynamic_cast<AudioTimeAxisView*>(clicked_trackview)) != 0 ) {
		if (atav->get_diskstream() != 0) {
			speed = atav->get_diskstream()->speed();
		}
	}

	begin_reversible_command (_("trim to edit"));
	session->add_undo (region.playlist()->get_memento());
	region.trim_end( session_frame_to_track_frame(edit_cursor->current_frame, speed), this);
	session->add_redo_no_execute (region.playlist()->get_memento());
	commit_reversible_command ();
}

void
Editor::trim_region_from_edit_cursor ()
{
	if (clicked_regionview == 0) {
		return;
	}

	Region& region (clicked_regionview->region);

	float speed = 1.0f;
	AudioTimeAxisView *atav;

	if ( clicked_trackview != 0 && (atav = dynamic_cast<AudioTimeAxisView*>(clicked_trackview)) != 0 ) {
		if (atav->get_diskstream() != 0) {
			speed = atav->get_diskstream()->speed();
		}
	}

	begin_reversible_command (_("trim to edit"));
	session->add_undo (region.playlist()->get_memento());
	region.trim_end( session_frame_to_track_frame(edit_cursor->current_frame, speed), this);
	session->add_redo_no_execute (region.playlist()->get_memento());
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
	
	interthread_progress_window->set_title (_("ardour: freeze"));
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

	pthread_create (&itt.thread, 0, _freeze_thread, this);

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

	TrackViewList *views = get_valid_views (selection->time.track, selection->time.group);

	jack_nframes_t start = selection->time[clicked_selection].start;
	jack_nframes_t end = selection->time[clicked_selection].end;
	jack_nframes_t cnt = end - start + 1;
	
	begin_reversible_command (_("bounce range"));

	for (TrackViewList::iterator i = views->begin(); i != views->end(); ++i) {

		AudioTimeAxisView* atv;

		if ((atv = dynamic_cast<AudioTimeAxisView*> (*i)) == 0) {
			continue;
		}
		
		Playlist* playlist;
		
		if ((playlist = atv->playlist()) == 0) {
			return;
		}

		InterThreadInfo itt;
		
		itt.done = false;
		itt.cancel = false;
		itt.progress = false;
		
		session->add_undo (playlist->get_memento());
		atv->audio_track()->bounce_range (start, cnt, itt);
		session->add_redo_no_execute (playlist->get_memento());
	}
	
	commit_reversible_command ();
	
	delete views;
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
		if (!selection->audio_regions.empty() || !selection->points.empty()) {

			begin_reversible_command (opname + _(" objects"));

			if (!selection->audio_regions.empty()) {
				
				cut_copy_regions (op);
				
				if (op == Cut) {
					selection->clear_audio_regions ();
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

void
Editor::cut_copy_regions (CutCopyOp op)
{
        typedef std::map<AudioPlaylist*,AudioPlaylist*> PlaylistMapping;
	PlaylistMapping pmap;
	jack_nframes_t first_position = max_frames;
	set<Playlist*> freezelist;
	pair<set<Playlist*>::iterator,bool> insert_result;

	for (AudioRegionSelection::iterator x = selection->audio_regions.begin(); x != selection->audio_regions.end(); ++x) {
		first_position = min ((*x)->region.position(), first_position);

		if (op == Cut || op == Clear) {
			AudioPlaylist *pl = dynamic_cast<AudioPlaylist*>((*x)->region.playlist());
			if (pl) {
				insert_result = freezelist.insert (pl);
				if (insert_result.second) {
					pl->freeze ();
					session->add_undo (pl->get_memento());
				}
			}
		}
	}

	for (AudioRegionSelection::iterator x = selection->audio_regions.begin(); x != selection->audio_regions.end(); ) {

		AudioPlaylist *pl = dynamic_cast<AudioPlaylist*>((*x)->region.playlist());
		AudioPlaylist* npl;
		AudioRegionSelection::iterator tmp;
		
		tmp = x;
		++tmp;

		if (pl) {

			PlaylistMapping::iterator pi = pmap.find (pl);
			
			if (pi == pmap.end()) {
				npl = new AudioPlaylist (*session, "cutlist", true);
				npl->freeze();
				pmap[pl] = npl;
			} else {
				npl = pi->second;
			}

			switch (op) {
			case Cut:
				npl->add_region (*(new AudioRegion ((*x)->region)), (*x)->region.position() - first_position);
				pl->remove_region (&((*x)->region));
				break;

			case Copy:
				npl->add_region (*(new AudioRegion ((*x)->region)), (*x)->region.position() - first_position);
				break;

			case Clear:
				pl->remove_region (&((*x)->region));
				break;
			}
		}

		x = tmp;
	}

	list<Playlist*> foo;

	for (PlaylistMapping::iterator i = pmap.begin(); i != pmap.end(); ++i) {
		foo.push_back (i->second);
	}

	if (!foo.empty()) {
		cut_buffer->set (foo);
	}
	
	for (set<Playlist*>::iterator pl = freezelist.begin(); pl != freezelist.end(); ++pl) {
		(*pl)->thaw ();
		session->add_redo_no_execute ((*pl)->get_memento());
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
	
	jack_nframes_t where = event_frame (&event, 0, 0);
	snap_to (where);
	paste_internal (where, 1);
}

void
Editor::paste_internal (jack_nframes_t position, float times)
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

	list<Playlist*>::iterator chunk;
	list<Playlist*>::iterator tmp;

	chunk = ns->playlists.begin();
		
	begin_reversible_command (_("paste chunk"));

	for (t = selection->tracks.begin(); t != selection->tracks.end(); ++t) {
		
		AudioTimeAxisView* atv;
		Playlist* pl;
		AudioPlaylist* apl;

		if ((atv = dynamic_cast<AudioTimeAxisView*> (*t)) == 0) {
			continue;
		}

		if ((pl = atv->playlist()) == 0) {
			continue;
		}

		if ((apl = dynamic_cast<AudioPlaylist*> (pl)) == 0) {
			continue;
		}

		tmp = chunk;
		++tmp;

		session->add_undo (apl->get_memento());
		apl->paste (**chunk, edit_cursor->current_frame, times);
		session->add_redo_no_execute (apl->get_memento());

		if (tmp != ns->playlists.end()) {
			chunk = tmp;
		}
	}

	commit_reversible_command();
}

void
Editor::duplicate_some_regions (AudioRegionSelection& regions, float times)
{
	Playlist *playlist; 
	AudioRegionSelection sel = regions; // clear (below) will clear the argument list
		
	begin_reversible_command (_("duplicate region"));

	selection->clear_audio_regions ();

	for (AudioRegionSelection::iterator i = sel.begin(); i != sel.end(); ++i) {

		Region& r ((*i)->region);

		TimeAxisView& tv = (*i)->get_time_axis_view();
		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (&tv);
		sigc::connection c = atv->view->AudioRegionViewAdded.connect (mem_fun(*this, &Editor::collect_new_region_view));
		
 		playlist = (*i)->region.playlist();
		session->add_undo (playlist->get_memento());
		playlist->duplicate (r, r.last_frame(), times);
		session->add_redo_no_execute (playlist->get_memento());

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

	Playlist *playlist; 
	vector<AudioRegion*> new_regions;
	vector<AudioRegion*>::iterator ri;
		
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
		session->add_undo (playlist->get_memento());
		playlist->duplicate (**ri, selection->time[clicked_selection].end, times);
		session->add_redo_no_execute (playlist->get_memento());

		++ri;
		if (ri == new_regions.end()) {
			--ri;
		}
	}

	commit_reversible_command ();
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
Editor::clear_playlist (Playlist& playlist)
{
	begin_reversible_command (_("clear playlist"));
	session->add_undo (playlist.get_memento());
	playlist.clear ();
	session->add_redo_no_execute (playlist.get_memento());
	commit_reversible_command ();
}

void
Editor::nudge_track (bool use_edit_cursor, bool forwards)
{
	Playlist *playlist; 
	jack_nframes_t distance;
	jack_nframes_t next_distance;
	jack_nframes_t start;

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
		
		session->add_undo (playlist->get_memento());
		playlist->nudge_after (start, distance, forwards);
		session->add_redo_no_execute (playlist->get_memento());
	}
	
	commit_reversible_command ();			
}

void
Editor::toggle_xfades_active ()
{
	if (session) {
		session->set_crossfades_active (!session->get_crossfades_active());
	}
}

void
Editor::set_xfade_visibility (bool yn)
{
	
}

void
Editor::toggle_xfade_visibility ()
{
	set_xfade_visibility (!xfade_visibility());
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

		choices.push_back (_("Yes, destroy it."));
		choices.push_back (_("No, do nothing."));
		
		Gtkmm2ext::Choice prompter (prompt, choices);
		prompter.chosen.connect (ptr_fun (Main::quit));
		prompter.show_all ();

		Main::run ();
		
		if (prompter.get_choice() == 0) {
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

	if (selection->audio_regions.empty()) {
		return;
	}

	begin_reversible_command (_("normalize"));

	track_canvas.get_window()->set_cursor (*wait_cursor);
	gdk_flush ();

	for (AudioRegionSelection::iterator r = selection->audio_regions.begin(); r != selection->audio_regions.end(); ++r) {
		session->add_undo ((*r)->region.get_memento());
		(*r)->region.normalize_to (0.0f);
		session->add_redo_no_execute ((*r)->region.get_memento());
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

	if (selection->audio_regions.empty()) {
		return;
	}

	begin_reversible_command ("denormalize");

	for (AudioRegionSelection::iterator r = selection->audio_regions.begin(); r != selection->audio_regions.end(); ++r) {
		session->add_undo ((*r)->region.get_memento());
		(*r)->region.set_scale_amplitude (1.0f);
		session->add_redo_no_execute ((*r)->region.get_memento());
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
	if (selection->audio_regions.empty()) {
		return;
	}

	begin_reversible_command (command);

	track_canvas.get_window()->set_cursor (*wait_cursor);
	gdk_flush ();

	for (AudioRegionSelection::iterator r = selection->audio_regions.begin(); r != selection->audio_regions.end(); ) {

		AudioRegion& region ((*r)->region);
		Playlist* playlist = region.playlist();

		AudioRegionSelection::iterator tmp;
		
		tmp = r;
		++tmp;

		if (region.apply (filter) == 0) {

			session->add_undo (playlist->get_memento());
			playlist->replace_region (region, *(filter.results.front()), region.position());
			session->add_redo_no_execute (playlist->get_memento());
		} else {
			goto out;
		}

		r = tmp;
	}

	commit_reversible_command ();
	selection->audio_regions.clear ();

  out:
	track_canvas.get_window()->set_cursor (*current_canvas_cursor);
}

void
Editor::region_selection_op (void (Region::*pmf)(void))
{
	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
		((*i)->region.*pmf)();
	}
}


void
Editor::region_selection_op (void (Region::*pmf)(void*), void *arg)
{
	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
		((*i)->region.*pmf)(arg);
	}
}

void
Editor::region_selection_op (void (Region::*pmf)(bool), bool yn)
{
	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
		((*i)->region.*pmf)(yn);
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
Editor::brush (jack_nframes_t pos)
{
	AudioRegionSelection sel;
	snap_to (pos);

	if (selection->audio_regions.empty()) {
		/* XXX get selection from region list */
	} else { 
		sel = selection->audio_regions;
	}

	if (sel.empty()) {
		return;
	}

	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
		mouse_brush_insert_region ((*i), pos);
	}
}

void
Editor::toggle_gain_envelope_visibility ()
{
	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
		(*i)->set_envelope_visible (!(*i)->envelope_visible());
	}
}

void
Editor::toggle_gain_envelope_active ()
{
	for (AudioRegionSelection::iterator i = selection->audio_regions.begin(); i != selection->audio_regions.end(); ++i) {
		AudioRegion* ar = dynamic_cast<AudioRegion*>(&(*i)->region);
		if (ar) {
			ar->set_envelope_active (true);
		}
	}
}
