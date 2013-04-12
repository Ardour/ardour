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

#ifdef WAF_BUILD
#include "gtk2ardour-config.h"
#endif

#include <stdint.h>
#include <algorithm>

#include "pbd/memento_command.h"
#include "pbd/basename.h"
#include "pbd/stateful_diff_command.h"

#include "gtkmm2ext/utils.h"

#include "ardour/audioengine.h"
#include "ardour/audioregion.h"
#include "ardour/dB.h"
#include "ardour/midi_region.h"
#include "ardour/operations.h"
#include "ardour/region_factory.h"
#include "ardour/session.h"

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
#include "selection.h"
#include "midi_selection.h"
#include "automation_time_axis.h"
#include "debug.h"
#include "editor_cursors.h"
#include "mouse_cursors.h"
#include "note_base.h"
#include "patch_change.h"
#include "verbose_cursor.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace Editing;
using namespace ArdourCanvas;

using Gtkmm2ext::Keyboard;

double ControlPointDrag::_zero_gain_fraction = -1.0;

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

	if (!_drags.empty ()) {
		_editor->set_follow_playhead (_old_follow_playhead, false);
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
	d->set_manager (this);
	_drags.push_back (d);
	start_grab (e, c);
}

void
DragManager::start_grab (GdkEvent* e, Gdk::Cursor* c)
{
	/* Prevent follow playhead during the drag to be nice to the user */
	_old_follow_playhead = _editor->follow_playhead ();
	_editor->set_follow_playhead (false);

	_current_pointer_frame = _editor->canvas_event_frame (e, &_current_pointer_x, &_current_pointer_y);

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

	_editor->set_follow_playhead (_old_follow_playhead, false);

	return r;
}

