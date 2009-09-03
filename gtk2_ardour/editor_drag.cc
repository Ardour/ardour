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

#include "pbd/memento_command.h"
#include "pbd/basename.h"
#include "ardour/diskstream.h"
#include "ardour/dB.h"
#include "ardour/region_factory.h"
#include "ardour/midi_diskstream.h"
#include "editor.h"
#include "i18n.h"
#include "keyboard.h"
#include "audio_region_view.h"
#include "midi_region_view.h"
#include "ardour_ui.h"
#include "control_point.h"
#include "utils.h"
#include "region_gain_line.h"
#include "editor_drag.h"
#include "audio_time_axis.h"
#include "midi_time_axis.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;
using namespace Gtk;
using namespace Editing;

double const ControlPointDrag::_zero_gain_fraction = gain_to_slider_position (dB_to_coefficient (0.0));

Drag::Drag (Editor* e, ArdourCanvas::Item* i) :
	_editor (e),
	_item (i),
	_pointer_frame_offset (0),
	_grab_frame (0),
	_last_pointer_frame (0),
	_current_pointer_frame (0),
	_had_movement (false),
	_move_threshold_passed (false)
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

	_grab_frame = _editor->event_frame (event, &_grab_x, &_grab_y);
	_last_pointer_frame = _grab_frame;
	_current_pointer_frame = _grab_frame;
	_current_pointer_x = _grab_x;
	_current_pointer_y = _grab_y;
	_last_pointer_x = _current_pointer_x;
	_last_pointer_y = _current_pointer_y;

	_original_x = 0;
	_original_y = 0;
	_item->i2w (_original_x, _original_y);

	_item->grab (Gdk::POINTER_MOTION_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK,
			      *cursor,
			      event->button.time);

	if (_editor->session && _editor->session->transport_rolling()) {
		_was_rolling = true;
	} else {
		_was_rolling = false;
	}

	switch (_editor->snap_type) {
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

/** @param event GDK event, or 0.
 *  @return true if some movement occurred, otherwise false.
 */
bool
Drag::end_grab (GdkEvent* event)
{
	_ending = true;
	
	_editor->stop_canvas_autoscroll ();

	_item->ungrab (event ? event->button.time : 0);

	_last_pointer_x = _current_pointer_x;
	_last_pointer_y = _current_pointer_y;
	finished (event, _had_movement);

	_editor->hide_verbose_canvas_cursor();

	_ending = false;

	return _had_movement;
}

nframes64_t
Drag::adjusted_current_frame (GdkEvent* event) const
{
	nframes64_t pos = 0;
	
	if (_current_pointer_frame > _pointer_frame_offset) {
		pos = _current_pointer_frame - _pointer_frame_offset;
	}

	_editor->snap_to_with_modifier (pos, event);
	
	return pos;
}

bool
Drag::motion_handler (GdkEvent* event, bool from_autoscroll)
{
	_last_pointer_x = _current_pointer_x;
	_last_pointer_y = _current_pointer_y;
	_current_pointer_frame = _editor->event_frame (event, &_current_pointer_x, &_current_pointer_y);

	if (!from_autoscroll && !_move_threshold_passed) {
		
		bool const xp = (::llabs ((nframes64_t) (_current_pointer_x - _grab_x)) > 4LL);
		bool const yp = (::llabs ((nframes64_t) (_current_pointer_y - _grab_y)) > 4LL);
			
		_move_threshold_passed = (xp || yp);

		if (apply_move_threshold() && _move_threshold_passed) {
			
			_grab_frame = _current_pointer_frame;
			_grab_x = _current_pointer_x;
			_grab_y = _current_pointer_y;
			_last_pointer_frame = _grab_frame;
			_pointer_frame_offset = _grab_frame - _last_frame_position;
			
		}
	}

	bool old_had_movement = _had_movement;

	/* a motion event has happened, so we've had movement... */
	_had_movement = true;

	/* ... unless we're using a move threshold and we've not yet passed it */
	if (apply_move_threshold() && !_move_threshold_passed) {
		_had_movement = false;
	}

	if (active (_editor->mouse_mode)) {

		if (event->motion.state & Gdk::BUTTON1_MASK || event->motion.state & Gdk::BUTTON2_MASK) {
			if (!from_autoscroll) {
				_editor->maybe_autoscroll (&event->motion, allow_vertical_autoscroll ());
			}

			motion (event, _had_movement != old_had_movement);
			return true;
		}
	}

	return false;
}


void
Drag::break_drag ()
{
	_editor->stop_canvas_autoscroll ();
	_editor->hide_verbose_canvas_cursor ();

	if (_item) {
		_item->ungrab (0);

		/* put it back where it came from */

		double cxw, cyw;
		cxw = 0;
		cyw = 0;
		_item->i2w (cxw, cyw);
		_item->move (_original_x - cxw, _original_y - cyw);
	}
}


RegionDrag::RegionDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: Drag (e, i),
	  _primary (p),
	  _views (v)
{
	RegionView::RegionViewGoingAway.connect (mem_fun (*this, &RegionDrag::region_going_away));
}

void
RegionDrag::region_going_away (RegionView* v)
{
	_views.remove (v);
}

RegionMotionDrag::RegionMotionDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v, bool b)
	: RegionDrag (e, i, p, v),
	  _dest_trackview (0),
	  _dest_layer (0),
	  _brushing (b)
{
	
}


void
RegionMotionDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);
	
	_editor->show_verbose_time_cursor (_last_frame_position, 10);
}

RegionMotionDrag::TimeAxisViewSummary
RegionMotionDrag::get_time_axis_view_summary ()
{
	int32_t children = 0;
	TimeAxisViewSummary sum;

	_editor->visible_order_range (&sum.visible_y_low, &sum.visible_y_high);
		
	/* get a bitmask representing the visible tracks */

	for (Editor::TrackViewList::iterator i = _editor->track_views.begin(); i != _editor->track_views.end(); ++i) {
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
		TimeAxisView::Children children_list;
		
		/* zeroes are audio/MIDI tracks. ones are other types. */
		
		if (!rtv->hidden()) {
			
			if (!rtv->is_track()) {
				/* not an audio nor MIDI track */
				sum.tracks = sum.tracks |= (0x01 << rtv->order());
			}
			
			sum.height_list[rtv->order()] = (*i)->current_height();
			children = 1;

			if ((children_list = rtv->get_child_list()).size() > 0) {
				for (TimeAxisView::Children::iterator j = children_list.begin(); j != children_list.end(); ++j) { 
					sum.tracks = sum.tracks |= (0x01 << (rtv->order() + children));
					sum.height_list[rtv->order() + children] = (*j)->current_height();
					children++;	
				}
			}
		}
	}

	return sum;
}

bool
RegionMotionDrag::compute_y_delta (
	TimeAxisView const * last_pointer_view, TimeAxisView* current_pointer_view,
	int32_t last_pointer_layer, int32_t current_pointer_layer,
	TimeAxisViewSummary const & tavs,
	int32_t* pointer_order_span, int32_t* pointer_layer_span,
	int32_t* canvas_pointer_order_span
	)
{
	if (_brushing) {
		*pointer_order_span = 0;
		*pointer_layer_span = 0;
		return true;
	}

	bool clamp_y_axis = false;
		
	/* the change in track order between this callback and the last */
	*pointer_order_span = last_pointer_view->order() - current_pointer_view->order();
	/* the change in layer between this callback and the last;
	   only meaningful if pointer_order_span == 0 (ie we've not moved tracks) */
	*pointer_layer_span = last_pointer_layer - current_pointer_layer;

	if (*pointer_order_span != 0) {

		/* find the actual pointer span, in terms of the number of visible tracks;
		   to do this, we reduce |pointer_order_span| by the number of hidden tracks
		   over the span */

		*canvas_pointer_order_span = *pointer_order_span;
		if (last_pointer_view->order() >= current_pointer_view->order()) {
			for (int32_t y = current_pointer_view->order(); y < last_pointer_view->order(); y++) {
				if (tavs.height_list[y] == 0) {
					*canvas_pointer_order_span--;
				}
			}
		} else {
			for (int32_t y = last_pointer_view->order(); y <= current_pointer_view->order(); y++) {
				if (tavs.height_list[y] == 0) {
					*canvas_pointer_order_span++;
				}
			}
		}

		for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			
			RegionView* rv = (*i);

			if (rv->region()->locked()) {
				continue;
			}

			double ix1, ix2, iy1, iy2;
			rv->get_canvas_frame()->get_bounds (ix1, iy1, ix2, iy2);
			rv->get_canvas_frame()->i2w (ix1, iy1);
			iy1 += _editor->vertical_adjustment.get_value() - _editor->canvas_timebars_vsize;

			/* get the new trackview for this particular region */
			pair<TimeAxisView*, int> const tvp = _editor->trackview_by_y_position (iy1);
			assert (tvp.first);
			RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tvp.first);

			/* XXX: not sure that we should be passing canvas_pointer_order_span in here,
			   as surely this is a per-region thing... */
			
			clamp_y_axis = y_movement_disallowed (
				rtv->order(), last_pointer_view->order(), *canvas_pointer_order_span, tavs
				);

			if (clamp_y_axis) {
				break;
			}
		}

	} else if (_dest_trackview == current_pointer_view) {

		if (current_pointer_layer == last_pointer_layer) {
			/* No movement; clamp */
			clamp_y_axis = true;
		} 
	}

	if (!clamp_y_axis) {
		_dest_trackview = current_pointer_view;
		_dest_layer = current_pointer_layer;
	}

	return clamp_y_axis;
}


