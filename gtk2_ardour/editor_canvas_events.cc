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

#include "ardour/audio_track.h"
#include "ardour/midi_track.h"
#include "ardour/midi_region.h"
#include "ardour/region_factory.h"
#include "ardour/profile.h"

#include "canvas/canvas.h"
#include "canvas/text.h"
#include "canvas/scroll_group.h"

#include "editor.h"
#include "keyboard.h"
#include "public_editor.h"
#include "ardour_ui.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "audio_time_axis.h"
#include "region_gain_line.h"
#include "automation_line.h"
#include "automation_time_axis.h"
#include "automation_line.h"
#include "control_point.h"
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
	if (Keyboard::some_magic_widget_has_focus()) {
		return false;
	}
	
	framepos_t xdelta;
	int direction = ev->direction;

	/* this event arrives without transformation by the canvas, so we have
	 * to transform the coordinates to be able to look things up.
	 */

	Duple event_coords = _track_canvas->window_to_canvas (Duple (ev->x, ev->y));

  retry:
	switch (direction) {
	case GDK_SCROLL_UP:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollZoomHorizontalModifier)) {
			//for mouse-wheel zoom, force zoom-focus to mouse
			Editing::ZoomFocus temp_focus = zoom_focus;
			zoom_focus = Editing::ZoomFocusMouse;
			temporal_zoom_step (false);
			zoom_focus = temp_focus;
			return true;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollHorizontalModifier)) {
			direction = GDK_SCROLL_LEFT;
			goto retry;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollZoomVerticalModifier)) {
			if (!current_stepping_trackview) {
				step_timeout = Glib::signal_timeout().connect (sigc::mem_fun(*this, &Editor::track_height_step_timeout), 500);
				std::pair<TimeAxisView*, int> const p = trackview_by_y_position (event_coords.y, false);
				current_stepping_trackview = p.first;
				if (!current_stepping_trackview) {
					return false;
				}
			}
			last_track_height_step_timestamp = get_microseconds();
			current_stepping_trackview->step_height (false);
			return true;
		} else {
			scroll_up_one_track ();
			return true;
		}
		break;

	case GDK_SCROLL_DOWN:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollZoomHorizontalModifier)) {
			//for mouse-wheel zoom, force zoom-focus to mouse
			Editing::ZoomFocus temp_focus = zoom_focus;
			zoom_focus = Editing::ZoomFocusMouse;
			temporal_zoom_step (true);
			zoom_focus = temp_focus;
			return true;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollHorizontalModifier)) {
			direction = GDK_SCROLL_RIGHT;
			goto retry;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::ScrollZoomVerticalModifier)) {
			if (!current_stepping_trackview) {
				step_timeout = Glib::signal_timeout().connect (sigc::mem_fun(*this, &Editor::track_height_step_timeout), 500);
				std::pair<TimeAxisView*, int> const p = trackview_by_y_position (event_coords.y, false);
				current_stepping_trackview = p.first;
				if (!current_stepping_trackview) {
					return false;
				}
			}
			last_track_height_step_timestamp = get_microseconds();
			current_stepping_trackview->step_height (true);
			return true;
		} else {
			scroll_down_one_track ();
			return true;
		}
		break;

	case GDK_SCROLL_LEFT:
		xdelta = (current_page_samples() / 8);
		if (leftmost_frame > xdelta) {
			reset_x_origin (leftmost_frame - xdelta);
		} else {
			reset_x_origin (0);
		}
		break;

	case GDK_SCROLL_RIGHT:
		xdelta = (current_page_samples() / 8);
		if (max_framepos - xdelta > leftmost_frame) {
			reset_x_origin (leftmost_frame + xdelta);
		} else {
			reset_x_origin (max_framepos - current_page_samples());
		}
		break;

	default:
		/* what? */
		break;
	}

	return false;
}

bool
Editor::canvas_scroll_event (GdkEventScroll *event, bool from_canvas)
{
	if (from_canvas) {
		boost::optional<ArdourCanvas::Rect> rulers = _time_markers_group->bounding_box();
		if (rulers && rulers->contains (Duple (event->x, event->y))) {
			return canvas_ruler_event ((GdkEvent*) event, timecode_ruler, TimecodeRulerItem);
		}
	}

	_track_canvas->grab_focus();
	return track_canvas_scroll (event);
}

bool
Editor::track_canvas_button_press_event (GdkEventButton *event)
{
	_track_canvas->grab_focus();
	if (!Keyboard::is_context_menu_event (event)) {
		begin_reversible_selection_op (X_("Clear Selection Click (track canvas)"));
		selection->clear ();
		commit_reversible_selection_op();
        }
	return false;
}

bool
Editor::track_canvas_button_release_event (GdkEventButton *event)
{
	if (!Keyboard::is_context_menu_event (event)) {
                if (_drags->active ()) {
                        _drags->end_grab ((GdkEvent*) event);
                }
        }
	return false;
}

