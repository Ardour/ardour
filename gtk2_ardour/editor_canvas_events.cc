/*
    Copyright (C) 2000 Paul Davis 

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

#include <cstdlib>
#include <cmath>
#include <algorithm>
#include <typeinfo>

#include "pbd/stacktrace.h"

#include "ardour/audio_diskstream.h"
#include "ardour/audioplaylist.h"
#include "ardour/midi_region.h"
#include "ardour/audioregion.h"
#include "ardour/region_factory.h"

#include "editor.h"
#include "keyboard.h"
#include "public_editor.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "crossfade_view.h"
#include "audio_time_axis.h"
#include "region_gain_line.h"
#include "automation_line.h"
#include "automation_time_axis.h"
#include "automation_line.h"
#include "control_point.h"
#include "canvas_impl.h"
#include "simplerect.h"
#include "interactive-item.h"
#include "editor_drag.h"
#include "midi_time_axis.h"

#include "i18n.h"

using namespace sigc;
using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace ArdourCanvas;

bool
Editor::track_canvas_scroll (GdkEventScroll* ev)
{
	int x, y;
	double wx, wy;
	nframes64_t xdelta;
	int direction = ev->direction;
	
	Gnome::Canvas::Item* item = track_canvas->get_item_at(ev->x, ev->y);
	InteractiveItem* interactive_item = dynamic_cast<InteractiveItem*>(item);
	if (interactive_item) {
		return interactive_item->on_event(reinterpret_cast<GdkEvent*>(ev));
	}

  retry:
	switch (direction) {
	case GDK_SCROLL_UP:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			//if (ev->state == GDK_CONTROL_MASK) {
			/* XXX 
			   the ev->x will be out of step with the canvas
			   if we're in mid zoom, so we have to get the damn mouse 
			   pointer again
			*/
			track_canvas->get_pointer (x, y);
			track_canvas->window_to_world (x, y, wx, wy);

			GdkEvent event;
			event.type = GDK_BUTTON_RELEASE;
			event.button.x = wx;
			event.button.y = wy;
			
			nframes64_t where = event_frame (&event, 0, 0);
			temporal_zoom_to_frame (false, where);
			return true;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			direction = GDK_SCROLL_LEFT;
			goto retry;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
			if (!current_stepping_trackview) {
				step_timeout = Glib::signal_timeout().connect (mem_fun(*this, &Editor::track_height_step_timeout), 500);
				std::pair<TimeAxisView*, int> const p = trackview_by_y_position (ev->y + vertical_adjustment.get_value() - canvas_timebars_vsize);
				current_stepping_trackview = p.first;
				if (!current_stepping_trackview) {
					return false;
				}
			}
			last_track_height_step_timestamp = get_microseconds();
			current_stepping_trackview->step_height (true);
			return true;
		} else {
			scroll_tracks_up_line ();
			return true;
		}
		break;

	case GDK_SCROLL_DOWN:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			//if (ev->state == GDK_CONTROL_MASK) {
			track_canvas->get_pointer (x, y);
			track_canvas->window_to_world (x, y, wx, wy);

			GdkEvent event;
			event.type = GDK_BUTTON_RELEASE;
			event.button.x = wx;
			event.button.y = wy;
			
			nframes64_t where = event_frame (&event, 0, 0);
			temporal_zoom_to_frame (true, where);
			return true;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			direction = GDK_SCROLL_RIGHT;
			goto retry;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
			if (!current_stepping_trackview) {
				step_timeout = Glib::signal_timeout().connect (mem_fun(*this, &Editor::track_height_step_timeout), 500);
				std::pair<TimeAxisView*, int> const p = trackview_by_y_position (ev->y + vertical_adjustment.get_value() - canvas_timebars_vsize);
				current_stepping_trackview = p.first;
				if (!current_stepping_trackview) {
					return false;
				}
			}
			last_track_height_step_timestamp = get_microseconds();
			current_stepping_trackview->step_height (false);
			return true;
		} else {
			scroll_tracks_down_line ();
			return true;
		}
		break;	

	case GDK_SCROLL_LEFT:
		xdelta = (current_page_frames() / 8);
		if (leftmost_frame > xdelta) {
			reset_x_origin (leftmost_frame - xdelta);
		} else {
			reset_x_origin (0);
		}
		break;

	case GDK_SCROLL_RIGHT:
		xdelta = (current_page_frames() / 8);
		if (max_frames - xdelta > leftmost_frame) {
			reset_x_origin (leftmost_frame + xdelta);
		} else {
			reset_x_origin (max_frames - current_page_frames());
		}
		break;

	default:
		/* what? */
		break;
	}

	return false;
}