bool
DragManager::motion_handler (GdkEvent* e, bool from_autoscroll)
{
	bool r = false;

	_current_pointer_frame = _editor->canvas_event_frame (e, &_current_pointer_x, &_current_pointer_y);

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
Drag::swap_grab (ArdourCanvas::Item* new_item, Gdk::Cursor* cursor, uint32_t /*time*/)
{
	_item->ungrab ();
	_item = new_item;

	if (cursor == 0) {
		_item->grab ();
	} else {
		_item->grab ();
	}
}

void
Drag::start_grab (GdkEvent* event, Gdk::Cursor *cursor)
{
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

	_raw_grab_frame = _editor->canvas_event_frame (event, &_grab_x, &_grab_y);
	setup_pointer_frame_offset ();
	_grab_frame = adjusted_frame (_raw_grab_frame, event);
	_last_pointer_frame = _grab_frame;
	_last_pointer_x = _grab_x;
	_last_pointer_y = _grab_y;

	if (cursor == 0) {
		_item->grab ();
			     
	} else {
		/* CAIROCANVAS need a variant here that passes *cursor */
		_item->grab ();

	}

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

	_item->ungrab ();

	finished (event, _move_threshold_passed);

	_editor->verbose_cursor()->hide ();

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
	if (_move_threshold_passed &&
	    (!x_movement_matters() || _last_pointer_frame == adjusted_current_frame (event)) &&
	    (!y_movement_matters() || _last_pointer_y == _drags->current_pointer_y ()) ) {
		return false;
	}

	pair<framecnt_t, int> const threshold = move_threshold ();

	bool const old_move_threshold_passed = _move_threshold_passed;

	if (!from_autoscroll && !_move_threshold_passed) {

		bool const xp = (::llabs (_drags->current_pointer_frame () - _raw_grab_frame) >= threshold.first);
		bool const yp = (::fabs ((_drags->current_pointer_y () - _grab_y)) >= threshold.second);

		_move_threshold_passed = ((xp && x_movement_matters()) || (yp && y_movement_matters()));
	}

	if (active (_editor->mouse_mode) && _move_threshold_passed) {

		if (event->motion.state & Gdk::BUTTON1_MASK || event->motion.state & Gdk::BUTTON2_MASK) {
			if (!from_autoscroll) {
				bool const moving_left = _drags->current_pointer_x() < _last_pointer_x;
				bool const moving_up = _drags->current_pointer_y() < _last_pointer_y;
				_editor->maybe_autoscroll (true, allow_vertical_autoscroll (), moving_left, moving_up);
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
		_item->ungrab ();
	}

	aborted (_move_threshold_passed);

	_editor->stop_canvas_autoscroll ();
	_editor->verbose_cursor()->hide ();
}

void
Drag::show_verbose_cursor_time (framepos_t frame)
{
	_editor->verbose_cursor()->set_time (
		frame,
		_drags->current_pointer_x() + 10 - _editor->horizontal_position(),
		_drags->current_pointer_y() + 10 - _editor->vertical_adjustment.get_value()
		);

	_editor->verbose_cursor()->show ();
}

void
Drag::show_verbose_cursor_duration (framepos_t start, framepos_t end, double xoffset)
{
	_editor->verbose_cursor()->show (xoffset);

	_editor->verbose_cursor()->set_duration (
		start, end,
		_drags->current_pointer_x() + 10 - _editor->horizontal_position(),
		_drags->current_pointer_y() + 10 - _editor->vertical_adjustment.get_value()
		);
}

void
Drag::show_verbose_cursor_text (string const & text)
{
	_editor->verbose_cursor()->show ();

	_editor->verbose_cursor()->set (
		text,
		_drags->current_pointer_x() + 10 - _editor->horizontal_position(),
		_drags->current_pointer_y() + 10 - _editor->vertical_adjustment.get_value()
		);
}

boost::shared_ptr<Region>
Drag::add_midi_region (MidiTimeAxisView* view)
{
	if (_editor->session()) {
		const TempoMap& map (_editor->session()->tempo_map());
		framecnt_t pos = grab_frame();
		const Meter& m = map.meter_at (pos);
		/* not that the frame rate used here can be affected by pull up/down which
		   might be wrong.
		*/
		framecnt_t len = m.frames_per_bar (map.tempo_at (pos), _editor->session()->frame_rate());
		return view->add_region (grab_frame(), len, true);
	}

	return boost::shared_ptr<Region>();
}

struct EditorOrderTimeAxisViewSorter {
    bool operator() (TimeAxisView* a, TimeAxisView* b) {
	    RouteTimeAxisView* ra = dynamic_cast<RouteTimeAxisView*> (a);
	    RouteTimeAxisView* rb = dynamic_cast<RouteTimeAxisView*> (b);
	    assert (ra && rb);
	    return ra->route()->order_key (EditorSort) < rb->route()->order_key (EditorSort);
    }
};

RegionDrag::RegionDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: Drag (e, i),
	  _primary (p)
{
	_editor->visible_order_range (&_visible_y_low, &_visible_y_high);

	/* Make a list of tracks to refer to during the drag; we include hidden tracks,
	   as some of the regions we are dragging may be on such tracks.
	*/

	TrackViewList track_views = _editor->track_views;
	track_views.sort (EditorOrderTimeAxisViewSorter ());

	for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		_time_axis_views.push_back (*i);
		
		TimeAxisView::Children children_list = (*i)->get_child_list ();
		for (TimeAxisView::Children::iterator j = children_list.begin(); j != children_list.end(); ++j) {
			_time_axis_views.push_back (j->get());
		}
	}

	/* the list of views can be empty at this point if this is a region list-insert drag
	 */

	for (list<RegionView*>::const_iterator i = v.begin(); i != v.end(); ++i) {
		_views.push_back (DraggingView (*i, this));
	}

	RegionView::RegionViewGoingAway.connect (death_connection, invalidator (*this), boost::bind (&RegionDrag::region_going_away, this, _1), gui_context());
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

/** Given a TimeAxisView, return the index of it into the _time_axis_views vector,
 *  or -1 if it is not found.
 */
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
	DEBUG_TRACE (DEBUG::Drags, "New RegionMotionDrag\n");
}

void
RegionMotionDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	show_verbose_cursor_time (_last_frame_position);

	pair<TimeAxisView*, double> const tv = _editor->trackview_by_y_position (_drags->current_pointer_y ());
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

	if (*pending_region_position > max_framepos - _primary->region()->length()) {
		*pending_region_position = _last_frame_position;
	}

	double dx = 0;

	/* in locked edit mode, reverse the usual meaning of _x_constrained */
	bool const x_move_allowed = Config->get_edit_mode() == Lock ? _x_constrained : !_x_constrained;

	if ((*pending_region_position != _last_frame_position) && x_move_allowed) {

		/* x movement since last time (in pixels) */
		dx = (static_cast<double> (*pending_region_position) - _last_frame_position) / _editor->frames_per_pixel;

		/* total x movement */
		framecnt_t total_dx = *pending_region_position;
		if (regions_came_from_canvas()) {
			total_dx = total_dx - grab_frame ();
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
RegionMotionDrag::y_movement_allowed (int delta_track, double delta_layer) const
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

		double const l = i->layer + delta_layer;

		/* Note that we allow layer to be up to 0.5 below zero, as this is used by `Expanded'
		   mode to allow the user to place a region below another on layer 0.
		*/
		if (delta_track == 0 && (l < -0.5 || l >= int (to->view()->layers()))) {
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
	assert (!_views.empty ());

	/* Find the TimeAxisView that the pointer is now over */
	pair<TimeAxisView*, double> const tv = _editor->trackview_by_y_position (_drags->current_pointer_y ());

	if (first_move && tv.first->view()->layer_display() == Stacked) {
		tv.first->view()->set_layer_display (Expanded);
	}

	/* Bail early if we're not over a track */
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv.first);
	if (!rtv || !rtv->is_track()) {
		_editor->verbose_cursor()->hide ();
		return;
	}

	/* Note: time axis views in this method are often expressed as an index into the _time_axis_views vector */

	/* Here's the current pointer position in terms of time axis view and layer */
	int const current_pointer_time_axis_view = find_time_axis_view (tv.first);
	double const current_pointer_layer = tv.first->layer_display() == Overlaid ? 0 : tv.second;

	/* Work out the change in x */
	framepos_t pending_region_position;
	double const x_delta = compute_x_delta (event, &pending_region_position);

	/* Work out the change in y */
	int delta_time_axis_view = current_pointer_time_axis_view - _last_pointer_time_axis_view;
	double delta_layer = current_pointer_layer - _last_pointer_layer;

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

		if (rv->region()->locked() || rv->region()->video_locked()) {
			continue;
		}

		if (first_move) {

			rv->drag_start (); 

			/* Absolutely no idea why this is necessary, but it is; without
			   it, the region view disappears after the reparent.
			*/
			_editor->update_canvas_now ();
			
			/* Reparent to a non scrolling group so that we can keep the
			   region selection above all time axis views.
			   Reparenting means that we will have to move the region view
			   later, as the two parent groups have different coordinates.
			*/

			rv->get_canvas_group()->reparent (_editor->_region_motion_group);
			
			rv->fake_set_opaque (true);
		}

		/* If we have moved tracks, we'll fudge the layer delta so that the
		   region gets moved back onto layer 0 on its new track; this avoids
		   confusion when dragging regions from non-zero layers onto different
		   tracks.
		*/
		double this_delta_layer = delta_layer;
		if (delta_time_axis_view != 0) {
			this_delta_layer = - i->layer;
		}

		/* The TimeAxisView that this region is now on */
		TimeAxisView* tv = _time_axis_views[i->time_axis_view + delta_time_axis_view];

		/* Ensure it is moved from stacked -> expanded if appropriate */
		if (tv->view()->layer_display() == Stacked) {
			tv->view()->set_layer_display (Expanded);
		}
		
		/* We're only allowed to go -ve in layer on Expanded views */
		if (tv->view()->layer_display() != Expanded && (i->layer + this_delta_layer) < 0) {
			this_delta_layer = - i->layer;
		}
		
		/* Set height */
		rv->set_height (tv->view()->child_height ());
		
		/* Update show/hidden status as the region view may have come from a hidden track,
		   or have moved to one.
		*/
		if (tv->hidden ()) {
			rv->get_canvas_group()->hide ();
		} else {
			rv->get_canvas_group()->show ();
		}

		/* Update the DraggingView */
		i->time_axis_view += delta_time_axis_view;
		i->layer += this_delta_layer;

		if (_brushing) {
			_editor->mouse_brush_insert_region (rv, pending_region_position);
		} else {
			double x = 0;
			double y = 0;

			/* Get the y coordinate of the top of the track that this region is now on */
			tv->canvas_display()->item_to_canvas (x, y);
			y += _editor->get_trackview_group_vertical_offset();
			
			/* And adjust for the layer that it should be on */
			StreamView* cv = tv->view ();
			switch (cv->layer_display ()) {
			case Overlaid:
				break;
			case Stacked:
				y += (cv->layers() - i->layer - 1) * cv->child_height ();
				break;
			case Expanded:
				y += (cv->layers() - i->layer - 0.5) * 2 * cv->child_height ();
				break;
			}

			/* Now move the region view */
			rv->move (x_delta, y - rv->get_canvas_group()->position().y);
		}

	} /* foreach region */

	_total_x_delta += x_delta;

	if (x_delta != 0 && !_brushing) {
		show_verbose_cursor_time (_last_frame_position);
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
			boost::shared_ptr<Region> region_copy = RegionFactory::create (original, true);
			region_copy->set_position (original->position());

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
RegionMotionDrag::finished (GdkEvent *, bool)
{
	for (vector<TimeAxisView*>::iterator i = _time_axis_views.begin(); i != _time_axis_views.end(); ++i) {
		if (!(*i)->view()) {
			continue;
		}

		if ((*i)->view()->layer_display() == Expanded) {
			(*i)->view()->set_layer_display (Stacked);
		}
	}
}

void
RegionMoveDrag::finished (GdkEvent* ev, bool movement_occurred)
{
	RegionMotionDrag::finished (ev, movement_occurred);
	
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

	assert (!_views.empty ());

	/* We might have hidden region views so that they weren't visible during the drag
	   (when they have been reparented).  Now everything can be shown again, as region
	   views are back in their track parent groups.
	*/
	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->get_canvas_group()->show ();
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

	_editor->maybe_locate_with_edit_preroll (_editor->get_selection().regions.start());
}

void
RegionMoveDrag::finished_copy (bool const changed_position, bool const /*changed_tracks*/, framecnt_t const drag_delta)
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
		_editor->begin_reversible_command (Operations::fixed_time_region_copy);
	} else {
		_editor->begin_reversible_command (Operations::region_copy);
	}

	/* insert the regions into their new playlists */
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {

		if (i->view->region()->locked() || i->view->region()->video_locked()) {
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
	set<RouteTimeAxisView*> views_to_update;

	if (_brushing) {
		/* all changes were made during motion event handlers */
		_editor->commit_reversible_command ();
		return;
	}

	if (_x_constrained) {
		_editor->begin_reversible_command (_("fixed time region drag"));
	} else {
		_editor->begin_reversible_command (Operations::region_drag);
	}

	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ) {

		RegionView* rv = i->view;

		RouteTimeAxisView* const dest_rtv = dynamic_cast<RouteTimeAxisView*> (_time_axis_views[i->time_axis_view]);
		double const dest_layer = i->layer;

		if (rv->region()->locked() || rv->region()->video_locked()) {
			++i;
			continue;
		}

		views_to_update.insert (dest_rtv);

		framepos_t where;

		if (changed_position && !_x_constrained) {
			where = rv->region()->position() - drag_delta;
		} else {
			where = rv->region()->position();
		}

		if (changed_tracks) {

			/* insert into new playlist */

			RegionView* new_view = insert_region_into_playlist (
				RegionFactory::create (rv->region (), true), dest_rtv, dest_layer, where, modified_playlists
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

			rv->get_canvas_group()->reparent (dest_rtv->view()->canvas_item());
			rv->get_canvas_group()->set_y_position (i->initial_y);
			rv->drag_end ();

			/* just change the model */

			boost::shared_ptr<Playlist> playlist = dest_rtv->playlist();

			if (dest_rtv->view()->layer_display() == Stacked || dest_rtv->view()->layer_display() == Expanded) {
				playlist->set_layer (rv->region(), dest_layer);
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

			rv->region()->set_position (where);

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

	/* We have futzed with the layering of canvas items on our streamviews.
	   If any region changed layer, this will have resulted in the stream
	   views being asked to set up their region views, and all will be well.
	   If not, we might now have badly-ordered region views.  Ask the StreamViews
	   involved to sort themselves out, just in case.
	*/

	for (set<RouteTimeAxisView*>::iterator i = views_to_update.begin(); i != views_to_update.end(); ++i) {
		(*i)->view()->playlist_layered ((*i)->track ());
	}
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

	if (dest_rtv->view()->layer_display() == Stacked || dest_rtv->view()->layer_display() == Expanded) {
		dest_playlist->set_layer (region, dest_layer);
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
			_editor->session()->add_command (c);
		} else {
			delete c;
		}
	}
}


void
RegionMoveDrag::aborted (bool movement_occurred)
{
	if (_copy) {

		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			delete i->view;
		}

		_views.clear ();

	} else {
		RegionMotionDrag::aborted (movement_occurred);
	}
}

void
RegionMotionDrag::aborted (bool)
{
	for (vector<TimeAxisView*>::iterator i = _time_axis_views.begin(); i != _time_axis_views.end(); ++i) {
		if ((*i)->view()->layer_display() == Expanded) {
			(*i)->view()->set_layer_display (Stacked);
		}
	}
	
	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		RegionView* rv = i->view;
		TimeAxisView* tv = &(rv->get_time_axis_view ());
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);
		assert (rtv);
		rv->get_canvas_group()->reparent (rtv->view()->canvas_item());
		rv->get_canvas_group()->set_y_position (0);
		rv->drag_end ();
		rv->fake_set_opaque (false);
		rv->move (-_total_x_delta, 0);
		rv->set_height (rtv->view()->child_height ());
	}

	_editor->update_canvas_now ();
}

/** @param b true to brush, otherwise false.
 *  @param c true to make copies of the regions being moved, otherwise false.
 */
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
RegionMoveDrag::setup_pointer_frame_offset ()
{
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

	_primary->get_canvas_group()->reparent (dest_rtv->view()->canvas_item());
	_primary->get_canvas_group()->set_y_position (0);

	boost::shared_ptr<Playlist> playlist = dest_rtv->playlist();

	_editor->begin_reversible_command (Operations::insert_region);
	playlist->clear_changes ();
	playlist->add_region (_primary->region (), _last_frame_position);
	_editor->session()->add_command (new StatefulDiffCommand (playlist));
	_editor->commit_reversible_command ();

	delete _primary;
	_primary = 0;
	_views.clear ();
}

void
RegionInsertDrag::aborted (bool)
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

	pair<TimeAxisView*, double> const tvp = _editor->trackview_by_y_position (_drags->current_pointer_y ());
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*> (tvp.first);

	/* The region motion is only processed if the pointer is over
	   an audio track.
	*/

	if (!tv || !tv->is_track()) {
		/* To make sure we hide the verbose canvas cursor when the mouse is
		   not held over and audiotrack.
		*/
		_editor->verbose_cursor()->hide ();
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
RegionSpliceDrag::aborted (bool)
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
		_region = add_midi_region (_view);
		_view->playlist()->freeze ();
	} else {
		if (_region) {
			framepos_t const f = adjusted_current_frame (event);
			if (f < grab_frame()) {
				_region->set_position (f);
			}

			/* Don't use a zero-length region, and subtract 1 frame from the snapped length
			   so that if this region is duplicated, its duplicate starts on
			   a snap point rather than 1 frame after a snap point.  Otherwise things get
			   a bit confusing as if a region starts 1 frame after a snap point, one cannot
			   place snapped notes at the start of the region.
			*/

			framecnt_t const len = (framecnt_t) fabs (f - grab_frame () - 1);
			_region->set_length (len < 1 ? 1 : len);
		}
	}
}

void
RegionCreateDrag::finished (GdkEvent*, bool movement_occurred)
{
	if (!movement_occurred) {
		add_midi_region (_view);
	} else {
		_view->playlist()->thaw ();
	}
}

void
RegionCreateDrag::aborted (bool)
{
	if (_region) {
		_view->playlist()->thaw ();
	}

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
	NoteBase* cnote = reinterpret_cast<NoteBase*> (_item->get_data ("notebase"));
	assert (cnote);
	float x_fraction = cnote->mouse_x_fraction ();

	if (x_fraction > 0.0 && x_fraction < 0.25) {
		cursor = _editor->cursors()->left_side_trim;
	} else  {
		cursor = _editor->cursors()->right_side_trim;
	}

	Drag::start_grab (event, cursor);

	region = &cnote->region_view();

	double const region_start = region->get_position_pixels();
	double const middle_point = region_start + cnote->x0() + (cnote->x1() - cnote->x0()) / 2.0L;

	if (grab_x() <= middle_point) {
		cursor = _editor->cursors()->left_side_trim;
		at_front = true;
	} else {
		cursor = _editor->cursors()->right_side_trim;
		at_front = false;
	}

	_item->grab ();

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
		NoteBase* nb = reinterpret_cast<NoteBase*> (_item->get_data ("notebase"));
		assert (nb);
		(*r)->update_resizing (nb, at_front, _drags->current_pointer_x() - grab_x(), relative);
	}
}

void
NoteResizeDrag::finished (GdkEvent*, bool /*movement_occurred*/)
{
	MidiRegionSelection& ms (_editor->get_selection().midi_regions);
	for (MidiRegionSelection::iterator r = ms.begin(); r != ms.end(); ++r) {
		NoteBase* nb = reinterpret_cast<NoteBase*> (_item->get_data ("notebase"));
		assert (nb);
		(*r)->commit_resizing (nb, at_front, _drags->current_pointer_x() - grab_x(), relative);
	}
}

void
NoteResizeDrag::aborted (bool)
{
	MidiRegionSelection& ms (_editor->get_selection().midi_regions);
	for (MidiRegionSelection::iterator r = ms.begin(); r != ms.end(); ++r) {
		(*r)->abort_resizing ();
	}
}

AVDraggingView::AVDraggingView (RegionView* v)
	: view (v)
{
	initial_position = v->region()->position ();
}

VideoTimeLineDrag::VideoTimeLineDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
{
	DEBUG_TRACE (DEBUG::Drags, "New VideoTimeLineDrag\n");

	RegionSelection rs;
	TrackViewList empty;
	empty.clear();
	_editor->get_regions_after(rs, (framepos_t) 0, empty);
	std::list<RegionView*> views = rs.by_layer();

	for (list<RegionView*>::iterator i = views.begin(); i != views.end(); ++i) {
		RegionView* rv = (*i);
		if (!rv->region()->video_locked()) {
			continue;
		}
		_views.push_back (AVDraggingView (rv));
	}
}

void
VideoTimeLineDrag::start_grab (GdkEvent* event, Gdk::Cursor*)
{
	Drag::start_grab (event);
	if (_editor->session() == 0) {
		return;
	}

	_startdrag_video_offset=ARDOUR_UI::instance()->video_timeline->get_offset();
	_max_backwards_drag = (
			  ARDOUR_UI::instance()->video_timeline->get_duration()
			+ ARDOUR_UI::instance()->video_timeline->get_offset()
			- ceil(ARDOUR_UI::instance()->video_timeline->get_apv())
			);

	for (list<AVDraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		if (i->initial_position < _max_backwards_drag || _max_backwards_drag < 0) {
			_max_backwards_drag = ARDOUR_UI::instance()->video_timeline->quantify_frames_to_apv (i->initial_position);
		}
	}
	DEBUG_TRACE (DEBUG::Drags, string_compose("VideoTimeLineDrag: max backwards-drag: %1\n", _max_backwards_drag));

	char buf[128];
	Timecode::Time timecode;
	_editor->session()->sample_to_timecode(abs(_startdrag_video_offset), timecode, true /* use_offset */, false /* use_subframes */ );
	snprintf (buf, sizeof (buf), "Video Start:\n%c%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32, (_startdrag_video_offset<0?'-':' '), timecode.hours, timecode.minutes, timecode.seconds, timecode.frames);
	_editor->verbose_cursor()->set(buf, event->button.x + 10, event->button.y + 10);
	_editor->verbose_cursor()->show ();
}

void
VideoTimeLineDrag::motion (GdkEvent* event, bool first_move)
{
	if (_editor->session() == 0) {
		return;
	}
	if (ARDOUR_UI::instance()->video_timeline->is_offset_locked()) {
		return;
	}

	framecnt_t dt = adjusted_current_frame (event) - raw_grab_frame() + _pointer_frame_offset;
	dt = ARDOUR_UI::instance()->video_timeline->quantify_frames_to_apv(dt);

	if (_max_backwards_drag >= 0 && dt <= - _max_backwards_drag) {
		dt = - _max_backwards_drag;
	}

	ARDOUR_UI::instance()->video_timeline->set_offset(_startdrag_video_offset+dt);
	ARDOUR_UI::instance()->flush_videotimeline_cache(true);

	for (list<AVDraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		RegionView* rv = i->view;
		DEBUG_TRACE (DEBUG::Drags, string_compose("SHIFT REGION at %1 by %2\n", i->initial_position, dt));
		if (first_move) {
			rv->drag_start ();
			_editor->update_canvas_now ();
			rv->fake_set_opaque (true);
			rv->region()->clear_changes ();
			rv->region()->suspend_property_changes();
		}
		rv->region()->set_position(i->initial_position + dt);
		rv->region_changed(ARDOUR::Properties::position);
	}

	const framepos_t offset = ARDOUR_UI::instance()->video_timeline->get_offset();
	Timecode::Time timecode;
	Timecode::Time timediff;
	char buf[128];
	_editor->session()->sample_to_timecode(abs(offset), timecode, true /* use_offset */, false /* use_subframes */ );
	_editor->session()->sample_to_timecode(abs(dt), timediff, false /* use_offset */, false /* use_subframes */ );
	snprintf (buf, sizeof (buf),
			"%s\n%c%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32
			"\n%s\n%c%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32
			, _("Video Start:"),
				(offset<0?'-':' '), timecode.hours, timecode.minutes, timecode.seconds, timecode.frames
			, _("Diff:"),
				(dt<0?'-':' '), timediff.hours, timediff.minutes, timediff.seconds, timediff.frames
				);
	_editor->verbose_cursor()->set(buf, event->button.x + 10, event->button.y + 10);
	_editor->verbose_cursor()->show ();
}

void
VideoTimeLineDrag::finished (GdkEvent * /*event*/, bool movement_occurred)
{
	if (ARDOUR_UI::instance()->video_timeline->is_offset_locked()) {
		return;
	}

	if (!movement_occurred || ! _editor->session()) {
		return;
	}

	ARDOUR_UI::instance()->flush_videotimeline_cache(true);

	_editor->begin_reversible_command (_("Move Video"));

	XMLNode &before = ARDOUR_UI::instance()->video_timeline->get_state();
	ARDOUR_UI::instance()->video_timeline->save_undo();
	XMLNode &after = ARDOUR_UI::instance()->video_timeline->get_state();
	_editor->session()->add_command(new MementoCommand<VideoTimeLine>(*(ARDOUR_UI::instance()->video_timeline), &before, &after));

	for (list<AVDraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->drag_end();
		i->view->fake_set_opaque (false);
		i->view->region()->resume_property_changes ();

		_editor->session()->add_command (new StatefulDiffCommand (i->view->region()));
	}

	_editor->session()->maybe_update_session_range(
			std::max(ARDOUR_UI::instance()->video_timeline->get_offset(), (ARDOUR::frameoffset_t) 0),
			std::max(ARDOUR_UI::instance()->video_timeline->get_offset() + ARDOUR_UI::instance()->video_timeline->get_duration(), (ARDOUR::frameoffset_t) 0)
			);


	_editor->commit_reversible_command ();
	_editor->update_canvas_now ();
}

void
VideoTimeLineDrag::aborted (bool)
{
	if (ARDOUR_UI::instance()->video_timeline->is_offset_locked()) {
		return;
	}
	ARDOUR_UI::instance()->video_timeline->set_offset(_startdrag_video_offset);
	ARDOUR_UI::instance()->flush_videotimeline_cache(true);

	for (list<AVDraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->region()->resume_property_changes ();
		i->view->region()->set_position(i->initial_position);
	}
}

TrimDrag::TrimDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v, bool preserve_fade_anchor)
	: RegionDrag (e, i, p, v)
{
	DEBUG_TRACE (DEBUG::Drags, "New TrimDrag\n");
	_preserve_fade_anchor = preserve_fade_anchor;
}

void
TrimDrag::start_grab (GdkEvent* event, Gdk::Cursor*)
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
		/* Move the contents of the region around without changing the region bounds */
		_operation = ContentsTrim;
		Drag::start_grab (event, _editor->cursors()->trimmer);
	} else {
		/* These will get overridden for a point trim.*/
		if (pf < (region_start + region_length/2)) {
			/* closer to front */
			_operation = StartTrim;
			Drag::start_grab (event, _editor->cursors()->left_side_trim);
		} else {
			/* closer to end */
			_operation = EndTrim;
			Drag::start_grab (event, _editor->cursors()->right_side_trim);
		}
	}

	switch (_operation) {
	case StartTrim:
		show_verbose_cursor_time (region_start);
		for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
			i->view->trim_front_starting ();
		}
		break;
	case EndTrim:
		show_verbose_cursor_time (region_end);
		break;
	case ContentsTrim:
		show_verbose_cursor_time (pf);
		break;
	}

	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->region()->suspend_property_changes ();
	}
}