bool
Editor::track_canvas_motion_notify_event (GdkEventMotion */*event*/)
{
	int x, y;
	/* keep those motion events coming */
	_track_canvas->get_pointer (x, y);
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
		set_entered_regionview (rv);
		ret = enter_handler (item, event, RegionItem);
		break;

	case GDK_LEAVE_NOTIFY:
		if (event->crossing.detail != GDK_NOTIFY_INFERIOR) {
			set_entered_regionview (0);
			ret = leave_handler (item, event, RegionItem);
		}
		break;

	default:
		break;
	}

	return ret;
}

bool
Editor::canvas_wave_view_event (GdkEvent *event, ArdourCanvas::Item* item, RegionView* rv)
{
	/* we only care about enter events here, required for mouse/cursor
	 * tracking. there is a non-linear (non-child/non-parent) relationship
	 * between various components of a regionview and so when we leave one
	 * of them (e.g. a trim handle) and enter another (e.g. the waveview)
	 * no other items get notified. enter/leave handling does not propagate
	 * in the same way as other events, so we need to catch this because
	 * entering (and leaving) the waveview is equivalent to
	 * entering/leaving the regionview (which is why it is passed in as a
	 * third argument).
	 *
	 * And in fact, we really only care about enter events.
	 */

	bool ret = false;

	if (!rv->sensitive ()) {
		return false;
	}

	switch (event->type) {
	case GDK_ENTER_NOTIFY:
		set_entered_regionview (rv);
		ret = enter_handler (item, event, WaveItem);
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
		ret = enter_handler (item, event, StreamItem);
		break;

	case GDK_LEAVE_NOTIFY:
		if (event->crossing.detail != GDK_NOTIFY_INFERIOR) {
			set_entered_track (0);
		}
		ret = leave_handler (item, event, StreamItem);
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
Editor::canvas_start_xfade_event (GdkEvent *event, ArdourCanvas::Item* item, AudioRegionView *rv)
{
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
			return button_press_handler (item, event, StartCrossFadeItem);
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 3) {
			return button_release_handler (item, event, StartCrossFadeItem);
		}
		break;

	default:
		break;

	}

	/* In Mixbus, the crossfade area is used to trim the region while leaving the fade anchor intact (see preserve_fade_anchor)*/
	/* however in A3 this feature is unfinished, and it might be better to do it with a modifier-trim instead, anyway */
	/* if we return RegionItem here then we avoid the issue until it is resolved later */
	return typed_event (item, event, RegionItem); // StartCrossFadeItem);
}

bool
Editor::canvas_end_xfade_event (GdkEvent *event, ArdourCanvas::Item* item, AudioRegionView *rv)
{
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
			return button_press_handler (item, event, EndCrossFadeItem);
		}
		break;

	case GDK_BUTTON_RELEASE:
		if (event->button.button == 3) {
			return button_release_handler (item, event, EndCrossFadeItem);
		}
		break;

	default:
		break;

	}

	/* In Mixbus, the crossfade area is used to trim the region while leaving the fade anchor intact (see preserve_fade_anchor)*/
	/* however in A3 this feature is unfinished, and it might be better to do it with a modifier-trim instead, anyway */
	/* if we return RegionItem here then we avoid the issue until it is resolved later */
	return typed_event (item, event, RegionItem); // EndCrossFadeItem);
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

	/* proxy for the regionview, except enter/leave events */

	if (event->type == GDK_ENTER_NOTIFY || event->type == GDK_LEAVE_NOTIFY) {
		return true;
	} else {
		return canvas_region_view_event (event, rv->get_canvas_group(), rv);
	}
}

bool
Editor::canvas_fade_in_handle_event (GdkEvent *event, ArdourCanvas::Item* item, AudioRegionView *rv, bool trim)
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
		ret = button_press_handler (item, event, trim ? FadeInTrimHandleItem : FadeInHandleItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, trim ? FadeInTrimHandleItem : FadeInHandleItem);
		maybe_locate_with_edit_preroll ( rv->region()->position() );
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, trim ? FadeInTrimHandleItem : FadeInHandleItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, trim ? FadeInTrimHandleItem : FadeInHandleItem);
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

	/* proxy for the regionview, except enter/leave events */

	if (event->type == GDK_ENTER_NOTIFY || event->type == GDK_LEAVE_NOTIFY) {
		return true;
	} else {
		return canvas_region_view_event (event, rv->get_canvas_group(), rv);
	}
}

