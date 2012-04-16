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

#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/region_factory.h"
#include "ardour/midi_region.h"

#include "editor.h"
#include "keyboard.h"
#include "public_editor.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "canvas-noevent-text.h"
#include "audio_time_axis.h"
#include "region_gain_line.h"
#include "automation_line.h"
#include "automation_time_axis.h"
#include "automation_line.h"
#include "control_point.h"
#include "canvas_impl.h"
#include "simplerect.h"
#include "editor_drag.h"
#include "midi_time_axis.h"
#include "editor_regions.h"
#include "verbose_cursor.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace ArdourCanvas;

using Gtkmm2ext::Keyboard;

bool
Editor::track_canvas_scroll (GdkEventScroll* ev)
{
	framepos_t xdelta;
	int direction = ev->direction;

  retry:
	switch (direction) {
	case GDK_SCROLL_UP:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			temporal_zoom_step (false);
			return true;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			direction = GDK_SCROLL_LEFT;
			goto retry;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
			if (!current_stepping_trackview) {
				step_timeout = Glib::signal_timeout().connect (sigc::mem_fun(*this, &Editor::track_height_step_timeout), 500);
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
			scroll_tracks_up_line ();
			return true;
		}
		break;

	case GDK_SCROLL_DOWN:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
			temporal_zoom_step (true);
			return true;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::SecondaryModifier)) {
			direction = GDK_SCROLL_RIGHT;
			goto retry;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
			if (!current_stepping_trackview) {
				step_timeout = Glib::signal_timeout().connect (sigc::mem_fun(*this, &Editor::track_height_step_timeout), 500);
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
		if (max_framepos - xdelta > leftmost_frame) {
			reset_x_origin (leftmost_frame + xdelta);
		} else {
			reset_x_origin (max_framepos - current_page_frames());
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
	return track_canvas_scroll (event);
}

bool
Editor::track_canvas_button_press_event (GdkEventButton */*event*/)
{
	selection->clear ();
	track_canvas->grab_focus();
	return false;
}

bool
Editor::track_canvas_button_release_event (GdkEventButton *event)
{
	if (_drags->active ()) {
		_drags->end_grab ((GdkEvent*) event);
	}
	return false;
}

bool
Editor::track_canvas_motion_notify_event (GdkEventMotion */*event*/)
{
	int x, y;
	/* keep those motion events coming */
	track_canvas->get_pointer (x, y);
	return false;
}

bool
Editor::track_canvas_motion (GdkEvent *ev)
{
	if (_verbose_cursor->visible ()) {
		_verbose_cursor->set_position (ev->motion.x + 10, ev->motion.y + 10);
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

	case GDK_KEY_PRESS:
		ret = key_press_handler (item, event, type);
		break;

	case GDK_KEY_RELEASE:
		ret = key_release_handler (item, event, type);
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
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		ret = button_press_handler (item, event, StreamItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, StreamItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
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
		set_entered_regionview (rv);
		ret = enter_handler (item, event, FadeInHandleItem);
		break;

	case GDK_LEAVE_NOTIFY:
		set_entered_regionview (0);
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
		set_entered_regionview (rv);
		ret = enter_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_LEAVE_NOTIFY:
		set_entered_regionview (0);
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
Editor::canvas_frame_handle_event (GdkEvent* event, ArdourCanvas::Item* item, RegionView* rv)
{
	bool ret = false;

	/* frame handles are not active when in internal edit mode, because actual notes
	   might be in the area occupied by the handle - we want them to be editable as normal.
	*/

	if (internal_editing() || !rv->sensitive()) {
		return false;
	}

	/* NOTE: frame handles pretend to be the colored trim bar from an event handling
	   perspective. XXX change this ??
	*/

	ItemType type;

	if (item->get_data ("isleft")) {
		type = LeftFrameHandle;
	} else {
		type = RightFrameHandle;
	}

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_axisview = &clicked_regionview->get_time_axis_view();
		clicked_routeview = dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		ret = button_press_handler (item, event, type);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, type);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;
	case GDK_ENTER_NOTIFY:
		set_entered_regionview (rv);
		ret = enter_handler (item, event, type);
		break;

	case GDK_LEAVE_NOTIFY:
		set_entered_regionview (0);
		ret = leave_handler (item, event, type);
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
		motion_handler (item, event);
		ret = true; // force this to avoid progagating the event into the regionview
		break;
	case GDK_ENTER_NOTIFY:
		set_entered_regionview (rv);
		ret = enter_handler (item, event, RegionViewNameHighlight);
		break;

	case GDK_LEAVE_NOTIFY:
		set_entered_regionview (0);
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
		set_entered_regionview (rv);
		ret = enter_handler (item, event, RegionViewName);
		break;

	case GDK_LEAVE_NOTIFY:
		set_entered_regionview (0);
		ret = leave_handler (item, event, RegionViewName);
		break;

	default:
		break;
	}

	return ret;
}

bool
Editor::canvas_feature_line_event (GdkEvent *event, ArdourCanvas::Item* item, RegionView*)
{
	bool ret = false;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = 0;
		clicked_control_point = 0;
		clicked_axisview = 0;
		clicked_routeview = 0; //dynamic_cast<RouteTimeAxisView*>(clicked_axisview);
		ret = button_press_handler (item, event, FeatureLineItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, FeatureLineItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, FeatureLineItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, FeatureLineItem);
		break;

	default:
		break;
	}

	return ret;
}

bool
Editor::canvas_marker_event (GdkEvent *event, ArdourCanvas::Item* item, Marker* /*marker*/)
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
Editor::canvas_tempo_marker_event (GdkEvent *event, ArdourCanvas::Item* item, TempoMarker* /*marker*/)
{
	return typed_event (item, event, TempoMarkerItem);
}

bool
Editor::canvas_meter_marker_event (GdkEvent *event, ArdourCanvas::Item* item, MeterMarker* /*marker*/)
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
Editor::canvas_note_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	if (!internal_editing()) {
		return false;
	}

	return typed_event (item, event, NoteItem);
}

bool
Editor::track_canvas_drag_motion (Glib::RefPtr<Gdk::DragContext> const & /*c*/, int x, int y, guint /*time*/)
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

	if (!_drags->active ()) {

		double px;
		double py;
		framepos_t const pos = event_frame (&event, &px, &py);

		std::pair<TimeAxisView*, int> const tv = trackview_by_y_position (py);
		if (tv.first == 0) {
			return true;
		}

		RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (tv.first);
		if (rtav == 0 || !rtav->is_track ()) {
			return true;
		}

		boost::shared_ptr<Region> region = _regions->get_dragged_region ();

		if (!region) {
			return true;
		}

		boost::shared_ptr<Region> region_copy = RegionFactory::create (region);

		if (boost::dynamic_pointer_cast<AudioRegion> (region_copy) != 0 &&
		    dynamic_cast<AudioTimeAxisView*> (tv.first) == 0) {

			/* audio -> non-audio */
			return true;
		}

		if (boost::dynamic_pointer_cast<MidiRegion> (region_copy) != 0 &&
		    dynamic_cast<MidiTimeAxisView*> (tv.first) == 0) {

			/* MIDI -> non-MIDI */
			return true;
		}

		_drags->set (new RegionInsertDrag (this, region_copy, rtav, pos), &event);
	}

	_drags->motion_handler (&event, false);

	return true;
}

bool
Editor::key_press_handler (ArdourCanvas::Item*, GdkEvent*, ItemType)
{
	return false;
}

bool
Editor::key_release_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType type)
{

	bool handled = false;

	switch (type) {
	case TempoMarkerItem:
		switch (event->key.keyval) {
		case GDK_Delete:
			remove_tempo_marker (item);
			handled = true;
			break;
		default:
			break;
		}
		break;

	case MeterMarkerItem:
		switch (event->key.keyval) {
		case GDK_Delete:
			remove_meter_marker (item);
			handled = true;
			break;
		default:
			break;
		}
		break;

	default:
		break;
	}

	return handled;
}