void
TrimDrag::motion (GdkEvent* event, bool first_move)
{
	RegionView* rv = _primary;

	double speed = 1.0;
	TimeAxisView* tvp = &_primary->get_time_axis_view ();
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);
	pair<set<boost::shared_ptr<Playlist> >::iterator,bool> insert_result;

	if (tv && tv->is_track()) {
		speed = tv->track()->speed();
	}

	framecnt_t const dt = adjusted_current_frame (event) - raw_grab_frame () + _pointer_frame_offset;

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

		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			RegionView* rv = i->view;
			rv->fake_set_opaque (false);
			rv->enable_display (false);
			rv->region()->playlist()->clear_owned_changes ();

			AudioRegionView* const arv = dynamic_cast<AudioRegionView*> (rv);

			if (arv) {
				arv->temporarily_hide_envelope ();
				arv->drag_start ();
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
			bool changed = i->view->trim_front (i->initial_position + dt, non_overlap_trim);
			if (changed && _preserve_fade_anchor) {
				AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
				if (arv) {
					double distance;
					double new_length;
					framecnt_t len;
					boost::shared_ptr<AudioRegion> ar (arv->audio_region());
					distance = _drags->current_pointer_x() - grab_x();
					len = ar->fade_in()->back()->when;
					new_length = len - _editor->pixel_to_sample (distance);
					new_length = ar->verify_xfade_bounds (new_length, true  /*START*/ );
					arv->reset_fade_in_shape_width (ar, new_length);  //the grey shape
				}
			}
		}
		break;

	case EndTrim:
		for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			bool changed = i->view->trim_end (i->initial_end + dt, non_overlap_trim);
			if (changed && _preserve_fade_anchor) {
				AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
				if (arv) {
					double distance;
					double new_length;
					framecnt_t len;
					boost::shared_ptr<AudioRegion> ar (arv->audio_region());
					distance = grab_x() - _drags->current_pointer_x();
					len = ar->fade_out()->back()->when;
					new_length = len - _editor->pixel_to_sample (distance);
					new_length = ar->verify_xfade_bounds (new_length, false  /*END*/ );
					arv->reset_fade_out_shape_width (ar, new_length);  //the grey shape
				}
			}
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
			if (last_pointer_frame() > adjusted_current_frame(event)) {
				left_direction = true;
			}

			if (left_direction) {
				frame_delta = (last_pointer_frame() - adjusted_current_frame(event));
			} else {
				frame_delta = (adjusted_current_frame(event) - last_pointer_frame());
			}

			for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				i->view->trim_contents (frame_delta, left_direction, swap_direction);
			}
		}
		break;
	}

	switch (_operation) {
	case StartTrim:
		show_verbose_cursor_time ((framepos_t) (rv->region()->position() / speed));
		break;
	case EndTrim:
		show_verbose_cursor_time ((framepos_t) (rv->region()->last_frame() / speed));
		break;
	case ContentsTrim:
		show_verbose_cursor_time (adjusted_current_frame (event));
		break;
	}
}