bool
Editor::canvas_fade_out_handle_event (GdkEvent *event, ArdourCanvas::Item* item, AudioRegionView *rv, bool trim)
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
		ret = button_press_handler (item, event, trim ? FadeOutTrimHandleItem : FadeOutHandleItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, trim ? FadeOutTrimHandleItem : FadeOutHandleItem);
		maybe_locate_with_edit_preroll ( rv->region()->last_frame() - rv->get_fade_out_shape_width() );
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event);
		break;

	case GDK_ENTER_NOTIFY:
		ret = enter_handler (item, event, trim ? FadeOutTrimHandleItem : FadeOutHandleItem);
		break;

	case GDK_LEAVE_NOTIFY:
		ret = leave_handler (item, event, trim ? FadeOutTrimHandleItem : FadeOutHandleItem);
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
Editor::canvas_videotl_bar_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, VideoBarItem);
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
Editor::canvas_ruler_event (GdkEvent *event, ArdourCanvas::Item* item, ItemType type)
{
	framepos_t xdelta;
	bool handled = false;

	if (event->type == GDK_SCROLL) {
		
		/* scroll events in the rulers are handled a little differently from
		   scrolling elsewhere in the canvas.
		*/

		switch (event->scroll.direction) {
		case GDK_SCROLL_UP:

			if (Profile->get_mixbus()) {
				//for mouse-wheel zoom, force zoom-focus to mouse
				Editing::ZoomFocus temp_focus = zoom_focus;
				zoom_focus = Editing::ZoomFocusMouse;
				temporal_zoom_step (false);
				zoom_focus = temp_focus;
			} else {
				temporal_zoom_step (false);
			}
			handled = true;
			break;
			
		case GDK_SCROLL_DOWN:
			if (Profile->get_mixbus()) {
				//for mouse-wheel zoom, force zoom-focus to mouse
				Editing::ZoomFocus temp_focus = zoom_focus;
				zoom_focus = Editing::ZoomFocusMouse;
				temporal_zoom_step (true);
				zoom_focus = temp_focus;
			} else {
				temporal_zoom_step (true);
			}
			handled = true;
			break;
			
		case GDK_SCROLL_LEFT:
			xdelta = (current_page_samples() / 2);
			if (leftmost_frame > xdelta) {
				reset_x_origin (leftmost_frame - xdelta);
			} else {
				reset_x_origin (0);
			}
			handled = true;
			break;
			
		case GDK_SCROLL_RIGHT:
			xdelta = (current_page_samples() / 2);
			if (max_framepos - xdelta > leftmost_frame) {
				reset_x_origin (leftmost_frame + xdelta);
			} else {
				reset_x_origin (max_framepos - current_page_samples());
			}
			handled = true;
			break;
			
		default:
			/* what? */
			break;
		}
		return handled;
	}

	return typed_event (item, event, type);
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
Editor::canvas_note_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	if (!internal_editing()) {
		return false;
	}

	return typed_event (item, event, NoteItem);
}

bool
Editor::canvas_drop_zone_event (GdkEvent* event)
{
	GdkEventScroll scroll;	
	ArdourCanvas::Duple winpos;
	
	switch (event->type) {
	case GDK_BUTTON_RELEASE:
		if (event->button.button == 1) {
			begin_reversible_selection_op (X_("Nowhere Click"));
			selection->clear_objects ();
			selection->clear_tracks ();
			commit_reversible_selection_op ();
		}
		break;

	case GDK_SCROLL:
		/* convert coordinates back into window space so that
		   we can just call canvas_scroll_event().
		*/
		winpos = _track_canvas->canvas_to_window (Duple (event->scroll.x, event->scroll.y));
		scroll = event->scroll;
		scroll.x = winpos.x;
		scroll.y = winpos.y;
		return canvas_scroll_event (&scroll, true);
		break;

	case GDK_ENTER_NOTIFY:
		return typed_event (_canvas_drop_zone, event, DropZoneItem);

	case GDK_LEAVE_NOTIFY:
		return typed_event (_canvas_drop_zone, event, DropZoneItem);

	default:
		break;
	}

	return true;
}

