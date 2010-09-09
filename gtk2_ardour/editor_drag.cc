/*
    Copyright (C) 2009 Paul Davis

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

#define __STDC_LIMIT_MACROS 1
#include <stdint.h>
#include "pbd/memento_command.h"
#include "pbd/basename.h"
#include "pbd/stateful_diff_command.h"
#include "ardour/session.h"
#include "ardour/dB.h"
#include "ardour/region_factory.h"
#include "editor.h"
#include "i18n.h"
#include "keyboard.h"
#include "audio_region_view.h"
#include "midi_region_view.h"
#include "ardour_ui.h"
#include "gui_thread.h"
#include "control_point.h"
#include "utils.h"
#include "region_gain_line.h"
#include "editor_drag.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"
#include "canvas-note.h"
#include "selection.h"
#include "midi_selection.h"
#include "automation_time_axis.h"
#include "debug.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;
using namespace ArdourCanvas;

using Gtkmm2ext::Keyboard;

double const ControlPointDrag::_zero_gain_fraction = gain_to_slider_position (dB_to_coefficient (0.0));

DragManager::DragManager (Editor* e)
	: _editor (e)
	, _ending (false)
	, _current_pointer_frame (0)
{

}

DragManager::~DragManager ()
{
	abort ();
}

/** Call abort for each active drag */
void
DragManager::abort ()
{
	_ending = true;
	
	for (list<Drag*>::const_iterator i = _drags.begin(); i != _drags.end(); ++i) {
		(*i)->abort ();
		delete *i;
	}

	_drags.clear ();

	_ending = false;
}

void
DragManager::add (Drag* d)
{
	d->set_manager (this);
	_drags.push_back (d);
}

void
DragManager::set (Drag* d, GdkEvent* e, Gdk::Cursor* c)
{
	assert (_drags.empty ());
	d->set_manager (this);
	_drags.push_back (d);
	start_grab (e, c);
}

void
DragManager::start_grab (GdkEvent* e, Gdk::Cursor* c)
{
	_current_pointer_frame = _editor->event_frame (e, &_current_pointer_x, &_current_pointer_y);
	
	for (list<Drag*>::const_iterator i = _drags.begin(); i != _drags.end(); ++i) {
		(*i)->start_grab (e, c);
	}
}

/** Call end_grab for each active drag.
 *  @return true if any drag reported movement having occurred.
 */
bool
DragManager::end_grab (GdkEvent* e)
{
	_ending = true;
	
	bool r = false;
	for (list<Drag*>::iterator i = _drags.begin(); i != _drags.end(); ++i) {
		bool const t = (*i)->end_grab (e);
		if (t) {
			r = true;
		}
		delete *i;
	}

	_drags.clear ();

	_ending = false;
	
	return r;
}

bool
DragManager::motion_handler (GdkEvent* e, bool from_autoscroll)
{
	bool r = false;

	_current_pointer_frame = _editor->event_frame (e, &_current_pointer_x, &_current_pointer_y);
	
	for (list<Drag*>::iterator i = _drags.begin(); i != _drags.end(); ++i) {
		bool const t = (*i)->motion_handler (e, from_autoscroll);
		if (t) {
			r = true;
		}
		
	}

	return r;
}

bool
DragManager::have_item (ArdourCanvas::Item* i) const
{
	list<Drag*>::const_iterator j = _drags.begin ();
	while (j != _drags.end() && (*j)->item () != i) {
		++j;
	}

	return j != _drags.end ();
}

Drag::Drag (Editor* e, ArdourCanvas::Item* i) 
	: _editor (e)
	, _item (i)
	, _pointer_frame_offset (0)
	, _move_threshold_passed (false)
	, _raw_grab_frame (0)
	, _grab_frame (0)
	, _last_pointer_frame (0)
{

}

void
Drag::swap_grab (ArdourCanvas::Item* new_item, Gdk::Cursor* cursor, uint32_t time)
{
	_item->ungrab (0);
	_item = new_item;

	if (cursor == 0) {
		cursor = _editor->which_grabber_cursor ();
	}

	_item->grab (Gdk::POINTER_MOTION_MASK | Gdk::BUTTON_PRESS_MASK | Gdk::BUTTON_RELEASE_MASK, *cursor, time);
}

void
Drag::start_grab (GdkEvent* event, Gdk::Cursor *cursor)
{
	if (cursor == 0) {
		cursor = _editor->which_grabber_cursor ();
	}

	// if dragging with button2, the motion is x constrained, with Alt-button2 it is y constrained

	if (Keyboard::is_button2_event (&event->button)) {
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::SecondaryModifier)) {
			_y_constrained = true;
			_x_constrained = false;
		} else {
			_y_constrained = false;
			_x_constrained = true;
		}
	} else {
		_x_constrained = false;
		_y_constrained = false;
	}

	_raw_grab_frame = _editor->event_frame (event, &_grab_x, &_grab_y);
	_grab_frame = adjusted_frame (_raw_grab_frame, event);
	_last_pointer_frame = _grab_frame;
	_last_pointer_x = _grab_x;
	_last_pointer_y = _grab_y;

	_item->grab (Gdk::POINTER_MOTION_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK,
                     *cursor,
                     event->button.time);

	if (_editor->session() && _editor->session()->transport_rolling()) {
		_was_rolling = true;
	} else {
		_was_rolling = false;
	}

	switch (_editor->snap_type()) {
	case SnapToRegionStart:
	case SnapToRegionEnd:
	case SnapToRegionSync:
	case SnapToRegionBoundary:
		_editor->build_region_boundary_cache ();
		break;
	default:
		break;
	}
}

/** Call to end a drag `successfully'.  Ungrabs item and calls
 *  subclass' finished() method.
 *
 *  @param event GDK event, or 0.
 *  @return true if some movement occurred, otherwise false.
 */
bool
Drag::end_grab (GdkEvent* event)
{
	_editor->stop_canvas_autoscroll ();

	_item->ungrab (event ? event->button.time : 0);

	finished (event, _move_threshold_passed);

	_editor->hide_verbose_canvas_cursor();

	return _move_threshold_passed;
}

framepos_t
Drag::adjusted_frame (framepos_t f, GdkEvent const * event, bool snap) const
{
	framepos_t pos = 0;

	if (f > _pointer_frame_offset) {
		pos = f - _pointer_frame_offset;
	}

	if (snap) {
		_editor->snap_to_with_modifier (pos, event);
	}

	return pos;
}

framepos_t
Drag::adjusted_current_frame (GdkEvent const * event, bool snap) const
{
	return adjusted_frame (_drags->current_pointer_frame (), event, snap);
}

bool
Drag::motion_handler (GdkEvent* event, bool from_autoscroll)
{
	/* check to see if we have moved in any way that matters since the last motion event */
	if ( (!x_movement_matters() || _last_pointer_frame == adjusted_current_frame (event)) &&
	     (!y_movement_matters() || _last_pointer_y == _drags->current_pointer_y ()) ) {
		return false;
	}

	pair<framecnt_t, int> const threshold = move_threshold ();

	bool const old_move_threshold_passed = _move_threshold_passed;

	if (!from_autoscroll && !_move_threshold_passed) {

		bool const xp = (::llabs (adjusted_current_frame (event) - _grab_frame) >= threshold.first);
		bool const yp = (::fabs ((_drags->current_pointer_y () - _grab_y)) >= threshold.second);

		_move_threshold_passed = ((xp && x_movement_matters()) || (yp && y_movement_matters()));
	}

	if (active (_editor->mouse_mode) && _move_threshold_passed) {

		if (event->motion.state & Gdk::BUTTON1_MASK || event->motion.state & Gdk::BUTTON2_MASK) {
			if (!from_autoscroll) {
				_editor->maybe_autoscroll (true, allow_vertical_autoscroll ());
			}

			motion (event, _move_threshold_passed != old_move_threshold_passed);

			_last_pointer_x = _drags->current_pointer_x ();
			_last_pointer_y = _drags->current_pointer_y ();
			_last_pointer_frame = adjusted_current_frame (event);
			
			return true;
		}
	}
	return false;
}

/** Call to abort a drag.  Ungrabs item and calls subclass's aborted () */
void
Drag::abort ()
{
	if (_item) {
		_item->ungrab (0);
	}

	aborted ();

	_editor->stop_canvas_autoscroll ();
	_editor->hide_verbose_canvas_cursor ();
}

struct EditorOrderTimeAxisViewSorter {
    bool operator() (TimeAxisView* a, TimeAxisView* b) {
	    RouteTimeAxisView* ra = dynamic_cast<RouteTimeAxisView*> (a);
	    RouteTimeAxisView* rb = dynamic_cast<RouteTimeAxisView*> (b);
	    assert (ra && rb);
	    return ra->route()->order_key (N_ ("editor")) < rb->route()->order_key (N_ ("editor"));
    }
};

RegionDrag::RegionDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: Drag (e, i),
	  _primary (p)
{
	_editor->visible_order_range (&_visible_y_low, &_visible_y_high);

	/* Make a list of non-hidden tracks to refer to during the drag */

	TrackViewList track_views = _editor->track_views;
	track_views.sort (EditorOrderTimeAxisViewSorter ());

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		if (!(*i)->hidden()) {
			
			_time_axis_views.push_back (*i);

			TimeAxisView::Children children_list = (*i)->get_child_list ();
			for (TimeAxisView::Children::iterator j = children_list.begin(); j != children_list.end(); ++j) {
				_time_axis_views.push_back (j->get());
			}
		}
	}
	
	for (list<RegionView*>::const_iterator i = v.begin(); i != v.end(); ++i) {
		_views.push_back (DraggingView (*i, this));
	}
	
	RegionView::RegionViewGoingAway.connect (death_connection, invalidator (*this), ui_bind (&RegionDrag::region_going_away, this, _1), gui_context());
}

void
RegionDrag::region_going_away (RegionView* v)
{
	list<DraggingView>::iterator i = _views.begin ();
	while (i != _views.end() && i->view != v) {
		++i;
	}

	if (i != _views.end()) {
		_views.erase (i);
	}
}

/** Given a non-hidden TimeAxisView, return the index of it into the _time_axis_views vector */
int
RegionDrag::find_time_axis_view (TimeAxisView* t) const
{
	int i = 0;
	int const N = _time_axis_views.size ();
	while (i < N && _time_axis_views[i] != t) {
		++i;
	}

	if (i == N) {
		return -1;
	}
	
	return i;
}

RegionMotionDrag::RegionMotionDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v, bool b)
	: RegionDrag (e, i, p, v),
	  _brushing (b),
	  _total_x_delta (0)
{

}


void
RegionMotionDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	_editor->show_verbose_time_cursor (_last_frame_position, 10);

	pair<TimeAxisView*, int> const tv = _editor->trackview_by_y_position (_drags->current_pointer_y ());
	_last_pointer_time_axis_view = find_time_axis_view (tv.first);
	_last_pointer_layer = tv.first->layer_display() == Overlaid ? 0 : tv.second;
}

double
RegionMotionDrag::compute_x_delta (GdkEvent const * event, framepos_t* pending_region_position)
{
	/* compute the amount of pointer motion in frames, and where
	   the region would be if we moved it by that much.
	*/
	*pending_region_position = adjusted_current_frame (event);

	framepos_t sync_frame;
	framecnt_t sync_offset;
	int32_t sync_dir;
	
	sync_offset = _primary->region()->sync_offset (sync_dir);
	
	/* we don't handle a sync point that lies before zero.
	 */
	if (sync_dir >= 0 || (sync_dir < 0 && *pending_region_position >= sync_offset)) {
		
		sync_frame = *pending_region_position + (sync_dir*sync_offset);
		
		_editor->snap_to_with_modifier (sync_frame, event);
		
		*pending_region_position = _primary->region()->adjust_to_sync (sync_frame);
		
	} else {
		*pending_region_position = _last_frame_position;
	}

	if (*pending_region_position > max_frames - _primary->region()->length()) {
		*pending_region_position = _last_frame_position;
	}

	double dx = 0;

	/* in locked edit mode, reverse the usual meaning of _x_constrained */
	bool const x_move_allowed = Config->get_edit_mode() == Lock ? _x_constrained : !_x_constrained;

	if ((*pending_region_position != _last_frame_position) && x_move_allowed) {

		/* x movement since last time */
		dx = (static_cast<double> (*pending_region_position) - _last_frame_position) / _editor->frames_per_unit;

		/* total x movement */
		framecnt_t total_dx = *pending_region_position;
		if (regions_came_from_canvas()) {
			total_dx = total_dx - grab_frame () + _pointer_frame_offset;
		}

		/* check that no regions have gone off the start of the session */
		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			if ((i->view->region()->position() + total_dx) < 0) {
				dx = 0;
				*pending_region_position = _last_frame_position;
				break;
			}
		}

		_last_frame_position = *pending_region_position;
	}

	return dx;
}