double
RegionMotionDrag::compute_x_delta (GdkEvent const * event, nframes64_t* pending_region_position)
{
	*pending_region_position = 0;
	
	/* compute the amount of pointer motion in frames, and where
	   the region would be if we moved it by that much.
	*/
	if (_current_pointer_frame >= _pointer_frame_offset) {

		nframes64_t sync_frame;
		nframes64_t sync_offset;
		int32_t sync_dir;
		
		*pending_region_position = _current_pointer_frame - _pointer_frame_offset;
		
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
		
	}
	
	if (*pending_region_position > max_frames - _primary->region()->length()) {
		*pending_region_position = _last_frame_position;
	}

	double x_delta = 0;
	
	if ((*pending_region_position != _last_frame_position) && x_move_allowed ()) {
		
		/* now compute the canvas unit distance we need to move the regionview
		   to make it appear at the new location.
		*/

		x_delta = (static_cast<double> (*pending_region_position) - _last_frame_position) / _editor->frames_per_unit;
		
		if (*pending_region_position <= _last_frame_position) {
			
			for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				
				RegionView* rv = (*i);
				
				// If any regionview is at zero, we need to know so we can stop further leftward motion.
				
				double ix1, ix2, iy1, iy2;
				rv->get_canvas_frame()->get_bounds (ix1, iy1, ix2, iy2);
				rv->get_canvas_frame()->i2w (ix1, iy1);
				
				if (-x_delta > ix1 + _editor->horizontal_adjustment.get_value()) {
					x_delta = 0;
					*pending_region_position = _last_frame_position;
					break;
				}
			}
			
		}
		
		_last_frame_position = *pending_region_position;
	}

	return x_delta;
}

void
RegionMotionDrag::motion (GdkEvent* event, bool first_move)
{
	double y_delta = 0;

	TimeAxisViewSummary tavs = get_time_axis_view_summary ();

	vector<int32_t>::iterator j;

	/* *pointer* variables reflect things about the pointer; as we may be moving
	   multiple regions, much detail must be computed per-region */

	/* current_pointer_view will become the TimeAxisView that we're currently pointing at, and
	   current_pointer_layer the current layer on that TimeAxisView; in this code layer numbers
	   are with respect to how the view's layers are displayed; if we are in Overlaid mode, layer
	   is always 0 regardless of what the region's "real" layer is */
	RouteTimeAxisView* current_pointer_view;
	layer_t current_pointer_layer;
	if (!check_possible (&current_pointer_view, &current_pointer_layer)) {
		return;
	}

	/* TimeAxisView that we were pointing at last time we entered this method */
	TimeAxisView const * const last_pointer_view = _dest_trackview;
	/* the order of the track that we were pointing at last time we entered this method */
	int32_t const last_pointer_order = last_pointer_view->order ();
	/* the layer that we were pointing at last time we entered this method */
	layer_t const last_pointer_layer = _dest_layer;

	int32_t pointer_order_span;
	int32_t pointer_layer_span;
	int32_t canvas_pointer_order_span;
	
	bool const clamp_y_axis = compute_y_delta (
		last_pointer_view, current_pointer_view,
		last_pointer_layer, current_pointer_layer, tavs,
		&pointer_order_span, &pointer_layer_span,
		&canvas_pointer_order_span
		);

	nframes64_t pending_region_position;
	double const x_delta = compute_x_delta (event, &pending_region_position);

	/*************************************************************
	    PREPARE TO MOVE
	************************************************************/

	if (x_delta == 0 && pointer_order_span == 0 && pointer_layer_span == 0) {
		/* haven't reached next snap point, and we're not switching
		   trackviews nor layers. nothing to do.
		*/
		return;
	}

	/*************************************************************
	    MOTION								      
	************************************************************/

	pair<set<boost::shared_ptr<Playlist> >::iterator,bool> insert_result;
	
	for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		
		RegionView* rv = (*i);
		
		if (rv->region()->locked()) {
			continue;
		}
		
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
		
		if (first_move) {
			
			// hide any dependent views 
			
			rv->get_time_axis_view().hide_dependent_views (*rv);
			
			/* 
			   reparent to a non scrolling group so that we can keep the 
			   region selection above all time axis views.
			   reparenting means we have to move the rv as the two 
			   parent groups have different coordinates.
			*/
			
			rv->get_canvas_group()->property_y() = iy1 - 1;
			rv->get_canvas_group()->reparent(*(_editor->_region_motion_group));
			
			rv->fake_set_opaque (true);
		}
		
		/* current view for this particular region */
		pair<TimeAxisView*, int> pos = _editor->trackview_by_y_position (iy1);
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (pos.first);
		
		if (pointer_order_span != 0 && !clamp_y_axis) {
			
			/* INTER-TRACK MOVEMENT */
			
			/* move through the height list to the track that the region is currently on */
			vector<int32_t>::iterator j = tavs.height_list.begin ();
			int32_t x = 0;
			while (j != tavs.height_list.end () && x != rtv->order ()) {
				++x;
				++j;
			}
			
			y_delta = 0;
			int32_t temp_pointer_order_span = canvas_pointer_order_span;
			
			if (j != tavs.height_list.end ()) {
				
				/* Account for layers in the original and
				   destination tracks.  If we're moving around in layers we assume
				   that only one track is involved, so it's ok to use *pointer*
				   variables here. */
				
				StreamView* lv = last_pointer_view->view ();
				assert (lv);
				
				/* move to the top of the last trackview */
				if (lv->layer_display () == Stacked) {
					y_delta -= (lv->layers() - last_pointer_layer - 1) * lv->child_height ();
				}
					
				StreamView* cv = current_pointer_view->view ();
				assert (cv);

				/* move to the right layer on the current trackview */
				if (cv->layer_display () == Stacked) {
					y_delta += (cv->layers() - current_pointer_layer - 1) * cv->child_height ();
				}
				
				/* And for being on a non-topmost layer on the new
				   track */
				
				while (temp_pointer_order_span > 0) {
					/* we're moving up canvas-wise,
					   so we need to find the next track height
					*/
					if (j != tavs.height_list.begin()) {		  
						j--;
					}
					
					if (x != last_pointer_order) {
						if ((*j) == 0) {
							++temp_pointer_order_span;
						}
					}
					
					y_delta -= (*j);
					temp_pointer_order_span--;
				}
				
				while (temp_pointer_order_span < 0) {
					
					y_delta += (*j);
					
					if (x != last_pointer_order) {
						if ((*j) == 0) {
							--temp_pointer_order_span;
						}
					}
					
					if (j != tavs.height_list.end()) {		      
						j++;
					}
					
					temp_pointer_order_span++;
				}
				
				
				/* find out where we'll be when we move and set height accordingly */
				
				pair<TimeAxisView*, int> const pos = _editor->trackview_by_y_position (iy1 + y_delta);
				RouteTimeAxisView const * temp_rtv = dynamic_cast<RouteTimeAxisView*> (pos.first);
				rv->set_height (temp_rtv->view()->child_height());
				
				/* if you un-comment the following, the region colours will follow
				   the track colours whilst dragging; personally
				   i think this can confuse things, but never mind.
				*/
				
				//const GdkColor& col (temp_rtv->view->get_region_color());
				//rv->set_color (const_cast<GdkColor&>(col));
			}
		}
		
		if (pointer_order_span == 0 && pointer_layer_span != 0 && !clamp_y_axis) {
			
			/* INTER-LAYER MOVEMENT in the same track */
			y_delta = rtv->view()->child_height () * pointer_layer_span;
		}
		
		
		if (_brushing) {
			_editor->mouse_brush_insert_region (rv, pending_region_position);
		} else {
			rv->move (x_delta, y_delta);
		}
		
	} /* foreach region */

	if (first_move) {
		_editor->cursor_group->raise_to_top();
	}

	if (x_delta != 0 && !_brushing) {
		_editor->show_verbose_time_cursor (_last_frame_position, 10);
	}
}

void
RegionMoveDrag::motion (GdkEvent* event, bool first_move)
{
	if (_copy && first_move) {
		copy_regions (event);
	}

	RegionMotionDrag::motion (event, first_move);
}