bool
Editor::track_canvas_drag_motion (Glib::RefPtr<Gdk::DragContext> const& context, int x, int y, guint time)
{
	boost::shared_ptr<Region> region;
	boost::shared_ptr<Region> region_copy;
	RouteTimeAxisView* rtav;
	GdkEvent event;
	double px;
	double py;

	string target = _track_canvas->drag_dest_find_target (context, _track_canvas->drag_dest_get_target_list());

	if (target.empty()) {
		return false;
	}

	event.type = GDK_MOTION_NOTIFY;
	event.button.x = x;
	event.button.y = y;
	/* assume we're dragging with button 1 */
	event.motion.state = Gdk::BUTTON1_MASK;

	(void) window_event_sample (&event, &px, &py);

	std::pair<TimeAxisView*, int> const tv = trackview_by_y_position (py, false);
	bool can_drop = false;
	
	if (tv.first != 0) {

		/* over a time axis view of some kind */

		rtav = dynamic_cast<RouteTimeAxisView*> (tv.first);
		
		if (rtav != 0 && rtav->is_track ()) {
			/* over a track, not a bus */
			can_drop = true;
		}
			

	} else {
		/* not over a time axis view, so drop is possible */
		can_drop = true;
	}

	if (can_drop) {
		region = _regions->get_dragged_region ();
		
		if (region) {

			if (tv.first == 0
			    && (
			        boost::dynamic_pointer_cast<AudioRegion> (region) != 0 ||
			        boost::dynamic_pointer_cast<MidiRegion> (region) != 0
			       )
			   )
			{
				/* drop to drop-zone */
				context->drag_status (context->get_suggested_action(), time);
				return true;
			}
			
			if ((boost::dynamic_pointer_cast<AudioRegion> (region) != 0 &&
			     dynamic_cast<AudioTimeAxisView*> (tv.first) != 0) ||
			    (boost::dynamic_pointer_cast<MidiRegion> (region) != 0 &&
			     dynamic_cast<MidiTimeAxisView*> (tv.first) != 0)) {
				
				/* audio to audio 
				   OR 
				   midi to midi
				*/
				
				context->drag_status (context->get_suggested_action(), time);
				return true;
			}
		} else {
			/* DND originating from outside ardour
			 *
			 * TODO: check if file is audio/midi, allow drops on same track-type only,
			 * currently: if audio is dropped on a midi-track, it is only added to the region-list
			 */
			if (Profile->get_sae() || ARDOUR_UI::config()->get_only_copy_imported_files()) {
				context->drag_status(Gdk::ACTION_COPY, time);
			} else {
				if ((context->get_actions() & (Gdk::ACTION_COPY | Gdk::ACTION_LINK | Gdk::ACTION_MOVE)) == Gdk::ACTION_COPY) {
					context->drag_status(Gdk::ACTION_COPY, time);
				} else {
					context->drag_status(Gdk::ACTION_LINK, time);
				}
			}
			return true;
		}
	}

	/* no drop here */
	context->drag_status (Gdk::DragAction (0), time);
	return false;
}

void
Editor::drop_regions (const Glib::RefPtr<Gdk::DragContext>& /*context*/,
		      int x, int y,
		      const SelectionData& /*data*/,
		      guint /*info*/, guint /*time*/)
{
	GdkEvent event;
	double px;
	double py;

	event.type = GDK_MOTION_NOTIFY;
	event.button.x = x;
	event.button.y = y;
	/* assume we're dragging with button 1 */
	event.motion.state = Gdk::BUTTON1_MASK;
	framepos_t const pos = window_event_sample (&event, &px, &py);

	boost::shared_ptr<Region> region = _regions->get_dragged_region ();
	if (!region) { return; }

	RouteTimeAxisView* rtav = 0;
	std::pair<TimeAxisView*, int> const tv = trackview_by_y_position (py, false);

	if (tv.first != 0) {
		rtav = dynamic_cast<RouteTimeAxisView*> (tv.first);
	} else {
		try {
			if (boost::dynamic_pointer_cast<AudioRegion> (region)) {
				uint32_t output_chan = region->n_channels();
				if ((Config->get_output_auto_connect() & AutoConnectMaster) && session()->master_out()) {
					output_chan =  session()->master_out()->n_inputs().n_audio();
				}
				list<boost::shared_ptr<AudioTrack> > audio_tracks;
				audio_tracks = session()->new_audio_track (region->n_channels(), output_chan, ARDOUR::Normal, 0, 1, region->name());
				rtav = axis_view_from_route (audio_tracks.front());
			} else if (boost::dynamic_pointer_cast<MidiRegion> (region)) {
				ChanCount one_midi_port (DataType::MIDI, 1);
				list<boost::shared_ptr<MidiTrack> > midi_tracks;
				midi_tracks = session()->new_midi_track (one_midi_port, one_midi_port, boost::shared_ptr<ARDOUR::PluginInfo>(), ARDOUR::Normal, 0, 1, region->name());
				rtav = axis_view_from_route (midi_tracks.front());
			} else {
				return;
			}
		} catch (...) {
			error << _("Could not create new track after region placed in the drop zone") << endmsg;
			return;
		}
	}

	if (rtav != 0 && rtav->is_track ()) {
		boost::shared_ptr<Region> region_copy = RegionFactory::create (region, true);

		if ((boost::dynamic_pointer_cast<AudioRegion> (region_copy) != 0 && dynamic_cast<AudioTimeAxisView*> (rtav) != 0) ||
		    (boost::dynamic_pointer_cast<MidiRegion> (region_copy) != 0 && dynamic_cast<MidiTimeAxisView*> (rtav) != 0)) {
			_drags->set (new RegionInsertDrag (this, region_copy, rtav, pos), &event);
			_drags->end_grab (0);
		}
	}
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