bool
RegionMotionDrag::y_movement_allowed (int delta_track, layer_t delta_layer) const
{
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		int const n = i->time_axis_view + delta_track;
		if (n < 0 || n >= int (_time_axis_views.size())) {
			/* off the top or bottom track */
			return false;
		}

		RouteTimeAxisView const * to = dynamic_cast<RouteTimeAxisView const *> (_time_axis_views[n]);
		if (to == 0 || !to->is_track() || to->track()->data_type() != i->view->region()->data_type()) {
			/* not a track, or the wrong type */
			return false;
		}
		
		int const l = i->layer + delta_layer;
		if (delta_track == 0 && (l < 0 || l >= int (to->view()->layers()))) {
			/* Off the top or bottom layer; note that we only refuse if the track hasn't changed.
			   If it has, the layers will be munged later anyway, so it's ok.
			*/
			return false;
		}
	}

	/* all regions being dragged are ok with this change */
	return true;
}

void
RegionMotionDrag::motion (GdkEvent* event, bool first_move)
{
	/* Find the TimeAxisView that the pointer is now over */
	pair<TimeAxisView*, int> const tv = _editor->trackview_by_y_position (_drags->current_pointer_y ());

	/* Bail early if we're not over a track */
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv.first);
	if (!rtv || !rtv->is_track()) {
		_editor->hide_verbose_canvas_cursor ();
		return;
	}

	/* Note: time axis views in this method are often expressed as an index into the _time_axis_views vector */

	/* Here's the current pointer position in terms of time axis view and layer */
	int const current_pointer_time_axis_view = find_time_axis_view (tv.first);
	layer_t const current_pointer_layer = tv.first->layer_display() == Overlaid ? 0 : tv.second;

	/* Work out the change in x */
	framepos_t pending_region_position;
	double const x_delta = compute_x_delta (event, &pending_region_position);

	/* Work out the change in y */
	int delta_time_axis_view = current_pointer_time_axis_view - _last_pointer_time_axis_view;
	int delta_layer = current_pointer_layer - _last_pointer_layer;

	if (!y_movement_allowed (delta_time_axis_view, delta_layer)) {
		/* this y movement is not allowed, so do no y movement this time */
		delta_time_axis_view = 0;
		delta_layer = 0;
	}

	if (x_delta == 0 && delta_time_axis_view == 0 && delta_layer == 0 && !first_move) {
		/* haven't reached next snap point, and we're not switching
		   trackviews nor layers. nothing to do.
		*/
		return;
	}

	pair<set<boost::shared_ptr<Playlist> >::iterator,bool> insert_result;

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		RegionView* rv = i->view;

		if (rv->region()->locked()) {
			continue;
		}

		if (first_move) {

			/* here we are calculating the y distance from the
			   top of the first track view to the top of the region
			   area of the track view that we're working on */
			
			/* this x value is just a dummy value so that we have something
			   to pass to i2w () */
			
			double ix1 = 0;
			
			/* distance from the top of this track view to the region area
			   of our track view is always 1 */
			
			double iy1 = 1;
			
			/* convert to world coordinates, ie distance from the top of
			   the ruler section */
			
			rv->get_canvas_frame()->i2w (ix1, iy1);
			
			/* compensate for the ruler section and the vertical scrollbar position */
			iy1 += _editor->get_trackview_group_vertical_offset ();
			
			// hide any dependent views
			
			rv->get_time_axis_view().hide_dependent_views (*rv);
			
			/*
			  reparent to a non scrolling group so that we can keep the
			  region selection above all time axis views.
			  reparenting means we have to move the rv as the two
			  parent groups have different coordinates.
			*/
			
			rv->get_canvas_group()->property_y() = iy1 - 1;
			rv->get_canvas_group()->reparent (*(_editor->_region_motion_group));
			
			rv->fake_set_opaque (true);
		}

		/* Work out the change in y position of this region view */

		double y_delta = 0;

		/* If we have moved tracks, we'll fudge the layer delta so that the
		   region gets moved back onto layer 0 on its new track; this avoids
		   confusion when dragging regions from non-zero layers onto different
		   tracks.
		*/
		int this_delta_layer = delta_layer;
		if (delta_time_axis_view != 0) {
			this_delta_layer = - i->layer;
		}

		/* Move this region to layer 0 on its old track */
		StreamView* lv = _time_axis_views[i->time_axis_view]->view ();
		if (lv->layer_display() == Stacked) {
			y_delta -= (lv->layers() - i->layer - 1) * lv->child_height ();
		}

		/* Now move it to its right layer on the current track */
		StreamView* cv = _time_axis_views[i->time_axis_view + delta_time_axis_view]->view ();
		if (cv->layer_display() == Stacked) {
			y_delta += (cv->layers() - (i->layer + this_delta_layer) - 1) * cv->child_height ();
		}

		/* Move tracks */
		if (delta_time_axis_view > 0) {
			for (int j = 0; j < delta_time_axis_view; ++j) {
				y_delta += _time_axis_views[i->time_axis_view + j]->current_height ();
			}
		} else {
			/* start by subtracting the height of the track above where we are now */
			for (int j = 1; j <= -delta_time_axis_view; ++j) {
				y_delta -= _time_axis_views[i->time_axis_view - j]->current_height ();
			}
		}

		/* Set height */
		rv->set_height (_time_axis_views[i->time_axis_view + delta_time_axis_view]->view()->child_height ());

		/* Update the DraggingView */
		i->time_axis_view += delta_time_axis_view;
		i->layer += this_delta_layer;

		if (_brushing) {
			_editor->mouse_brush_insert_region (rv, pending_region_position);
		} else {
			rv->move (x_delta, y_delta);
		}

	} /* foreach region */

	_total_x_delta += x_delta;
	
	if (first_move) {
		_editor->cursor_group->raise_to_top();
	}

	if (x_delta != 0 && !_brushing) {
		_editor->show_verbose_time_cursor (_last_frame_position, 10);
	}

	_last_pointer_time_axis_view += delta_time_axis_view;
	_last_pointer_layer += delta_layer;
}

void
RegionMoveDrag::motion (GdkEvent* event, bool first_move)
{
	if (_copy && first_move) {

		/* duplicate the regionview(s) and region(s) */

		list<DraggingView> new_regionviews;

		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			
			RegionView* rv = i->view;
			AudioRegionView* arv = dynamic_cast<AudioRegionView*>(rv);
			MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(rv);

			const boost::shared_ptr<const Region> original = rv->region();
			boost::shared_ptr<Region> region_copy = RegionFactory::create (original);
			region_copy->set_position (original->position(), this);
			
			RegionView* nrv;
			if (arv) {
				boost::shared_ptr<AudioRegion> audioregion_copy
				= boost::dynamic_pointer_cast<AudioRegion>(region_copy);
				
				nrv = new AudioRegionView (*arv, audioregion_copy);
			} else if (mrv) {
				boost::shared_ptr<MidiRegion> midiregion_copy
					= boost::dynamic_pointer_cast<MidiRegion>(region_copy);
				nrv = new MidiRegionView (*mrv, midiregion_copy);
			} else {
				continue;
			}
			
			nrv->get_canvas_group()->show ();
			new_regionviews.push_back (DraggingView (nrv, this));
			
			/* swap _primary to the copy */
			
			if (rv == _primary) {
				_primary = nrv;
			}
			
			/* ..and deselect the one we copied */
			
			rv->set_selected (false);
		}
		
		if (!new_regionviews.empty()) {
			
			/* reflect the fact that we are dragging the copies */
			
			_views = new_regionviews;
			
			swap_grab (new_regionviews.front().view->get_canvas_group (), 0, event ? event->motion.time : 0);
			
			/*
			  sync the canvas to what we think is its current state
			  without it, the canvas seems to
			  "forget" to update properly after the upcoming reparent()
			  ..only if the mouse is in rapid motion at the time of the grab.
			  something to do with regionview creation taking so long?
			*/
			_editor->update_canvas_now();
		}
	}

	RegionMotionDrag::motion (event, first_move);
}

void
RegionMoveDrag::finished (GdkEvent *, bool movement_occurred)
{
	if (!movement_occurred) {
		/* just a click */
		return;
	}

	/* reverse this here so that we have the correct logic to finalize
	   the drag.
	*/

	if (Config->get_edit_mode() == Lock) {
		_x_constrained = !_x_constrained;
	}

	bool const changed_position = (_last_frame_position != _primary->region()->position());
	bool const changed_tracks = (_time_axis_views[_views.front().time_axis_view] != &_views.front().view->get_time_axis_view());
	framecnt_t const drag_delta = _primary->region()->position() - _last_frame_position;

	_editor->update_canvas_now ();
	
	if (_copy) {
		
		finished_copy (
			changed_position,
			changed_tracks,
			drag_delta
			);
		
	} else {
		
		finished_no_copy (
			changed_position,
			changed_tracks,
			drag_delta
			);
		
	}
}

void
RegionMoveDrag::finished_copy (
	bool const changed_position,
	bool const changed_tracks,
	framecnt_t const drag_delta
	)
{
	RegionSelection new_views;
	PlaylistSet modified_playlists;
	list<RegionView*> views_to_delete;

	if (_brushing) {
		/* all changes were made during motion event handlers */

		for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
			delete i->view;
		}

		_editor->commit_reversible_command ();
		return;
	}

	if (_x_constrained) {
		_editor->begin_reversible_command (_("fixed time region copy"));
	} else {
		_editor->begin_reversible_command (_("region copy"));
	}

	/* insert the regions into their new playlists */
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {

		if (i->view->region()->locked()) {
			continue;
		}

		framepos_t where;

		if (changed_position && !_x_constrained) {
			where = i->view->region()->position() - drag_delta;
		} else {
			where = i->view->region()->position();
		}

		RegionView* new_view = insert_region_into_playlist (
			i->view->region(), dynamic_cast<RouteTimeAxisView*> (_time_axis_views[i->time_axis_view]), i->layer, where, modified_playlists
			);
		
		if (new_view == 0) {
			continue;
		}

		new_views.push_back (new_view);
		
		/* we don't need the copied RegionView any more */
		views_to_delete.push_back (i->view);
	}

	/* Delete views that are no longer needed; we can't do this directly in the iteration over _views
	   because when views are deleted they are automagically removed from _views, which messes
	   up the iteration.
	*/
	for (list<RegionView*>::iterator i = views_to_delete.begin(); i != views_to_delete.end(); ++i) {
		delete *i;
	}

	/* If we've created new regions either by copying or moving 
	   to a new track, we want to replace the old selection with the new ones 
	*/

	if (new_views.size() > 0) {
		_editor->selection->set (new_views);
	}

	/* write commands for the accumulated diffs for all our modified playlists */
	add_stateful_diff_commands_for_playlists (modified_playlists);

	_editor->commit_reversible_command ();
}