bool
Editor::track_canvas_scroll_event (GdkEventScroll *event)
{
	track_canvas->grab_focus();
	track_canvas_scroll (event);
	return false;
}

bool
Editor::track_canvas_button_press_event (GdkEventButton *event)
{
	selection->clear ();
	track_canvas->grab_focus();
	return false;
}

bool
Editor::track_canvas_button_release_event (GdkEventButton *event)
{
	if (_drag) {
		_drag->end_grab ((GdkEvent*) event);
		delete _drag;
		_drag = 0;
	}
	return false;
}

bool
Editor::track_canvas_motion_notify_event (GdkEventMotion *event)
{
	int x, y;
	/* keep those motion events coming */
	track_canvas->get_pointer (x, y);
	return false;
}

bool
Editor::track_canvas_motion (GdkEvent *ev)
{
	if (verbose_cursor_visible) {
		verbose_canvas_cursor->property_x() = clamp_verbose_cursor_x (ev->motion.x + 10);
		verbose_canvas_cursor->property_y() = clamp_verbose_cursor_y (ev->motion.y + 10);
	}

	return false;
}

bool
Editor::typed_event (ArdourCanvas::Item* item, GdkEvent *event, ItemType type)
{
	gint ret = FALSE;
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		ret = button_press_handler (item, event, type);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, type);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, type);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, type);
		break;

	default:
		break;
	}
	return ret;
}

bool
Editor::canvas_region_view_event (GdkEvent *event, ArdourCanvas::Item* item, RegionView *rv)
{
	bool ret = false;

	if (!rv->sensitive ()) {
		return false;
	}


	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_axisview = &rv->get_time_axis_view();
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		ret = button_press_handler (item, event, RegionItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, RegionItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;

	case GDK_ENTER_NOTIFY:
		set_entered_track (&rv->get_time_axis_view ());
		set_entered_regionview (rv);
		break;

	case GDK_LEAVE_NOTIFY:
		set_entered_track (0);
		set_entered_regionview (0);
		break;

	default:
		break;
	}

	return ret;
}

bool
Editor::canvas_stream_view_event (GdkEvent *event, ArdourCanvas::Item* item, RouteTimeAxisView *tv)
{
	bool ret = FALSE;
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = 0;
		clicked_control_point = 0;
		clicked_axisview = tv;
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(tv);
		ret = button_press_handler (item, event, StreamItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, StreamItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, StreamItem);
		break;

	case GDK_ENTER_NOTIFY:
		set_entered_track (tv);
		break;

	case GDK_LEAVE_NOTIFY:
		set_entered_track (0);
		break;

	default:
		break;
	}

	return ret;
}

bool
Editor::canvas_automation_track_event (GdkEvent *event, ArdourCanvas::Item* item, AutomationTimeAxisView *atv)
{
	bool ret = false;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = 0;
		clicked_control_point = 0;
		clicked_axisview = atv;
		clicked_routeview = 0;
		ret = button_press_handler (item, event, AutomationTrackItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, AutomationTrackItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, AutomationTrackItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, AutomationTrackItem);
		break;

	default:
		break;
	}

	return ret;
}

bool
Editor::canvas_fade_in_event (GdkEvent *event, ArdourCanvas::Item* item, AudioRegionView *rv)
{
	/* we handle only button 3 press/release events */

	if (!rv->sensitive()) {
		return false;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_axisview = &rv->get_time_axis_view();
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		if (event->button.button == 3) {
			return button_press_handler (item, event, FadeInItem);
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 3) {
			return button_release_handler (item, event, FadeInItem);
		}
		break;

	default:
		break;
		
	}

	/* proxy for the regionview */
	
	return canvas_region_view_event (event, rv->get_canvas_group(), rv);
}

bool
Editor::canvas_fade_in_handle_event (GdkEvent *event, ArdourCanvas::Item* item, AudioRegionView *rv)
{
	bool ret = false;
	
	if (!rv->sensitive()) {
		return false;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_axisview = &rv->get_time_axis_view();
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		ret = button_press_handler (item, event, FadeInHandleItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, FadeInHandleItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, FadeInHandleItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, FadeInHandleItem);
		break;

	default:
		break;
	}

	return ret;
}