void
TrimDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		motion (event, false);

		if (_operation == StartTrim) {
			for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				{
					/* This must happen before the region's StatefulDiffCommand is created, as it may
					   `correct' (ahem) the region's _start from being negative to being zero.  It
					   needs to be zero in the undo record.
					*/
					i->view->trim_front_ending ();
				}
				if (_preserve_fade_anchor) {
					AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
					if (arv) {
						double distance;
						double new_length;
						framecnt_t len;
						boost::shared_ptr<AudioRegion> ar (arv->audio_region());
						distance = _drags->current_pointer_x() - grab_x();
						len = ar->fade_in()->back()->when;
						new_length = len - _editor->pixel_to_sample (distance);
						new_length = ar->verify_xfade_bounds (new_length, true  /*START*/ );
						ar->set_fade_in_length(new_length);
					}
				}
			}
		} else if (_operation == EndTrim) {
			for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				if (_preserve_fade_anchor) {
					AudioRegionView* arv = dynamic_cast<AudioRegionView*> (i->view);
					if (arv) {
						double distance;
						double new_length;
						framecnt_t len;
						boost::shared_ptr<AudioRegion> ar (arv->audio_region());
						distance = _drags->current_pointer_x() - grab_x();
						len = ar->fade_out()->back()->when;
						new_length = len - _editor->pixel_to_sample (distance);
						new_length = ar->verify_xfade_bounds (new_length, false  /*END*/ );
						ar->set_fade_out_length(new_length);
					}
				}
			}
		}

		if (!_views.empty()) {
			if (_operation == StartTrim) {
				_editor->maybe_locate_with_edit_preroll(
					_views.begin()->view->region()->position());
			}
			if (_operation == EndTrim) {
				_editor->maybe_locate_with_edit_preroll(
					_views.begin()->view->region()->position() +
					_views.begin()->view->region()->length());
			}
		}
	
		if (!_editor->selection->selected (_primary)) {
			_primary->thaw_after_trim ();
		} else {

			set<boost::shared_ptr<Playlist> > diffed_playlists;

			for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
                                i->view->thaw_after_trim ();
				i->view->enable_display (true);
				i->view->fake_set_opaque (true);

				/* Trimming one region may affect others on the playlist, so we need
				   to get undo Commands from the whole playlist rather than just the
				   region.  Use diffed_playlists to make sure we don't diff a given
				   playlist more than once.
				*/
				boost::shared_ptr<Playlist> p = i->view->region()->playlist ();
				if (diffed_playlists.find (p) == diffed_playlists.end()) {
					vector<Command*> cmds;
					p->rdiff (cmds);
					_editor->session()->add_commands (cmds);
					diffed_playlists.insert (p);
				}
			}
		}

		for (set<boost::shared_ptr<Playlist> >::iterator p = _editor->motion_frozen_playlists.begin(); p != _editor->motion_frozen_playlists.end(); ++p) {
			(*p)->thaw ();
		}

		_editor->motion_frozen_playlists.clear ();
		_editor->commit_reversible_command();

	} else {
		/* no mouse movement */
		_editor->point_trim (event, adjusted_current_frame (event));
	}

	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		if (_operation == StartTrim) {
			i->view->trim_front_ending ();
		}

		i->view->region()->resume_property_changes ();
	}
}

void
TrimDrag::aborted (bool movement_occurred)
{
	/* Our motion method is changing model state, so use the Undo system
	   to cancel.  Perhaps not ideal, as this will leave an Undo point
	   behind which may be slightly odd from the user's point of view.
	*/

	finished (0, true);

	if (movement_occurred) {
		_editor->undo ();
	}

	for (list<DraggingView>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		i->view->region()->resume_property_changes ();
	}
}

void
TrimDrag::setup_pointer_frame_offset ()
{
	list<DraggingView>::iterator i = _views.begin ();
	while (i != _views.end() && i->view != _primary) {
		++i;
	}

	if (i == _views.end()) {
		return;
	}

	switch (_operation) {
	case StartTrim:
		_pointer_frame_offset = raw_grab_frame() - i->initial_position;
		break;
	case EndTrim:
		_pointer_frame_offset = raw_grab_frame() - i->initial_end;
		break;
	case ContentsTrim:
		break;
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
	Drag::start_grab (event, cursor);
	show_verbose_cursor_time (adjusted_current_frame(event));
}

void
MeterMarkerDrag::setup_pointer_frame_offset ()
{
	_pointer_frame_offset = raw_grab_frame() - _marker->meter().frame();
}

void
MeterMarkerDrag::motion (GdkEvent* event, bool first_move)
{
	if (first_move) {

		// create a dummy marker for visual representation of moving the
		// section, because whether its a copy or not, we're going to 
		// leave or lose the original marker (leave if its a copy; lose if its
		// not, because we'll remove it from the map).
		
		MeterSection section (_marker->meter());

		if (!section.movable()) {
			return;
		}
		
		char name[64];
		snprintf (name, sizeof(name), "%g/%g", _marker->meter().divisions_per_bar(), _marker->meter().note_divisor ());
		
		_marker = new MeterMarker (
			*_editor,
			*_editor->meter_group,
			ARDOUR_UI::config()->canvasvar_MeterMarker.get(),
			name,
			*new MeterSection (_marker->meter())
		);
		
		/* use the new marker for the grab */
		swap_grab (&_marker->the_item(), 0, GDK_CURRENT_TIME);

		if (!_copy) {
			TempoMap& map (_editor->session()->tempo_map());
			/* get current state */
			before_state = &map.get_state();
			/* remove the section while we drag it */
			map.remove_meter (section, true);
		}
	}

	framepos_t const pf = adjusted_current_frame (event);
	_marker->set_position (pf);
	show_verbose_cursor_time (pf);
}

void
MeterMarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	motion (event, false);

	Timecode::BBT_Time when;

	TempoMap& map (_editor->session()->tempo_map());
	map.bbt_time (last_pointer_frame(), when);
	
	if (_copy == true) {
		_editor->begin_reversible_command (_("copy meter mark"));
		XMLNode &before = map.get_state();
		map.add_meter (_marker->meter(), when);
		XMLNode &after = map.get_state();
		_editor->session()->add_command(new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();

	} else {
		_editor->begin_reversible_command (_("move meter mark"));

		/* we removed it before, so add it back now */
		
		map.add_meter (_marker->meter(), when);
		XMLNode &after = map.get_state();
		_editor->session()->add_command(new MementoCommand<TempoMap>(map, before_state, &after));
		_editor->commit_reversible_command ();
	}

	// delete the dummy marker we used for visual representation while moving.
	// a new visual marker will show up automatically.
	delete _marker;
}

void
MeterMarkerDrag::aborted (bool moved)
{
	_marker->set_position (_marker->meter().frame ());

	if (moved) {
		TempoMap& map (_editor->session()->tempo_map());
		/* we removed it before, so add it back now */
		map.add_meter (_marker->meter(), _marker->meter().frame());
		// delete the dummy marker we used for visual representation while moving.
		// a new visual marker will show up automatically.
		delete _marker;
	}
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
	Drag::start_grab (event, cursor);
	show_verbose_cursor_time (adjusted_current_frame (event));
}

void
TempoMarkerDrag::setup_pointer_frame_offset ()
{
	_pointer_frame_offset = raw_grab_frame() - _marker->tempo().frame();
}

void
TempoMarkerDrag::motion (GdkEvent* event, bool first_move)
{
	if (first_move) {

		// create a dummy marker for visual representation of moving the
		// section, because whether its a copy or not, we're going to 
		// leave or lose the original marker (leave if its a copy; lose if its
		// not, because we'll remove it from the map).
		
		// create a dummy marker for visual representation of moving the copy.
		// The actual copying is not done before we reach the finish callback.

		char name[64];
		snprintf (name, sizeof (name), "%.2f", _marker->tempo().beats_per_minute());

		TempoSection section (_marker->tempo());

		_marker = new TempoMarker (
			*_editor,
			*_editor->tempo_group,
			ARDOUR_UI::config()->canvasvar_TempoMarker.get(),
			name,
			*new TempoSection (_marker->tempo())
			);

		/* use the new marker for the grab */
		swap_grab (&_marker->the_item(), 0, GDK_CURRENT_TIME);

		if (!_copy) {
			TempoMap& map (_editor->session()->tempo_map());
			/* get current state */
			before_state = &map.get_state();
			/* remove the section while we drag it */
			map.remove_tempo (section, true);
		}
	}

	framepos_t const pf = adjusted_current_frame (event);
	_marker->set_position (pf);
	show_verbose_cursor_time (pf);
}

void
TempoMarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	motion (event, false);

	TempoMap& map (_editor->session()->tempo_map());
	framepos_t beat_time = map.round_to_beat (last_pointer_frame(), 0);
	Timecode::BBT_Time when;

	map.bbt_time (beat_time, when);

	if (_copy == true) {
		_editor->begin_reversible_command (_("copy tempo mark"));
		XMLNode &before = map.get_state();
		map.add_tempo (_marker->tempo(), when);
		XMLNode &after = map.get_state();
		_editor->session()->add_command (new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();

	} else {
		_editor->begin_reversible_command (_("move tempo mark"));
		/* we removed it before, so add it back now */
		map.add_tempo (_marker->tempo(), when);
		XMLNode &after = map.get_state();
		_editor->session()->add_command (new MementoCommand<TempoMap>(map, before_state, &after));
		_editor->commit_reversible_command ();
	}

	// delete the dummy marker we used for visual representation while moving.
	// a new visual marker will show up automatically.
	delete _marker;
}

void
TempoMarkerDrag::aborted (bool moved)
{
	_marker->set_position (_marker->tempo().frame());
	if (moved) {
		TempoMap& map (_editor->session()->tempo_map());
		/* we removed it before, so add it back now */
		map.add_tempo (_marker->tempo(), _marker->tempo().start());
		// delete the dummy marker we used for visual representation while moving.
		// a new visual marker will show up automatically.
		delete _marker;
	}
}

CursorDrag::CursorDrag (Editor* e, ArdourCanvas::Item* i, bool s)
	: Drag (e, i),
	  _stop (s)
{
	DEBUG_TRACE (DEBUG::Drags, "New CursorDrag\n");
}

/** Do all the things we do when dragging the playhead to make it look as though
 *  we have located, without actually doing the locate (because that would cause
 *  the diskstream buffers to be refilled, which is too slow).
 */
void
CursorDrag::fake_locate (framepos_t t)
{
	_editor->playhead_cursor->set_position (t);

	Session* s = _editor->session ();
	if (s->timecode_transmission_suspended ()) {
		framepos_t const f = _editor->playhead_cursor->current_frame ();
		s->send_mmc_locate (f);
		s->send_full_time_code (f);
	}

	show_verbose_cursor_time (t);
	_editor->UpdateAllTransportClocks (t);
}

void
CursorDrag::start_grab (GdkEvent* event, Gdk::Cursor* c)
{
	Drag::start_grab (event, c);

	_grab_zoom = _editor->frames_per_pixel;

	framepos_t where = _editor->canvas_event_frame (event, 0, 0);
	_editor->snap_to_with_modifier (where, event);

	_editor->_dragging_playhead = true;

	Session* s = _editor->session ();

	if (s) {
		if (_was_rolling && _stop) {
			s->request_stop ();
		}

		if (s->is_auditioning()) {
			s->cancel_audition ();
		}


		if (AudioEngine::instance()->connected()) {
			
			/* do this only if we're the engine is connected
			 * because otherwise this request will never be
			 * serviced and we'll busy wait forever. likewise,
			 * notice if we are disconnected while waiting for the
			 * request to be serviced.
			 */

			s->request_suspend_timecode_transmission ();
			while (AudioEngine::instance()->connected() && !s->timecode_transmission_suspended ()) {
				/* twiddle our thumbs */
			}
		}
	}

	fake_locate (where);
}

void
CursorDrag::motion (GdkEvent* event, bool)
{
	framepos_t const adjusted_frame = adjusted_current_frame (event);
	if (adjusted_frame != last_pointer_frame()) {
		fake_locate (adjusted_frame);
#ifdef GTKOSX
		_editor->update_canvas_now ();
#endif
	}
}

void
CursorDrag::finished (GdkEvent* event, bool movement_occurred)
{
	_editor->_dragging_playhead = false;

	if (!movement_occurred && _stop) {
		return;
	}

	motion (event, false);

	Session* s = _editor->session ();
	if (s) {
		s->request_locate (_editor->playhead_cursor->current_frame (), _was_rolling);
		_editor->_pending_locate_request = true;
		s->request_resume_timecode_transmission ();
	}
}

void
CursorDrag::aborted (bool)
{
	if (_editor->_dragging_playhead) {
		_editor->session()->request_resume_timecode_transmission ();
		_editor->_dragging_playhead = false;
	}

	_editor->playhead_cursor->set_position (adjusted_frame (grab_frame (), 0, false));
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

	show_verbose_cursor_duration (r->position(), r->position() + r->fade_in()->back()->when, 32);
}

void
FadeInDrag::setup_pointer_frame_offset ()
{
	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> const r = arv->audio_region ();
	_pointer_frame_offset = raw_grab_frame() - ((framecnt_t) r->fade_in()->back()->when + r->position());
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

		tmp->reset_fade_in_shape_width (tmp->audio_region(), fade_length);
	}

	show_verbose_cursor_duration (region->position(), region->position() + fade_length, 32);
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

		XMLNode &after = alist->get_state();
		_editor->session()->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	_editor->commit_reversible_command ();
}