void
RegionMoveDrag::finished_no_copy (
	bool const changed_position,
	bool const changed_tracks,
	framecnt_t const drag_delta
	)
{
	RegionSelection new_views;
	PlaylistSet modified_playlists;
	PlaylistSet frozen_playlists;

	if (_brushing) {
		/* all changes were made during motion event handlers */
		_editor->commit_reversible_command ();
		return;
	}

	if (_x_constrained) {
		_editor->begin_reversible_command (_("fixed time region drag"));
	} else {
		_editor->begin_reversible_command (_("region drag"));
	}

	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ) {

		RegionView* rv = i->view;

		RouteTimeAxisView* const dest_rtv = dynamic_cast<RouteTimeAxisView*> (_time_axis_views[i->time_axis_view]);
		layer_t const dest_layer = i->layer;

		if (rv->region()->locked()) {
			++i;
			continue;
		}

		framepos_t where;

		if (changed_position && !_x_constrained) {
			where = rv->region()->position() - drag_delta;
		} else {
			where = rv->region()->position();
		}

		if (changed_tracks) {

			/* insert into new playlist */

			RegionView* new_view = insert_region_into_playlist (
				RegionFactory::create (rv->region ()), dest_rtv, dest_layer, where, modified_playlists
				);

			if (new_view == 0) {
				++i;
				continue;
			}

			new_views.push_back (new_view);

			/* remove from old playlist */

			/* the region that used to be in the old playlist is not
			   moved to the new one - we use a copy of it. as a result,
			   any existing editor for the region should no longer be
			   visible.
			*/
			rv->hide_region_editor();
			rv->fake_set_opaque (false);

			remove_region_from_playlist (rv->region(), i->initial_playlist, modified_playlists);

		} else {
			
			rv->region()->clear_changes ();

			/*
			   motion on the same track. plonk the previously reparented region
			   back to its original canvas group (its streamview).
			   No need to do anything for copies as they are fake regions which will be deleted.
			*/

			rv->get_canvas_group()->reparent (*dest_rtv->view()->canvas_item());
			rv->get_canvas_group()->property_y() = i->initial_y;
			rv->get_time_axis_view().reveal_dependent_views (*rv);

			/* just change the model */

			boost::shared_ptr<Playlist> playlist = dest_rtv->playlist();
			
			if (dest_rtv->view()->layer_display() == Stacked) {
				rv->region()->set_layer (dest_layer);
				rv->region()->set_pending_explicit_relayer (true);
			}
			
			/* freeze playlist to avoid lots of relayering in the case of a multi-region drag */

			pair<PlaylistSet::iterator, bool> r = frozen_playlists.insert (playlist);

			if (r.second) {
				playlist->freeze ();
			}

			/* this movement may result in a crossfade being modified, so we need to get undo
			   data from the playlist as well as the region.
			*/
			
			r = modified_playlists.insert (playlist);
			if (r.second) {
				playlist->clear_changes ();
			}

			rv->region()->set_position (where, (void*) this);

			_editor->session()->add_command (new StatefulDiffCommand (rv->region()));
		}

		if (changed_tracks) {
			
			/* OK, this is where it gets tricky. If the playlist was being used by >1 tracks, and the region
			   was selected in all of them, then removing it from a playlist will have removed all
			   trace of it from _views (i.e. there were N regions selected, we removed 1,
			   but since its the same playlist for N tracks, all N tracks updated themselves, removed the
			   corresponding regionview, and _views is now empty).

			   This could have invalidated any and all iterators into _views.

			   The heuristic we use here is: if the region selection is empty, break out of the loop
			   here. if the region selection is not empty, then restart the loop because we know that
			   we must have removed at least the region(view) we've just been working on as well as any
			   that we processed on previous iterations.

			   EXCEPT .... if we are doing a copy drag, then _views hasn't been modified and
			   we can just iterate.
			*/

			
			if (_views.empty()) {
                                break;
			} else {
				i = _views.begin();
			}

		} else {
			++i;
		}
	}

	/* If we've created new regions either by copying or moving 
	   to a new track, we want to replace the old selection with the new ones 
	*/

	if (new_views.size() > 0) {
		_editor->selection->set (new_views);
	}

	for (set<boost::shared_ptr<Playlist> >::iterator p = frozen_playlists.begin(); p != frozen_playlists.end(); ++p) {
		(*p)->thaw();
	}

	/* write commands for the accumulated diffs for all our modified playlists */
	add_stateful_diff_commands_for_playlists (modified_playlists);

	_editor->commit_reversible_command ();
}

/** Remove a region from a playlist, clearing the diff history of the playlist first if necessary.
 *  @param region Region to remove.
 *  @param playlist playlist To remove from.
 *  @param modified_playlists The playlist will be added to this if it is not there already; used to ensure
 *  that clear_changes () is only called once per playlist.
 */
void
RegionMoveDrag::remove_region_from_playlist (
	boost::shared_ptr<Region> region,
	boost::shared_ptr<Playlist> playlist,
	PlaylistSet& modified_playlists
	)
{
	pair<set<boost::shared_ptr<Playlist> >::iterator, bool> r = modified_playlists.insert (playlist);

	if (r.second) {
		playlist->clear_changes ();
	}

	playlist->remove_region (region);
}
	

/** Insert a region into a playlist, handling the recovery of the resulting new RegionView, and
 *  clearing the playlist's diff history first if necessary.
 *  @param region Region to insert.
 *  @param dest_rtv Destination RouteTimeAxisView.
 *  @param dest_layer Destination layer.
 *  @param where Destination position.
 *  @param modified_playlists The playlist will be added to this if it is not there already; used to ensure
 *  that clear_changes () is only called once per playlist. 
 *  @return New RegionView, or 0 if no insert was performed.
 */
RegionView *
RegionMoveDrag::insert_region_into_playlist (
	boost::shared_ptr<Region> region,
	RouteTimeAxisView* dest_rtv,
	layer_t dest_layer,
	framecnt_t where,
	PlaylistSet& modified_playlists
	)
{
	boost::shared_ptr<Playlist> dest_playlist = dest_rtv->playlist ();
	if (!dest_playlist) {
		return 0;
	}

	/* arrange to collect the new region view that will be created as a result of our playlist insertion */
	_new_region_view = 0;
	sigc::connection c = dest_rtv->view()->RegionViewAdded.connect (sigc::mem_fun (*this, &RegionMoveDrag::collect_new_region_view));

	/* clear history for the playlist we are about to insert to, provided we haven't already done so */	
	pair<PlaylistSet::iterator, bool> r = modified_playlists.insert (dest_playlist);
	if (r.second) {
		dest_playlist->clear_changes ();
	}

	dest_playlist->add_region (region, where);

	if (dest_rtv->view()->layer_display() == Stacked) {
		region->set_layer (dest_layer);
		region->set_pending_explicit_relayer (true);
	}

	c.disconnect ();

	assert (_new_region_view);

	return _new_region_view;
}

void
RegionMoveDrag::collect_new_region_view (RegionView* rv)
{
	_new_region_view = rv;
}

void
RegionMoveDrag::add_stateful_diff_commands_for_playlists (PlaylistSet const & playlists)
{
	for (PlaylistSet::const_iterator i = playlists.begin(); i != playlists.end(); ++i) {
		StatefulDiffCommand* c = new StatefulDiffCommand (*i);
		if (!c->empty()) {
			_editor->session()->add_command (new StatefulDiffCommand (*i));
		} else {
			delete c;
		}
	}
}


void
RegionMoveDrag::aborted ()
{
	if (_copy) {

		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			delete i->view;
		}

		_views.clear ();

	} else {
		RegionMotionDrag::aborted ();
	}
}

void
RegionMotionDrag::aborted ()
{
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		RegionView* rv = i->view;
		TimeAxisView* tv = &(rv->get_time_axis_view ());
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);
		assert (rtv);
		rv->get_canvas_group()->reparent (*rtv->view()->canvas_item());
		rv->get_canvas_group()->property_y() = 0;
		rv->get_time_axis_view().reveal_dependent_views (*rv);
		rv->fake_set_opaque (false);
		rv->move (-_total_x_delta, 0);
		rv->set_height (rtv->view()->child_height ());
	}

	_editor->update_canvas_now ();
}
				      
RegionMoveDrag::RegionMoveDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v, bool b, bool c)
	: RegionMotionDrag (e, i, p, v, b),
	  _copy (c)
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionMoveDrag\n");
	
	double speed = 1;
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (&_primary->get_time_axis_view ());
	if (rtv && rtv->is_track()) {
		speed = rtv->track()->speed ();
	}

	_last_frame_position = static_cast<framepos_t> (_primary->region()->position() / speed);
}

void
RegionMoveDrag::start_grab (GdkEvent* event, Gdk::Cursor* c)
{
	RegionMotionDrag::start_grab (event, c);

	_pointer_frame_offset = raw_grab_frame() - _last_frame_position;
}

RegionInsertDrag::RegionInsertDrag (Editor* e, boost::shared_ptr<Region> r, RouteTimeAxisView* v, framepos_t pos)
	: RegionMotionDrag (e, 0, 0, list<RegionView*> (), false)
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionInsertDrag\n");
	
	assert ((boost::dynamic_pointer_cast<AudioRegion> (r) && dynamic_cast<AudioTimeAxisView*> (v)) ||
		(boost::dynamic_pointer_cast<MidiRegion> (r) && dynamic_cast<MidiTimeAxisView*> (v)));

	_primary = v->view()->create_region_view (r, false, false);

	_primary->get_canvas_group()->show ();
	_primary->set_position (pos, 0);
	_views.push_back (DraggingView (_primary, this));

	_last_frame_position = pos;

	_item = _primary->get_canvas_group ();
}

void
RegionInsertDrag::finished (GdkEvent *, bool)
{
	_editor->update_canvas_now ();

	RouteTimeAxisView* dest_rtv = dynamic_cast<RouteTimeAxisView*> (_time_axis_views[_views.front().time_axis_view]);

	_primary->get_canvas_group()->reparent (*dest_rtv->view()->canvas_item());
	_primary->get_canvas_group()->property_y() = 0;

	boost::shared_ptr<Playlist> playlist = dest_rtv->playlist();

	_editor->begin_reversible_command (_("insert region"));
        playlist->clear_changes ();
	playlist->add_region (_primary->region (), _last_frame_position);
	_editor->session()->add_command (new StatefulDiffCommand (playlist));
	_editor->commit_reversible_command ();

	delete _primary;
	_primary = 0;
	_views.clear ();
}

void
RegionInsertDrag::aborted ()
{
	delete _primary;
	_primary = 0;
	_views.clear ();
}

RegionSpliceDrag::RegionSpliceDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionMoveDrag (e, i, p, v, false, false)
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionSpliceDrag\n");
}

struct RegionSelectionByPosition {
    bool operator() (RegionView*a, RegionView* b) {
	    return a->region()->position () < b->region()->position();
    }
};

void
RegionSpliceDrag::motion (GdkEvent* event, bool)
{
	/* Which trackview is this ? */

	pair<TimeAxisView*, int> const tvp = _editor->trackview_by_y_position (_drags->current_pointer_y ());
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*> (tvp.first);
	layer_t layer = tvp.second;

	if (tv && tv->layer_display() == Overlaid) {
		layer = 0;
	}

	/* The region motion is only processed if the pointer is over
	   an audio track.
	*/

	if (!tv || !tv->is_track()) {
		/* To make sure we hide the verbose canvas cursor when the mouse is
		   not held over and audiotrack.
		*/
		_editor->hide_verbose_canvas_cursor ();
		return;
	}

	int dir;

	if ((_drags->current_pointer_x() - last_pointer_x()) > 0) {
		dir = 1;
	} else {
		dir = -1;
	}

	RegionSelection copy (_editor->selection->regions);

	RegionSelectionByPosition cmp;
	copy.sort (cmp);

	framepos_t const pf = adjusted_current_frame (event);

	for (RegionSelection::iterator i = copy.begin(); i != copy.end(); ++i) {

		RouteTimeAxisView* atv = dynamic_cast<RouteTimeAxisView*> (&(*i)->get_time_axis_view());

		if (!atv) {
			continue;
		}

		boost::shared_ptr<Playlist> playlist;

		if ((playlist = atv->playlist()) == 0) {
			continue;
		}

		if (!playlist->region_is_shuffle_constrained ((*i)->region())) {
			continue;
		}

		if (dir > 0) {
			if (pf < (*i)->region()->last_frame() + 1) {
				continue;
			}
		} else {
			if (pf > (*i)->region()->first_frame()) {
				continue;
			}
		}


		playlist->shuffle ((*i)->region(), dir);
	}
}

void
RegionSpliceDrag::finished (GdkEvent* event, bool movement_occurred)
{
	RegionMoveDrag::finished (event, movement_occurred);
}

void
RegionSpliceDrag::aborted ()
{
	/* XXX: TODO */
}

RegionCreateDrag::RegionCreateDrag (Editor* e, ArdourCanvas::Item* i, TimeAxisView* v)
	: Drag (e, i),
	  _view (dynamic_cast<MidiTimeAxisView*> (v))
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionCreateDrag\n");
	
	assert (_view);
}

void
RegionCreateDrag::motion (GdkEvent* event, bool first_move)
{
	if (first_move) {
		/* don't use a zero-length region otherwise its region view will be hidden when it is created */
		_region = _view->add_region (grab_frame(), 1, false);
	} else {
		framepos_t const f = adjusted_current_frame (event);
		if (f < grab_frame()) {
			_region->set_position (f, this);
		}

		/* again, don't use a zero-length region (see above) */
		framecnt_t const len = abs (f - grab_frame ());
		_region->set_length (len < 1 ? 1 : len, this);
	}
}

void
RegionCreateDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		_editor->commit_reversible_command ();
	}
}

void
RegionCreateDrag::aborted ()
{
	/* XXX */
}

NoteResizeDrag::NoteResizeDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
	, region (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New NoteResizeDrag\n");
}

void
NoteResizeDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*ignored*/)
{
	Gdk::Cursor* cursor;
	ArdourCanvas::CanvasNote* cnote = dynamic_cast<ArdourCanvas::CanvasNote*>(_item);

	Drag::start_grab (event);

	region = &cnote->region_view();

	double const region_start = region->get_position_pixels();
	double const middle_point = region_start + cnote->x1() + (cnote->x2() - cnote->x1()) / 2.0L;

	if (grab_x() <= middle_point) {
		cursor = _editor->left_side_trim_cursor;
		at_front = true;
	} else {
		cursor = _editor->right_side_trim_cursor;
		at_front = false;
	}

	_item->grab(GDK_POINTER_MOTION_MASK | GDK_BUTTON_RELEASE_MASK, *cursor, event->motion.time);

	if (event->motion.state & Keyboard::PrimaryModifier) {
		relative = false;
	} else {
		relative = true;
	}

	MidiRegionSelection& ms (_editor->get_selection().midi_regions);

	if (ms.size() > 1) {
		/* has to be relative, may make no sense otherwise */
		relative = true;
	}

	/* select this note; if it is already selected, preserve the existing selection,
	   otherwise make this note the only one selected.
	*/
	region->note_selected (cnote, cnote->selected ());

	for (MidiRegionSelection::iterator r = ms.begin(); r != ms.end(); ) {
		MidiRegionSelection::iterator next;
		next = r;
		++next;
		(*r)->begin_resizing (at_front);
		r = next;
	}
}