bool
Editor::canvas_fade_out_event (GdkEvent *event, ArdourCanvas::Item* item, AudioRegionView *rv)
{
	/* we handle only button 3 press/release events */

	if (!rv->sensitive()) {
		return false;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_axisview = &rv->get_time_axis_view();
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		if (event->button.button == 3) {
			return button_press_handler (item, event, FadeOutItem);
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 3) {
			return button_release_handler (item, event, FadeOutItem);
		}
		break;

	default:
		break;
		
	}

	/* proxy for the regionview */
	
	return canvas_region_view_event (event, rv->get_canvas_group(), rv);
}

bool
Editor::canvas_fade_out_handle_event (GdkEvent *event, ArdourCanvas::Item* item, AudioRegionView *rv)
{
	bool ret = false;
	
	if (!rv->sensitive()) {
		return false;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_axisview = &rv->get_time_axis_view();
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		ret = button_press_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, FadeOutHandleItem);
		break;

	default:
		break;
	}

	return ret;
}

struct DescendingRegionLayerSorter {
    bool operator()(boost::shared_ptr<Region> a, boost::shared_ptr<Region> b) {
	    return a->layer() > b->layer();
    }
};

bool
Editor::canvas_crossfade_view_event (GdkEvent* event, ArdourCanvas::Item* item, CrossfadeView* xfv)
{
	/* we handle only button 3 press/release events */

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		clicked_crossfadeview = xfv;
		clicked_axisview = &clicked_crossfadeview->get_time_axis_view();
		if (event->button.button == 3) {
			return button_press_handler (item, event, CrossfadeViewItem);
		} 
		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 3) {
			bool ret = button_release_handler (item, event, CrossfadeViewItem);
			return ret;
		}
		break;

	default:
		break;
		
	}

	/* XXX do not forward double clicks */

	if (event->type == GDK_2BUTTON_PRESS) {
		return false;
	}
	
	/* proxy for the upper most regionview */

	/* XXX really need to check if we are in the name highlight,
	   and proxy to that when required.
	*/
	
	TimeAxisView& tv (xfv->get_time_axis_view());
	AudioTimeAxisView* atv;

	if ((atv = dynamic_cast<AudioTimeAxisView*>(&tv)) != 0) {

		if (atv->is_audio_track()) {

			boost::shared_ptr<AudioPlaylist> pl;
			if ((pl = boost::dynamic_pointer_cast<AudioPlaylist> (atv->get_diskstream()->playlist())) != 0) {

				Playlist::RegionList* rl = pl->regions_at (event_frame (event));

				if (!rl->empty()) {
					DescendingRegionLayerSorter cmp;
					rl->sort (cmp);

					RegionView* rv = atv->view()->find_view (rl->front());

					delete rl;

					/* proxy */
					
					return canvas_region_view_event (event, rv->get_canvas_group(), rv);
				} 

				delete rl;
			}
		}
	}

	return TRUE;
}

bool
Editor::canvas_control_point_event (GdkEvent *event, ArdourCanvas::Item* item, ControlPoint* cp)
{
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_control_point = cp;
		clicked_axisview = &cp->line().trackview;
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		clicked_regionview = 0;
		break;

	case GDK_SCROLL_UP:
		break;

	case GDK_SCROLL_DOWN:
		break;

	default:
		break;
	}

	return typed_event (item, event, ControlPointItem);
}

bool
Editor::canvas_line_event (GdkEvent *event, ArdourCanvas::Item* item, AutomationLine* al)
{
	ItemType type;

	if (dynamic_cast<AudioRegionGainLine*> (al) != 0) {
		type = GainLineItem;
	} else {
		type = AutomationLineItem;
	}

	return typed_event (item, event, type);
}

bool
Editor::canvas_selection_rect_event (GdkEvent *event, ArdourCanvas::Item* item, SelectionRect* rect)
{
	bool ret = false;
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_selection = rect->id;
		ret = button_press_handler (item, event, SelectionItem);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, SelectionItem);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;
		/* Don't need these at the moment. */
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, SelectionItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, SelectionItem);
		break;

	default:
		break;
	}
			
	return ret;
}

bool
Editor::canvas_selection_start_trim_event (GdkEvent *event, ArdourCanvas::Item* item, SelectionRect* rect)
{
	bool ret = false;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_selection = rect->id;
		ret = button_press_handler (item, event, StartSelectionTrimItem);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, StartSelectionTrimItem);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, StartSelectionTrimItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, StartSelectionTrimItem);
		break;

	default:
		break;
	}
			
	return ret;
}

