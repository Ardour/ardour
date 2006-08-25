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

    $Id$
*/

#include <cstdlib>
#include <cmath>

#include <ardour/audio_diskstream.h>
#include <ardour/audioplaylist.h>

#include "editor.h"
#include "public_editor.h"
#include "audio_region_view.h"
#include "audio_streamview.h"
#include "crossfade_view.h"
#include "audio_time_axis.h"
#include "region_gain_line.h"
#include "automation_gain_line.h"
#include "automation_pan_line.h"
#include "automation_time_axis.h"
#include "redirect_automation_line.h"
#include "canvas_impl.h"
#include "simplerect.h"

#include "i18n.h"

using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;

bool
Editor::track_canvas_scroll (GdkEventScroll* ev)
{
	int x, y;
	double wx, wy;

	switch (ev->direction) {
	case GDK_SCROLL_UP:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
			//if (ev->state == GDK_CONTROL_MASK) {
			/* XXX 
			   the ev->x will be out of step with the canvas
			   if we're in mid zoom, so we have to get the damn mouse 
			   pointer again
			*/
			track_canvas.get_pointer (x, y);
			track_canvas.window_to_world (x, y, wx, wy);
			wx += horizontal_adjustment.get_value();
			wy += vertical_adjustment.get_value();
			
			GdkEvent event;
			event.type = GDK_BUTTON_RELEASE;
			event.button.x = wx;
			event.button.y = wy;
			
			jack_nframes_t where = event_frame (&event, 0, 0);
			temporal_zoom_to_frame (true, where);
			return true;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::Shift)) {
			if (!current_stepping_trackview) {
				step_timeout = Glib::signal_timeout().connect (mem_fun(*this, &Editor::track_height_step_timeout), 500);
				if (!(current_stepping_trackview = dynamic_cast<AudioTimeAxisView*> (trackview_by_y_position (ev->y)))) {
					return false;
				}
			}
			gettimeofday (&last_track_height_step_timestamp, 0);
			current_stepping_trackview->step_height (true);
			return true;
		} else {
			scroll_tracks_up_line ();
			return true;
		}
		break;
	case GDK_SCROLL_DOWN:
		if (Keyboard::modifier_state_equals (ev->state, Keyboard::Control)) {
			//if (ev->state == GDK_CONTROL_MASK) {
			track_canvas.get_pointer (x, y);
			track_canvas.window_to_world (x, y, wx, wy);
			wx += horizontal_adjustment.get_value();
			wy += vertical_adjustment.get_value();
			
			GdkEvent event;
			event.type = GDK_BUTTON_RELEASE;
			event.button.x = wx;
			event.button.y = wy;
			
			jack_nframes_t where = event_frame (&event, 0, 0);
			temporal_zoom_to_frame (false, where);
			return true;
		} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::Shift)) {
			if (!current_stepping_trackview) {
				step_timeout = Glib::signal_timeout().connect (mem_fun(*this, &Editor::track_height_step_timeout), 500);
				if (!(current_stepping_trackview = dynamic_cast<AudioTimeAxisView*> (trackview_by_y_position (ev->y)))) {
					return false;
				}
			}
			gettimeofday (&last_track_height_step_timestamp, 0);
			current_stepping_trackview->step_height (false);
			return true;
		} else {
			scroll_tracks_down_line ();
			return true;
		}
		break;	

	default:
		/* no left/right handling yet */
		break;
	}
	
	return false;
}

bool
Editor::track_canvas_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	gint x, y;

	/* this is the handler for events that are not handled by
	   items.
	*/

	switch (event->type) {
	case GDK_MOTION_NOTIFY:
		/* keep those motion events coming */
		track_canvas.get_pointer (x, y);
		return track_canvas_motion (event);

	case GDK_BUTTON_PRESS:
		track_canvas.grab_focus();
		break;

	case GDK_BUTTON_RELEASE:
		switch (event->button.button) {
		case 4:
		case 5:
			button_release_handler (item, event, NoItem);
			break;
		}
		break;

	case GDK_SCROLL:
		track_canvas.grab_focus();
		track_canvas_scroll (&event->scroll);
		break;

	default:
		break;
	}

	return FALSE;
}