void
RegionMoveDrag::finished (GdkEvent* /*event*/, bool movement_occurred)
{
	vector<RegionView*> copies;
	boost::shared_ptr<Diskstream> ds;
	boost::shared_ptr<Playlist> from_playlist;
	RegionSelection new_views;
	typedef set<boost::shared_ptr<Playlist> > PlaylistSet;
	PlaylistSet modified_playlists;
	PlaylistSet frozen_playlists;
	list <sigc::connection> modified_playlist_connections;
	pair<PlaylistSet::iterator,bool> insert_result, frozen_insert_result;
	nframes64_t drag_delta;
	bool changed_tracks, changed_position;
	map<RegionView*, pair<RouteTimeAxisView*, int> > final;
	RouteTimeAxisView* source_tv;

	if (!movement_occurred) {
		/* just a click */
		return;
	}

	if (Config->get_edit_mode() == Splice && !_editor->pre_drag_region_selection.empty()) {
		_editor->selection->set (_editor->pre_drag_region_selection);
		_editor->pre_drag_region_selection.clear ();
	}

	if (_brushing) {
		/* all changes were made during motion event handlers */
		
		if (_copy) {
			for (list<RegionView*>::iterator i = _views.begin(); i != _views.end(); ++i) {
				copies.push_back (*i);
			}
		}

		goto out;
	}

	/* reverse this here so that we have the correct logic to finalize
	   the drag.
	*/
	
	if (Config->get_edit_mode() == Lock && !_copy) {
		_x_constrained = !_x_constrained;
	}

	if (_copy) {
		if (_x_constrained) {
			_editor->begin_reversible_command (_("fixed time region copy"));
		} else {
			_editor->begin_reversible_command (_("region copy"));
		} 
	} else {
		if (_x_constrained) {
			_editor->begin_reversible_command (_("fixed time region drag"));
		} else {
			_editor->begin_reversible_command (_("region drag"));
		}
	}

	changed_position = (_last_frame_position != (nframes64_t) (_primary->region()->position()));
	changed_tracks = (_dest_trackview != &_primary->get_time_axis_view());

	drag_delta = _primary->region()->position() - _last_frame_position;

	_editor->update_canvas_now ();

	/* make a list of where each region ended up */
	final = find_time_axis_views_and_layers ();

	for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ) {

		RegionView* rv = (*i);
		RouteTimeAxisView* dest_rtv = final[*i].first;
		layer_t dest_layer = final[*i].second;

		nframes64_t where;

		if (rv->region()->locked()) {
			++i;
			continue;
		}

		if (changed_position && !_x_constrained) {
			where = rv->region()->position() - drag_delta;
		} else {
			where = rv->region()->position();
		}
			
		boost::shared_ptr<Region> new_region;

		if (_copy) {
			/* we already made a copy */
			new_region = rv->region();

			/* undo the previous hide_dependent_views so that xfades don't
			   disappear on copying regions 
			*/
		
			//rv->get_time_axis_view().reveal_dependent_views (*rv);
		
		} else if (changed_tracks && dest_rtv->playlist()) {
			new_region = RegionFactory::create (rv->region());
		}

		if (changed_tracks || _copy) {

			boost::shared_ptr<Playlist> to_playlist = dest_rtv->playlist();
			
			if (!to_playlist) {
				++i;
				continue;
			}

			_editor->latest_regionviews.clear ();

			sigc::connection c = dest_rtv->view()->RegionViewAdded.connect (mem_fun(*_editor, &Editor::collect_new_region_view));
			
			insert_result = modified_playlists.insert (to_playlist);
			
			if (insert_result.second) {
				_editor->session->add_command (new MementoCommand<Playlist>(*to_playlist, &to_playlist->get_state(), 0));
			}

			to_playlist->add_region (new_region, where);
			if (dest_rtv->view()->layer_display() == Stacked) {
				new_region->set_layer (dest_layer);
				new_region->set_pending_explicit_relayer (true);
			}

			c.disconnect ();
							      
			if (!_editor->latest_regionviews.empty()) {
				// XXX why just the first one ? we only expect one
				// commented out in nick_m's canvas reworking. is that intended?
				//dest_atv->reveal_dependent_views (*latest_regionviews.front());
				new_views.push_back (_editor->latest_regionviews.front());
			}

		} else {
			/* 
			   motion on the same track. plonk the previously reparented region 
			   back to its original canvas group (its streamview).
			   No need to do anything for copies as they are fake regions which will be deleted.
			*/

			rv->get_canvas_group()->reparent (*dest_rtv->view()->canvas_item());
			rv->get_canvas_group()->property_y() = 0;
		  
			/* just change the model */
			
			boost::shared_ptr<Playlist> playlist = dest_rtv->playlist();

			if (dest_rtv->view()->layer_display() == Stacked) {
				rv->region()->set_layer (dest_layer);
				rv->region()->set_pending_explicit_relayer (true);
			}
			
			insert_result = modified_playlists.insert (playlist);
			
			if (insert_result.second) {
				_editor->session->add_command (new MementoCommand<Playlist>(*playlist, &playlist->get_state(), 0));
			}
			/* freeze to avoid lots of relayering in the case of a multi-region drag */
			frozen_insert_result = frozen_playlists.insert(playlist);
			
			if (frozen_insert_result.second) {
				playlist->freeze();
			}

			rv->region()->set_position (where, (void*) this);
		}

		if (changed_tracks && !_copy) {

			/* get the playlist where this drag started. we can't use rv->region()->playlist()
			   because we may have copied the region and it has not been attached to a playlist.
			*/
			
			source_tv = dynamic_cast<RouteTimeAxisView*> (&rv->get_time_axis_view());
			ds = source_tv->get_diskstream();
			from_playlist = ds->playlist();

			assert (source_tv);
			assert (ds);
			assert (from_playlist);

			/* moved to a different audio track, without copying */

			/* the region that used to be in the old playlist is not
			   moved to the new one - we use a copy of it. as a result,
			   any existing editor for the region should no longer be
			   visible.
			*/ 
	    
			rv->hide_region_editor();
			rv->fake_set_opaque (false);
			
			/* remove the region from the old playlist */

			insert_result = modified_playlists.insert (from_playlist);
			
			if (insert_result.second) {
				_editor->session->add_command (new MementoCommand<Playlist>(*from_playlist, &from_playlist->get_state(), 0));
			}
	
			from_playlist->remove_region (rv->region());
			
			/* OK, this is where it gets tricky. If the playlist was being used by >1 tracks, and the region
			   was selected in all of them, then removing it from a playlist will have removed all
			   trace of it from the selection (i.e. there were N regions selected, we removed 1,
			   but since its the same playlist for N tracks, all N tracks updated themselves, removed the
			   corresponding regionview, and the selection is now empty).

			   this could have invalidated any and all iterators into the region selection.

			   the heuristic we use here is: if the region selection is empty, break out of the loop
			   here. if the region selection is not empty, then restart the loop because we know that
			   we must have removed at least the region(view) we've just been working on as well as any
			   that we processed on previous iterations.

			   EXCEPT .... if we are doing a copy drag, then the selection hasn't been modified and
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
		
		if (_copy) {
			copies.push_back (rv);
		}
	}

	_editor->selection->add (new_views);

	for (set<boost::shared_ptr<Playlist> >::iterator p = frozen_playlists.begin(); p != frozen_playlists.end(); ++p) {
		(*p)->thaw();
	}
			
  out:
	for (set<boost::shared_ptr<Playlist> >::iterator p = modified_playlists.begin(); p != modified_playlists.end(); ++p) {
		_editor->session->add_command (new MementoCommand<Playlist>(*(*p), 0, &(*p)->get_state()));	
	}
	
	_editor->commit_reversible_command ();

	for (vector<RegionView*>::iterator x = copies.begin(); x != copies.end(); ++x) {
		delete *x;
	}
}


bool
RegionMoveDrag::x_move_allowed () const
{
	if (Config->get_edit_mode() == Lock) {
		if (_copy) {
			return !_x_constrained;
		} else {
			/* in locked edit mode, reverse the usual meaning of _x_constrained */
			return _x_constrained;
		}
	}
	
	return !_x_constrained;
}

bool
RegionInsertDrag::x_move_allowed () const
{
	if (Config->get_edit_mode() == Lock) {
		return _x_constrained;
	}

	return !_x_constrained;
}

void
RegionMotionDrag::copy_regions (GdkEvent* event)
{
	/* duplicate the regionview(s) and region(s) */

	list<RegionView*> new_regionviews;
	
	for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
		
		RegionView* rv = (*i);
		AudioRegionView* arv = dynamic_cast<AudioRegionView*>(rv);
		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(rv);
		
		const boost::shared_ptr<const Region> original = rv->region();
		boost::shared_ptr<Region> region_copy = RegionFactory::create (original);

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
		new_regionviews.push_back (nrv);
	}
	
	if (new_regionviews.empty()) {
		return;
	}
	
	/* reflect the fact that we are dragging the copies */
	
	_primary = new_regionviews.front();
	_views = new_regionviews;
		
	swap_grab (new_regionviews.front()->get_canvas_group (), 0, event ? event->motion.time : 0);
	
	/* 
	   sync the canvas to what we think is its current state 
	   without it, the canvas seems to 
	   "forget" to update properly after the upcoming reparent() 
	   ..only if the mouse is in rapid motion at the time of the grab. 
	   something to do with regionview creation raking so long?
	*/
	_editor->update_canvas_now();
}