void
FadeInDrag::aborted (bool)
{
	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_in_shape_width (tmp->audio_region(), tmp->audio_region()->fade_in()->back()->when);
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

	show_verbose_cursor_duration (r->last_frame() - r->fade_out()->back()->when, r->last_frame());
}

void
FadeOutDrag::setup_pointer_frame_offset ()
{
	AudioRegionView* arv = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> r = arv->audio_region ();
	_pointer_frame_offset = raw_grab_frame() - (r->length() - (framecnt_t) r->fade_out()->back()->when + r->position());
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

		tmp->reset_fade_out_shape_width (tmp->audio_region(), fade_length);
	}

	show_verbose_cursor_duration (region->last_frame() - fade_length, region->last_frame());
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

		XMLNode &after = alist->get_state();
		_editor->session()->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	_editor->commit_reversible_command ();
}

void
FadeOutDrag::aborted (bool)
{
	for (list<DraggingView>::iterator i = _views.begin(); i != _views.end(); ++i) {
		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (i->view);

		if (!tmp) {
			continue;
		}

		tmp->reset_fade_out_shape_width (tmp->audio_region(), tmp->audio_region()->fade_out()->back()->when);
	}
}

MarkerDrag::MarkerDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
{
	DEBUG_TRACE (DEBUG::Drags, "New MarkerDrag\n");

	_marker = reinterpret_cast<Marker*> (_item->get_data ("marker"));
	assert (_marker);

	_points.push_back (ArdourCanvas::Duple (0, 0));
	_points.push_back (ArdourCanvas::Duple (0, physical_screen_height (_editor->get_window())));
}

MarkerDrag::~MarkerDrag ()
{
	for (CopiedLocationInfo::iterator i = _copied_locations.begin(); i != _copied_locations.end(); ++i) {
		delete i->location;
	}
}

MarkerDrag::CopiedLocationMarkerInfo::CopiedLocationMarkerInfo (Location* l, Marker* m)
{
	location = new Location (*l);
	markers.push_back (m);
	move_both = false;
}

void
MarkerDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	bool is_start;

	Location *location = _editor->find_location_from_marker (_marker, is_start);
	_editor->_dragging_edit_point = true;

	update_item (location);

	// _drag_line->show();
	// _line->raise_to_top();

	if (is_start) {
		show_verbose_cursor_time (location->start());
	} else {
		show_verbose_cursor_time (location->end());
	}

	Selection::Operation op = ArdourKeyboard::selection_type (event->button.state);

	switch (op) {
	case Selection::Toggle:
		/* we toggle on the button release */
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
		if (e < max_framepos) {
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

	/* Set up copies for us to manipulate during the drag 
	 */

	for (MarkerSelection::iterator i = _editor->selection->markers.begin(); i != _editor->selection->markers.end(); ++i) {

		Location* l = _editor->find_location_from_marker (*i, is_start);

		if (!l) {
			continue;
		}

		if (l->is_mark()) {
			_copied_locations.push_back (CopiedLocationMarkerInfo (l, *i));
		} else {
			/* range: check that the other end of the range isn't
			   already there.
			*/
			CopiedLocationInfo::iterator x;
			for (x = _copied_locations.begin(); x != _copied_locations.end(); ++x) {
				if (*(*x).location == *l) {
					break;
				}
			}
			if (x == _copied_locations.end()) {
				_copied_locations.push_back (CopiedLocationMarkerInfo (l, *i));
			} else {
				(*x).markers.push_back (*i);
				(*x).move_both = true;
			}
		}
			
	}
}

void
MarkerDrag::setup_pointer_frame_offset ()
{
	bool is_start;
	Location *location = _editor->find_location_from_marker (_marker, is_start);
	_pointer_frame_offset = raw_grab_frame() - (is_start ? location->start() : location->end());
}

void
MarkerDrag::motion (GdkEvent* event, bool)
{
	framecnt_t f_delta = 0;
	bool is_start;
	bool move_both = false;
	Location *real_location;
	Location *copy_location = 0;

	framepos_t const newframe = adjusted_current_frame (event);
	framepos_t next = newframe;

	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
		move_both = true;
	}

	CopiedLocationInfo::iterator x;

	/* find the marker we're dragging, and compute the delta */

	for (x = _copied_locations.begin(); x != _copied_locations.end(); ++x) {

		copy_location = (*x).location;

		if (find (x->markers.begin(), x->markers.end(), _marker) != x->markers.end()) {

			/* this marker is represented by this
			 * CopiedLocationMarkerInfo 
			 */

			if ((real_location = _editor->find_location_from_marker (_marker, is_start)) == 0) {
				/* que pasa ?? */
				return;
			}

			if (real_location->is_mark()) {
				f_delta = newframe - copy_location->start();
			} else {


				switch (_marker->type()) {
				case Marker::SessionStart:
				case Marker::RangeStart:
				case Marker::LoopStart:
				case Marker::PunchIn:
					f_delta = newframe - copy_location->start();
					break;

				case Marker::SessionEnd:
				case Marker::RangeEnd:
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

	if (x == _copied_locations.end()) {
		/* hmm, impossible - we didn't find the dragged marker */
		return;
	}

	/* now move them all */

	for (x = _copied_locations.begin(); x != _copied_locations.end(); ++x) {

		copy_location = x->location;

		/* call this to find out if its the start or end */

		if ((real_location = _editor->find_location_from_marker (x->markers.front(), is_start)) == 0) {
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
				
				if (move_both || (*x).move_both) {
					copy_location->set_start (new_start);
					copy_location->set_end (new_end);
				} else 	if (new_start < copy_location->end()) {
					copy_location->set_start (new_start);
				} else if (newframe > 0) {
					_editor->snap_to (next, 1, true);
					copy_location->set_end (next);
					copy_location->set_start (newframe);
				}

			} else { // end marker

				if (move_both || (*x).move_both) {
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
		
		/* now lookup the actual GUI items used to display this
		 * location and move them to wherever the copy of the location
		 * is now. This means that the logic in ARDOUR::Location is
		 * still enforced, even though we are not (yet) modifying 
		 * the real Location itself.
		 */
		
		Editor::LocationMarkers* lm = _editor->find_location_markers (real_location);

		if (lm) {
			lm->set_position (copy_location->start(), copy_location->end());
		}

	}

	assert (!_copied_locations.empty());

	show_verbose_cursor_time (newframe);

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
			/* we toggle on the button release, click only */
			_editor->selection->toggle (_marker);
			break;

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
	CopiedLocationInfo::iterator x;
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
				location->set_start (((*x).location)->start());
			} else {
				location->set (((*x).location)->start(), ((*x).location)->end());
			}
		}
	}

	XMLNode &after = _editor->session()->locations()->get_state();
	_editor->session()->add_command(new MementoCommand<Locations>(*(_editor->session()->locations()), &before, &after));
	_editor->commit_reversible_command ();
}

void
MarkerDrag::aborted (bool)
{
	/* XXX: TODO */
}

void
MarkerDrag::update_item (Location*)
{
        /* noop */
}

ControlPointDrag::ControlPointDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i),
	  _cumulative_x_drag (0),
	  _cumulative_y_drag (0)
{
	if (_zero_gain_fraction < 0.0) {
		_zero_gain_fraction = gain_to_slider_position_with_max (dB_to_coefficient (0.0), Config->get_max_gain());
	}

	DEBUG_TRACE (DEBUG::Drags, "New ControlPointDrag\n");

	_point = reinterpret_cast<ControlPoint*> (_item->get_data ("control_point"));
	assert (_point);
}


void
ControlPointDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*cursor*/)
{
	Drag::start_grab (event, _editor->cursors()->fader);

	// start the grab at the center of the control point so
	// the point doesn't 'jump' to the mouse after the first drag
	_fixed_grab_x = _point->get_x();
	_fixed_grab_y = _point->get_y();

	float const fraction = 1 - (_point->get_y() / _point->line().height());

	_point->line().start_drag_single (_point, _fixed_grab_x, fraction);

	_editor->verbose_cursor()->set (_point->line().get_verbose_cursor_string (fraction),
					event->button.x + 10, event->button.y + 10);

	_editor->verbose_cursor()->show ();

	_pushing = Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier);

	if (!_point->can_slide ()) {
		_x_constrained = true;
	}
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

	framepos_t cx_frames = _editor->pixel_to_sample (cx);

	if (!_x_constrained) {
		_editor->snap_to_with_modifier (cx_frames, event);
	}

	cx_frames = min (cx_frames, _point->line().maximum_time());

	float const fraction = 1.0 - (cy / _point->line().height());

	_point->line().drag_motion (_editor->sample_to_pixel_unrounded (cx_frames), fraction, false, _pushing, _final_index);

	_editor->verbose_cursor()->set_text (_point->line().get_verbose_cursor_string (fraction));
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

	_point->line().end_drag (_pushing, _final_index);
	_editor->session()->commit_reversible_command ();
}