void
NoteResizeDrag::motion (GdkEvent* /*event*/, bool /*first_move*/)
{
	MidiRegionSelection& ms (_editor->get_selection().midi_regions);
	for (MidiRegionSelection::iterator r = ms.begin(); r != ms.end(); ++r) {
		(*r)->update_resizing (dynamic_cast<ArdourCanvas::CanvasNote*>(_item), at_front, _drags->current_pointer_x() - grab_x(), relative);
	}
}

void
NoteResizeDrag::finished (GdkEvent*, bool /*movement_occurred*/)
{
	MidiRegionSelection& ms (_editor->get_selection().midi_regions);
	for (MidiRegionSelection::iterator r = ms.begin(); r != ms.end(); ++r) {
		(*r)->commit_resizing (dynamic_cast<ArdourCanvas::CanvasNote*>(_item), at_front, _drags->current_pointer_x() - grab_x(), relative);
	}
}

void
NoteResizeDrag::aborted ()
{
	/* XXX: TODO */
}

RegionGainDrag::RegionGainDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
{
	DEBUG_TRACE (DEBUG::Drags, "New RegionGainDrag\n");
}

void
RegionGainDrag::motion (GdkEvent* /*event*/, bool)
{

}

void
RegionGainDrag::finished (GdkEvent *, bool)
{

}

void
RegionGainDrag::aborted ()
{
	/* XXX: TODO */
}

TrimDrag::TrimDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
	, _have_transaction (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New TrimDrag\n");
}

void
TrimDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	double speed = 1.0;
	TimeAxisView* tvp = &_primary->get_time_axis_view ();
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);

	if (tv && tv->is_track()) {
		speed = tv->track()->speed();
	}

	framepos_t const region_start = (framepos_t) (_primary->region()->position() / speed);
	framepos_t const region_end = (framepos_t) (_primary->region()->last_frame() / speed);
	framecnt_t const region_length = (framecnt_t) (_primary->region()->length() / speed);

	framepos_t const pf = adjusted_current_frame (event);

	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
		_operation = ContentsTrim;
                Drag::start_grab (event, _editor->trimmer_cursor);
	} else {
		/* These will get overridden for a point trim.*/
		if (pf < (region_start + region_length/2)) {
			/* closer to start */
			_operation = StartTrim;
                        Drag::start_grab (event, _editor->left_side_trim_cursor);
		} else {
			/* closer to end */
			_operation = EndTrim;
                        Drag::start_grab (event, _editor->right_side_trim_cursor);
                }
	}

	switch (_operation) {
	case StartTrim:
		_editor->show_verbose_time_cursor (region_start, 10);
		break;
	case EndTrim:
		_editor->show_verbose_time_cursor (region_end, 10);
		break;
	case ContentsTrim:
		_editor->show_verbose_time_cursor (pf, 10);
		break;
	}
}

void
TrimDrag::motion (GdkEvent* event, bool first_move)
{
	RegionView* rv = _primary;

	/* snap modifier works differently here..
	   its current state has to be passed to the
	   various trim functions in order to work properly
	*/

	double speed = 1.0;
	TimeAxisView* tvp = &_primary->get_time_axis_view ();
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);
	pair<set<boost::shared_ptr<Playlist> >::iterator,bool> insert_result;

	if (tv && tv->is_track()) {
		speed = tv->track()->speed();
	}

	framepos_t const pf = adjusted_current_frame (event);

	if (first_move) {

		string trim_type;

		switch (_operation) {
		case StartTrim:
			trim_type = "Region start trim";
			break;
		case EndTrim:
			trim_type = "Region end trim";
			break;
		case ContentsTrim:
			trim_type = "Region content trim";
			break;
		}

		_editor->begin_reversible_command (trim_type);
		_have_transaction = true;

		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			RegionView* rv = i->view;
			rv->fake_set_opaque(false);
			rv->enable_display (false);
                        rv->region()->clear_changes ();
			rv->region()->suspend_property_changes ();

			AudioRegionView* const arv = dynamic_cast<AudioRegionView*> (rv);

			if (arv){
				arv->temporarily_hide_envelope ();
			}

			boost::shared_ptr<Playlist> pl = rv->region()->playlist();
			insert_result = _editor->motion_frozen_playlists.insert (pl);

			if (insert_result.second) {
				pl->freeze();
			}
		}
	}

	bool non_overlap_trim = false;

	if (event && Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
		non_overlap_trim = true;
	}

	switch (_operation) {
	case StartTrim:
		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			_editor->single_start_trim (*i->view, pf, non_overlap_trim);
		}
		break;

	case EndTrim:
		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			_editor->single_end_trim (*i->view, pf, non_overlap_trim);
		}
		break;

	case ContentsTrim:
		{
			bool swap_direction = false;

			if (event && Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
				swap_direction = true;
			}

			framecnt_t frame_delta = 0;
			
			bool left_direction = false;
			if (last_pointer_frame() > pf) {
				left_direction = true;
			}

			if (left_direction) {
				frame_delta = (last_pointer_frame() - pf);
			} else {
				frame_delta = (pf - last_pointer_frame());
			}

			for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				_editor->single_contents_trim (*i->view, frame_delta, left_direction, swap_direction);
			}
		}
		break;
	}

	switch (_operation) {
	case StartTrim:
		_editor->show_verbose_time_cursor((framepos_t) (rv->region()->position()/speed), 10);
		break;
	case EndTrim:
		_editor->show_verbose_time_cursor((framepos_t) (rv->region()->last_frame()/speed), 10);
		break;
	case ContentsTrim:
		_editor->show_verbose_time_cursor (pf, 10);
		break;
	}
}


void
TrimDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		motion (event, false);

		if (!_editor->selection->selected (_primary)) {
			_editor->thaw_region_after_trim (*_primary);
		} else {

			for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				_editor->thaw_region_after_trim (*i->view);
				i->view->enable_display (true);
				i->view->fake_set_opaque (true);
                                if (_have_transaction) {
                                        _editor->session()->add_command (new StatefulDiffCommand (i->view->region()));
                                }
			}
		}
		for (set<boost::shared_ptr<Playlist> >::iterator p = _editor->motion_frozen_playlists.begin(); p != _editor->motion_frozen_playlists.end(); ++p) {
			(*p)->thaw ();
		}

		_editor->motion_frozen_playlists.clear ();

		if (_have_transaction) {
			_editor->commit_reversible_command();
		}

	} else {
		/* no mouse movement */
		_editor->point_trim (event, adjusted_current_frame (event));
	}
}

void
TrimDrag::aborted ()
{
	/* Our motion method is changing model state, so use the Undo system
	   to cancel.  Perhaps not ideal, as this will leave an Undo point
	   behind which may be slightly odd from the user's point of view.
	*/

	finished (0, true);
	
	if (_have_transaction) {
		_editor->undo ();
	}
}

MeterMarkerDrag::MeterMarkerDrag (Editor* e, ArdourCanvas::Item* i, bool c)
	: Drag (e, i),
	  _copy (c)
{
	DEBUG_TRACE (DEBUG::Drags, "New MeterMarkerDrag\n");
	
	_marker = reinterpret_cast<MeterMarker*> (_item->get_data ("marker"));
	assert (_marker);
}

void
MeterMarkerDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	if (_copy) {
		// create a dummy marker for visual representation of moving the copy.
		// The actual copying is not done before we reach the finish callback.
		char name[64];
		snprintf (name, sizeof(name), "%g/%g", _marker->meter().beats_per_bar(), _marker->meter().note_divisor ());
		MeterMarker* new_marker = new MeterMarker(*_editor, *_editor->meter_group, ARDOUR_UI::config()->canvasvar_MeterMarker.get(), name,
							  *new MeterSection (_marker->meter()));

		_item = &new_marker->the_item ();
		_marker = new_marker;

	} else {

		MetricSection& section (_marker->meter());

		if (!section.movable()) {
			return;
		}

	}

	Drag::start_grab (event, cursor);

	_pointer_frame_offset = raw_grab_frame() - _marker->meter().frame();

	_editor->show_verbose_time_cursor (adjusted_current_frame(event), 10);
}

void
MeterMarkerDrag::motion (GdkEvent* event, bool)
{
	framepos_t const pf = adjusted_current_frame (event);

	_marker->set_position (pf);
	
	_editor->show_verbose_time_cursor (pf, 10);
}

void
MeterMarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	motion (event, false);

	BBT_Time when;

	TempoMap& map (_editor->session()->tempo_map());
	map.bbt_time (last_pointer_frame(), when);

	if (_copy == true) {
		_editor->begin_reversible_command (_("copy meter mark"));
		XMLNode &before = map.get_state();
		map.add_meter (_marker->meter(), when);
		XMLNode &after = map.get_state();
		_editor->session()->add_command(new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();

		// delete the dummy marker we used for visual representation of copying.
		// a new visual marker will show up automatically.
		delete _marker;
	} else {
		_editor->begin_reversible_command (_("move meter mark"));
		XMLNode &before = map.get_state();
		map.move_meter (_marker->meter(), when);
		XMLNode &after = map.get_state();
		_editor->session()->add_command(new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();
	}
}

void
MeterMarkerDrag::aborted ()
{
	_marker->set_position (_marker->meter().frame ());
}

TempoMarkerDrag::TempoMarkerDrag (Editor* e, ArdourCanvas::Item* i, bool c)
	: Drag (e, i),
	  _copy (c)
{
	DEBUG_TRACE (DEBUG::Drags, "New TempoMarkerDrag\n");
	
	_marker = reinterpret_cast<TempoMarker*> (_item->get_data ("marker"));
	assert (_marker);
}

void
TempoMarkerDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{

	if (_copy) {

		// create a dummy marker for visual representation of moving the copy.
		// The actual copying is not done before we reach the finish callback.
		char name[64];
		snprintf (name, sizeof (name), "%.2f", _marker->tempo().beats_per_minute());
		TempoMarker* new_marker = new TempoMarker(*_editor, *_editor->tempo_group, ARDOUR_UI::config()->canvasvar_TempoMarker.get(), name,
							  *new TempoSection (_marker->tempo()));

		_item = &new_marker->the_item ();
		_marker = new_marker;

	} else {

		MetricSection& section (_marker->tempo());

		if (!section.movable()) {
			return;
		}
	}

	Drag::start_grab (event, cursor);

	_pointer_frame_offset = raw_grab_frame() - _marker->tempo().frame();
	_editor->show_verbose_time_cursor (adjusted_current_frame (event), 10);
}

void
TempoMarkerDrag::motion (GdkEvent* event, bool)
{
	framepos_t const pf = adjusted_current_frame (event);
	_marker->set_position (pf);
	_editor->show_verbose_time_cursor (pf, 10);
}

void
TempoMarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	motion (event, false);

	BBT_Time when;

	TempoMap& map (_editor->session()->tempo_map());
	map.bbt_time (last_pointer_frame(), when);

	if (_copy == true) {
		_editor->begin_reversible_command (_("copy tempo mark"));
		XMLNode &before = map.get_state();
		map.add_tempo (_marker->tempo(), when);
		XMLNode &after = map.get_state();
		_editor->session()->add_command (new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();

		// delete the dummy marker we used for visual representation of copying.
		// a new visual marker will show up automatically.
		delete _marker;
	} else {
		_editor->begin_reversible_command (_("move tempo mark"));
		XMLNode &before = map.get_state();
		map.move_tempo (_marker->tempo(), when);
		XMLNode &after = map.get_state();
		_editor->session()->add_command (new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();
	}
}

void
TempoMarkerDrag::aborted ()
{
	_marker->set_position (_marker->tempo().frame());
}

CursorDrag::CursorDrag (Editor* e, ArdourCanvas::Item* i, bool s)
	: Drag (e, i),
	  _stop (s)
{
	DEBUG_TRACE (DEBUG::Drags, "New CursorDrag\n");
	
	_cursor = reinterpret_cast<EditorCursor*> (_item->get_data ("cursor"));
	assert (_cursor);
}

void
CursorDrag::start_grab (GdkEvent* event, Gdk::Cursor* c)
{
	Drag::start_grab (event, c);

	if (!_stop) {

		framepos_t where = _editor->event_frame (event, 0, 0);

		_editor->snap_to_with_modifier (where, event);
		_editor->playhead_cursor->set_position (where);

	}

	if (_cursor == _editor->playhead_cursor) {
		_editor->_dragging_playhead = true;

		Session* s = _editor->session ();

		if (s) {
			if (_was_rolling && _stop) {
				s->request_stop ();
			}

			if (s->is_auditioning()) {
				s->cancel_audition ();
			}

			s->request_suspend_timecode_transmission ();

			if (s->timecode_transmission_suspended ()) {
				framepos_t const f = _editor->playhead_cursor->current_frame;
				s->send_mmc_locate (f);
				s->send_full_time_code (f);
			}
		}
	}

	_pointer_frame_offset = raw_grab_frame() - _cursor->current_frame;

	_editor->show_verbose_time_cursor (_cursor->current_frame, 10);
}

void
CursorDrag::motion (GdkEvent* event, bool)
{
	framepos_t const adjusted_frame = adjusted_current_frame (event);

	if (adjusted_frame == last_pointer_frame()) {
		return;
	}

	_cursor->set_position (adjusted_frame);

	_editor->show_verbose_time_cursor (_cursor->current_frame, 10);

	Session* s = _editor->session ();
	if (s && _item == &_editor->playhead_cursor->canvas_item && s->timecode_transmission_suspended ()) {
		framepos_t const f = _editor->playhead_cursor->current_frame;
		s->send_mmc_locate (f);
		s->send_full_time_code (f);
	}
	

#ifdef GTKOSX
	_editor->update_canvas_now ();
#endif
	_editor->UpdateAllTransportClocks (_cursor->current_frame);
}

void
CursorDrag::finished (GdkEvent* event, bool movement_occurred)
{
	_editor->_dragging_playhead = false;

	if (!movement_occurred && _stop) {
		return;
	}

	motion (event, false);

	if (_item == &_editor->playhead_cursor->canvas_item) {
		Session* s = _editor->session ();
		if (s) {
			s->request_locate (_editor->playhead_cursor->current_frame, _was_rolling);
			_editor->_pending_locate_request = true;
			s->request_resume_timecode_transmission ();
		}
	}
}

void
CursorDrag::aborted ()
{
	if (_editor->_dragging_playhead) {
		_editor->session()->request_resume_timecode_transmission ();
		_editor->_dragging_playhead = false;
	}
	
	_cursor->set_position (adjusted_frame (grab_frame (), 0, false));
}

FadeInDrag::FadeInDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	DEBUG_TRACE (DEBUG::Drags, "New FadeInDrag\n");
}

void
FadeInDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> const r = arv->audio_region ();

	_pointer_frame_offset = raw_grab_frame() - ((framecnt_t) r->fade_in()->back()->when + r->position());
	_editor->show_verbose_duration_cursor (r->position(), r->position() + r->fade_in()->back()->when, 10);
	
	arv->show_fade_line((framepos_t) r->fade_in()->back()->when);
}