bool
RegionMotionDrag::check_possible (RouteTimeAxisView** tv, layer_t* layer)
{
	/* Which trackview is this ? */

	pair<TimeAxisView*, int> const tvp = _editor->trackview_by_y_position (current_pointer_y ());
	(*tv) = dynamic_cast<RouteTimeAxisView*> (tvp.first);
	(*layer) = tvp.second;

	if (*tv && (*tv)->layer_display() == Overlaid) {
		*layer = 0;
	}

	/* The region motion is only processed if the pointer is over
	   an audio track.
	*/
	
	if (!(*tv) || !(*tv)->is_track()) {
		/* To make sure we hide the verbose canvas cursor when the mouse is 
		   not held over and audiotrack. 
		*/
		_editor->hide_verbose_canvas_cursor ();
		return false;
	}

	return true;
}

/** @param new_order New track order.
 *  @param old_order Old track order.
 *  @param visible_y_low Lowest visible order.
 *  @return true if y movement should not happen, otherwise false.
 */
bool
RegionMotionDrag::y_movement_disallowed (int new_order, int old_order, int y_span, TimeAxisViewSummary const & tavs) const
{
	if (new_order != old_order) {

		/* this isn't the pointer track */	

		if (y_span > 0) {

			/* moving up the canvas */
			if ( (new_order - y_span) >= tavs.visible_y_low) {

				int32_t n = 0;

				/* work out where we'll end up with this y span, taking hidden TimeAxisViews into account */
				int32_t visible_tracks = 0;
				while (visible_tracks < y_span ) {
					visible_tracks++;
					while (tavs.height_list[new_order - (visible_tracks - n)] == 0) {
						/* passing through a hidden track */
						n--;
					}		  
				}
		 
				if (tavs.tracks[new_order - (y_span - n)] != 0x00) {
					/* moving to a non-track; disallow */
					return true;
				}
				

			} else {
				/* moving beyond the lowest visible track; disallow */
				return true;
			}		  
		  
		} else if (y_span < 0) {

			/* moving down the canvas */
			if ((new_order - y_span) <= tavs.visible_y_high) {

				int32_t visible_tracks = 0;
				int32_t n = 0;
				while (visible_tracks > y_span ) {
					visible_tracks--;
		      
					while (tavs.height_list[new_order - (visible_tracks - n)] == 0) {
						/* passing through a hidden track */
						n++;
					}		 
				}
						
				if (tavs.tracks[new_order - (y_span - n)] != 0x00) {
					/* moving to a non-track; disallow */
					return true;
				}

				
			} else {

				/* moving beyond the highest visible track; disallow */
				return true;
			}
		}		
		
	} else {
		
		/* this is the pointer's track */
		
		if ((new_order - y_span) > tavs.visible_y_high) {
			/* we will overflow */
			return true;
		} else if ((new_order - y_span) < tavs.visible_y_low) {
			/* we will overflow */
			return true;
		}
	}

	return false;
}


RegionMoveDrag::RegionMoveDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v, bool b, bool c)
	: RegionMotionDrag (e, i, p, v, b),
	  _copy (c)
{
	TimeAxisView* const tv = &_primary->get_time_axis_view ();
	
	_dest_trackview = tv;
	if (tv->layer_display() == Overlaid) {
		_dest_layer = 0;
	} else {
		_dest_layer = _primary->region()->layer ();
	}

	double speed = 1;
	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tv);
	if (rtv && rtv->is_track()) {
		speed = rtv->get_diskstream()->speed ();
	}

	_last_frame_position = static_cast<nframes64_t> (_primary->region()->position() / speed);
}

void
RegionMoveDrag::start_grab (GdkEvent* event, Gdk::Cursor* c)
{
	RegionMotionDrag::start_grab (event, c);
	
	_pointer_frame_offset = _grab_frame - _last_frame_position;
}

RegionInsertDrag::RegionInsertDrag (Editor* e, boost::shared_ptr<Region> r, RouteTimeAxisView* v, nframes64_t pos)
	: RegionMotionDrag (e, 0, 0, list<RegionView*> (), false)
{
	assert ((boost::dynamic_pointer_cast<AudioRegion> (r) && dynamic_cast<AudioTimeAxisView*> (v)) ||
		(boost::dynamic_pointer_cast<MidiRegion> (r) && dynamic_cast<MidiTimeAxisView*> (v)));

	_primary = v->view()->create_region_view (r, false, false);
	
	_primary->get_canvas_group()->show ();
	_primary->set_position (pos, 0);
	_views.push_back (_primary);

	_last_frame_position = pos;

	_item = _primary->get_canvas_group ();
	_dest_trackview = v;
	_dest_layer = _primary->region()->layer ();
}

map<RegionView*, pair<RouteTimeAxisView*, int> >
RegionMotionDrag::find_time_axis_views_and_layers ()
{
	map<RegionView*, pair<RouteTimeAxisView*, int> > tav;
	
	for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i) {

		double ix1, ix2, iy1, iy2;
		(*i)->get_canvas_frame()->get_bounds (ix1, iy1, ix2, iy2);
		(*i)->get_canvas_frame()->i2w (ix1, iy1);
		iy1 += _editor->vertical_adjustment.get_value() - _editor->canvas_timebars_vsize;

		pair<TimeAxisView*, int> tv = _editor->trackview_by_y_position (iy1);
		tav[*i] = make_pair (dynamic_cast<RouteTimeAxisView*> (tv.first), tv.second);
	}

	return tav;
}


void
RegionInsertDrag::finished (GdkEvent* /*event*/, bool /*movement_occurred*/)
{
	_editor->update_canvas_now ();

	map<RegionView*, pair<RouteTimeAxisView*, int> > final = find_time_axis_views_and_layers ();
	
	RouteTimeAxisView* dest_rtv = final[_primary].first;

	_primary->get_canvas_group()->reparent (*dest_rtv->view()->canvas_item());
	_primary->get_canvas_group()->property_y() = 0;

	boost::shared_ptr<Playlist> playlist = dest_rtv->playlist();

	_editor->begin_reversible_command (_("insert region"));
	XMLNode& before = playlist->get_state ();
	playlist->add_region (_primary->region (), _last_frame_position);
	_editor->session->add_command (new MementoCommand<Playlist> (*playlist, &before, &playlist->get_state()));
	_editor->commit_reversible_command ();

	delete _primary;
	_primary = 0;
	_views.clear ();
}

RegionSpliceDrag::RegionSpliceDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionMoveDrag (e, i, p, v, false, false)
{

}

struct RegionSelectionByPosition {
    bool operator() (RegionView*a, RegionView* b) {
	    return a->region()->position () < b->region()->position();
    }
};

void
RegionSpliceDrag::motion (GdkEvent* /*event*/, bool)
{
	RouteTimeAxisView* tv;
	layer_t layer;
	
	if (!check_possible (&tv, &layer)) {
		return;
	}

	int dir;

	if (_current_pointer_x - _grab_x > 0) {
		dir = 1;
	} else {
		dir = -1;
	}

	RegionSelection copy (_editor->selection->regions);

	RegionSelectionByPosition cmp;
	copy.sort (cmp);

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
			if (_current_pointer_frame < (*i)->region()->last_frame() + 1) {
				continue;
			}
		} else {
			if (_current_pointer_frame > (*i)->region()->first_frame()) {
				continue;
			}
		}

		
		playlist->shuffle ((*i)->region(), dir);

		_grab_x = _current_pointer_x;
	}
}

void
RegionSpliceDrag::finished (GdkEvent* /*event*/, bool)
{
	
}


RegionCreateDrag::RegionCreateDrag (Editor* e, ArdourCanvas::Item* i, TimeAxisView* v)
	: Drag (e, i),
	  _view (v)
{
	
}

void
RegionCreateDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	_dest_trackview = _view;
	
	Drag::start_grab (event);
}


void
RegionCreateDrag::motion (GdkEvent* /*event*/, bool first_move)
{
	if (first_move) {
		// TODO: create region-create-drag region view here
	}

	// TODO: resize region-create-drag region view here
} 

void
RegionCreateDrag::finished (GdkEvent* event, bool movement_occurred)
{
	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*> (_dest_trackview);

	if (!mtv) {
		return;
	}

	if (!movement_occurred) {
		mtv->add_region (_grab_frame);
	} else {
		motion (event, false);
		// TODO: create region-create-drag region here
	}
}