bool
Editor::canvas_selection_end_trim_event (GdkEvent *event, ArdourCanvas::Item* item, SelectionRect* rect)
{
	bool ret = false;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_selection = rect->id;
		ret = button_press_handler (item, event, EndSelectionTrimItem);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, EndSelectionTrimItem);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, EndSelectionTrimItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, EndSelectionTrimItem);
		break;

	default:
		break;
	}
			
	return ret;
}


bool
Editor::canvas_region_view_name_highlight_event (GdkEvent* event, ArdourCanvas::Item* item, RegionView* rv)
{
	bool ret = false;
	
	if (!rv->sensitive()) {
		return false;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_axisview = &clicked_regionview->get_time_axis_view();
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		ret = button_press_handler (item, event, RegionViewNameHighlight);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, RegionViewNameHighlight);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, RegionViewNameHighlight);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, RegionViewNameHighlight);
		break;

	default:
		break;
	}

	return ret;
}

bool
Editor::canvas_region_view_name_event (GdkEvent *event, ArdourCanvas::Item* item, RegionView* rv)
{
	bool ret = false;

	if (!rv->sensitive()) {
		return false;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_axisview = &clicked_regionview->get_time_axis_view();
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		ret = button_press_handler (item, event, RegionViewName);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, RegionViewName);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;
	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, RegionViewName);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, RegionViewName);
		break;

	default:
		break;
	}

	return ret;
}

bool
Editor::canvas_marker_event (GdkEvent *event, ArdourCanvas::Item* item, Marker* marker)
{
	return typed_event (item, event, MarkerItem);
}

bool
Editor::canvas_marker_bar_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, MarkerBarItem);
}

bool
Editor::canvas_range_marker_bar_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, RangeMarkerBarItem);
}

bool
Editor::canvas_transport_marker_bar_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, TransportMarkerBarItem);
}

bool
Editor::canvas_cd_marker_bar_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, CdMarkerBarItem);
}

bool
Editor::canvas_tempo_marker_event (GdkEvent *event, ArdourCanvas::Item* item, TempoMarker* marker)
{
	return typed_event (item, event, TempoMarkerItem);
}

bool
Editor::canvas_meter_marker_event (GdkEvent *event, ArdourCanvas::Item* item, MeterMarker* marker)
{
	return typed_event (item, event, MeterMarkerItem);
}

bool
Editor::canvas_tempo_bar_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, TempoBarItem);
}

bool
Editor::canvas_meter_bar_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, MeterBarItem);
}

bool
Editor::canvas_playhead_cursor_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, PlayheadCursorItem);
}

bool
Editor::canvas_zoom_rect_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, NoItem);
}

bool
Editor::track_canvas_drag_motion (Glib::RefPtr<Gdk::DragContext> const & c, int x, int y, guint time)
{
	double wx;
	double wy;
	track_canvas->window_to_world (x, y, wx, wy);

	GdkEvent event;
	event.type = GDK_MOTION_NOTIFY;
	event.button.x = wx;
	event.button.y = wy;
	/* assume we're dragging with button 1 */
	event.motion.state = Gdk::BUTTON1_MASK;

	if (_drag == 0) {

		double px;
		double py;
		nframes64_t const pos = event_frame (&event, &px, &py);
	
		std::pair<TimeAxisView*, int> const tv = trackview_by_y_position (py);
		if (tv.first == 0) {
			return true;
		}

		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (tv.first);
		if (rtav == 0 || !rtav->is_track ()) {
			return true;
		}

		list<boost::shared_ptr<Region> > regions;
		TreeView* source;
		region_list_display.get_object_drag_data (regions, &source);
		assert (regions.size() == 1);
		boost::shared_ptr<Region> region = regions.front ();

		boost::shared_ptr<Region> region_copy = RegionFactory::create (region);

		if (boost::dynamic_pointer_cast<AudioRegion> (region_copy) != 0 && 
		    dynamic_cast<AudioTimeAxisView*> (tv.first) == 0) {

			/* audio -> non-audio */
			return true;
		}

		if (boost::dynamic_pointer_cast<MidiRegion> (region_copy) == 0 && 
		    dynamic_cast<MidiTimeAxisView*> (tv.first) != 0) {

			/* MIDI -> non-MIDI */
			return true;
		}
		
		_drag = new RegionInsertDrag (this, region_copy, rtav, pos);
		_drag->start_grab (&event);
	}

	_drag->motion_handler (&event, false);

	return true;
}