void
ControlPointDrag::aborted (bool)
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

	_line->parent_group().canvas_to_item (cx, cy);

	framecnt_t const frame_within_region = (framecnt_t) floor (cx * _editor->frames_per_pixel);

	uint32_t before;
	uint32_t after;

	if (!_line->control_points_adjacent (frame_within_region, before, after)) {
		/* no adjacent points */
		return;
	}

	Drag::start_grab (event, _editor->cursors()->fader);

	/* store grab start in parent frame */

	_fixed_grab_x = cx;
	_fixed_grab_y = cy;

	double fraction = 1.0 - (cy / _line->height());

	_line->start_drag_line (before, after, fraction);

	_editor->verbose_cursor()->set (_line->get_verbose_cursor_string (fraction),
					event->button.x + 10, event->button.y + 10);

	_editor->verbose_cursor()->show ();
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
	uint32_t ignored;

	/* we are ignoring x position for this drag, so we can just pass in anything */
	_line->drag_motion (0, fraction, true, false, ignored);

	_editor->verbose_cursor()->set_text (_line->get_verbose_cursor_string (fraction));
}

void
LineDrag::finished (GdkEvent* event, bool)
{
	motion (event, false);
	_line->end_drag (false, 0);
	_editor->session()->commit_reversible_command ();
}

void
LineDrag::aborted (bool)
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

	_line = reinterpret_cast<Line*> (_item);
	assert (_line);

	/* need to get x coordinate in terms of parent (AudioRegionView) origin. */

	double cx = event->button.x;
	double cy = event->button.y;

	_item->parent()->canvas_to_item (cx, cy);

	/* store grab start in parent frame */
	_region_view_grab_x = cx;

	_before = *(float*) _item->get_data ("position");

	_arv = reinterpret_cast<AudioRegionView*> (_item->get_data ("regionview"));

	_max_x = _editor->sample_to_pixel(_arv->get_duration());
}

void
FeatureLineDrag::motion (GdkEvent*, bool)
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

	boost::optional<Rect> bbox = _line->bounding_box ();
	assert (bbox);
	_line->set (ArdourCanvas::Duple (cx, 2.0), ArdourCanvas::Duple (cx, bbox.get().height ()));

	float *pos = new float;
	*pos = cx;

	_line->set_data ("position", pos);

	_before = cx;
}

void
FeatureLineDrag::finished (GdkEvent*, bool)
{
	_arv = reinterpret_cast<AudioRegionView*> (_item->get_data ("regionview"));
	_arv->update_transient(_before, _before);
}

void
FeatureLineDrag::aborted (bool)
{
	//_line->reset ();
}

RubberbandSelectDrag::RubberbandSelectDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
	, _vertical_only (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New RubberbandSelectDrag\n");
}

void
RubberbandSelectDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);
	show_verbose_cursor_time (adjusted_current_frame (event));
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

		double x1 = _editor->sample_to_pixel (start);
		double x2 = _editor->sample_to_pixel (end);

		_editor->rubberband_rect->set_x0 (x1);
		if (_vertical_only) {
			/* fixed 10 pixel width */
			_editor->rubberband_rect->set_x1 (x1 + 10);
		} else {
			_editor->rubberband_rect->set_x1 (x2);
		} 

		_editor->rubberband_rect->set_y0 (y1);
		_editor->rubberband_rect->set_y1 (y2);

		_editor->rubberband_rect->show();
		_editor->rubberband_rect->raise_to_top();

		show_verbose_cursor_time (pf);

		do_select_things (event, true);
	}
}

void
RubberbandSelectDrag::do_select_things (GdkEvent* event, bool drag_in_progress)
{
	framepos_t x1;
	framepos_t x2;
	
	if (grab_frame() < last_pointer_frame()) {
		x1 = grab_frame ();
		x2 = last_pointer_frame ();
	} else {
		x2 = grab_frame ();
		x1 = last_pointer_frame ();
	}

	double y1;
	double y2;
	
	if (_drags->current_pointer_y() < grab_y()) {
		y1 = _drags->current_pointer_y();
		y2 = grab_y();
	} else {
		y2 = _drags->current_pointer_y();
		y1 = grab_y();
	}

	select_things (event->button.state, x1, x2, y1, y2, drag_in_progress);
}

void
RubberbandSelectDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {

		motion (event, false);
		do_select_things (event, false);

	} else {

		/* just a click */

		bool do_deselect = true;
		MidiTimeAxisView* mtv;

		if ((mtv = dynamic_cast<MidiTimeAxisView*>(_editor->clicked_axisview)) != 0) {
			/* MIDI track */
			if (_editor->selection->empty()) {
				/* nothing selected */
				add_midi_region (mtv);
				do_deselect = false;
			}
		} 

		/* do not deselect if Primary or Tertiary (toggle-select or
		 * extend-select are pressed.
		 */

		if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier) && 
		    !Keyboard::modifier_state_contains (event->button.state, Keyboard::TertiaryModifier) && 
		    do_deselect) {
			deselect_things ();
		}

	}

	_editor->rubberband_rect->hide();
}

void
RubberbandSelectDrag::aborted (bool)
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

	show_verbose_cursor_time (adjusted_current_frame (event));
}

void
TimeFXDrag::motion (GdkEvent* event, bool)
{
	RegionView* rv = _primary;
	StreamView* cv = rv->get_time_axis_view().view ();

	pair<TimeAxisView*, double> const tv = _editor->trackview_by_y_position (grab_y());
	int layer = tv.first->layer_display() == Overlaid ? 0 : tv.second;
	int layers = tv.first->layer_display() == Overlaid ? 1 : cv->layers();

	framepos_t const pf = adjusted_current_frame (event);

	if (pf > rv->region()->position()) {
		rv->get_time_axis_view().show_timestretch (rv->region()->position(), pf, layers, layer);
	}

	show_verbose_cursor_time (pf);
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

	if (!_editor->get_selection().regions.empty()) {
		/* primary will already be included in the selection, and edit
		   group shared editing will propagate selection across
		   equivalent regions, so just use the current region
		   selection.
		*/

		if (_editor->time_stretch (_editor->get_selection().regions, percentage) == -1) {
			error << _("An error occurred while executing time stretch operation") << endmsg;
		}
	}
}

void
TimeFXDrag::aborted (bool)
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
ScrubDrag::aborted (bool)
{
	/* XXX: TODO */
}

SelectionDrag::SelectionDrag (Editor* e, ArdourCanvas::Item* i, Operation o)
	: Drag (e, i)
	, _operation (o)
	, _add (false)
	, _extend (false)
	, _original_pointer_time_axis (-1)
	, _last_pointer_time_axis (-1)
	, _time_selection_at_start (!_editor->get_selection().time.empty())
{
	DEBUG_TRACE (DEBUG::Drags, "New SelectionDrag\n");
	
	if (_time_selection_at_start) {
		start_at_start = _editor->get_selection().time.start();
		end_at_start = _editor->get_selection().time.end_frame();
	}
}

void
SelectionDrag::start_grab (GdkEvent* event, Gdk::Cursor*)
{
	if (_editor->session() == 0) {
		return;
	}

	Gdk::Cursor* cursor = 0;

	switch (_operation) {
	case CreateSelection:
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::CopyModifier)) {
			_add = true;
		} else {
			_add = false;
		}
		cursor = _editor->cursors()->selector;
		Drag::start_grab (event, cursor);
		break;

	case SelectionStartTrim:
		if (_editor->clicked_axisview) {
			_editor->clicked_axisview->order_selection_trims (_item, true);
		}
		Drag::start_grab (event, _editor->cursors()->left_side_trim);
		break;

	case SelectionEndTrim:
		if (_editor->clicked_axisview) {
			_editor->clicked_axisview->order_selection_trims (_item, false);
		}
		Drag::start_grab (event, _editor->cursors()->right_side_trim);
		break;

	case SelectionMove:
		Drag::start_grab (event, cursor);
		break;

	case SelectionExtend:
		Drag::start_grab (event, cursor);
		break;
	}

	if (_operation == SelectionMove) {
		show_verbose_cursor_time (_editor->selection->time[_editor->clicked_selection].start);
	} else {
		show_verbose_cursor_time (adjusted_current_frame (event));
	}

	_original_pointer_time_axis = _editor->trackview_by_y_position (_drags->current_pointer_y ()).first->order ();
}

void
SelectionDrag::setup_pointer_frame_offset ()
{
	switch (_operation) {
	case CreateSelection:
		_pointer_frame_offset = 0;
		break;

	case SelectionStartTrim:
	case SelectionMove:
		_pointer_frame_offset = raw_grab_frame() - _editor->selection->time[_editor->clicked_selection].start;
		break;

	case SelectionEndTrim:
		_pointer_frame_offset = raw_grab_frame() - _editor->selection->time[_editor->clicked_selection].end;
		break;

	case SelectionExtend:
		break;
	}
}

void
SelectionDrag::motion (GdkEvent* event, bool first_move)
{
	framepos_t start = 0;
	framepos_t end = 0;
	framecnt_t length = 0;
	framecnt_t distance = 0;

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
			grab = adjusted_current_frame (event, false);
			if (grab < pending_position) {
				_editor->snap_to (grab, -1);
			}  else {
				_editor->snap_to (grab, 1);
			}
		}

		if (pending_position < grab) {
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

			if (_add) {
				/* adding to the selection */
				_editor->set_selected_track_as_side_effect (Selection::Add);
				//_editor->selection->add (_editor->clicked_axisview);
				_editor->clicked_selection = _editor->selection->add (start, end);
				_add = false;
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
		distance = pending_position - start;
		start = pending_position;
		_editor->snap_to (start);

		end = start + length;

		break;

	case SelectionExtend:
		break;
	}

	if (event->button.x >= _editor->horizontal_position() + _editor->_visible_canvas_width) {
		_editor->start_canvas_autoscroll (1, 0);
	}

	if (start != end) {
		switch (_operation) {
		case SelectionMove:	
			if (_time_selection_at_start) {
				_editor->selection->move_time (distance);
			}
			break;
		default:
			_editor->selection->replace (_editor->clicked_selection, start, end);
		}
	}

	if (_operation == SelectionMove) {
		show_verbose_cursor_time(start);
	} else {
		show_verbose_cursor_time(pending_position);
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
		if (s) {
			if ( s->get_play_range() && s->transport_rolling() ) {
				s->request_play_range (&_editor->selection->time, true);
			} else {
				if (Config->get_always_play_range() && !s->transport_rolling()) {
					s->request_locate (_editor->get_selection().time.start());
				}
			}
		}

	} else {
		/* just a click, no pointer movement.
		 */

		if (_operation == SelectionExtend) {
			if (_time_selection_at_start) {
				framepos_t pos = adjusted_current_frame (event, false);
				framepos_t start = min (pos, start_at_start);
				framepos_t end = max (pos, end_at_start);
				_editor->selection->set (start, end);
			}
		} else {
			if (Keyboard::modifier_state_equals (event->button.state, Keyboard::CopyModifier)) {
				if (_editor->clicked_selection) {
					_editor->selection->remove (_editor->clicked_selection);
				}
			} else {
				if (!_editor->clicked_selection) {
					_editor->selection->clear_time();
				}
			}
		}

		if (_editor->clicked_axisview && !_editor->selection->selected (_editor->clicked_axisview)) {
			_editor->selection->set (_editor->clicked_axisview);
		}
			
		if (s && s->get_play_range () && s->transport_rolling()) {
			s->request_stop (false, false);
		}

	}

	_editor->stop_canvas_autoscroll ();
	_editor->clicked_selection = 0;
}