void
RegionGainDrag::motion (GdkEvent* /*event*/, bool)
{
	
}

void
RegionGainDrag::finished (GdkEvent *, bool)
{

}

TrimDrag::TrimDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{

}

void
TrimDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	double speed = 1.0;
	TimeAxisView* tvp = &_primary->get_time_axis_view ();
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);

	if (tv && tv->is_track()) {
		speed = tv->get_diskstream()->speed();
	}
	
	nframes64_t region_start = (nframes64_t) (_primary->region()->position() / speed);
	nframes64_t region_end = (nframes64_t) (_primary->region()->last_frame() / speed);
	nframes64_t region_length = (nframes64_t) (_primary->region()->length() / speed);

	Drag::start_grab (event, _editor->trimmer_cursor);
	
	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
		_operation = ContentsTrim;
	} else {
		/* These will get overridden for a point trim.*/
		if (_current_pointer_frame < (region_start + region_length/2)) {
			/* closer to start */
			_operation = StartTrim;
		} else if (_current_pointer_frame > (region_end - region_length/2)) {
			/* closer to end */
			_operation = EndTrim;
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
		_editor->show_verbose_time_cursor (_current_pointer_frame, 10);	
		break;
	}
}

void
TrimDrag::motion (GdkEvent* event, bool first_move)
{
	RegionView* rv = _primary;
	nframes64_t frame_delta = 0;

	bool left_direction;
	bool obey_snap = !Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier());

	/* snap modifier works differently here..
	   its' current state has to be passed to the 
	   various trim functions in order to work properly 
	*/ 

	double speed = 1.0;
	TimeAxisView* tvp = &_primary->get_time_axis_view ();
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);
	pair<set<boost::shared_ptr<Playlist> >::iterator,bool> insert_result;

	if (tv && tv->is_track()) {
		speed = tv->get_diskstream()->speed();
	}
	
	if (_last_pointer_frame > _current_pointer_frame) {
		left_direction = true;
	} else {
		left_direction = false;
	}

	_editor->snap_to_with_modifier (_current_pointer_frame, event);

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

		for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
			(*i)->fake_set_opaque(false);
			(*i)->region()->freeze ();
		
			AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);

			if (arv){
				arv->temporarily_hide_envelope ();
			}

			boost::shared_ptr<Playlist> pl = (*i)->region()->playlist();
			insert_result = _editor->motion_frozen_playlists.insert (pl);

			if (insert_result.second) {
				_editor->session->add_command(new MementoCommand<Playlist>(*pl, &pl->get_state(), 0));
				pl->freeze();
			}
		}
	}

	if (_current_pointer_frame == _last_pointer_frame) {
		return;
	}

	if (left_direction) {
		frame_delta = (_last_pointer_frame - _current_pointer_frame);
	} else {
		frame_delta = (_current_pointer_frame - _last_pointer_frame);
	}

	bool non_overlap_trim = false;

	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
		non_overlap_trim = true;
	}

	switch (_operation) {		
	case StartTrim:
		if ((left_direction == false) && (_current_pointer_frame <= rv->region()->first_frame()/speed)) {
			break;
		} else {

			for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				_editor->single_start_trim (**i, frame_delta, left_direction, obey_snap, non_overlap_trim);
			}
			break;
		}
		
	case EndTrim:
		if ((left_direction == true) && (_current_pointer_frame > (nframes64_t) (rv->region()->last_frame()/speed))) {
			break;
		} else {

			for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				_editor->single_end_trim (**i, frame_delta, left_direction, obey_snap, non_overlap_trim);
			}
			break;
		}
		
	case ContentsTrim:
		{
			bool swap_direction = false;

			if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
				swap_direction = true;
			}
			
			for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i)
			{
				_editor->single_contents_trim (**i, frame_delta, left_direction, swap_direction, obey_snap);
			}
		}
		break;
	}

	switch (_operation) {
	case StartTrim:
		_editor->show_verbose_time_cursor((nframes64_t) (rv->region()->position()/speed), 10);	
		break;
	case EndTrim:
		_editor->show_verbose_time_cursor((nframes64_t) (rv->region()->last_frame()/speed), 10);	
		break;
	case ContentsTrim:
		_editor->show_verbose_time_cursor(_current_pointer_frame, 10);	
		break;
	}

	_last_pointer_frame = _current_pointer_frame;
}


void
TrimDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		motion (event, false);
		
		if (!_editor->selection->selected (_primary)) {
			_editor->thaw_region_after_trim (*_primary);		
		} else {
			
			for (list<RegionView*>::const_iterator i = _views.begin(); i != _views.end(); ++i) {
				_editor->thaw_region_after_trim (**i);
				(*i)->fake_set_opaque (true);
			}
		}
		
		for (set<boost::shared_ptr<Playlist> >::iterator p = _editor->motion_frozen_playlists.begin(); p != _editor->motion_frozen_playlists.end(); ++p) {
			(*p)->thaw ();
			_editor->session->add_command (new MementoCommand<Playlist>(*(*p).get(), 0, &(*p)->get_state()));
		}
		
		_editor->motion_frozen_playlists.clear ();

		_editor->commit_reversible_command();
	} else {
		/* no mouse movement */
		_editor->point_trim (event);
	}
}

MeterMarkerDrag::MeterMarkerDrag (Editor* e, ArdourCanvas::Item* i, bool c)
	: Drag (e, i),
	  _copy (c)
{
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
	
	_pointer_frame_offset = _grab_frame - _marker->meter().frame();	

	_editor->show_verbose_time_cursor (_current_pointer_frame, 10);
}

void
MeterMarkerDrag::motion (GdkEvent* event, bool)
{
	nframes64_t const adjusted_frame = adjusted_current_frame (event);
	
	if (adjusted_frame == _last_pointer_frame) {
		return;
	}

	_marker->set_position (adjusted_frame);
	
	_last_pointer_frame = adjusted_frame;

	_editor->show_verbose_time_cursor (adjusted_frame, 10);
}

void
MeterMarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	motion (event, false);
	
	BBT_Time when;
	
	TempoMap& map (_editor->session->tempo_map());
	map.bbt_time (_last_pointer_frame, when);
	
	if (_copy == true) {
		_editor->begin_reversible_command (_("copy meter mark"));
		XMLNode &before = map.get_state();
		map.add_meter (_marker->meter(), when);
		XMLNode &after = map.get_state();
		_editor->session->add_command(new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();

		// delete the dummy marker we used for visual representation of copying.
		// a new visual marker will show up automatically.
		delete _marker;
	} else {
		_editor->begin_reversible_command (_("move meter mark"));
		XMLNode &before = map.get_state();
		map.move_meter (_marker->meter(), when);
		XMLNode &after = map.get_state();
		_editor->session->add_command(new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();
	}
}

TempoMarkerDrag::TempoMarkerDrag (Editor* e, ArdourCanvas::Item* i, bool c)
	: Drag (e, i),
	  _copy (c)
{
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

	_pointer_frame_offset = _grab_frame - _marker->tempo().frame();	
	_editor->show_verbose_time_cursor (_current_pointer_frame, 10);
}

void
TempoMarkerDrag::motion (GdkEvent* event, bool)
{
	nframes64_t const adjusted_frame = adjusted_current_frame (event);
	
	if (adjusted_frame == _last_pointer_frame) {
		return;
	}

	/* OK, we've moved far enough to make it worth actually move the thing. */
		
	_marker->set_position (adjusted_frame);
	
	_editor->show_verbose_time_cursor (adjusted_frame, 10);

	_last_pointer_frame = adjusted_frame;
}

void
TempoMarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}
	
	motion (event, false);
	
	BBT_Time when;
	
	TempoMap& map (_editor->session->tempo_map());
	map.bbt_time (_last_pointer_frame, when);

	if (_copy == true) {
		_editor->begin_reversible_command (_("copy tempo mark"));
		XMLNode &before = map.get_state();
		map.add_tempo (_marker->tempo(), when);
		XMLNode &after = map.get_state();
		_editor->session->add_command (new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();
		
		// delete the dummy marker we used for visual representation of copying.
		// a new visual marker will show up automatically.
		delete _marker;
	} else {
		_editor->begin_reversible_command (_("move tempo mark"));
		XMLNode &before = map.get_state();
		map.move_tempo (_marker->tempo(), when);
		XMLNode &after = map.get_state();
		_editor->session->add_command (new MementoCommand<TempoMap>(map, &before, &after));
		_editor->commit_reversible_command ();
	}
}


CursorDrag::CursorDrag (Editor* e, ArdourCanvas::Item* i, bool s)
	: Drag (e, i),
	  _stop (s)
{
	_cursor = reinterpret_cast<EditorCursor*> (_item->get_data ("cursor"));
	assert (_cursor);
}