void
FadeInDrag::motion (GdkEvent* event, bool)
{
	framecnt_t fade_length;

	framepos_t const pos = adjusted_current_frame (event);

	boost::shared_ptr<Region> region = _primary->region ();

	if (pos < (region->position() + 64)) {
		fade_length = 64; // this should be a minimum defined somewhere
	} else if (pos > region->last_frame()) {
		fade_length = region->length();
	} else {
		fade_length = pos - region->position();
	}

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_in_shape_width (fade_length);
		tmp->show_fade_line((framecnt_t) fade_length);
	}

	_editor->show_verbose_duration_cursor (region->position(), region->position() + fade_length, 10);
}

void
FadeInDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	framecnt_t fade_length;

	framepos_t const pos = adjusted_current_frame (event);

	boost::shared_ptr<Region> region = _primary->region ();

	if (pos < (region->position() + 64)) {
		fade_length = 64; // this should be a minimum defined somewhere
	} else if (pos > region->last_frame()) {
		fade_length = region->length();
	} else {
		fade_length = pos - region->position();
	}

	_editor->begin_reversible_command (_("change fade in length"));

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_in();
		XMLNode &before = alist->get_state();

		tmp->audio_region()->set_fade_in_length (fade_length);
		tmp->audio_region()->set_fade_in_active (true);
		tmp->hide_fade_line();

		XMLNode &after = alist->get_state();
		_editor->session()->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	_editor->commit_reversible_command ();
}

void
FadeInDrag::aborted ()
{
	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_in_shape_width (tmp->audio_region()->fade_in()->back()->when);
		tmp->hide_fade_line();
	}
}

FadeOutDrag::FadeOutDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	DEBUG_TRACE (DEBUG::Drags, "New FadeOutDrag\n");
}

void
FadeOutDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> r = arv->audio_region ();

	_pointer_frame_offset = raw_grab_frame() - (r->length() - (framecnt_t) r->fade_out()->back()->when + r->position());
	_editor->show_verbose_duration_cursor (r->last_frame() - r->fade_out()->back()->when, r->last_frame(), 10);
	
	arv->show_fade_line(r->length() - r->fade_out()->back()->when);
}

void
FadeOutDrag::motion (GdkEvent* event, bool)
{
	framecnt_t fade_length;

	framepos_t const pos = adjusted_current_frame (event);

	boost::shared_ptr<Region> region = _primary->region ();

	if (pos > (region->last_frame() - 64)) {
		fade_length = 64; // this should really be a minimum fade defined somewhere
	}
	else if (pos < region->position()) {
		fade_length = region->length();
	}
	else {
		fade_length = region->last_frame() - pos;
	}

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_out_shape_width (fade_length);
		tmp->show_fade_line(region->length() - fade_length);
	}

	_editor->show_verbose_duration_cursor (region->last_frame() - fade_length, region->last_frame(), 10);
}

void
FadeOutDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	framecnt_t fade_length;

	framepos_t const pos = adjusted_current_frame (event);

	boost::shared_ptr<Region> region = _primary->region ();

	if (pos > (region->last_frame() - 64)) {
		fade_length = 64; // this should really be a minimum fade defined somewhere
	}
	else if (pos < region->position()) {
		fade_length = region->length();
	}
	else {
		fade_length = region->last_frame() - pos;
	}

	_editor->begin_reversible_command (_("change fade out length"));

	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_out();
		XMLNode &before = alist->get_state();

		tmp->audio_region()->set_fade_out_length (fade_length);
		tmp->audio_region()->set_fade_out_active (true);
		tmp->hide_fade_line();

		XMLNode &after = alist->get_state();
		_editor->session()->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	_editor->commit_reversible_command ();
}

void
FadeOutDrag::aborted ()
{
	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_out_shape_width (tmp->audio_region()->fade_out()->back()->when);
		tmp->hide_fade_line();
	}
}

MarkerDrag::MarkerDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
{
	DEBUG_TRACE (DEBUG::Drags, "New MarkerDrag\n");
	
	_marker = reinterpret_cast<Marker*> (_item->get_data ("marker"));
	assert (_marker);

	_points.push_back (Gnome::Art::Point (0, 0));
	_points.push_back (Gnome::Art::Point (0, physical_screen_height (_editor->get_window())));

	_line = new ArdourCanvas::Line (*_editor->timebar_group);
	_line->property_width_pixels() = 1;
	_line->property_points () = _points;
	_line->hide ();

	_line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_MarkerDragLine.get();
}

MarkerDrag::~MarkerDrag ()
{
	for (list<Location*>::iterator i = _copied_locations.begin(); i != _copied_locations.end(); ++i) {
		delete *i;
	}
}

void
MarkerDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	bool is_start;

	Location *location = _editor->find_location_from_marker (_marker, is_start);
	_editor->_dragging_edit_point = true;

	_pointer_frame_offset = raw_grab_frame() - (is_start ? location->start() : location->end());

	update_item (location);

	// _drag_line->show();
	// _line->raise_to_top();

	if (is_start) {
		_editor->show_verbose_time_cursor (location->start(), 10);
	} else {
		_editor->show_verbose_time_cursor (location->end(), 10);
	}

	Selection::Operation op = ArdourKeyboard::selection_type (event->button.state);

	switch (op) {
	case Selection::Toggle:
		_editor->selection->toggle (_marker);
		break;
	case Selection::Set:
		if (!_editor->selection->selected (_marker)) {
			_editor->selection->set (_marker);
		}
		break;
	case Selection::Extend:
	{
		Locations::LocationList ll;
		list<Marker*> to_add;
		framepos_t s, e;
		_editor->selection->markers.range (s, e);
		s = min (_marker->position(), s);
		e = max (_marker->position(), e);
		s = min (s, e);
		e = max (s, e);
		if (e < max_frames) {
			++e;
		}
		_editor->session()->locations()->find_all_between (s, e, ll, Location::Flags (0));
		for (Locations::LocationList::iterator i = ll.begin(); i != ll.end(); ++i) {
			Editor::LocationMarkers* lm = _editor->find_location_markers (*i);
			if (lm) {
				if (lm->start) {
					to_add.push_back (lm->start);
				}
				if (lm->end) {
					to_add.push_back (lm->end);
				}
			}
		}
		if (!to_add.empty()) {
			_editor->selection->add (to_add);
		}
		break;
	}
	case Selection::Add:
		_editor->selection->add (_marker);
		break;
	}

	/* Set up copies for us to manipulate during the drag */

	for (MarkerSelection::iterator i = _editor->selection->markers.begin(); i != _editor->selection->markers.end(); ++i) {
		Location* l = _editor->find_location_from_marker (*i, is_start);
		_copied_locations.push_back (new Location (*l));
	}
}

void
MarkerDrag::motion (GdkEvent* event, bool)
{
	framecnt_t f_delta = 0;
	bool is_start;
	bool move_both = false;
	Marker* marker;
	Location *real_location;
	Location *copy_location = 0;

	framepos_t const newframe = adjusted_current_frame (event);

	framepos_t next = newframe;

	if (newframe == last_pointer_frame()) {
		return;
	}

	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
		move_both = true;
	}

	MarkerSelection::iterator i;
	list<Location*>::iterator x;

	/* find the marker we're dragging, and compute the delta */

	for (i = _editor->selection->markers.begin(), x = _copied_locations.begin();
	     x != _copied_locations.end() && i != _editor->selection->markers.end();
	     ++i, ++x) {

		copy_location = *x;
		marker = *i;

		if (marker == _marker) {

			if ((real_location = _editor->find_location_from_marker (marker, is_start)) == 0) {
				/* que pasa ?? */
				return;
			}

			if (real_location->is_mark()) {
				f_delta = newframe - copy_location->start();
			} else {


				switch (marker->type()) {
				case Marker::Start:
				case Marker::LoopStart:
				case Marker::PunchIn:
					f_delta = newframe - copy_location->start();
					break;

				case Marker::End:
				case Marker::LoopEnd:
				case Marker::PunchOut:
					f_delta = newframe - copy_location->end();
					break;
				default:
					/* what kind of marker is this ? */
					return;
				}
			}
			break;
		}
	}

	if (i == _editor->selection->markers.end()) {
		/* hmm, impossible - we didn't find the dragged marker */
		return;
	}

	/* now move them all */

	for (i = _editor->selection->markers.begin(), x = _copied_locations.begin();
	     x != _copied_locations.end() && i != _editor->selection->markers.end();
	     ++i, ++x) {

		copy_location = *x;
		marker = *i;

		/* call this to find out if its the start or end */

		if ((real_location = _editor->find_location_from_marker (marker, is_start)) == 0) {
			continue;
		}

		if (real_location->locked()) {
			continue;
		}

		if (copy_location->is_mark()) {

			/* now move it */

			copy_location->set_start (copy_location->start() + f_delta);

		} else {

			framepos_t new_start = copy_location->start() + f_delta;
			framepos_t new_end = copy_location->end() + f_delta;

			if (is_start) { // start-of-range marker

				if (move_both) {
					copy_location->set_start (new_start);
					copy_location->set_end (new_end);
				} else 	if (new_start < copy_location->end()) {
					copy_location->set_start (new_start);
				} else {
					_editor->snap_to (next, 1, true);
					copy_location->set_end (next);
					copy_location->set_start (newframe);
				}

			} else { // end marker

				if (move_both) {
					copy_location->set_end (new_end);
					copy_location->set_start (new_start);
				} else if (new_end > copy_location->start()) {
					copy_location->set_end (new_end);
				} else if (newframe > 0) {
					_editor->snap_to (next, -1, true);
					copy_location->set_start (next);
					copy_location->set_end (newframe);
				}
			}
		}

		update_item (copy_location);

		Editor::LocationMarkers* lm = _editor->find_location_markers (real_location);

		if (lm) {
			lm->set_position (copy_location->start(), copy_location->end());
		}
	}

	assert (!_copied_locations.empty());

	_editor->show_verbose_time_cursor (newframe, 10);