void
SelectionDrag::aborted (bool)
{
	/* XXX: TODO */
}

RangeMarkerBarDrag::RangeMarkerBarDrag (Editor* e, ArdourCanvas::Item* i, Operation o)
	: Drag (e, i),
	  _operation (o),
	  _copy (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New RangeMarkerBarDrag\n");

	_drag_rect = new ArdourCanvas::Rectangle (_editor->time_line_group, 
						  ArdourCanvas::Rect (0.0, 0.0, 0.0,
								      physical_screen_height (_editor->get_window())));
	_drag_rect->hide ();

	_drag_rect->set_fill_color (ARDOUR_UI::config()->canvasvar_RangeDragRect.get());
	_drag_rect->set_outline_color (ARDOUR_UI::config()->canvasvar_RangeDragRect.get());
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
		cursor = _editor->cursors()->selector;
		break;
	}

	Drag::start_grab (event, cursor);

	show_verbose_cursor_time (adjusted_current_frame (event));
}

void
RangeMarkerBarDrag::motion (GdkEvent* event, bool first_move)
{
	framepos_t start = 0;
	framepos_t end = 0;
	ArdourCanvas::Rectangle *crect;

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
		error << string_compose (_("programming_error: %1"), "Error: unknown range marker op passed to Editor::drag_range_markerbar_op ()") << endmsg;
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

	if (event->button.x >= _editor->horizontal_position() + _editor->_visible_canvas_width) {
		_editor->start_canvas_autoscroll (1, 0);
	}

	if (start != end) {
		_editor->temp_location->set (start, end);

		double x1 = _editor->sample_to_pixel (start);
		double x2 = _editor->sample_to_pixel (end);
		crect->set_x0 (x1);
		crect->set_x1 (x2);

		update_item (_editor->temp_location);
	}

	show_verbose_cursor_time (pf);

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

			if (end == max_framepos) {
				end = _editor->session()->current_end_frame ();
			}

			if (start == max_framepos) {
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
RangeMarkerBarDrag::aborted (bool)
{
	/* XXX: TODO */
}

void
RangeMarkerBarDrag::update_item (Location* location)
{
	double const x1 = _editor->sample_to_pixel (location->start());
	double const x2 = _editor->sample_to_pixel (location->end());

	_drag_rect->set_x0 (x1);
	_drag_rect->set_x1 (x2);
}

MouseZoomDrag::MouseZoomDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
	, _zoom_out (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New MouseZoomDrag\n");
}

void
MouseZoomDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	if (Keyboard::the_keyboard().key_is_down (GDK_Control_L)) {
		Drag::start_grab (event, _editor->cursors()->zoom_out);
		_zoom_out = true;
	} else {
		Drag::start_grab (event, _editor->cursors()->zoom_in);
		_zoom_out = false;
	}

	show_verbose_cursor_time (adjusted_current_frame (event));
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

		show_verbose_cursor_time (pf);
	}
}

void
MouseZoomDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		motion (event, false);

		if (grab_frame() < last_pointer_frame()) {
			_editor->temporal_zoom_by_frame (grab_frame(), last_pointer_frame());
		} else {
			_editor->temporal_zoom_by_frame (last_pointer_frame(), grab_frame());
		}
	} else {
		if (Keyboard::the_keyboard().key_is_down (GDK_Shift_L)) {
			_editor->tav_zoom_step (_zoom_out);
		} else {
			_editor->temporal_zoom_to_frame (_zoom_out, grab_frame());
		}
	}

	_editor->zoom_rect->hide();
}

void
MouseZoomDrag::aborted (bool)
{
	_editor->zoom_rect->hide ();
}

NoteDrag::NoteDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
	, _cumulative_dx (0)
	, _cumulative_dy (0)
{
	DEBUG_TRACE (DEBUG::Drags, "New NoteDrag\n");

	_primary = reinterpret_cast<NoteBase*> (_item->get_data ("notebase"));
	assert (_primary);
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
	frameoffset_t const dx = _editor->pixel_to_sample (_drags->current_pointer_x() - grab_x());

	/* primary note time */
	frameoffset_t const n = _region->source_beats_to_absolute_frames (_primary->note()->time ());

	/* new time of the primary note in session frames */
	frameoffset_t st = n + dx;

	framepos_t const rp = _region->region()->position ();

	/* prevent the note being dragged earlier than the region's position */
	st = max (st, rp);

	/* snap and return corresponding delta */
	return _region->snap_frame_to_frame (st - rp) + rp - n;
}

/** @return Current total drag y change in note number */
int8_t
NoteDrag::total_dy () const
{
	MidiStreamView* msv = _region->midi_stream_view ();
	double const y = _region->midi_view()->y_position ();
	/* new current note */
	uint8_t n = msv->y_to_note (_drags->current_pointer_y () - y);
	/* clamp */
	n = max (msv->lowest_note(), n);
	n = min (msv->highest_note(), n);
	/* and work out delta */
	return n - msv->y_to_note (grab_y() - y);
}

void
NoteDrag::motion (GdkEvent *, bool)
{
	/* Total change in x and y since the start of the drag */
	frameoffset_t const dx = total_dx ();
	int8_t const dy = total_dy ();

	/* Now work out what we have to do to the note canvas items to set this new drag delta */
	double const tdx = _editor->sample_to_pixel (dx) - _cumulative_dx;
	double const tdy = -dy * _note_height - _cumulative_dy;

	if (tdx || tdy) {
		_cumulative_dx += tdx;
		_cumulative_dy += tdy;

		int8_t note_delta = total_dy();

		_region->move_selection (tdx, tdy, note_delta);

		/* the new note value may be the same as the old one, but we
		 * don't know what that means because the selection may have
		 * involved more than one note and we might be doing something
		 * odd with them. so show the note value anyway, always.
		 */

		char buf[12];
		uint8_t new_note = min (max (_primary->note()->note() + note_delta, 0), 127);
		
		snprintf (buf, sizeof (buf), "%s (%d)", Evoral::midi_note_name (new_note).c_str(),
		          (int) floor (new_note));

		show_verbose_cursor_text (buf);
	}
}