bool
Editor::track_canvas_motion (GdkEvent *ev)
{
	if (verbose_cursor_visible) {
		verbose_canvas_cursor->property_x() = ev->motion.x + 20;
		verbose_canvas_cursor->property_y() = ev->motion.y + 20;
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
		ret = motion_handler (item, event, type);
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
	gint ret = FALSE;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, RegionItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, RegionItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, RegionItem);
		break;

	case GDK_ENTER_NOTIFY:
		set_entered_regionview (rv);
		break;

	case GDK_LEAVE_NOTIFY:
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
		clicked_trackview = tv;
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(tv);
		ret = button_press_handler (item, event, StreamItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, StreamItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, StreamItem);
		break;

	case GDK_ENTER_NOTIFY:
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
		clicked_trackview = atv;
		clicked_audio_trackview = 0;
		ret = button_press_handler (item, event, AutomationTrackItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, AutomationTrackItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, AutomationTrackItem);
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

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
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
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, FadeInHandleItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, FadeInHandleItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, FadeInHandleItem);
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

	switch (event->type) {
	case GDK_BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
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
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &rv->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, FadeOutHandleItem);
		break;

	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, FadeOutHandleItem);
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
		clicked_trackview = &clicked_crossfadeview->get_time_axis_view();
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

	
	/* proxy for the upper most regionview */

	/* XXX really need to check if we are in the name highlight,
	   and proxy to that when required.
	*/
	
	TimeAxisView& tv (xfv->get_time_axis_view());
	AudioTimeAxisView* atv;

	if ((atv = dynamic_cast<AudioTimeAxisView*>(&tv)) != 0) {

		if (atv->is_audio_track()) {

			AudioPlaylist* pl;
			if ((pl = dynamic_cast<AudioPlaylist*> (atv->get_diskstream()->playlist())) != 0) {

				Playlist::RegionList* rl = pl->regions_at (event_frame (event));

				if (!rl->empty()) {
					DescendingRegionLayerSorter cmp;
					rl->sort (cmp);

					RegionView* rv = atv->view()->find_view (rl->front());

					/* proxy */

					delete rl;

					return canvas_region_view_event (event, rv->get_canvas_group(), rv);
				} 
			}
		}
	}

	return TRUE;
}

bool
Editor::canvas_control_point_event (GdkEvent *event, ArdourCanvas::Item* item, ControlPoint* cp)
{
	ItemType type;

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_control_point = cp;
		clicked_trackview = &cp->line.trackview;
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		clicked_regionview = 0;
		break;

	case GDK_SCROLL_UP:
		break;

	case GDK_SCROLL_DOWN:
		break;

	default:
		break;
	}

	if (dynamic_cast<AudioRegionGainLine*> (&cp->line) != 0) {
		type = GainControlPointItem;
	} else if (dynamic_cast<AutomationGainLine*> (&cp->line) != 0) {
		type = GainAutomationControlPointItem;
	} else if (dynamic_cast<AutomationPanLine*> (&cp->line) != 0) {
		type = PanAutomationControlPointItem;
	} else if (dynamic_cast<RedirectAutomationLine*> (&cp->line) != 0) {
		type = RedirectAutomationControlPointItem;
	} else {
		return false;
	}

	return typed_event (item, event, type);
}

bool
Editor::canvas_line_event (GdkEvent *event, ArdourCanvas::Item* item, AutomationLine* al)
{
	ItemType type;

	if (dynamic_cast<AudioRegionGainLine*> (al) != 0) {
		type = GainLineItem;
	} else if (dynamic_cast<AutomationGainLine*> (al) != 0) {
		type = GainAutomationLineItem;
	} else if (dynamic_cast<AutomationPanLine*> (al) != 0) {
		type = PanAutomationLineItem;
	} else if (dynamic_cast<RedirectAutomationLine*> (al) != 0) {
		type = RedirectAutomationLineItem;
	} else {
		return false;
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
		ret = motion_handler (item, event, SelectionItem);
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
		ret = motion_handler (item, event, StartSelectionTrimItem);
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
		ret = motion_handler (item, event, EndSelectionTrimItem);
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
	
	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &clicked_regionview->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, RegionViewNameHighlight);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, RegionViewNameHighlight);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, RegionViewNameHighlight);
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

	switch (event->type) {
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:
		clicked_regionview = rv;
		clicked_control_point = 0;
		clicked_trackview = &clicked_regionview->get_time_axis_view();
		clicked_audio_trackview = dynamic_cast<AudioTimeAxisView*>(clicked_trackview);
		ret = button_press_handler (item, event, RegionViewName);
		break;
	case GDK_BUTTON_RELEASE:
		ret = button_release_handler (item, event, RegionViewName);
		break;
	case GDK_MOTION_NOTIFY:
		ret = motion_handler (item, event, RegionViewName);
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
Editor::canvas_edit_cursor_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, EditCursorItem);
}

bool
Editor::canvas_zoom_rect_event (GdkEvent *event, ArdourCanvas::Item* item)
{
	return typed_event (item, event, NoItem);
}