void
CursorDrag::start_grab (GdkEvent* event, Gdk::Cursor* c)
{
	Drag::start_grab (event, c);

	if (!_stop) {
		
		nframes64_t where = _editor->event_frame (event, 0, 0);

		_editor->snap_to_with_modifier (where, event);
		_editor->playhead_cursor->set_position (where);

	}

	if (_cursor == _editor->playhead_cursor) {
		_editor->_dragging_playhead = true;

		if (_editor->session && _was_rolling && _stop) {
			_editor->session->request_stop ();
		}

		if (_editor->session && _editor->session->is_auditioning()) {
			_editor->session->cancel_audition ();
		}
	}

	_editor->show_verbose_time_cursor (_cursor->current_frame, 10);
}

void
CursorDrag::motion (GdkEvent* event, bool)
{
	nframes64_t const adjusted_frame = adjusted_current_frame (event);

	if (adjusted_frame == _last_pointer_frame) {
		return;
	}

	_cursor->set_position (adjusted_frame);

	_editor->show_verbose_time_cursor (_cursor->current_frame, 10);

#ifdef GTKOSX
	_editor->update_canvas_now ();
#endif
	_editor->UpdateAllTransportClocks (_cursor->current_frame);

	_last_pointer_frame = adjusted_frame;
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
		if (_editor->session) {
			_editor->session->request_locate (_editor->playhead_cursor->current_frame, _was_rolling);
			_editor->_pending_locate_request = true;
		}
	} 
}

FadeInDrag::FadeInDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	
}

void
FadeInDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	AudioRegionView* a = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> const r = a->audio_region ();
	
	_pointer_frame_offset = _grab_frame - ((nframes64_t) r->fade_in()->back()->when + r->position());	
}

void
FadeInDrag::motion (GdkEvent* event, bool)
{
	nframes64_t fade_length;

	nframes64_t const pos = adjusted_current_frame (event);
	
	boost::shared_ptr<Region> region = _primary->region ();

	if (pos < (region->position() + 64)) {
		fade_length = 64; // this should be a minimum defined somewhere
	} else if (pos > region->last_frame()) {
		fade_length = region->length();
	} else {
		fade_length = pos - region->position();
	}		

	for (RegionSelection::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*i);
		
		if (!tmp) {
			continue;
		}
	
		tmp->reset_fade_in_shape_width (fade_length);
	}

	_editor->show_verbose_duration_cursor (region->position(), region->position() + fade_length, 10);
}

void
FadeInDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	nframes64_t fade_length;

	nframes64_t const pos = adjusted_current_frame (event);
	
	boost::shared_ptr<Region> region = _primary->region ();

	if (pos < (region->position() + 64)) {
		fade_length = 64; // this should be a minimum defined somewhere
	} else if (pos > region->last_frame()) {
		fade_length = region->length();
	} else {
		fade_length = pos - region->position();
	}
		
	_editor->begin_reversible_command (_("change fade in length"));

	for (RegionSelection::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*i);
		
		if (!tmp) {
			continue;
		}
	
		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_in();
		XMLNode &before = alist->get_state();

		tmp->audio_region()->set_fade_in_length (fade_length);
		tmp->audio_region()->set_fade_in_active (true);
		
		XMLNode &after = alist->get_state();
		_editor->session->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	_editor->commit_reversible_command ();
}

FadeOutDrag::FadeOutDrag (Editor* e, ArdourCanvas::Item* i, RegionView* p, list<RegionView*> const & v)
	: RegionDrag (e, i, p, v)
{
	
}

void
FadeOutDrag::start_grab (GdkEvent* event, Gdk::Cursor* cursor)
{
	Drag::start_grab (event, cursor);

	AudioRegionView* a = dynamic_cast<AudioRegionView*> (_primary);
	boost::shared_ptr<AudioRegion> r = a->audio_region ();
	
	_pointer_frame_offset = _grab_frame - (r->length() - (nframes64_t) r->fade_out()->back()->when + r->position());
}

void
FadeOutDrag::motion (GdkEvent* event, bool)
{
	nframes64_t fade_length;

	nframes64_t const pos = adjusted_current_frame (event);

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
		
	for (RegionSelection::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*i);
		
		if (!tmp) {
			continue;
		}
	
		tmp->reset_fade_out_shape_width (fade_length);
	}

	_editor->show_verbose_duration_cursor (region->last_frame() - fade_length, region->last_frame(), 10);
}

void
FadeOutDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {
		return;
	}

	nframes64_t fade_length;

	nframes64_t const pos = adjusted_current_frame (event);

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

	for (RegionSelection::iterator i = _views.begin(); i != _views.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*i);
		
		if (!tmp) {
			continue;
		}
	
		boost::shared_ptr<AutomationList> alist = tmp->audio_region()->fade_out();
		XMLNode &before = alist->get_state();
		
		tmp->audio_region()->set_fade_out_length (fade_length);
		tmp->audio_region()->set_fade_out_active (true);

		XMLNode &after = alist->get_state();
		_editor->session->add_command(new MementoCommand<AutomationList>(*alist.get(), &before, &after));
	}

	_editor->commit_reversible_command ();
}

MarkerDrag::MarkerDrag (Editor* e, ArdourCanvas::Item* i)
	: Drag (e, i)
{
	_marker = reinterpret_cast<Marker*> (_item->get_data ("marker"));
	assert (_marker);

	_points.push_back (Gnome::Art::Point (0, 0));
	_points.push_back (Gnome::Art::Point (0, _editor->physical_screen_height));

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

	_pointer_frame_offset = _grab_frame - (is_start ? location->start() : location->end());	

	update_item (location);

	// _drag_line->show();
	// _line->raise_to_top();

	if (is_start) {
		_editor->show_verbose_time_cursor (location->start(), 10);
	} else {
		_editor->show_verbose_time_cursor (location->end(), 10);
	}

	Selection::Operation op = Keyboard::selection_type (event->button.state);

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
		nframes64_t s, e;
		_editor->selection->markers.range (s, e);
		s = min (_marker->position(), s);
		e = max (_marker->position(), e);
		s = min (s, e);
		e = max (s, e);
		if (e < max_frames) {
			++e;
		}
		_editor->session->locations()->find_all_between (s, e, ll, Location::Flags (0));
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

	/* set up copies for us to manipulate during the drag */

	for (MarkerSelection::iterator i = _editor->selection->markers.begin(); i != _editor->selection->markers.end(); ++i) {
		Location *l = _editor->find_location_from_marker (*i, is_start);
		_copied_locations.push_back (new Location (*l));
	}
}

void
MarkerDrag::motion (GdkEvent* event, bool)
{
	nframes64_t f_delta = 0;
	bool is_start;
	bool move_both = false;
	Marker* marker;
	Location  *real_location;
	Location *copy_location = 0;

	nframes64_t const newframe = adjusted_current_frame (event);

	nframes64_t next = newframe;
	
	if (_current_pointer_frame == _last_pointer_frame) { 
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

			/* just move it */
			
			copy_location->set_start (copy_location->start() + f_delta);

		} else {
			
			nframes64_t new_start = copy_location->start() + f_delta;
			nframes64_t new_end = copy_location->end() + f_delta;
			
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

	_last_pointer_frame = _current_pointer_frame;

	assert (!_copied_locations.empty());

	_editor->edit_point_clock.set (_copied_locations.front()->start());
	_editor->show_verbose_time_cursor (newframe, 10);

#ifdef GTKOSX
	_editor->update_canvas_now ();
#endif
	_editor->edit_point_clock.set (copy_location->start());
}

void
MarkerDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {

		/* just a click, do nothing but finish
		   off the selection process
		*/

		Selection::Operation op = Keyboard::selection_type (event->button.state);

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
	XMLNode &before = _editor->session->locations()->get_state();

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

	XMLNode &after = _editor->session->locations()->get_state();
	_editor->session->add_command(new MementoCommand<Locations>(*(_editor->session->locations()), &before, &after));
	_editor->commit_reversible_command ();
	
	_line->hide();
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
	_point = reinterpret_cast<ControlPoint*> (_item->get_data ("control_point"));
	assert (_point);
}


void
ControlPointDrag::start_grab (GdkEvent* event, Gdk::Cursor* /*cursor*/)
{
	Drag::start_grab (event, _editor->fader_cursor);

	// start the grab at the center of the control point so
	// the point doesn't 'jump' to the mouse after the first drag
	_grab_x = _point->get_x();
	_grab_y = _point->get_y();

	_point->line().parent_group().i2w (_grab_x, _grab_y);
	_editor->track_canvas->w2c (_grab_x, _grab_y, _grab_x, _grab_y);

	_grab_frame = _editor->pixel_to_frame (_grab_x);

	_point->line().start_drag (_point, _grab_frame, 0);

	float fraction = 1.0 - (_point->get_y() / _point->line().height());
	_editor->set_verbose_canvas_cursor (_point->line().get_verbose_cursor_string (fraction), 
					    _current_pointer_x + 10, _current_pointer_y + 10);

	_editor->show_verbose_canvas_cursor ();
}

void
ControlPointDrag::motion (GdkEvent* event, bool)
{
	double dx = _current_pointer_x - _last_pointer_x;
	double dy = _current_pointer_y - _last_pointer_y;

	if (event->button.state & Keyboard::SecondaryModifier) {
		dx *= 0.1;
		dy *= 0.1;
	}

	double cx = _grab_x + _cumulative_x_drag + dx;
	double cy = _grab_y + _cumulative_y_drag + dy;

	// calculate zero crossing point. back off by .01 to stay on the
	// positive side of zero
	double _unused = 0;
	double zero_gain_y = (1.0 - _zero_gain_fraction) * _point->line().height() - .01;
	_point->line().parent_group().i2w(_unused, zero_gain_y);

	// make sure we hit zero when passing through
	if ((cy < zero_gain_y and (cy - dy) > zero_gain_y)
			or (cy > zero_gain_y and (cy - dy) < zero_gain_y)) {
		cy = zero_gain_y;
	}

	if (_x_constrained) {
		cx = _grab_x;
	}
	if (_y_constrained) {
		cy = _grab_y;
	}

	_cumulative_x_drag = cx - _grab_x;
	_cumulative_y_drag = cy - _grab_y;

	_point->line().parent_group().w2i (cx, cy);

	cx = max (0.0, cx);
	cy = max (0.0, cy);
	cy = min ((double) _point->line().height(), cy);

	//translate cx to frames
	nframes64_t cx_frames = _editor->unit_to_frame (cx);

	if (!_x_constrained) {
		_editor->snap_to_with_modifier (cx_frames, event);
	}

	float const fraction = 1.0 - (cy / _point->line().height());

	bool const push = Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier);

	_point->line().point_drag (*_point, cx_frames, fraction, push);
	
	_editor->set_verbose_canvas_cursor_text (_point->line().get_verbose_cursor_string (fraction));
}