void
NoteDrag::finished (GdkEvent* ev, bool moved)
{
	if (!moved) {
		/* no motion - select note */
		
		if (_editor->current_mouse_mode() == Editing::MouseObject ||
		    _editor->current_mouse_mode() == Editing::MouseDraw) {
			
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
		_region->note_dropped (_primary, total_dx(), total_dy());
	}
}

void
NoteDrag::aborted (bool)
{
	/* XXX: TODO */
}

/** Make an AutomationRangeDrag for lines in an AutomationTimeAxisView */
AutomationRangeDrag::AutomationRangeDrag (Editor* editor, AutomationTimeAxisView* atv, list<AudioRange> const & r)
	: Drag (editor, atv->base_item ())
	, _ranges (r)
	, _nothing_to_drag (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New AutomationRangeDrag\n");
	y_origin = atv->y_position();
	setup (atv->lines ());
}

/** Make an AutomationRangeDrag for region gain lines */
AutomationRangeDrag::AutomationRangeDrag (Editor* editor, AudioRegionView* rv, list<AudioRange> const & r)
	: Drag (editor, rv->get_canvas_group ())
	, _ranges (r)
	, _nothing_to_drag (false)
{
	DEBUG_TRACE (DEBUG::Drags, "New AutomationRangeDrag\n");

	list<boost::shared_ptr<AutomationLine> > lines;
	lines.push_back (rv->get_gain_line ());
	y_origin = rv->get_time_axis_view().y_position();
	setup (lines);
}

/** @param lines AutomationLines to drag.
 *  @param offset Offset from the session start to the points in the AutomationLines.
 */
void
AutomationRangeDrag::setup (list<boost::shared_ptr<AutomationLine> > const & lines)
{
	/* find the lines that overlap the ranges being dragged */
	list<boost::shared_ptr<AutomationLine> >::const_iterator i = lines.begin ();
	while (i != lines.end ()) {
		list<boost::shared_ptr<AutomationLine> >::const_iterator j = i;
		++j;

		pair<framepos_t, framepos_t> r = (*i)->get_point_x_range ();

		/* check this range against all the AudioRanges that we are using */
		list<AudioRange>::const_iterator k = _ranges.begin ();
		while (k != _ranges.end()) {
			if (k->coverage (r.first, r.second) != Evoral::OverlapNone) {
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

double
AutomationRangeDrag::y_fraction (boost::shared_ptr<AutomationLine> line, double global_y) const
{
	return 1.0 - ((global_y - y_origin) / line->height());
}

void
AutomationRangeDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	/* Get line states before we start changing things */
	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		i->state = &i->line->get_state ();
		i->original_fraction = y_fraction (i->line, _drags->current_pointer_y());
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
				the_list->add (q, the_list->eval (q));
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
				the_list->add (q, the_list->eval (q));
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
		i->line->start_drag_multiple (i->points, y_fraction (i->line, _drags->current_pointer_y()), i->state);
	}
}

void
AutomationRangeDrag::motion (GdkEvent*, bool /*first_move*/)
{
	if (_nothing_to_drag) {
		return;
	}

	for (list<Line>::iterator l = _lines.begin(); l != _lines.end(); ++l) {
		float const f = y_fraction (l->line, _drags->current_pointer_y());
		/* we are ignoring x position for this drag, so we can just pass in anything */
		uint32_t ignored;
		l->line->drag_motion (0, f, true, false, ignored);
		show_verbose_cursor_text (l->line->get_verbose_cursor_relative_string (l->original_fraction, f));
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
		i->line->end_drag (false, 0);
	}

	_editor->session()->commit_reversible_command ();
}

void
AutomationRangeDrag::aborted (bool)
{
	for (list<Line>::iterator i = _lines.begin(); i != _lines.end(); ++i) {
		i->line->reset ();
	}
}

DraggingView::DraggingView (RegionView* v, RegionDrag* parent)
	: view (v)
{
	time_axis_view = parent->find_time_axis_view (&v->get_time_axis_view ());
	layer = v->region()->layer ();
	initial_y = v->get_canvas_group()->position().y;
	initial_playlist = v->region()->playlist ();
	initial_position = v->region()->position ();
	initial_end = v->region()->position () + v->region()->length ();
}

PatchChangeDrag::PatchChangeDrag (Editor* e, PatchChange* i, MidiRegionView* r)
	: Drag (e, i->canvas_item ())
	, _region_view (r)
	, _patch_change (i)
	, _cumulative_dx (0)
{
	DEBUG_TRACE (DEBUG::Drags, string_compose ("New PatchChangeDrag, patch @ %1, grab @ %2\n",
						   _region_view->source_beats_to_absolute_frames (_patch_change->patch()->time()),
						   grab_frame()));
}

void
PatchChangeDrag::motion (GdkEvent* ev, bool)
{
	framepos_t f = adjusted_current_frame (ev);
	boost::shared_ptr<Region> r = _region_view->region ();
	f = max (f, r->position ());
	f = min (f, r->last_frame ());

	framecnt_t const dxf = f - grab_frame(); // permitted dx in frames
	double const dxu = _editor->sample_to_pixel (dxf); // permitted fx in units
	_patch_change->move (ArdourCanvas::Duple (dxu - _cumulative_dx, 0));
	_cumulative_dx = dxu;
}

void
PatchChangeDrag::finished (GdkEvent* ev, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	boost::shared_ptr<Region> r (_region_view->region ());
	framepos_t f = adjusted_current_frame (ev);
	f = max (f, r->position ());
	f = min (f, r->last_frame ());

	_region_view->move_patch_change (
		*_patch_change,
		_region_view->region_frames_to_region_beats (f - (r->position() - r->start()))
		);
}

void
PatchChangeDrag::aborted (bool)
{
	_patch_change->move (ArdourCanvas::Duple (-_cumulative_dx, 0));
}

void
PatchChangeDrag::setup_pointer_frame_offset ()
{
	boost::shared_ptr<Region> region = _region_view->region ();
	_pointer_frame_offset = raw_grab_frame() - _region_view->source_beats_to_absolute_frames (_patch_change->patch()->time());
}

MidiRubberbandSelectDrag::MidiRubberbandSelectDrag (Editor* e, MidiRegionView* rv)
	: RubberbandSelectDrag (e, rv->get_canvas_frame ())
	, _region_view (rv)
{

}

void
MidiRubberbandSelectDrag::select_things (int button_state, framepos_t x1, framepos_t x2, double y1, double y2, bool /*drag_in_progress*/)
{
	framepos_t const p = _region_view->region()->position ();
	double const y = _region_view->midi_view()->y_position ();

	x1 = max ((framepos_t) 0, x1 - p);
	x2 = max ((framepos_t) 0, x2 - p);
	y1 = max (0.0, y1 - y);
	y2 = max (0.0, y2 - y);
	
	_region_view->update_drag_selection (
		_editor->sample_to_pixel (x1),
		_editor->sample_to_pixel (x2),
		y1,
		y2,
		Keyboard::modifier_state_contains (button_state, Keyboard::TertiaryModifier)
		);
}

void
MidiRubberbandSelectDrag::deselect_things ()
{
	/* XXX */
}

MidiVerticalSelectDrag::MidiVerticalSelectDrag (Editor* e, MidiRegionView* rv)
	: RubberbandSelectDrag (e, rv->get_canvas_frame ())
	, _region_view (rv)
{
	_vertical_only = true;
}

void
MidiVerticalSelectDrag::select_things (int button_state, framepos_t /*x1*/, framepos_t /*x2*/, double y1, double y2, bool /*drag_in_progress*/)
{
	double const y = _region_view->midi_view()->y_position ();

	y1 = max (0.0, y1 - y);
	y2 = max (0.0, y2 - y);
	
	_region_view->update_vertical_drag_selection (
		y1,
		y2,
		Keyboard::modifier_state_contains (button_state, Keyboard::TertiaryModifier)
		);
}

void
MidiVerticalSelectDrag::deselect_things ()
{
	/* XXX */
}

EditorRubberbandSelectDrag::EditorRubberbandSelectDrag (Editor* e, ArdourCanvas::Item* i)
	: RubberbandSelectDrag (e, i)
{

}

void
EditorRubberbandSelectDrag::select_things (int button_state, framepos_t x1, framepos_t x2, double y1, double y2, bool drag_in_progress)
{
	if (drag_in_progress) {
		/* We just want to select things at the end of the drag, not during it */
		return;
	}
	
	Selection::Operation op = ArdourKeyboard::selection_type (button_state);
	
	_editor->begin_reversible_command (_("rubberband selection"));
	_editor->select_all_within (x1, x2 - 1, y1, y2, _editor->track_views, op, false);
	_editor->commit_reversible_command ();
}

void
EditorRubberbandSelectDrag::deselect_things ()
{
	if (!getenv("ARDOUR_SAE")) {
		_editor->selection->clear_tracks();
	}
	_editor->selection->clear_regions();
	_editor->selection->clear_points ();
	_editor->selection->clear_lines ();
}

NoteCreateDrag::NoteCreateDrag (Editor* e, ArdourCanvas::Item* i, MidiRegionView* rv)
	: Drag (e, i)
	, _region_view (rv)
	, _drag_rect (0)
{
	
}

NoteCreateDrag::~NoteCreateDrag ()
{
	delete _drag_rect;
}

framecnt_t
NoteCreateDrag::grid_frames (framepos_t t) const
{
	bool success;
	Evoral::MusicalTime grid_beats = _editor->get_grid_type_as_beats (success, t);
	if (!success) {
		grid_beats = 1;
	}

	return _region_view->region_beats_to_region_frames (grid_beats);
}

void
NoteCreateDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);
						 
	_drag_rect = new ArdourCanvas::Rectangle (_region_view->get_canvas_group ());

	framepos_t pf = _drags->current_pointer_frame ();
	framecnt_t const g = grid_frames (pf);

	/* Hack so that we always snap to the note that we are over, instead of snapping
	   to the next one if we're more than halfway through the one we're over.
	*/
	if (_editor->snap_mode() == SnapNormal && pf > g / 2) {
		pf -= g / 2;
	}

	_note[0] = adjusted_frame (pf, event) - _region_view->region()->position ();

	MidiStreamView* sv = _region_view->midi_stream_view ();
	double const x = _editor->sample_to_pixel (_note[0]);
	double const y = sv->note_to_y (sv->y_to_note (y_to_region (event->button.y)));

	_drag_rect->set (ArdourCanvas::Rect (x, y, x, y + floor (_region_view->midi_stream_view()->note_height ())));
	_drag_rect->set_outline_what (0xff);
	_drag_rect->set_outline_color (0xffffff99);
	_drag_rect->set_fill_color (0xffffff66);
}

void
NoteCreateDrag::motion (GdkEvent* event, bool)
{
	_note[1] = max ((framepos_t)0, adjusted_current_frame (event) - _region_view->region()->position ());
	double const x = _editor->sample_to_pixel (_note[1]);
	if (_note[1] > _note[0]) {
		_drag_rect->set_x1 (x);
	} else {
		_drag_rect->set_x0 (x);
	}
}

void
NoteCreateDrag::finished (GdkEvent*, bool had_movement)
{
	if (!had_movement) {
		return;
	}
	
	framepos_t const start = min (_note[0], _note[1]);
	framecnt_t length = (framecnt_t) fabs (_note[0] - _note[1]);

	framecnt_t const g = grid_frames (start);
	double const one_tick = 1 / Timecode::BBT_Time::ticks_per_beat;
	
	if (_editor->snap_mode() == SnapNormal && length < g) {
		length = g - one_tick;
	}

	double const length_beats = max (one_tick, _region_view->region_frames_to_region_beats (length));

	_region_view->create_note_at (start, _drag_rect->y0(), length_beats, false);
}

double
NoteCreateDrag::y_to_region (double y) const
{
	double x = 0;
	_region_view->get_canvas_group()->canvas_to_item (x, y);
	return y;
}

void
NoteCreateDrag::aborted (bool)
{
	
}

CrossfadeEdgeDrag::CrossfadeEdgeDrag (Editor* e, AudioRegionView* rv, ArdourCanvas::Item* i, bool start_yn)
	: Drag (e, i)
	, arv (rv)
	, start (start_yn)
{
	std::cout << ("CrossfadeEdgeDrag is DEPRECATED.  See TrimDrag::preserve_fade_anchor") << endl;
}

void
CrossfadeEdgeDrag::start_grab (GdkEvent* event, Gdk::Cursor *cursor)
{
	Drag::start_grab (event, cursor);
}

void
CrossfadeEdgeDrag::motion (GdkEvent*, bool)
{
	double distance;
	double new_length;
	framecnt_t len;

	boost::shared_ptr<AudioRegion> ar (arv->audio_region());

	if (start) {
		distance = _drags->current_pointer_x() - grab_x();
		len = ar->fade_in()->back()->when;
	} else {
		distance = grab_x() - _drags->current_pointer_x();
		len = ar->fade_out()->back()->when;
	}

	/* how long should it be ? */

	new_length = len + _editor->pixel_to_sample (distance);

	/* now check with the region that this is legal */

	new_length = ar->verify_xfade_bounds (new_length, start);

	if (start) {
		arv->redraw_start_xfade_to (ar, new_length);
	} else {
		arv->redraw_end_xfade_to (ar, new_length);
	}
}

void
CrossfadeEdgeDrag::finished (GdkEvent*, bool)
{
	double distance;
	double new_length;
	framecnt_t len;

	boost::shared_ptr<AudioRegion> ar (arv->audio_region());

	if (start) {
		distance = _drags->current_pointer_x() - grab_x();
		len = ar->fade_in()->back()->when;
	} else {
		distance = grab_x() - _drags->current_pointer_x();
		len = ar->fade_out()->back()->when;
	}

	new_length = ar->verify_xfade_bounds (len + _editor->pixel_to_sample (distance), start);
	
	_editor->begin_reversible_command ("xfade trim");
	ar->playlist()->clear_owned_changes ();	

	if (start) {
		ar->set_fade_in_length (new_length);
	} else {
		ar->set_fade_out_length (new_length);
	}

	/* Adjusting the xfade may affect other regions in the playlist, so we need
	   to get undo Commands from the whole playlist rather than just the
	   region.
	*/

	vector<Command*> cmds;
	ar->playlist()->rdiff (cmds);
	_editor->session()->add_commands (cmds);
	_editor->commit_reversible_command ();

}

void
CrossfadeEdgeDrag::aborted (bool)
{
	if (start) {
		arv->redraw_start_xfade ();
	} else {
		arv->redraw_end_xfade ();
	}
}