#ifdef GTKOSX
	_editor->update_canvas_now ();
#endif
}

void
MarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {

		/* just a click, do nothing but finish
		   off the selection process
		*/

		Selection::Operation op = ArdourKeyboard::selection_type (event->button.state);

		switch (op) {
		case Selection::Set:
			if (_editor->selection->selected (_marker) && _editor->selection->markers.size() > 1) {
				_editor->selection->set (_marker);
			}
			break;

		case Selection::Toggle:
		case Selection::Extend:
		case Selection::Add:
			break;
		}

		return;
	}

	_editor->_dragging_edit_point = false;

	_editor->begin_reversible_command ( _("move marker") );
	XMLNode &before = _editor->session()->locations()->get_state();

	MarkerSelection::iterator i;
	list<Location*>::iterator x;
	bool is_start;

	for (i = _editor->selection->markers.begin(), x = _copied_locations.begin();
	     x != _copied_locations.end() && i != _editor->selection->markers.end();
	     ++i, ++x) {

		Location * location = _editor->find_location_from_marker (*i, is_start);

		if (location) {

			if (location->locked()) {
				return;
			}

			if (location->is_mark()) {
				location->set_start ((*x)->start());
			} else {
				location->set ((*x)->start(), (*x)->end());
			}
		}
	}

	XMLNode &after = _editor->session()->locations()->get_state();
	_editor->session()->add_command(new MementoCommand<Locations>(*(_editor->session()->locations()), &before, &after));
	_editor->commit_reversible_command ();

	_line->hide();
}

void
MarkerDrag::aborted ()
{
	/* XXX: TODO */
}

void
MarkerDrag::update_item (Location* location)
{
	double const x1 = _editor->frame_to_pixel (location->start());

	_points.front().set_x(x1);
	_points.back().set_x(x1);
	_line->property_points() = _points;
}

ControlPointDrag::ControlPointDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i),
	  _cumulative_x_drag (0),
	  _cumulative_y_drag (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New ControlPointDrag\n");
	
	_point = reinterpret_cast<ControlPoint*> (_item->get_data ("control_point"));
	assert (_point);
}


void
ControlPointDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*cursor*/)
{
	Drag::start_grab (event, _editor->fader_cursor);

	// start the grab at the center of the control point so
	// the point doesn't 'jump' to the mouse after the first drag
	_fixed_grab_x = _point->get_x();
	_fixed_grab_y = _point->get_y();

	float const fraction = 1 - (_point->get_y() / _point->line().height());

	_point->line().start_drag_single (_point, _fixed_grab_x, fraction);

	_editor->set_verbose_canvas_cursor (_point->line().get_verbose_cursor_string (fraction),
					    event->button.x + 10, event->button.y + 10);

	_editor->show_verbose_canvas_cursor ();
}

void
ControlPointDrag::motion (GdkEvent* event, bool)
{
	double dx = _drags->current_pointer_x() - last_pointer_x();
	double dy = _drags->current_pointer_y() - last_pointer_y();

	if (event->button.state & Keyboard::SecondaryModifier) {
		dx *= 0.1;
		dy *= 0.1;
	}

	/* coordinate in pixels relative to the start of the region (for region-based automation)
	   or track (for track-based automation) */
	double cx = _fixed_grab_x + _cumulative_x_drag + dx;
	double cy = _fixed_grab_y + _cumulative_y_drag + dy;

	// calculate zero crossing point. back off by .01 to stay on the
	// positive side of zero
	double const zero_gain_y = (1.0 - _zero_gain_fraction) * _point->line().height() - .01;

	// make sure we hit zero when passing through
	if ((cy < zero_gain_y && (cy - dy) > zero_gain_y) || (cy > zero_gain_y && (cy - dy) < zero_gain_y)) {
		cy = zero_gain_y;
	}

	if (_x_constrained) {
		cx = _fixed_grab_x;
	}
	if (_y_constrained) {
		cy = _fixed_grab_y;
	}

	_cumulative_x_drag = cx - _fixed_grab_x;
	_cumulative_y_drag = cy - _fixed_grab_y;

	cx = max (0.0, cx);
	cy = max (0.0, cy);
	cy = min ((double) _point->line().height(), cy);

	framepos_t cx_frames = _editor->unit_to_frame (cx);
	
	if (!_x_constrained) {
		_editor->snap_to_with_modifier (cx_frames, event);
	}

	cx_frames = min (cx_frames, _point->line().maximum_time());

	float const fraction = 1.0 - (cy / _point->line().height());

	bool const push = Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier);

	_point->line().drag_motion (_editor->frame_to_unit (cx_frames), fraction, false, push);

	_editor->set_verbose_canvas_cursor_text (_point->line().get_verbose_cursor_string (fraction));
}

void
ControlPointDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {

		/* just a click */

		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
			_editor->reset_point_selection ();
		}

	} else {
		motion (event, false);
	}
	
	_point->line().end_drag ();
	_editor->session()->commit_reversible_command ();
}

void
ControlPointDrag::aborted ()
{
	_point->line().reset ();
}

bool
ControlPointDrag::active (Editing::MouseMode m)
{
	if (m == Editing::MouseGain) {
		/* always active in mouse gain */
		return true;
	}

	/* otherwise active if the point is on an automation line (ie not if its on a region gain line) */
	return dynamic_cast<AutomationLine*> (&(_point->line())) != 0;
}

LineDrag::LineDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i),
	  _line (0),
	  _cumulative_y_drag (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New LineDrag\n");
}

void
LineDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*cursor*/)
{
	_line = reinterpret_cast<AutomationLine*> (_item->get_data ("line"));
	assert (_line);

	_item = &_line->grab_item ();

	/* need to get x coordinate in terms of parent (TimeAxisItemView)
	   origin, and ditto for y.
	*/

	double cx = event->button.x;
	double cy = event->button.y;

	_line->parent_group().w2i (cx, cy);

	framecnt_t const frame_within_region = (framecnt_t) floor (cx * _editor->frames_per_unit);

	uint32_t before;
	uint32_t after;
	
	if (!_line->control_points_adjacent (frame_within_region, before, after)) {
		/* no adjacent points */
		return;
	}

	Drag::start_grab (event, _editor->fader_cursor);

	/* store grab start in parent frame */

	_fixed_grab_x = cx;
	_fixed_grab_y = cy;

	double fraction = 1.0 - (cy / _line->height());

	_line->start_drag_line (before, after, fraction);

	_editor->set_verbose_canvas_cursor (_line->get_verbose_cursor_string (fraction),
					    event->button.x + 10, event->button.y + 10);

	_editor->show_verbose_canvas_cursor ();
}

void
LineDrag::motion (GdkEvent* event, bool)
{
	double dy = _drags->current_pointer_y() - last_pointer_y();

	if (event->button.state & Keyboard::SecondaryModifier) {
		dy *= 0.1;
	}

	double cy = _fixed_grab_y + _cumulative_y_drag + dy;

	_cumulative_y_drag = cy - _fixed_grab_y;

	cy = max (0.0, cy);
	cy = min ((double) _line->height(), cy);

	double const fraction = 1.0 - (cy / _line->height());

	bool push;

	if (Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier)) {
		push = false;
	} else {
		push = true;
	}

	/* we are ignoring x position for this drag, so we can just pass in anything */
	_line->drag_motion (0, fraction, true, push);

	_editor->set_verbose_canvas_cursor_text (_line->get_verbose_cursor_string (fraction));
}

void
LineDrag::finished (GdkEvent* event, bool)
{
	motion (event, false);
	_line->end_drag ();
	_editor->session()->commit_reversible_command ();
}

void
LineDrag::aborted ()
{
	_line->reset ();
}

FeatureLineDrag::FeatureLineDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i),
	  _line (0),
	  _cumulative_x_drag (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New FeatureLineDrag\n");
}

void
FeatureLineDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*cursor*/)
{
	Drag::start_grab (event);
	
	_line = reinterpret_cast<SimpleLine*> (_item);
	assert (_line);

	/* need to get x coordinate in terms of parent (AudioRegionView) origin. */

	double cx = event->button.x;
	double cy = event->button.y;

	_item->property_parent().get_value()->w2i(cx, cy);

	/* store grab start in parent frame */
	_region_view_grab_x = cx;
	
	_before = _line->property_x1();
	
	_arv = reinterpret_cast<AudioRegionView*> (_item->get_data ("regionview"));
		
	_max_x = _editor->frame_to_pixel(_arv->get_duration());
}

void
FeatureLineDrag::motion (GdkEvent* event, bool)
{
	double dx = _drags->current_pointer_x() - last_pointer_x();
	
	double cx = _region_view_grab_x + _cumulative_x_drag + dx;
	
	_cumulative_x_drag += dx;
		
	/* Clamp the min and max extent of the drag to keep it within the region view bounds */
	
	if (cx > _max_x){
		cx = _max_x;
	}
	else if(cx < 0){
		cx = 0;
	}
	
	_line->property_x1() = cx; 
	_line->property_x2() = cx;

	_before = _line->property_x1();
}

void
FeatureLineDrag::finished (GdkEvent* event, bool)
{
	_arv = reinterpret_cast<AudioRegionView*> (_item->get_data ("regionview"));
	_arv->update_transient(_before, _line->property_x1());
}

void
FeatureLineDrag::aborted ()
{
	//_line->reset ();
}

RubberbandSelectDrag::RubberbandSelectDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
{
	DEBUG_TRACE (DEBUG::Drags, "New RubberbandSelectDrag\n");
}

void
RubberbandSelectDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);
	_editor->show_verbose_time_cursor (adjusted_current_frame (event), 10);
}

void
RubberbandSelectDrag::motion (GdkEvent* event, bool)
{
	framepos_t start;
	framepos_t end;
	double y1;
	double y2;

	framepos_t const pf = adjusted_current_frame (event, Config->get_rubberbanding_snaps_to_grid ());

	framepos_t grab = grab_frame ();
	if (Config->get_rubberbanding_snaps_to_grid ()) {
		_editor->snap_to_with_modifier (grab, event);
	}

	/* base start and end on initial click position */

	if (pf < grab) {
		start = pf;
		end = grab;
	} else {
		end = pf;
		start = grab;
	}

	if (_drags->current_pointer_y() < grab_y()) {
		y1 = _drags->current_pointer_y();
		y2 = grab_y();
	} else {
		y2 = _drags->current_pointer_y();
		y1 = grab_y();
	}


	if (start != end || y1 != y2) {

		double x1 = _editor->frame_to_pixel (start);
		double x2 = _editor->frame_to_pixel (end);

		_editor->rubberband_rect->property_x1() = x1;
		_editor->rubberband_rect->property_y1() = y1;
		_editor->rubberband_rect->property_x2() = x2;
		_editor->rubberband_rect->property_y2() = y2;

		_editor->rubberband_rect->show();
		_editor->rubberband_rect->raise_to_top();

		_editor->show_verbose_time_cursor (pf, 10);
	}
}

void
RubberbandSelectDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {

		motion (event, false);

		double y1,y2;
		if (_drags->current_pointer_y() < grab_y()) {
			y1 = _drags->current_pointer_y();
			y2 = grab_y();
		} else {
			y2 = _drags->current_pointer_y();
			y1 = grab_y();
		}


		Selection::Operation op = ArdourKeyboard::selection_type (event->button.state);
		bool committed;

		_editor->begin_reversible_command (_("rubberband selection"));

		if (grab_frame() < last_pointer_frame()) {
			committed = _editor->select_all_within (grab_frame(), last_pointer_frame() - 1, y1, y2, _editor->track_views, op, false);
		} else {
			committed = _editor->select_all_within (last_pointer_frame(), grab_frame() - 1, y1, y2, _editor->track_views, op, false);
		}

		if (!committed) {
			_editor->commit_reversible_command ();
		}

	} else {
		if (!getenv("ARDOUR_SAE")) {
			_editor->selection->clear_tracks();
		}
		_editor->selection->clear_regions();
		_editor->selection->clear_points ();
		_editor->selection->clear_lines ();
	}

	_editor->rubberband_rect->hide();
}

void
RubberbandSelectDrag::aborted ()
{
	_editor->rubberband_rect->hide ();
}

TimeFXDrag::TimeFXDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, std::list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	DEBUG_TRACE (DEBUG::Drags, "New TimeFXDrag\n");
}

void
TimeFXDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	_editor->show_verbose_time_cursor (adjusted_current_frame (event), 10);
}

void
TimeFXDrag::motion (GdkEvent* event, bool)
{
	RegionView* rv = _primary;

	framepos_t const pf = adjusted_current_frame (event);

	if (pf > rv->region()->position()) {
		rv->get_time_axis_view().show_timestretch (rv->region()->position(), pf);
	}

	_editor->show_verbose_time_cursor (pf, 10);
}