void
ControlPointDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (!movement_occurred) {

		/* just a click */
		
		if ((event->type == GDK_BUTTON_RELEASE) && (event->button.button == 1) && Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
			_editor->reset_point_selection ();
		}

	} else {
		motion (event, false);
	}
	_point->line().end_drag (_point);
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

	nframes64_t const frame_within_region = (nframes64_t) floor (cx * _editor->frames_per_unit);

	if (!_line->control_points_adjacent (frame_within_region, _before, _after)) {
		/* no adjacent points */
		return;
	}

	Drag::start_grab (event, _editor->fader_cursor);

	/* store grab start in parent frame */

	_grab_x = cx;
	_grab_y = cy;

	double fraction = 1.0 - (cy / _line->height());

	_line->start_drag (0, _grab_frame, fraction);
	
	_editor->set_verbose_canvas_cursor (_line->get_verbose_cursor_string (fraction),
					    _current_pointer_x + 10, _current_pointer_y + 10);
	
	_editor->show_verbose_canvas_cursor ();
}

void
LineDrag::motion (GdkEvent* event, bool)
{
	double dy = _current_pointer_y - _last_pointer_y;
	
	if (event->button.state & Keyboard::SecondaryModifier) {
		dy *= 0.1;
	}

	double cy = _grab_y + _cumulative_y_drag + dy;

	_cumulative_y_drag = cy - _grab_y;

	cy = max (0.0, cy);
	cy = min ((double) _line->height(), cy);

	double const fraction = 1.0 - (cy / _line->height());

	bool push;

	if (Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier)) {
		push = false;
	} else {
		push = true;
	}

	_line->line_drag (_before, _after, fraction, push);
	
	_editor->set_verbose_canvas_cursor_text (_line->get_verbose_cursor_string (fraction));
}

void
LineDrag::finished (GdkEvent* event, bool)
{
	motion (event, false);
	_line->end_drag (0);
}

void
RubberbandSelectDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);
	_editor->show_verbose_time_cursor (_current_pointer_frame, 10);
}

void
RubberbandSelectDrag::motion (GdkEvent* event, bool first_move)
{
	nframes64_t start;
	nframes64_t end;
	double y1;
	double y2;

	/* use a bigger drag threshold than the default */

	if (abs ((int) (_current_pointer_frame - _grab_frame)) < 8) {
		return;
	}

 	if (Config->get_rubberbanding_snaps_to_grid()) {
 		if (first_move) {
 			_editor->snap_to_with_modifier (_grab_frame, event);
		} 
		_editor->snap_to_with_modifier (_current_pointer_frame, event);
 	}

	/* base start and end on initial click position */

	if (_current_pointer_frame < _grab_frame) {
		start = _current_pointer_frame;
		end = _grab_frame;
	} else {
		end = _current_pointer_frame;
		start = _grab_frame;
	}

	if (_current_pointer_y < _grab_y) {
		y1 = _current_pointer_y;
		y2 = _grab_y;
	} else {
		y2 = _current_pointer_y;
		y1 = _grab_y;
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
		
		_last_pointer_frame = _current_pointer_frame;

		_editor->show_verbose_time_cursor (_current_pointer_frame, 10);
	}
}

void
RubberbandSelectDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {

		motion (event, false);

		double y1,y2;
		if (_current_pointer_y < _grab_y) {
			y1 = _current_pointer_y;
			y2 = _grab_y;
		} else {
			y2 = _current_pointer_y;
			y1 = _grab_y;
		}


		Selection::Operation op = Keyboard::selection_type (event->button.state);
		bool committed;

		_editor->begin_reversible_command (_("rubberband selection"));

		if (_grab_frame < _last_pointer_frame) {
			committed = _editor->select_all_within (_grab_frame, _last_pointer_frame - 1, y1, y2, _editor->track_views, op);
		} else {
			committed = _editor->select_all_within (_last_pointer_frame, _grab_frame - 1, y1, y2, _editor->track_views, op);
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
TimeFXDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);
	
	_editor->show_verbose_time_cursor (_current_pointer_frame, 10);
}

void
TimeFXDrag::motion (GdkEvent* event, bool)
{
	RegionView* rv = _primary;

	_editor->snap_to_with_modifier (_current_pointer_frame, event);

	if (_current_pointer_frame == _last_pointer_frame) {
		return;
	}

	if (_current_pointer_frame > rv->region()->position()) {
		rv->get_time_axis_view().show_timestretch (rv->region()->position(), _current_pointer_frame);
	}

	_last_pointer_frame = _current_pointer_frame;

	_editor->show_verbose_time_cursor (_current_pointer_frame, 10);
}

void
TimeFXDrag::finished (GdkEvent* /*event*/, bool movement_occurred)
{
	_primary->get_time_axis_view().hide_timestretch ();

 	if (!movement_occurred) {
		return;
	}

	if (_last_pointer_frame < _primary->region()->position()) {
		/* backwards drag of the left edge - not usable */
		return;
	}
	
	nframes64_t newlen = _last_pointer_frame - _primary->region()->position();

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

	if (_editor->time_stretch (rs, percentage) == 0) {
		_editor->session->commit_reversible_command ();
	}
}

void
ScrubDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event);
}

void
ScrubDrag::motion (GdkEvent* /*event*/, bool)
{
	_editor->scrub ();
}

void
ScrubDrag::finished (GdkEvent* /*event*/, bool movement_occurred)
{
	if (movement_occurred && _editor->session) {
		/* make sure we stop */
		_editor->session->request_transport_speed (0.0);
	} 
}

SelectionDrag::SelectionDrag (Editor* e, ArdourCanvas::Item* i, Operation o)
	: Drag (e, i),
	  _operation (o),
	  _copy (false)
{

}

void
SelectionDrag::start_grab (GdkEvent* event, Gdk::Cursor*)
{
	nframes64_t start = 0;
	nframes64_t end = 0;

	if (_editor->session == 0) {
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
		Drag::start_grab (event, cursor);
		cursor = _editor->trimmer_cursor;
		start = _editor->selection->time[_editor->clicked_selection].start;
		_pointer_frame_offset = _grab_frame - start;	
		break;
		
	case SelectionEndTrim:
		if (_editor->clicked_axisview) {
			_editor->clicked_axisview->order_selection_trims (_item, false);
		}
		Drag::start_grab (event, cursor);
		cursor = _editor->trimmer_cursor;
		end = _editor->selection->time[_editor->clicked_selection].end;
		_pointer_frame_offset = _grab_frame - end;	
		break;

	case SelectionMove:
		start = _editor->selection->time[_editor->clicked_selection].start;
		Drag::start_grab (event, cursor);
		_pointer_frame_offset = _grab_frame - start;	
		break;
	}

	if (_operation == SelectionMove) {
		_editor->show_verbose_time_cursor (start, 10);	
	} else {
		_editor->show_verbose_time_cursor (_current_pointer_frame, 10);	
	}
}