void
TimeFXDrag::finished (GdkEvent* /*event*/, bool movement_occurred)
{
	_primary->get_time_axis_view().hide_timestretch ();

	if (!movement_occurred) {
		return;
	}

	if (last_pointer_frame() < _primary->region()->position()) {
		/* backwards drag of the left edge - not usable */
		return;
	}

	framecnt_t newlen = last_pointer_frame() - _primary->region()->position();

	float percentage = (double) newlen / (double) _primary->region()->length();

#ifndef USE_RUBBERBAND
	// Soundtouch uses percentage / 100 instead of normal (/ 1)
	if (_primary->region()->data_type() == DataType::AUDIO) {
		percentage = (float) ((double) newlen - (double) _primary->region()->length()) / ((double) newlen) * 100.0f;
	}
#endif

	_editor->begin_reversible_command (_("timestretch"));

	// XXX how do timeFX on multiple regions ?

	RegionSelection rs;
	rs.add (_primary);

	if (_editor->time_stretch (rs, percentage) == -1) {
		error << _("An error occurred while executing time stretch operation") << endmsg;
	}
}

void
TimeFXDrag::aborted ()
{
	_primary->get_time_axis_view().hide_timestretch ();
}

ScrubDrag::ScrubDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
{
	DEBUG_TRACE (DEBUG::Drags, "New ScrubDrag\n");
}

void
ScrubDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);
}

void
ScrubDrag::motion (GdkEvent* /*event*/, bool)
{
	_editor->scrub (adjusted_current_frame (0, false), _drags->current_pointer_x ());
}

void
ScrubDrag::finished (GdkEvent* /*event*/, bool movement_occurred)
{
	if (movement_occurred && _editor->session()) {
		/* make sure we stop */
		_editor->session()->request_transport_speed (0.0);
	}
}

void
ScrubDrag::aborted ()
{
	/* XXX: TODO */
}

SelectionDrag::SelectionDrag (Editor* e, ArdourCanvas::Item* i, Operation o)
	: Drag (e, i)
	, _operation (o)
	, _copy (false)
	, _original_pointer_time_axis (-1)
	, _last_pointer_time_axis (-1)
{
	DEBUG_TRACE (DEBUG::Drags, "New SelectionDrag\n");
}

void
SelectionDrag::start_grab (GdkEvent* event, Gdk::Cursor*)
{
	framepos_t start = 0;
	framepos_t end = 0;

	if (_editor->session() == 0) {
		return;
	}

	Gdk::Cursor* cursor = 0;

	switch (_operation) {
	case CreateSelection:
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
			_copy = true;
		} else {
			_copy = false;
		}
		cursor = _editor->selector_cursor;
		Drag::start_grab (event, cursor);
		break;

	case SelectionStartTrim:
		if (_editor->clicked_axisview) {
			_editor->clicked_axisview->order_selection_trims (_item, true);
		}
		Drag::start_grab (event, _editor->left_side_trim_cursor);
		start = _editor->selection->time[_editor->clicked_selection].start;
		_pointer_frame_offset = raw_grab_frame() - start;
		break;

	case SelectionEndTrim:
		if (_editor->clicked_axisview) {
			_editor->clicked_axisview->order_selection_trims (_item, false);
		}
		Drag::start_grab (event, _editor->right_side_trim_cursor);
		end = _editor->selection->time[_editor->clicked_selection].end;
		_pointer_frame_offset = raw_grab_frame() - end;
		break;

	case SelectionMove:
		start = _editor->selection->time[_editor->clicked_selection].start;
		Drag::start_grab (event, cursor);
		_pointer_frame_offset = raw_grab_frame() - start;
		break;
	}

	if (_operation == SelectionMove) {
		_editor->show_verbose_time_cursor (start, 10);
	} else {
		_editor->show_verbose_time_cursor (adjusted_current_frame (event), 10);
	}

	_original_pointer_time_axis = _editor->trackview_by_y_position (_drags->current_pointer_y ()).first->order ();
}

void
SelectionDrag::motion (GdkEvent* event, bool first_move)
{
	framepos_t start = 0;
	framepos_t end = 0;
	framecnt_t length;

	pair<TimeAxisView*, int> const pending_time_axis = _editor->trackview_by_y_position (_drags->current_pointer_y ());
	if (pending_time_axis.first == 0) {
		return;
	}
	
	framepos_t const pending_position = adjusted_current_frame (event);

	/* only alter selection if things have changed */

	if (pending_time_axis.first->order() == _last_pointer_time_axis && pending_position == last_pointer_frame()) {
		return;
	}

	switch (_operation) {
	case CreateSelection:
	{
		framepos_t grab = grab_frame ();

		if (first_move) {
			_editor->snap_to (grab);
		}

		if (pending_position < grab_frame()) {
			start = pending_position;
			end = grab;
		} else {
			end = pending_position;
			start = grab;
		}

		/* first drag: Either add to the selection
		   or create a new selection
		*/

		if (first_move) {

			if (_copy) {
				/* adding to the selection */
                                _editor->set_selected_track_as_side_effect (Selection::Add);
				//_editor->selection->add (_editor->clicked_axisview);
				_editor->clicked_selection = _editor->selection->add (start, end);
				_copy = false;
			} else {
				/* new selection */

				if (_editor->clicked_axisview && !_editor->selection->selected (_editor->clicked_axisview)) {
                                        //_editor->selection->set (_editor->clicked_axisview);
                                        _editor->set_selected_track_as_side_effect (Selection::Set);
				}
				
				_editor->clicked_selection = _editor->selection->set (start, end);
			}
		}

		/* select the track that we're in */
		if (find (_added_time_axes.begin(), _added_time_axes.end(), pending_time_axis.first) == _added_time_axes.end()) {
                        // _editor->set_selected_track_as_side_effect (Selection::Add);
			_editor->selection->add (pending_time_axis.first);
			_added_time_axes.push_back (pending_time_axis.first);
		}

		/* deselect any tracks that this drag no longer includes, being careful to only deselect
		   tracks that we selected in the first place.
		*/
		
		int min_order = min (_original_pointer_time_axis, pending_time_axis.first->order());
		int max_order = max (_original_pointer_time_axis, pending_time_axis.first->order());

		list<TimeAxisView*>::iterator i = _added_time_axes.begin();
		while (i != _added_time_axes.end()) {

			list<TimeAxisView*>::iterator tmp = i;
			++tmp;
			
			if ((*i)->order() < min_order || (*i)->order() > max_order) {
				_editor->selection->remove (*i);
				_added_time_axes.remove (*i);
			}

			i = tmp;
		}

	}
	break;

	case SelectionStartTrim:

		start = _editor->selection->time[_editor->clicked_selection].start;
		end = _editor->selection->time[_editor->clicked_selection].end;

		if (pending_position > end) {
			start = end;
		} else {
			start = pending_position;
		}
		break;

	case SelectionEndTrim:

		start = _editor->selection->time[_editor->clicked_selection].start;
		end = _editor->selection->time[_editor->clicked_selection].end;

		if (pending_position < start) {
			end = start;
		} else {
			end = pending_position;
		}

		break;

	case SelectionMove:

		start = _editor->selection->time[_editor->clicked_selection].start;
		end = _editor->selection->time[_editor->clicked_selection].end;

		length = end - start;

		start = pending_position;
		_editor->snap_to (start);

		end = start + length;

		break;
	}

	if (event->button.x >= _editor->horizontal_position() + _editor->_canvas_width) {
		_editor->start_canvas_autoscroll (1, 0);
	}

	if (start != end) {
		_editor->selection->replace (_editor->clicked_selection, start, end);
	}

	if (_operation == SelectionMove) {
		_editor->show_verbose_time_cursor(start, 10);
	} else {
		_editor->show_verbose_time_cursor(pending_position, 10);
	}
}

void
SelectionDrag::finished (GdkEvent* event, bool movement_occurred)
{
	Session* s = _editor->session();

	if (movement_occurred) {
		motion (event, false);
		/* XXX this is not object-oriented programming at all. ick */
		if (_editor->selection->time.consolidate()) {
			_editor->selection->TimeChanged ();
		}

		/* XXX what if its a music time selection? */
		if (s && (s->config.get_auto_play() || (s->get_play_range() && s->transport_rolling()))) {
			s->request_play_range (&_editor->selection->time, true);
		}


	} else {
		/* just a click, no pointer movement.*/

		if (Keyboard::no_modifier_keys_pressed (&event->button)) {
			_editor->selection->clear_time();
		}

		if (_editor->clicked_axisview && !_editor->selection->selected (_editor->clicked_axisview)) {
			_editor->selection->set (_editor->clicked_axisview);
		}
		
		if (s && s->get_play_range () && s->transport_rolling()) {
			s->request_stop (false, false);
		}

	}

	_editor->stop_canvas_autoscroll ();
}

void
SelectionDrag::aborted ()
{
	/* XXX: TODO */
}

RangeMarkerBarDrag::RangeMarkerBarDrag (Editor* e, ArdourCanvas::Item* i, Operation o)
	: Drag (e, i),
	  _operation (o),
	  _copy (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New RangeMarkerBarDrag\n");
	
	_drag_rect = new ArdourCanvas::SimpleRect (*_editor->time_line_group, 0.0, 0.0, 0.0, 
                                                   physical_screen_height (_editor->get_window()));
	_drag_rect->hide ();

	_drag_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_RangeDragRect.get();
	_drag_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_RangeDragRect.get();
}

void
RangeMarkerBarDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	if (_editor->session() == 0) {
		return;
	}

	Gdk::Cursor* cursor = 0;

	if (!_editor->temp_location) {
		_editor->temp_location = new Location (*_editor->session());
	}

	switch (_operation) {
	case CreateRangeMarker:
	case CreateTransportMarker:
	case CreateCDMarker:

		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
			_copy = true;
		} else {
			_copy = false;
		}
		cursor = _editor->selector_cursor;
		break;
	}

	Drag::start_grab (event, cursor);

	_editor->show_verbose_time_cursor (adjusted_current_frame (event), 10);
}

void
RangeMarkerBarDrag::motion (GdkEvent* event, bool first_move)
{
	framepos_t start = 0;
	framepos_t end = 0;
	ArdourCanvas::SimpleRect *crect;

	switch (_operation) {
	case CreateRangeMarker:
		crect = _editor->range_bar_drag_rect;
		break;
	case CreateTransportMarker:
		crect = _editor->transport_bar_drag_rect;
		break;
	case CreateCDMarker:
		crect = _editor->cd_marker_bar_drag_rect;
		break;
	default:
		cerr << "Error: unknown range marker op passed to Editor::drag_range_markerbar_op ()" << endl;
		return;
		break;
	}

	framepos_t const pf = adjusted_current_frame (event);

	if (_operation == CreateRangeMarker || _operation == CreateTransportMarker || _operation == CreateCDMarker) {
		framepos_t grab = grab_frame ();
		_editor->snap_to (grab);
		
		if (pf < grab_frame()) {
			start = pf;
			end = grab;
		} else {
			end = pf;
			start = grab;
		}

		/* first drag: Either add to the selection
		   or create a new selection.
		*/

		if (first_move) {

			_editor->temp_location->set (start, end);

			crect->show ();

			update_item (_editor->temp_location);
			_drag_rect->show();
			//_drag_rect->raise_to_top();

		}
	}

	if (event->button.x >= _editor->horizontal_position() + _editor->_canvas_width) {
		_editor->start_canvas_autoscroll (1, 0);
	}

	if (start != end) {
		_editor->temp_location->set (start, end);

		double x1 = _editor->frame_to_pixel (start);
		double x2 = _editor->frame_to_pixel (end);
		crect->property_x1() = x1;
		crect->property_x2() = x2;

		update_item (_editor->temp_location);
	}

	_editor->show_verbose_time_cursor (pf, 10);

}

void
RangeMarkerBarDrag::finished (GdkEvent* event, bool movement_occurred)
{
	Location * newloc = 0;
	string rangename;
	int flags;

	if (movement_occurred) {
		motion (event, false);
		_drag_rect->hide();

		switch (_operation) {
		case CreateRangeMarker:
		case CreateCDMarker:
		    {
			_editor->begin_reversible_command (_("new range marker"));
			XMLNode &before = _editor->session()->locations()->get_state();
			_editor->session()->locations()->next_available_name(rangename,"unnamed");
			if (_operation == CreateCDMarker) {
				flags = Location::IsRangeMarker | Location::IsCDMarker;
				_editor->cd_marker_bar_drag_rect->hide();
			}
			else {
				flags = Location::IsRangeMarker;
				_editor->range_bar_drag_rect->hide();
			}
			newloc = new Location (
				*_editor->session(), _editor->temp_location->start(), _editor->temp_location->end(), rangename, (Location::Flags) flags
				);
			
			_editor->session()->locations()->add (newloc, true);
			XMLNode &after = _editor->session()->locations()->get_state();
			_editor->session()->add_command(new MementoCommand<Locations>(*(_editor->session()->locations()), &before, &after));
			_editor->commit_reversible_command ();
			break;
		    }

		case CreateTransportMarker:
			// popup menu to pick loop or punch
			_editor->new_transport_marker_context_menu (&event->button, _item);
			break;
		}
	} else {
		/* just a click, no pointer movement. remember that context menu stuff was handled elsewhere */

		if (Keyboard::no_modifier_keys_pressed (&event->button) && _operation != CreateCDMarker) {

			framepos_t start;
			framepos_t end;

			_editor->session()->locations()->marks_either_side (grab_frame(), start, end);

			if (end == max_frames) {
				end = _editor->session()->current_end_frame ();
			}

			if (start == max_frames) {
				start = _editor->session()->current_start_frame ();
			}

			switch (_editor->mouse_mode) {
			case MouseObject:
				/* find the two markers on either side and then make the selection from it */
				_editor->select_all_within (start, end, 0.0f, FLT_MAX, _editor->track_views, Selection::Set, false);
				break;

			case MouseRange:
				/* find the two markers on either side of the click and make the range out of it */
				_editor->selection->set (start, end);
				break;

			default:
				break;
			}
		}
	}

	_editor->stop_canvas_autoscroll ();
}

void
RangeMarkerBarDrag::aborted ()
{
	/* XXX: TODO */
}

void
RangeMarkerBarDrag::update_item (Location* location)
{
	double const x1 = _editor->frame_to_pixel (location->start());
	double const x2 = _editor->frame_to_pixel (location->end());

	_drag_rect->property_x1() = x1;
	_drag_rect->property_x2() = x2;
}

MouseZoomDrag::MouseZoomDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
{
	DEBUG_TRACE (DEBUG::Drags, "New MouseZoomDrag\n");
}

void
MouseZoomDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event, _editor->zoom_cursor);
	_editor->show_verbose_time_cursor (adjusted_current_frame (event), 10);
}

void
MouseZoomDrag::motion (GdkEvent* event, bool first_move)
{
	framepos_t start;
	framepos_t end;

	framepos_t const pf = adjusted_current_frame (event);

	framepos_t grab = grab_frame ();
	_editor->snap_to_with_modifier (grab, event);

	/* base start and end on initial click position */
	if (pf < grab) {
		start = pf;
		end = grab;
	} else {
		end = pf;
		start = grab;
	}

	if (start != end) {

		if (first_move) {
			_editor->zoom_rect->show();
			_editor->zoom_rect->raise_to_top();
		}

		_editor->reposition_zoom_rect(start, end);

		_editor->show_verbose_time_cursor (pf, 10);
	}
}

void
MouseZoomDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		motion (event, false);

		if (grab_frame() < last_pointer_frame()) {
			_editor->temporal_zoom_by_frame (grab_frame(), last_pointer_frame(), "mouse zoom");
		} else {
			_editor->temporal_zoom_by_frame (last_pointer_frame(), grab_frame(), "mouse zoom");
		}
	} else {
		_editor->temporal_zoom_to_frame (false, grab_frame());
		/*
		temporal_zoom_step (false);
		center_screen (grab_frame());
		*/
	}

	_editor->zoom_rect->hide();
}

void
MouseZoomDrag::aborted ()
{
	_editor->zoom_rect->hide ();
}

NoteDrag::NoteDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
	, _cumulative_dx (0)
	, _cumulative_dy (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New NoteDrag\n");

	_primary = dynamic_cast<CanvasNoteEvent*> (_item);
	_region = &_primary->region_view ();
	_note_height = _region->midi_stream_view()->note_height ();
}

void
NoteDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);

	if (!(_was_selected = _primary->selected())) {

		/* tertiary-click means extend selection - we'll do that on button release,
		   so don't add it here, because otherwise we make it hard to figure
		   out the "extend-to" range.
		*/

		bool extend = Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier);

		if (!extend) {
			bool add = Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier);

			if (add) {
				_region->note_selected (_primary, true);
			} else {
				_region->unique_select (_primary);
			}
		}
	}
}

/** @return Current total drag x change in frames */
frameoffset_t
NoteDrag::total_dx () const
{
	/* dx in frames */
	frameoffset_t const dx = _editor->unit_to_frame (_drags->current_pointer_x() - grab_x());

	/* primary note time */
	frameoffset_t const n = _region->beats_to_frames (_primary->note()->time ());
	
	/* new time of the primary note relative to the region position */
	frameoffset_t const st = n + dx;

	/* snap and return corresponding delta */
	return _region->snap_frame_to_frame (st) - n;
}

/** @return Current total drag y change in notes */
int8_t
NoteDrag::total_dy () const
{
	/* this is `backwards' to make increasing note number go in the right direction */
	double const dy = _drags->current_pointer_y() - grab_y();

	/* dy in notes */
	int8_t ndy = 0;

	if (abs (dy) >= _note_height) {
		if (dy > 0) {
			ndy = (int8_t) ceil (dy / _note_height / 2.0);
		} else {
			ndy = (int8_t) floor (dy / _note_height / 2.0);
		}
	}

 	return ndy;
}
	

void
NoteDrag::motion (GdkEvent *, bool)
{
	/* Total change in x and y since the start of the drag */
	frameoffset_t const dx = total_dx ();
	int8_t const dy = total_dy ();

	/* Now work out what we have to do to the note canvas items to set this new drag delta */
	double const tdx = _editor->frame_to_unit (dx) - _cumulative_dx;
	double const tdy = dy * _note_height - _cumulative_dy;

	if (tdx || tdy) {
		_region->move_selection (tdx, tdy);
		_cumulative_dx += tdx;
		_cumulative_dy += tdy;

                char buf[12];
                snprintf (buf, sizeof (buf), "%s (%d)", Evoral::midi_note_name (_primary->note()->note() + dy).c_str(),
                          (int) floor (_primary->note()->note() + dy));
		
		_editor->show_verbose_canvas_cursor_with (buf);
        }
}

void
NoteDrag::finished (GdkEvent* ev, bool moved)
{
	if (!moved) {
		if (_editor->current_mouse_mode() == Editing::MouseObject) {

			if (_was_selected) {
				bool add = Keyboard::modifier_state_equals (ev->button.state, Keyboard::PrimaryModifier);
				if (add) {
					_region->note_deselected (_primary);
				}
			} else {
				bool extend = Keyboard::modifier_state_equals (ev->button.state, Keyboard::TertiaryModifier);
				bool add = Keyboard::modifier_state_equals (ev->button.state, Keyboard::PrimaryModifier);

				if (!extend && !add && _region->selection_size() > 1) {
					_region->unique_select (_primary);
				} else if (extend) {
					_region->note_selected (_primary, true, true);
				} else {
					/* it was added during button press */
				}
			}
		}
	} else {
		_region->note_dropped (_primary, total_dx(), - total_dy());
	}
}

void
NoteDrag::aborted ()
{
	/* XXX: TODO */
}

AutomationRangeDrag::AutomationRangeDrag (Editor* editor, ArdourCanvas::Item* item, list<AudioRange> const & r)
	: Drag (editor, item)
	, _ranges (r)
	, _nothing_to_drag (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New AutomationRangeDrag\n");
	
	_atav = reinterpret_cast<AutomationTimeAxisView*> (_item->get_data ("trackview"));
	assert (_atav);

	/* get all lines in the automation view */
	list<boost::shared_ptr<AutomationLine> > lines = _atav->lines ();

	/* find those that overlap the ranges being dragged */
	list<boost::shared_ptr<AutomationLine> >::iterator i = lines.begin ();
	while (i != lines.end ()) {
		list<boost::shared_ptr<AutomationLine> >::iterator j = i;
		++j;

		pair<framepos_t, framepos_t> const r = (*i)->get_point_x_range ();

		/* check this range against all the AudioRanges that we are using */
		list<AudioRange>::const_iterator k = _ranges.begin ();
		while (k != _ranges.end()) {
			if (k->coverage (r.first, r.second) != OverlapNone) {
				break;
			}
			++k;
		}

		/* add it to our list if it overlaps at all */
		if (k != _ranges.end()) {
			Line n;
			n.line = *i;
			n.state = 0;
			n.range = r;
			_lines.push_back (n);
		}

		i = j;
	}

	/* Now ::lines contains the AutomationLines that somehow overlap our drag */
}

void
AutomationRangeDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	/* Get line states before we start changing things */
	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		i->state = &i->line->get_state ();
	}

	if (_ranges.empty()) {

		/* No selected time ranges: drag all points */
		for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
			uint32_t const N = i->line->npoints ();
			for (uint32_t j = 0; j < N; ++j) {
				i->points.push_back (i->line->nth (j));
			}
		}
		
	} else {

		for (list<AudioRange>::const_iterator i = _ranges.begin(); i != _ranges.end(); ++i) {

			framecnt_t const half = (i->start + i->end) / 2;
			
			/* find the line that this audio range starts in */
			list<Line>::iterator j = _lines.begin();
			while (j != _lines.end() && (j->range.first > i->start || j->range.second < i->start)) {
				++j;
			}

			if (j != _lines.end()) {
				boost::shared_ptr<AutomationList> the_list = j->line->the_list ();
				
				/* j is the line that this audio range starts in; fade into it;
				   64 samples length plucked out of thin air.
				*/

				framepos_t a = i->start + 64;
				if (a > half) {
					a = half;
				}

				double const p = j->line->time_converter().from (i->start - j->line->time_converter().origin_b ());
				double const q = j->line->time_converter().from (a - j->line->time_converter().origin_b ());

				the_list->add (p, the_list->eval (p));
				j->line->add_always_in_view (p);
				the_list->add (q, the_list->eval (q));
				j->line->add_always_in_view (q);
			}

			/* same thing for the end */
			
			j = _lines.begin();
			while (j != _lines.end() && (j->range.first > i->end || j->range.second < i->end)) {
				++j;
			}

			if (j != _lines.end()) {
				boost::shared_ptr<AutomationList> the_list = j->line->the_list ();
				
				/* j is the line that this audio range starts in; fade out of it;
				   64 samples length plucked out of thin air.
				*/
				
				framepos_t b = i->end - 64;
				if (b < half) {
					b = half;
				}

				double const p = j->line->time_converter().from (b - j->line->time_converter().origin_b ());
				double const q = j->line->time_converter().from (i->end - j->line->time_converter().origin_b ());
				
				the_list->add (p, the_list->eval (p));
				j->line->add_always_in_view (p);
				the_list->add (q, the_list->eval (q));
				j->line->add_always_in_view (q);
			}
		}

		_nothing_to_drag = true;

		/* Find all the points that should be dragged and put them in the relevant
		   points lists in the Line structs.
		*/

		for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {

			uint32_t const N = i->line->npoints ();
			for (uint32_t j = 0; j < N; ++j) {

				/* here's a control point on this line */
				ControlPoint* p = i->line->nth (j);
				double const w = i->line->time_converter().to ((*p->model())->when) + i->line->time_converter().origin_b ();

				/* see if it's inside a range */
				list<AudioRange>::const_iterator k = _ranges.begin ();
				while (k != _ranges.end() && (k->start >= w || k->end <= w)) {
					++k;
				}

				if (k != _ranges.end()) {
					/* dragging this point */
					_nothing_to_drag = false;
					i->points.push_back (p);
				}
			}
		}
	}

	if (_nothing_to_drag) {
		return;
	}

	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		i->line->start_drag_multiple (i->points, 1 - (_drags->current_pointer_y() / i->line->height ()), i->state);
	}
}

void
AutomationRangeDrag::motion (GdkEvent* event, bool first_move)
{
	if (_nothing_to_drag) {
		return;
	}

	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		float const f = 1 - (_drags->current_pointer_y() / i->line->height());

		/* we are ignoring x position for this drag, so we can just pass in anything */
		i->line->drag_motion (0, f, true, false);
	}
}

void
AutomationRangeDrag::finished (GdkEvent* event, bool)
{
	if (_nothing_to_drag) {
		return;
	}
	
	motion (event, false);
	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		i->line->end_drag ();
		i->line->clear_always_in_view ();
	}

	_editor->session()->commit_reversible_command ();
}

void
AutomationRangeDrag::aborted ()
{
	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		i->line->clear_always_in_view ();
		i->line->reset ();
	}
}

DraggingView::DraggingView (RegionView* v, RegionDrag* parent)
	: view (v)
{
	time_axis_view = parent->find_time_axis_view (&v->get_time_axis_view ());
	layer = v->region()->layer ();
	initial_y = v->get_canvas_group()->property_y ();
	initial_playlist = v->region()->playlist ();
}