void
SelectionDrag::motion (GdkEvent* event, bool first_move)
{
	nframes64_t start = 0;
	nframes64_t end = 0;
	nframes64_t length;

	nframes64_t const pending_position = adjusted_current_frame (event);
	
	/* only alter selection if the current frame is 
	   different from the last frame position (adjusted)
	 */
	
	if (pending_position == _last_pointer_frame) {
		return;
	}
	
	switch (_operation) {
	case CreateSelection:
		
		if (first_move) {
			_editor->snap_to (_grab_frame);
		}
		
		if (pending_position < _grab_frame) {
			start = pending_position;
			end = _grab_frame;
		} else {
			end = pending_position;
			start = _grab_frame;
		}
		
		/* first drag: Either add to the selection
		   or create a new selection->
		*/
		
		if (first_move) {
			
			_editor->begin_reversible_command (_("range selection"));
			
			if (_copy) {
				/* adding to the selection */
				_editor->clicked_selection = _editor->selection->add (start, end);
				_copy = false;
			} else {
				/* new selection-> */
				_editor->clicked_selection = _editor->selection->set (_editor->clicked_axisview, start, end);
			}
		} 
		break;
		
	case SelectionStartTrim:
		
		if (first_move) {
			_editor->begin_reversible_command (_("trim selection start"));
		}
		
		start = _editor->selection->time[_editor->clicked_selection].start;
		end = _editor->selection->time[_editor->clicked_selection].end;

		if (pending_position > end) {
			start = end;
		} else {
			start = pending_position;
		}
		break;
		
	case SelectionEndTrim:
		
		if (first_move) {
			_editor->begin_reversible_command (_("trim selection end"));
		}
		
		start = _editor->selection->time[_editor->clicked_selection].start;
		end = _editor->selection->time[_editor->clicked_selection].end;

		if (pending_position < start) {
			end = start;
		} else {
			end = pending_position;
		}
		
		break;
		
	case SelectionMove:
		
		if (first_move) {
			_editor->begin_reversible_command (_("move selection"));
		}
		
		start = _editor->selection->time[_editor->clicked_selection].start;
		end = _editor->selection->time[_editor->clicked_selection].end;
		
		length = end - start;
		
		start = pending_position;
		_editor->snap_to (start);
		
		end = start + length;
		
		break;
	}
	
	if (event->button.x >= _editor->horizontal_adjustment.get_value() + _editor->_canvas_width) {
		_editor->start_canvas_autoscroll (1, 0);
	}

	if (start != end) {
		_editor->selection->replace (_editor->clicked_selection, start, end);
	}

	_last_pointer_frame = pending_position;

	if (_operation == SelectionMove) {
		_editor->show_verbose_time_cursor(start, 10);	
	} else {
		_editor->show_verbose_time_cursor(pending_position, 10);	
	}
}

void
SelectionDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		motion (event, false);
		/* XXX this is not object-oriented programming at all. ick */
		if (_editor->selection->time.consolidate()) {
			_editor->selection->TimeChanged ();
		}
		_editor->commit_reversible_command ();
	} else {
		/* just a click, no pointer movement.*/

		if (Keyboard::no_modifier_keys_pressed (&event->button)) {

			_editor->selection->clear_time();

		} 
	}

	/* XXX what happens if its a music selection? */
	_editor->session->set_audio_range (_editor->selection->time);
	_editor->stop_canvas_autoscroll ();
}

RangeMarkerBarDrag::RangeMarkerBarDrag (Editor* e, ArdourCanvas::Item* i, Operation o)
	: Drag (e, i),
	  _operation (o),
	  _copy (false)
{
	_drag_rect = new ArdourCanvas::SimpleRect (*_editor->time_line_group, 0.0, 0.0, 0.0, _editor->physical_screen_height);
	_drag_rect->hide ();

	_drag_rect->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_RangeDragRect.get();
	_drag_rect->property_outline_color_rgba() = ARDOUR_UI::config()->canvasvar_RangeDragRect.get();
}

void
RangeMarkerBarDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	if (_editor->session == 0) {
		return;
	}

	Gdk::Cursor* cursor = 0;

	if (!_editor->temp_location) {
		_editor->temp_location = new Location;
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

	_editor->show_verbose_time_cursor (_current_pointer_frame, 10);	
}

void
RangeMarkerBarDrag::motion (GdkEvent* event, bool first_move)
{
	nframes64_t start = 0;
	nframes64_t end = 0;
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
	
	_editor->snap_to_with_modifier (_current_pointer_frame, event);

	/* only alter selection if the current frame is 
	   different from the last frame position.
	 */
	
	if (_current_pointer_frame == _last_pointer_frame) {
		return;
	}
	
	switch (_operation) {
	case CreateRangeMarker:
	case CreateTransportMarker:
	case CreateCDMarker:
		if (first_move) {
			_editor->snap_to (_grab_frame);
		}
		
		if (_current_pointer_frame < _grab_frame) {
			start = _current_pointer_frame;
			end = _grab_frame;
		} else {
			end = _current_pointer_frame;
			start = _grab_frame;
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
		break;		
	}
	
	if (event->button.x >= _editor->horizontal_adjustment.get_value() + _editor->_canvas_width) {
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

	_last_pointer_frame = _current_pointer_frame;

	_editor->show_verbose_time_cursor (_current_pointer_frame, 10);	
	
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
			XMLNode &before = _editor->session->locations()->get_state();
			_editor->session->locations()->next_available_name(rangename,"unnamed");
			if (_operation == CreateCDMarker) {
				flags = Location::IsRangeMarker | Location::IsCDMarker;
				_editor->cd_marker_bar_drag_rect->hide();
			}
			else {
				flags = Location::IsRangeMarker;
				_editor->range_bar_drag_rect->hide();
			}
			newloc = new Location(_editor->temp_location->start(), _editor->temp_location->end(), rangename, (Location::Flags) flags);
			_editor->session->locations()->add (newloc, true);
			XMLNode &after = _editor->session->locations()->get_state();
			_editor->session->add_command(new MementoCommand<Locations>(*(_editor->session->locations()), &before, &after));
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

			nframes64_t start;
			nframes64_t end;

			start = _editor->session->locations()->first_mark_before (_grab_frame);
			end = _editor->session->locations()->first_mark_after (_grab_frame);
			
			if (end == max_frames) {
				end = _editor->session->current_end_frame ();
			}

			if (start == 0) {
				start = _editor->session->current_start_frame ();
			}

			switch (_editor->mouse_mode) {
			case MouseObject:
				/* find the two markers on either side and then make the selection from it */
				_editor->select_all_within (start, end, 0.0f, FLT_MAX, _editor->track_views, Selection::Set);
				break;

			case MouseRange:
				/* find the two markers on either side of the click and make the range out of it */
				_editor->selection->set (0, start, end);
				break;

			default:
				break;
			}
		} 
	}

	_editor->stop_canvas_autoscroll ();
}



void
RangeMarkerBarDrag::update_item (Location* location)
{
	double const x1 = _editor->frame_to_pixel (location->start());
	double const x2 = _editor->frame_to_pixel (location->end());

	_drag_rect->property_x1() = x1;
	_drag_rect->property_x2() = x2;
}

void
MouseZoomDrag::start_grab (GdkEvent* event, Gdk::Cursor *)
{
	Drag::start_grab (event, _editor->zoom_cursor);
	_editor->show_verbose_time_cursor (_current_pointer_frame, 10);
}

void
MouseZoomDrag::motion (GdkEvent* event, bool first_move)
{
	nframes64_t start;
	nframes64_t end;

	_editor->snap_to_with_modifier (_current_pointer_frame, event);
	
	if (first_move) {
		_editor->snap_to_with_modifier (_grab_frame, event);
	}
		
	if (_current_pointer_frame == _last_pointer_frame) {
		return;
	}

	/* base start and end on initial click position */
	if (_current_pointer_frame < _grab_frame) {
		start = _current_pointer_frame;
		end = _grab_frame;
	} else {
		end = _current_pointer_frame;
		start = _grab_frame;
	}
	
	if (start != end) {

		if (first_move) {
			_editor->zoom_rect->show();
			_editor->zoom_rect->raise_to_top();
		}

		_editor->reposition_zoom_rect(start, end);

		_last_pointer_frame = _current_pointer_frame;

		_editor->show_verbose_time_cursor (_current_pointer_frame, 10);
	}
}

void
MouseZoomDrag::finished (GdkEvent* event, bool movement_occurred)
{
	if (movement_occurred) {
		motion (event, false);
		
		if (_grab_frame < _last_pointer_frame) {
			_editor->temporal_zoom_by_frame (_grab_frame, _last_pointer_frame, "mouse zoom");
		} else {
			_editor->temporal_zoom_by_frame (_last_pointer_frame, _grab_frame, "mouse zoom");
		}		
	} else {
		_editor->temporal_zoom_to_frame (false, _grab_frame);
		/*
		temporal_zoom_step (false);
		center_screen (_grab_frame);
		*/
	}

	_editor->zoom_rect->hide();
}
