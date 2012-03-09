
/*
    Copyright (C) 2000-2001 Paul Davis 

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

#include <cassert>
#include <cstdlib>
#include <stdint.h>
#include <cmath>
#include <set>
#include <string>
#include <algorithm>
#include <sys/time.h>

#include <pbd/error.h>
#include <gtkmm2ext/utils.h>
#include <pbd/memento_command.h>

#include "ardour_ui.h"
#include "editor.h"
#include "time_axis_view.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "marker.h"
#include "streamview.h"
#include "region_gain_line.h"
#include "automation_time_axis.h"
#include "prompter.h"
#include "utils.h"
#include "selection.h"
#include "keyboard.h"
#include "editing.h"
#include "rgb_macros.h"

#include <ardour/types.h>
#include <ardour/profile.h>
#include <ardour/route.h>
#include <ardour/audio_track.h>
#include <ardour/audio_diskstream.h>
#include <ardour/playlist.h>
#include <ardour/audioplaylist.h>
#include <ardour/audioregion.h>
#include <ardour/dB.h>
#include <ardour/utils.h>
#include <ardour/region_factory.h>

#include <bitset>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace sigc;
using namespace Gtk;
using namespace Editing;

bool
Editor::mouse_frame (nframes64_t& where, bool& in_track_canvas) const
{
        /* gdk_window_get_pointer() has X11's XQueryPointer semantics in that it only
           pays attentions to subwindows. this means that menu windows are ignored, and 
           if the pointer is in a menu, the return window from the call will be the
           the regular subwindow *under* the menu.

           this matters quite a lot if the pointer is moving around in a menu that overlaps
           the track canvas because we will believe that we are within the track canvas
           when we are not. therefore, we track enter/leave events for the track canvas
           and allow that to override the result of gdk_window_get_pointer().
        */

        if (!within_track_canvas) {
                return false;
        }

	int x, y;
	double wx, wy;
	Gdk::ModifierType mask;
	Glib::RefPtr<Gdk::Window> canvas_window = const_cast<Editor*>(this)->track_canvas->get_window();
	Glib::RefPtr<const Gdk::Window> pointer_window;

	if (!canvas_window) {
		return false;
	}
	
	pointer_window = canvas_window->get_pointer (x, y, mask);

	if (pointer_window == track_canvas->get_bin_window()) {
		wx = x;
		wy = y;
		in_track_canvas = true;

	} else {
		in_track_canvas = false;
			return false;
	}

	GdkEvent event;
	event.type = GDK_BUTTON_RELEASE;
	event.button.x = wx;
	event.button.y = wy;
	
	where = event_frame (&event, 0, 0);
	return true;
}

nframes64_t
Editor::event_frame (GdkEvent* event, double* pcx, double* pcy) const
{
	double cx, cy;

	if (pcx == 0) {
		pcx = &cx;
	}
	if (pcy == 0) {
		pcy = &cy;
	}

	*pcx = 0;
	*pcy = 0;

	switch (event->type) {
	case GDK_BUTTON_RELEASE:
	case GDK_BUTTON_PRESS:
	case GDK_2BUTTON_PRESS:
	case GDK_3BUTTON_PRESS:

		*pcx = event->button.x;
		*pcy = event->button.y;
		_trackview_group->w2i(*pcx, *pcy);
		break;
	case GDK_MOTION_NOTIFY:
	
		*pcx = event->motion.x;
		*pcy = event->motion.y;
		_trackview_group->w2i(*pcx, *pcy);
		break;
	case GDK_ENTER_NOTIFY:
	case GDK_LEAVE_NOTIFY:
		track_canvas->w2c(event->crossing.x, event->crossing.y, *pcx, *pcy);
		break;
	case GDK_KEY_PRESS:
	case GDK_KEY_RELEASE:
		// track_canvas->w2c(event->key.x, event->key.y, *pcx, *pcy);
		break;
	default:
		warning << string_compose (_("Editor::event_frame() used on unhandled event type %1"), event->type) << endmsg;
		break;
	}

	/* note that pixel_to_frame() never returns less than zero, so even if the pixel
	   position is negative (as can be the case with motion events in particular),
	   the frame location is always positive.
	*/
	
	return pixel_to_frame (*pcx);
}

void
Editor::mouse_mode_toggled (MouseMode m)
{
	if (ignore_mouse_mode_toggle) {
		return;
	}

	switch (m) {
	case MouseRange:
		if (mouse_select_button.get_active()) {
			set_mouse_mode (m);
		}
		break;

	case MouseObject:
		if (mouse_move_button.get_active()) {
			set_mouse_mode (m);
		}
		break;

	case MouseGain:
		if (mouse_gain_button.get_active()) {
			set_mouse_mode (m);
		}
		break;

	case MouseZoom:
		if (mouse_zoom_button.get_active()) {
			set_mouse_mode (m);
		}
		break;

	case MouseTimeFX:
		if (mouse_timefx_button.get_active()) {
			set_mouse_mode (m);
		}
		break;

	case MouseAudition:
		if (mouse_audition_button.get_active()) {
			set_mouse_mode (m);
		}
		break;

	default:
		break;
	}
}	

Gdk::Cursor*
Editor::which_grabber_cursor ()
{
	switch (_edit_point) {
	case EditAtMouse:
		return grabber_edit_point_cursor;
		break;
	default:
		break;
	}
	return grabber_cursor;
}

void
Editor::set_canvas_cursor ()
{
	switch (mouse_mode) {
	case MouseRange:
		current_canvas_cursor = selector_cursor;
		break;

	case MouseObject:
		current_canvas_cursor = which_grabber_cursor();
		break;

	case MouseGain:
		current_canvas_cursor = cross_hair_cursor;
		break;

	case MouseZoom:
		current_canvas_cursor = zoom_cursor;
		break;

	case MouseTimeFX:
		current_canvas_cursor = time_fx_cursor; // just use playhead
		break;

	case MouseAudition:
		current_canvas_cursor = speaker_cursor;
		break;
	}

	if (is_drawable()) {
	        track_canvas->get_window()->set_cursor(*current_canvas_cursor);
	}
}

void
Editor::set_mouse_mode (MouseMode m, bool force)
{
	if (drag_info.item) {
		return;
	}

	if (!force && m == mouse_mode) {
		return;
	}
	
	mouse_mode = m;

	instant_save ();

	if (mouse_mode != MouseRange) {

		/* in all modes except range, hide the range selection,
		   show the object (region) selection.
		*/

		for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
			(*i)->set_should_show_selection (true);
		}
		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			(*i)->hide_selection ();
		}

	} else {

		/* 
		   in range mode,show the range selection.
		*/

		for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
			if ((*i)->get_selected()) {
				(*i)->show_selection (selection->time);
			}
		}
	}

	/* XXX the hack of unsetting all other buttons should go 
	   away once GTK2 allows us to use regular radio buttons drawn like
	   normal buttons, rather than my silly GroupedButton hack.
	*/
	
	ignore_mouse_mode_toggle = true;

	switch (mouse_mode) {
	case MouseRange:
		mouse_select_button.set_active (true);
		break;

	case MouseObject:
		mouse_move_button.set_active (true);
		break;

	case MouseGain:
		mouse_gain_button.set_active (true);
		break;

	case MouseZoom:
		mouse_zoom_button.set_active (true);
		break;

	case MouseTimeFX:
		mouse_timefx_button.set_active (true);
		break;

	case MouseAudition:
		mouse_audition_button.set_active (true);
		break;
	}

	ignore_mouse_mode_toggle = false;
	
	set_canvas_cursor ();
}

void
Editor::step_mouse_mode (bool next)
{
	switch (current_mouse_mode()) {
	case MouseObject:
		if (next) {
			if (Profile->get_sae()) {
				set_mouse_mode (MouseZoom);
			} else {
				set_mouse_mode (MouseRange);
			}
		} else {
			set_mouse_mode (MouseTimeFX);
		}
		break;

	case MouseRange:
		if (next) set_mouse_mode (MouseZoom);
		else set_mouse_mode (MouseObject);
		break;

	case MouseZoom:
		if (next) {
			if (Profile->get_sae()) {
				set_mouse_mode (MouseTimeFX);
			} else {
				set_mouse_mode (MouseGain);
			}
		} else {
			if (Profile->get_sae()) {
				set_mouse_mode (MouseObject);
			} else {
				set_mouse_mode (MouseRange);
			}
		}
		break;
	
	case MouseGain:
		if (next) set_mouse_mode (MouseTimeFX);
		else set_mouse_mode (MouseZoom);
		break;
	
	case MouseTimeFX:
		if (next) {
			set_mouse_mode (MouseAudition);
		} else {
			if (Profile->get_sae()) {
				set_mouse_mode (MouseZoom);
			} else {
				set_mouse_mode (MouseGain);
			}
		}
		break;

	case MouseAudition:
		if (next) set_mouse_mode (MouseObject);
		else set_mouse_mode (MouseTimeFX);
		break;
	}
}

void
Editor::button_selection (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
 	/* in object/audition/timefx/gain-automation mode,
	   any button press sets the selection if the object
	   can be selected. this is a bit of hack, because
	   we want to avoid this if the mouse operation is a
	   region alignment.

	   note: not dbl-click or triple-click
	*/

	if (((mouse_mode != MouseObject) &&
	     (mouse_mode != MouseAudition || item_type != RegionItem) &&
	     (mouse_mode != MouseTimeFX || item_type != RegionItem) &&
	     (mouse_mode != MouseGain) &&
	     (mouse_mode != MouseRange)) ||

	    ((event->type != GDK_BUTTON_PRESS && event->type != GDK_BUTTON_RELEASE) || event->button.button > 3)) {
		
		return;
	}

	if (event->type == GDK_BUTTON_PRESS || event->type == GDK_BUTTON_RELEASE) {

		if ((event->button.state & Keyboard::RelevantModifierKeyMask) && event->button.button != 1) {
			
			/* almost no selection action on modified button-2 or button-3 events */
		
			if (item_type != RegionItem && event->button.button != 2) {
				return;
			}
		}
	}

	Selection::Operation op = Keyboard::selection_type (event->button.state);
	bool press = (event->type == GDK_BUTTON_PRESS);

	// begin_reversible_command (_("select on click"));
	
	switch (item_type) {
	case RegionItem:
		if (mouse_mode != MouseRange) {
			set_selected_regionview_from_click (press, op, true);
		} else if (event->type == GDK_BUTTON_PRESS) {
			set_selected_track_as_side_effect (op);
		}
		break;
		
	case RegionViewNameHighlight:
	case RegionViewName:
		if (mouse_mode != MouseRange) {
			set_selected_regionview_from_click (press, op, true);
		} else if (event->type == GDK_BUTTON_PRESS) {
			set_selected_track_as_side_effect (op);
		}
		break;

	case FadeInHandleItem:
	case FadeInItem:
	case FadeOutHandleItem:
	case FadeOutItem:
		if (mouse_mode != MouseRange) {
			set_selected_regionview_from_click (press, op, true);
		} else if (event->type == GDK_BUTTON_PRESS) {
			set_selected_track_as_side_effect (op);
		}
		break;
		
	case GainAutomationControlPointItem:
	case PanAutomationControlPointItem:
	case RedirectAutomationControlPointItem:
		set_selected_track_as_side_effect (op);
		if (mouse_mode != MouseRange) {
			set_selected_control_point_from_click (op, false);
		}
		break;
		
	case StreamItem:
		/* for context click or range selection, select track */
		if (event->button.button == 3) {
			set_selected_track_as_side_effect (op);
		} else if (event->type == GDK_BUTTON_PRESS && mouse_mode == MouseRange) {
			set_selected_track_as_side_effect (op);
		}
		break;
		    
	case AutomationTrackItem:
		set_selected_track_as_side_effect (op, true);
		break;
		
	default:
		break;
	}
}

const static double ZERO_GAIN_FRACTION = gain_to_slider_position(dB_to_coefficient(0.0));

bool
Editor::button_press_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	Glib::RefPtr<Gdk::Window> canvas_window = const_cast<Editor*>(this)->track_canvas->get_window();

	if (canvas_window) {
		Glib::RefPtr<const Gdk::Window> pointer_window;
		int x, y;
		double wx, wy;
		Gdk::ModifierType mask;

		pointer_window = canvas_window->get_pointer (x, y, mask);
		
		if (pointer_window == track_canvas->get_bin_window()) {
			track_canvas->window_to_world (x, y, wx, wy);
			allow_vertical_scroll = true;
		} else {
			allow_vertical_scroll = false;
		}
	}

	track_canvas->grab_focus();

	if (session && session->actively_recording()) {
		return true;
	}

	button_selection (item, event, item_type);

	//ctrl-drag or right-click-drag on a "range" ruler should start a range drag
	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button.button == 3 || ( (event->button.button == 1) && (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier) ))) {
			if (item_type == TransportMarkerBarItem) {
				start_range_markerbar_op (item, event, CreateTransportMarker);
				return true;
			}
			if (item_type == RangeMarkerBarItem) {
				start_range_markerbar_op (item, event, CreateRangeMarker);
				return true;
			}
			if (item_type == CdMarkerBarItem) {
				start_range_markerbar_op (item, event, CreateCDMarker);
				return true;
			}
		}
	}

	if (drag_info.item == 0 &&
	    (Keyboard::is_delete_event (&event->button) ||
	     Keyboard::is_context_menu_event (&event->button) ||
	     Keyboard::is_edit_event (&event->button))) {
		
		/* handled by button release */
		return true;
	}

	switch (event->button.button) {
	case 1:

		if (event->type == GDK_BUTTON_PRESS) {

			if (drag_info.item) {
				drag_info.item->ungrab (event->button.time);
			}

			/* single mouse clicks on any of these item types operate
			   independent of mouse mode, mostly because they are
			   not on the main track canvas or because we want
			   them to be modeless.
			*/
			
			switch (item_type) {
			case PlayheadCursorItem:
				start_cursor_grab (item, event);
				return true;

			case MarkerItem:
				if (Keyboard::modifier_state_equals (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
					hide_marker (item, event);
				} else {
					start_marker_grab (item, event);
				}
				return true;

			case TempoMarkerItem:
				if (Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)) {
					start_tempo_marker_copy_grab (item, event);
				} else {
					start_tempo_marker_grab (item, event);
				}
				return true;

			case MeterMarkerItem:
				if (Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)) {
					start_meter_marker_copy_grab (item, event);
				} else {
					start_meter_marker_grab (item, event);
				}
				return true;

			case MarkerBarItem:
			case TempoBarItem:
			case MeterBarItem:
			case TransportMarkerBarItem:
			case RangeMarkerBarItem:
			case CdMarkerBarItem:
				if (!Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
					start_cursor_grab_no_stop(&playhead_cursor->canvas_item, event);
				}
				return true;
				break;

			default:
				break;
			}
		}

		switch (mouse_mode) {
		case MouseRange:
			switch (item_type) {
			case StartSelectionTrimItem:
				start_selection_op (item, event, SelectionStartTrim);
				break;
				
			case EndSelectionTrimItem:
				start_selection_op (item, event, SelectionEndTrim);
				break;

			case SelectionItem:
				if (Keyboard::modifier_state_contains 
				    (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier))) {
					// contains and not equals because I can't use alt as a modifier alone.
					start_selection_grab (item, event);
				} else if (Keyboard::modifier_state_equals (event->button.state, Keyboard::SecondaryModifier)) {
					/* grab selection for moving */
					start_selection_op (item, event, SelectionMove);
				} else {
					/* this was debated, but decided the more common action was to
					   make a new selection */
					start_selection_op (item, event, CreateSelection);
				}
				break;

			default:
				start_selection_op (item, event, CreateSelection);
			}
			return true;
			break;
			
		case MouseObject:
			if (Keyboard::modifier_state_contains (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)) &&
			    event->type == GDK_BUTTON_PRESS) {
				
				start_rubberband_select (item, event);

			} else if (event->type == GDK_BUTTON_PRESS) {

				switch (item_type) {
				case FadeInHandleItem:
					start_fade_in_grab (item, event);
					return true;
					
				case FadeOutHandleItem:
					start_fade_out_grab (item, event);
					return true;

				case RegionItem:
					if (Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)) {
						start_region_copy_grab (item, event);
					} else if (Keyboard::the_keyboard().key_is_down (GDK_b)) {
						start_region_brush_grab (item, event);
					} else {
						start_region_grab (item, event);
					}
					break;
					
				case RegionViewNameHighlight:
					start_trim (item, event);
					return true;
					break;
					
				case RegionViewName:
					/* rename happens on edit clicks */
					start_trim (clicked_regionview->get_name_highlight(), event);
					return true;
					break;

				case GainAutomationControlPointItem:
				case PanAutomationControlPointItem:
				case RedirectAutomationControlPointItem:
					start_control_point_grab (item, event);
					return true;
					break;
					
				case GainAutomationLineItem:
				case PanAutomationLineItem:
				case RedirectAutomationLineItem:
					start_line_grab_from_line (item, event);
					return true;
					break;

				case StreamItem:
				case AutomationTrackItem:
					start_rubberband_select (item, event);
					break;
					
				/* <CMT Additions> */
				case ImageFrameHandleStartItem:
					imageframe_start_handle_op(item, event) ;
					return(true) ;
					break ;
				case ImageFrameHandleEndItem:
					imageframe_end_handle_op(item, event) ;
					return(true) ;
					break ;
				case MarkerViewHandleStartItem:
					markerview_item_start_handle_op(item, event) ;
					return(true) ;
					break ;
				case MarkerViewHandleEndItem:
					markerview_item_end_handle_op(item, event) ;
					return(true) ;
					break ;
				/* </CMT Additions> */
				
				/* <CMT Additions> */
				case MarkerViewItem:
					start_markerview_grab(item, event) ;
					break ;
				case ImageFrameItem:
					start_imageframe_grab(item, event) ;
					break ;
				/* </CMT Additions> */

				case MarkerBarItem:
					
					break;

				default:
					break;
				}
			}
			return true;
			break;
			
		case MouseGain:
			switch (item_type) {
			case RegionItem:
				/* start a grab so that if we finish after moving
				   we can tell what happened.
				*/
				drag_info.item = item;
				drag_info.motion_callback = &Editor::region_gain_motion_callback;
				drag_info.finished_callback = 0;
				start_grab (event, current_canvas_cursor);
				break;

			case GainControlPointItem:
				start_control_point_grab (item, event);
				return true;
				
			case GainLineItem:
				start_line_grab_from_line (item, event);
				return true;

			case GainAutomationControlPointItem:
			case PanAutomationControlPointItem:
			case RedirectAutomationControlPointItem:
				start_control_point_grab (item, event);
				return true;
				break;

			default:
				break;
			}
			return true;
			break;

			switch (item_type) {
			case GainAutomationControlPointItem:
			case PanAutomationControlPointItem:
			case RedirectAutomationControlPointItem:
				start_control_point_grab (item, event);
				break;

			case GainAutomationLineItem:
			case PanAutomationLineItem:
			case RedirectAutomationLineItem:
				start_line_grab_from_line (item, event);
				break;

			case RegionItem:
				// XXX need automation mode to identify which
				// line to use
				// start_line_grab_from_regionview (item, event);
				break;

			default:
				break;
			}
			return true;
			break;

		case MouseZoom:
			if (event->type == GDK_BUTTON_PRESS) {
				start_mouse_zoom (item, event);
			}

			return true;
			break;

		case MouseTimeFX:
			if (item_type == RegionItem) {
				start_time_fx (item, event);
			}
			break;

		case MouseAudition:
			_scrubbing = true;
			scrub_reversals = 0;
			scrub_reverse_distance = 0;
			last_scrub_x = event->button.x;
			scrubbing_direction = 0;
			track_canvas->get_window()->set_cursor (*transparent_cursor);
			/* rest handled in motion & release */
			break;

		default:
			break;
		}
		break;

	case 2:
		switch (mouse_mode) {
		case MouseObject:
			if (event->type == GDK_BUTTON_PRESS) {
				switch (item_type) {
				case RegionItem:
					if (Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)) {
						start_region_copy_grab (item, event);
					} else {
						start_region_grab (item, event);
					}
					return true;
					break;

				case GainAutomationControlPointItem:
				case PanAutomationControlPointItem:
				case RedirectAutomationControlPointItem:
					start_control_point_grab (item, event);
					return true;
					break;
					
				default:
					break;
				}
			}
			
			
			switch (item_type) {
			case RegionViewNameHighlight:
				start_trim (item, event);
				return true;
				break;
				
			case RegionViewName:
				start_trim (clicked_regionview->get_name_highlight(), event);
				return true;
				break;
				
			default:
				break;
			}
			
			break;

		case MouseRange:
			if (event->type == GDK_BUTTON_PRESS) {
				/* relax till release */
			}
			return true;
			break;
					
				
		case MouseZoom:
			if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
				temporal_zoom_session();
			} else {
				temporal_zoom_to_frame (true, event_frame(event));
			}
			return true;
			break;

		default:
			break;
		}

		break;

	case 3:
		break;

	default:
		break;

	}

	return false;
}

bool
Editor::button_release_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	nframes64_t where = event_frame (event, 0, 0);
	AutomationTimeAxisView* atv = 0;
	
	/* no action if we're recording */
						
	if (session && session->actively_recording()) {
		return true;
	}

	/* first, see if we're finishing a drag ... */

	if (drag_info.item) {
		if (end_grab (item, event)) {
			/* grab dragged, so do nothing else */
			return true;
		}
	}
	
	button_selection (item, event, item_type);
	update_region_layering_order_editor (where);

	/* edit events get handled here */

	if (drag_info.item == 0 && Keyboard::is_edit_event (&event->button)) {
		switch (item_type) {
		case RegionItem:
			edit_region ();
			break;

		case TempoMarkerItem:
			edit_tempo_marker (item);
			break;
			
		case MeterMarkerItem:
			edit_meter_marker (item);
			break;
			
		case RegionViewName:
			if (clicked_regionview->name_active()) {
				return mouse_rename_region (item, event);
			}
			break;

		default:
			break;
		}
		return true;
	}

	/* context menu events get handled here */

	if (Keyboard::is_context_menu_event (&event->button)) {

		if (drag_info.item == 0) {

			/* no matter which button pops up the context menu, tell the menu
			   widget to use button 1 to drive menu selection.
			*/

			switch (item_type) {
			case FadeInItem:
			case FadeInHandleItem:
			case FadeOutItem:
			case FadeOutHandleItem:
				popup_fade_context_menu (1, event->button.time, item, item_type);
				break;

			case StreamItem:
				popup_track_context_menu (1, event->button.time, item_type, false, where);
				break;
				
			case RegionItem:
			case RegionViewNameHighlight:
			case RegionViewName:
				popup_track_context_menu (1, event->button.time, item_type, false, where);
				break;
				
			case SelectionItem:
				popup_track_context_menu (1, event->button.time, item_type, true, where);
				break;

			case AutomationTrackItem:
				popup_track_context_menu (1, event->button.time, item_type, false, where);
				break;

			case MarkerBarItem: 
			case RangeMarkerBarItem: 
			case TransportMarkerBarItem:
			case CdMarkerBarItem:
			case TempoBarItem:
			case MeterBarItem:
				popup_ruler_menu (where, item_type);
				break;

			case MarkerItem:
				marker_context_menu (&event->button, item);
				break;

			case TempoMarkerItem:
				tm_marker_context_menu (&event->button, item);
				break;
				
			case MeterMarkerItem:
				tm_marker_context_menu (&event->button, item);
				break;

			case CrossfadeViewItem:
				popup_track_context_menu (1, event->button.time, item_type, false, where);
				break;

			/* <CMT Additions> */
			case ImageFrameItem:
				popup_imageframe_edit_menu(1, event->button.time, item, true) ;
				break ;
			case ImageFrameTimeAxisItem:
				popup_imageframe_edit_menu(1, event->button.time, item, false) ;
				break ;
			case MarkerViewItem:
				popup_marker_time_axis_edit_menu(1, event->button.time, item, true) ;
				break ;
			case MarkerTimeAxisItem:
				popup_marker_time_axis_edit_menu(1, event->button.time, item, false) ;
				break ;
			/* <CMT Additions> */

				
			default:
				break;
			}

			return true;
		}
	}

	/* delete events get handled here */

	if (drag_info.item == 0 && Keyboard::is_delete_event (&event->button)) {

		switch (item_type) {
		case TempoMarkerItem:
			remove_tempo_marker (item);
			break;
			
		case MeterMarkerItem:
			remove_meter_marker (item);
			break;

		case MarkerItem:
			remove_marker (*item, event);
			break;

		case RegionItem:
			if (mouse_mode == MouseObject) {
				remove_clicked_region ();
			}
			break;
			
		case GainControlPointItem:
			if (mouse_mode == MouseGain) {
				remove_gain_control_point (item, event);
			}
			break;

		case GainAutomationControlPointItem:
		case PanAutomationControlPointItem:
		case RedirectAutomationControlPointItem:
			remove_control_point (item, event);
			break;

		default:
			break;
		}
		return true;
	}

	switch (event->button.button) {
	case 1:

		switch (item_type) {
		/* see comments in button_press_handler */
		case PlayheadCursorItem:
		case MarkerItem:
		case GainLineItem:
		case GainAutomationLineItem:
		case PanAutomationLineItem:
		case RedirectAutomationLineItem:
		case StartSelectionTrimItem:
		case EndSelectionTrimItem:
			return true;

		case MarkerBarItem:
			if (!_dragging_playhead) {
				if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
					snap_to (where, 0, true);
				}
				mouse_add_new_marker (where);
			}
			return true;

		case CdMarkerBarItem:
			if (!_dragging_playhead) {
				// if we get here then a dragged range wasn't done
				if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
					snap_to (where, 0, true);
				}
				mouse_add_new_marker (where, true);
			}
			return true;

		case TempoBarItem:
			if (!_dragging_playhead) {
				if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
					snap_to (where);
				}
				mouse_add_new_tempo_event (where);
			}
			return true;
			
		case MeterBarItem:
			if (!_dragging_playhead) {
				mouse_add_new_meter_event (pixel_to_frame (event->button.x));
			} 
			return true;
			break;

		default:
			break;
		}
		
		switch (mouse_mode) {
		case MouseObject:
			switch (item_type) {
			case AutomationTrackItem:
				atv = dynamic_cast<AutomationTimeAxisView*>(clicked_trackview);
				if (atv) {
					atv->add_automation_event (item, event, where, event->button.y);
				}
				return true;
				
				break;
				
			default:
				break;
			}
			break;

		case MouseGain:
			// Gain only makes sense for audio regions

			if (!dynamic_cast<AudioRegionView*>(clicked_regionview)) {
				break;
			}

			switch (item_type) {
			case RegionItem:
				/* check that we didn't drag before releasing, since
				   its really annoying to create new control
				   points when doing this.
				*/
				if (drag_info.first_move) { 
					dynamic_cast<AudioRegionView*>(clicked_regionview)->add_gain_point_event (item, event);
				}
				return true;
				break;
				
			case AutomationTrackItem:
				dynamic_cast<AutomationTimeAxisView*>(clicked_trackview)->
					add_automation_event (item, event, where, event->button.y);
				return true;
				break;
			default:
				break;
			}
			break;
			
		case MouseAudition:
			_scrubbing = false;
			track_canvas->get_window()->set_cursor (*current_canvas_cursor);
			if (scrubbing_direction == 0) {
				/* no drag, just a click */
				switch (item_type) {
				case RegionItem:
					play_selected_region ();
					break;
				default:
					break;
				}
			} else {
				/* make sure we stop */
				session->request_stop ();
			}
			break;

		default:
			break;

		}

		return true;
		break;


	case 2:
		switch (mouse_mode) {
			
		case MouseObject:
			switch (item_type) {
			case RegionItem:
				if (Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
					raise_region ();
				} else if (Keyboard::modifier_state_equals (event->button.state, Keyboard::ModifierMask (Keyboard::TertiaryModifier|Keyboard::SecondaryModifier))) {
					lower_region ();
				} else {
					// Button2 click is unused
				}
				return true;
				
				break;
				
			default:
				break;
			}
			break;
			
		case MouseRange:
			
			// x_style_paste (where, 1.0);
			return true;
			break;
			
		default:
			break;
		}

		break;
	
	case 3:
		break;
		
	default:
		break;
	}
	return false;
}

bool
Editor::enter_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	ControlPoint* cp;
	Marker * marker;
	double fraction;
	
	if (last_item_entered != item) {
		last_item_entered = item;
		last_item_entered_n = 0;
	}

	switch (item_type) {
	case GainControlPointItem:
		if (mouse_mode == MouseGain) {
			cp = static_cast<ControlPoint*>(item->get_data ("control_point"));
			cp->set_visible (true);

			double at_x, at_y;
			at_x = cp->get_x();
			at_y = cp->get_y ();
			cp->item->i2w (at_x, at_y);
			at_x += 10.0;
			at_y += 10.0;

			fraction = 1.0 - (cp->get_y() / cp->line.height());

			if (is_drawable() && !_scrubbing) {
			        track_canvas->get_window()->set_cursor (*fader_cursor);
			}

			last_item_entered_n++;
			set_verbose_canvas_cursor (cp->line.get_verbose_cursor_string (fraction), at_x, at_y);
			if (last_item_entered_n < 10) {
				show_verbose_canvas_cursor ();
			}
		}
		break;

	case GainAutomationControlPointItem:
	case PanAutomationControlPointItem:
	case RedirectAutomationControlPointItem:
		if (mouse_mode == MouseGain || mouse_mode == MouseObject) {
			cp = static_cast<ControlPoint*>(item->get_data ("control_point"));
			cp->set_visible (true);

			double at_x, at_y;
			at_x = cp->get_x();
			at_y = cp->get_y ();
			cp->item->i2w (at_x, at_y);
			at_x += 10.0;
			at_y += 10.0;

			fraction = 1.0 - (cp->get_y() / cp->line.height());

			set_verbose_canvas_cursor (cp->line.get_verbose_cursor_string (fraction), at_x, at_y);
			show_verbose_canvas_cursor ();

			if (is_drawable()) {
				track_canvas->get_window()->set_cursor (*fader_cursor);
			}
		}
		break;
		
	case GainLineItem:
		if (mouse_mode == MouseGain) {
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			if (line)
				line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_EnteredGainLine.get();
			if (is_drawable()) {
				track_canvas->get_window()->set_cursor (*fader_cursor);
			}
		}
		break;
			
	case GainAutomationLineItem:
	case RedirectAutomationLineItem:
	case PanAutomationLineItem:
		if (mouse_mode == MouseGain || mouse_mode == MouseObject) {
			{
				ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
				if (line)
					line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_EnteredAutomationLine.get();
			}
			if (is_drawable()) {
				track_canvas->get_window()->set_cursor (*fader_cursor);
			}
		}
		break;
		
	case RegionViewNameHighlight:
		if (is_drawable() && mouse_mode == MouseObject) {
			track_canvas->get_window()->set_cursor (*trimmer_cursor);
		}
		break;

	case StartSelectionTrimItem:
	case EndSelectionTrimItem:
	/* <CMT Additions> */
	case ImageFrameHandleStartItem:
	case ImageFrameHandleEndItem:
	case MarkerViewHandleStartItem:
	case MarkerViewHandleEndItem:
	/* </CMT Additions> */

		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*trimmer_cursor);
		}
		break;

	case PlayheadCursorItem:
		if (is_drawable()) {
			switch (_edit_point) {
			case EditAtMouse:
				track_canvas->get_window()->set_cursor (*grabber_edit_point_cursor);
				break;
			default:
				track_canvas->get_window()->set_cursor (*grabber_cursor);
				break;
			}
		}
		break;

	case RegionViewName:
		
		/* when the name is not an active item, the entire name highlight is for trimming */

		if (!reinterpret_cast<RegionView *> (item->get_data ("regionview"))->name_active()) {
			if (mouse_mode == MouseObject && is_drawable()) {
				track_canvas->get_window()->set_cursor (*trimmer_cursor);
			}
		} 
		break;


	case AutomationTrackItem:
		if (is_drawable()) {
		        Gdk::Cursor *cursor;
			switch (mouse_mode) {
			case MouseRange:
				cursor = selector_cursor;
				break;
			case MouseZoom:
	 			cursor = zoom_cursor;
				break;
			default:
				cursor = cross_hair_cursor;
				break;
			}

			track_canvas->get_window()->set_cursor (*cursor);

			AutomationTimeAxisView* atv;
			if ((atv = static_cast<AutomationTimeAxisView*>(item->get_data ("trackview"))) != 0) {
				clear_entered_track = false;
				set_entered_track (atv);
			}
		}
		break;

	case MarkerBarItem:
	case RangeMarkerBarItem:
	case TransportMarkerBarItem:
	case CdMarkerBarItem:
	case MeterBarItem:
	case TempoBarItem:
		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*timebar_cursor);
		}
		break;

	case MarkerItem:
		if ((marker = static_cast<Marker *> (item->get_data ("marker"))) == 0) {
			break;
		}
		entered_marker = marker;
		marker->set_color_rgba (ARDOUR_UI::config()->canvasvar_EnteredMarker.get());
		// fall through
	case MeterMarkerItem:
	case TempoMarkerItem:
		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*timebar_cursor);
		}
		break;
	case FadeInHandleItem:
	case FadeOutHandleItem:
		if (mouse_mode == MouseObject) {
			ArdourCanvas::SimpleRect *rect = dynamic_cast<ArdourCanvas::SimpleRect *> (item);
			if (rect) {
				rect->property_fill_color_rgba() = 0;
				rect->property_outline_pixels() = 1;
			}
		}
		break;

	default:
		break;
	}

	/* second pass to handle entered track status in a comprehensible way.
	 */

	switch (item_type) {
	case GainLineItem:
	case GainAutomationLineItem:
	case RedirectAutomationLineItem:
	case PanAutomationLineItem:
	case GainControlPointItem:
	case GainAutomationControlPointItem:
	case PanAutomationControlPointItem:
	case RedirectAutomationControlPointItem:
		/* these do not affect the current entered track state */
		clear_entered_track = false;
		break;

	case AutomationTrackItem:
		/* handled above already */
		break;

	default:
		set_entered_track (0);
		break;
	}

	return false;
}

bool
Editor::leave_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	AutomationLine* al;
	ControlPoint* cp;
	Marker *marker;
	Location *loc;
	RegionView* rv;
	bool is_start;

	switch (item_type) {
	case GainControlPointItem:
	case GainAutomationControlPointItem:
	case PanAutomationControlPointItem:
	case RedirectAutomationControlPointItem:
		cp = reinterpret_cast<ControlPoint*>(item->get_data ("control_point"));
		if (cp->line.npoints() > 1) {
			if (!cp->selected) {
				cp->set_visible (false);
			}
		}
		
		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*current_canvas_cursor);
		}

		hide_verbose_canvas_cursor ();
		break;
		
	case RegionViewNameHighlight:
	case StartSelectionTrimItem:
	case EndSelectionTrimItem:
	case PlayheadCursorItem:
	/* <CMT Additions> */
	case ImageFrameHandleStartItem:
	case ImageFrameHandleEndItem:
	case MarkerViewHandleStartItem:
	case MarkerViewHandleEndItem:
	/* </CMT Additions> */
		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*current_canvas_cursor);
		}
		break;

	case GainLineItem:
	case GainAutomationLineItem:
	case RedirectAutomationLineItem:
	case PanAutomationLineItem:
		al = reinterpret_cast<AutomationLine*> (item->get_data ("line"));
		{
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			if (line)
				line->property_fill_color_rgba() = al->get_line_color();
		}
		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*current_canvas_cursor);
		}
		break;

	case RegionViewName:
		/* see enter_handler() for notes */
		if (!reinterpret_cast<RegionView *> (item->get_data ("regionview"))->name_active()) {
			if (is_drawable() && mouse_mode == MouseObject) {
				track_canvas->get_window()->set_cursor (*current_canvas_cursor);
			}
		}
		break;

	case RangeMarkerBarItem:
	case TransportMarkerBarItem:
	case CdMarkerBarItem:
	case MeterBarItem:
	case TempoBarItem:
	case MarkerBarItem:
		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*current_canvas_cursor);
		}
		break;
		
	case MarkerItem:
		if ((marker = static_cast<Marker *> (item->get_data ("marker"))) == 0) {
			break;
		}
		entered_marker = 0;
		if ((loc = find_location_from_marker (marker, is_start)) != 0) {
			location_flags_changed (loc, this);
		}
		// fall through
	case MeterMarkerItem:
	case TempoMarkerItem:
		
		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*timebar_cursor);
		}

		break;

	case FadeInHandleItem:
	case FadeOutHandleItem:
		rv = static_cast<RegionView*>(item->get_data ("regionview"));
		{
			ArdourCanvas::SimpleRect *rect = dynamic_cast<ArdourCanvas::SimpleRect *> (item);
			if (rect) {
				rect->property_fill_color_rgba() = rv->get_fill_color();
				rect->property_outline_pixels() = 0;
			}
		}
		break;

	case AutomationTrackItem:
		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*current_canvas_cursor);
			clear_entered_track = true;
			Glib::signal_idle().connect (mem_fun(*this, &Editor::left_automation_track));
		}
		break;
		
	default:
		break;
	}

	return false;
}

gint
Editor::left_automation_track ()
{
	if (clear_entered_track) {
		set_entered_track (0);
		clear_entered_track = false;
	}
	return false;
}

void
Editor::scrub ()
{
	double delta;
	
	if (scrubbing_direction == 0) {
		/* first move */
		session->request_locate (drag_info.current_pointer_frame, false);
		session->request_transport_speed (0.1);
		scrubbing_direction = 1;
		
	} else {
		
		if (last_scrub_x > drag_info.current_pointer_x) {
			
			/* pointer moved to the left */
			
			if (scrubbing_direction > 0) {
				
				/* we reversed direction to go backwards */
				
				scrub_reversals++;
				scrub_reverse_distance += (int) (last_scrub_x - drag_info.current_pointer_x);
				
			} else {
				
				/* still moving to the left (backwards) */
				
				scrub_reversals = 0;
				scrub_reverse_distance = 0;
				
				delta = 0.01 * (last_scrub_x - drag_info.current_pointer_x);
				session->request_transport_speed (session->transport_speed() - delta);
			}
			
		} else {
			/* pointer moved to the right */
			
			if (scrubbing_direction < 0) {
				/* we reversed direction to go forward */
				
				scrub_reversals++;
				scrub_reverse_distance += (int) (drag_info.current_pointer_x - last_scrub_x);
				
			} else {
				/* still moving to the right */
				
				scrub_reversals = 0;
				scrub_reverse_distance = 0;
				
				delta = 0.01 * (drag_info.current_pointer_x - last_scrub_x);
				session->request_transport_speed (session->transport_speed() + delta);
			}
		}
		
		/* if there have been more than 2 opposite motion moves detected, or one that moves
		   back more than 10 pixels, reverse direction
		*/
		
		if (scrub_reversals >= 2 || scrub_reverse_distance > 10) {
			
			if (scrubbing_direction > 0) {
				/* was forwards, go backwards */
				session->request_transport_speed (-0.1);
				scrubbing_direction = -1;
			} else {
				/* was backwards, go forwards */
				session->request_transport_speed (0.1);
				scrubbing_direction = 1;
			}
			
			scrub_reverse_distance = 0;
			scrub_reversals = 0;
		}
	}
	
	last_scrub_x = drag_info.current_pointer_x;
}

bool
Editor::motion_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type, bool from_autoscroll)
{
	if (event->motion.is_hint) {
		gint x, y;
		
		/* We call this so that MOTION_NOTIFY events continue to be
		   delivered to the canvas. We need to do this because we set
		   Gdk::POINTER_MOTION_HINT_MASK on the canvas. This reduces
		   the density of the events, at the expense of a round-trip
		   to the server. Given that this will mostly occur on cases
		   where DISPLAY = :0.0, and given the cost of what the motion
		   event might do, its a good tradeoff.  
		*/

		track_canvas->get_pointer (x, y);
	} 

	if (current_stepping_trackview) {
		/* don't keep the persistent stepped trackview if the mouse moves */
		current_stepping_trackview = 0;
		step_timeout.disconnect ();
	}

	if (session && session->actively_recording()) {
		/* Sorry. no dragging stuff around while we record */
		return true;
	}

	drag_info.item_type = item_type;
	drag_info.last_pointer_x = drag_info.current_pointer_x;
	drag_info.last_pointer_y = drag_info.current_pointer_y;
	drag_info.current_pointer_frame = event_frame (event, &drag_info.current_pointer_x,
						       &drag_info.current_pointer_y);

	
	switch (mouse_mode) {
	case MouseAudition:
		if (_scrubbing) {
			scrub ();
		}
		break;

	default:
		break;
	}

	if (!from_autoscroll && drag_info.item) {
		/* item != 0 is the best test i can think of for dragging.
		*/
		if (!drag_info.move_threshold_passed) {

			bool x_threshold_passed =  (::llabs ((nframes64_t) (drag_info.current_pointer_x - drag_info.grab_x)) > 4LL);
			bool y_threshold_passed =  (::llabs ((nframes64_t) (drag_info.current_pointer_y - drag_info.grab_y)) > 4LL);
			
			drag_info.move_threshold_passed = (x_threshold_passed || y_threshold_passed);
			
			// and change the initial grab loc/frame if this drag info wants us to

			if (drag_info.want_move_threshold && drag_info.move_threshold_passed) {
				drag_info.grab_frame = drag_info.current_pointer_frame;
				drag_info.grab_x = drag_info.current_pointer_x;
				drag_info.grab_y = drag_info.current_pointer_y;
				drag_info.last_pointer_frame = drag_info.grab_frame;
				drag_info.pointer_frame_offset = drag_info.grab_frame - drag_info.last_frame_position;
			}
		}
	}

	switch (item_type) {
	case PlayheadCursorItem:
	case MarkerItem:
	case MarkerBarItem:
	case TempoBarItem:
	case MeterBarItem:
	case RangeMarkerBarItem:
	case TransportMarkerBarItem:
	case CdMarkerBarItem:
	case GainControlPointItem:
	case RedirectAutomationControlPointItem:
	case GainAutomationControlPointItem:
	case PanAutomationControlPointItem:
	case TempoMarkerItem:
	case MeterMarkerItem:
	case RegionViewNameHighlight:
	case StartSelectionTrimItem:
	case EndSelectionTrimItem:
	case SelectionItem:
	case GainLineItem:
	case RedirectAutomationLineItem:
	case GainAutomationLineItem:
	case PanAutomationLineItem:
	case FadeInHandleItem:
	case FadeOutHandleItem:
	/* <CMT Additions> */
	case ImageFrameHandleStartItem:
	case ImageFrameHandleEndItem:
	case MarkerViewHandleStartItem:
	case MarkerViewHandleEndItem:
	/* </CMT Additions> */
	  if (drag_info.item && (event->motion.state & Gdk::BUTTON1_MASK ||
				 (event->motion.state & Gdk::BUTTON3_MASK) ||
				 (event->motion.state & Gdk::BUTTON2_MASK))) {
		  if (!from_autoscroll) {
			  maybe_autoscroll_horizontally (&event->motion);
		  }
		  if (drag_info.motion_callback) {
			  (this->*(drag_info.motion_callback)) (item, event);
		  }
		  goto handled;
	  }
	  goto not_handled;
	  break;
	default:
		break;
	}

	switch (mouse_mode) {
	case MouseGain:
		if (item_type == RegionItem) {
			if (drag_info.item && drag_info.motion_callback) {
				(this->*(drag_info.motion_callback)) (item, event);
			}
			goto handled;
		}
		break;

	case MouseObject:
	case MouseRange:
	case MouseZoom:
	case MouseTimeFX:
		if (drag_info.item && (event->motion.state & GDK_BUTTON1_MASK ||
				       (event->motion.state & GDK_BUTTON2_MASK))) {
			if (!from_autoscroll) {
				maybe_autoscroll (&event->motion);
			}
			if (drag_info.motion_callback) {
				(this->*(drag_info.motion_callback)) (item, event);
			}
			goto handled;
		}
		goto not_handled;
		break;

	default:
		break;
	}

  handled:
	track_canvas_motion (event);
	// drag_info.last_pointer_frame = drag_info.current_pointer_frame;
	return true;
	
  not_handled:
	return false;
}

void
Editor::break_drag ()
{
	stop_canvas_autoscroll ();
	hide_verbose_canvas_cursor ();

	if (drag_info.item) {
		drag_info.item->ungrab (0);

		/* put it back where it came from */

		double cxw, cyw;
		cxw = 0;
		cyw = 0;
		drag_info.item->i2w (cxw, cyw);
		drag_info.item->move (drag_info.original_x - cxw, drag_info.original_y - cyw);
	}

	finalize_drag ();
}

void
Editor::finalize_drag ()
{
	drag_info.item = 0;
	drag_info.copy = false;
	drag_info.motion_callback = 0;
	drag_info.finished_callback = 0;
	drag_info.dest_trackview = 0;
	drag_info.source_trackview = 0;
	drag_info.last_frame_position = 0;
	drag_info.grab_frame = 0;
	drag_info.last_pointer_frame = 0;
	drag_info.current_pointer_frame = 0;
	drag_info.brushing = false;
	range_marker_drag_rect->hide();
	drag_info.clear_copied_locations ();
}

void
Editor::start_grab (GdkEvent* event, Gdk::Cursor *cursor)
{
	if (drag_info.item == 0) {
		fatal << _("programming error: start_grab called without drag item") << endmsg;
		/*NOTREACHED*/
		return;
	}

	if (cursor == 0) {
		cursor = which_grabber_cursor ();
	}

        // if dragging with button2, the motion is x constrained, with Alt-button2 it is y constrained

	if (Keyboard::is_button2_event (&event->button)) {
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::SecondaryModifier)) {
			drag_info.y_constrained = true;
			drag_info.x_constrained = false;
		} else {
			drag_info.y_constrained = false;
			drag_info.x_constrained = true;
		}
	} else {
		drag_info.x_constrained = false;
		drag_info.y_constrained = false;
	}

	drag_info.grab_frame = event_frame (event, &drag_info.grab_x, &drag_info.grab_y);
	drag_info.last_pointer_frame = drag_info.grab_frame;
	drag_info.current_pointer_frame = drag_info.grab_frame;
	drag_info.current_pointer_x = drag_info.grab_x;
	drag_info.current_pointer_y = drag_info.grab_y;
	drag_info.last_pointer_x = drag_info.current_pointer_x;
	drag_info.last_pointer_y = drag_info.current_pointer_y;
	drag_info.cumulative_x_drag = 0;
	drag_info.cumulative_y_drag = 0;
	drag_info.first_move = true;
	drag_info.move_threshold_passed = false;
	drag_info.want_move_threshold = false;
	drag_info.pointer_frame_offset = 0;
	drag_info.brushing = false;
	drag_info.clear_copied_locations ();

	drag_info.original_x = 0;
	drag_info.original_y = 0;
	drag_info.item->i2w (drag_info.original_x, drag_info.original_y);

	drag_info.item->grab (Gdk::POINTER_MOTION_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK,
			      *cursor,
			      event->button.time);

	if (session && session->transport_rolling()) {
		drag_info.was_rolling = true;
	} else {
		drag_info.was_rolling = false;
	}

	switch (snap_type) {
	case SnapToRegionStart:
	case SnapToRegionEnd:
	case SnapToRegionSync:
	case SnapToRegionBoundary:
		build_region_boundary_cache ();
		break;
	default:
		break;
	}
}

void
Editor::swap_grab (ArdourCanvas::Item* new_item, Gdk::Cursor* cursor, uint32_t time)
{
	drag_info.item->ungrab (0);
	drag_info.item = new_item;

	if (cursor == 0) {
		cursor = which_grabber_cursor ();
	}

	drag_info.item->grab (Gdk::POINTER_MOTION_MASK|Gdk::BUTTON_PRESS_MASK|Gdk::BUTTON_RELEASE_MASK, *cursor, time);
}

bool
Editor::end_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	bool did_drag = false;

	stop_canvas_autoscroll ();

	if (drag_info.item == 0) {
		return false;
	}
	
	drag_info.item->ungrab (event ? event->button.time : 0);

	if (drag_info.finished_callback && event) {
		drag_info.last_pointer_x = drag_info.current_pointer_x;
		drag_info.last_pointer_y = drag_info.current_pointer_y;
		(this->*(drag_info.finished_callback)) (item, event);
	}

	did_drag = !drag_info.first_move;

	hide_verbose_canvas_cursor();

	finalize_drag ();

	return did_drag;
}

void
Editor::region_gain_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (drag_info.first_move && drag_info.move_threshold_passed) {
		drag_info.first_move = false;
	}
}

void
Editor::start_fade_in_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	drag_info.item = item;
	drag_info.motion_callback = &Editor::fade_in_drag_motion_callback;
	drag_info.finished_callback = &Editor::fade_in_drag_finished_callback;

	start_grab (event);

	if ((drag_info.data = (item->get_data ("regionview"))) == 0) {
		fatal << _("programming error: fade in canvas item has no regionview data pointer!") << endmsg;
		/*NOTREACHED*/
	}

	AudioRegionView* arv = static_cast<AudioRegionView*>(drag_info.data);

	drag_info.pointer_frame_offset = drag_info.grab_frame - ((nframes64_t) arv->audio_region()->fade_in().back()->when + arv->region()->position());	
}

void
Editor::fade_in_drag_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	AudioRegionView* arv = static_cast<AudioRegionView*>(drag_info.data);
	nframes64_t pos;
	nframes64_t fade_length;

	if (drag_info.current_pointer_frame > drag_info.pointer_frame_offset) {
		pos = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;
	}
	else {
		pos = 0;
	}

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (pos);
	}

	if (pos < (arv->region()->position() + 64)) {
		fade_length = 64; // this should be a minimum defined somewhere
	} else if (pos > arv->region()->last_frame()) {
		fade_length = arv->region()->length();
	} else {
		fade_length = pos - arv->region()->position();
	}		
	/* mapover the region selection */

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*i);
		
		if (!tmp) {
			continue;
		}
	
		tmp->reset_fade_in_shape_width (fade_length);
	}

	show_verbose_duration_cursor (arv->region()->position(),  arv->region()->position() + fade_length, 10);

	drag_info.first_move = false;
}

void
Editor::fade_in_drag_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	AudioRegionView* arv = static_cast<AudioRegionView*>(drag_info.data);
	nframes64_t pos;
	nframes64_t fade_length;

	if (drag_info.first_move) return;

	if (drag_info.current_pointer_frame > drag_info.pointer_frame_offset) {
		pos = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;
	} else {
		pos = 0;
	}

	if (pos < (arv->region()->position() + 64)) {
		fade_length = 64; // this should be a minimum defined somewhere
	} else if (pos > arv->region()->last_frame()) {
		fade_length = arv->region()->length();
	} else {
		fade_length = pos - arv->region()->position();
	}
		
	begin_reversible_command (_("change fade in length"));

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*i);
		
		if (!tmp) {
			continue;
		}
	
		AutomationList& alist = tmp->audio_region()->fade_in();
		XMLNode &before = alist.get_state();

		tmp->audio_region()->set_fade_in_length (fade_length);
		tmp->audio_region()->set_fade_in_active (true);
		
		XMLNode &after = alist.get_state();
		session->add_command(new MementoCommand<AutomationList>(alist, &before, &after));
	}

	commit_reversible_command ();
}

void
Editor::start_fade_out_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	drag_info.item = item;
	drag_info.motion_callback = &Editor::fade_out_drag_motion_callback;
	drag_info.finished_callback = &Editor::fade_out_drag_finished_callback;

	start_grab (event);

	if ((drag_info.data = (item->get_data ("regionview"))) == 0) {
		fatal << _("programming error: fade out canvas item has no regionview data pointer!") << endmsg;
		/*NOTREACHED*/
	}

	AudioRegionView* arv = static_cast<AudioRegionView*>(drag_info.data);

	drag_info.pointer_frame_offset = drag_info.grab_frame - (arv->region()->length() - (nframes64_t) arv->audio_region()->fade_out().back()->when + arv->region()->position());	
}

void
Editor::fade_out_drag_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	AudioRegionView* arv = static_cast<AudioRegionView*>(drag_info.data);
	nframes64_t pos;
	nframes64_t fade_length;

	if (drag_info.current_pointer_frame > drag_info.pointer_frame_offset) {
		pos = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;
	} else {
		pos = 0;
	}

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (pos);
	}
	
	if (pos > (arv->region()->last_frame() - 64)) {
		fade_length = 64; // this should really be a minimum fade defined somewhere
	}
	else if (pos < arv->region()->position()) {
		fade_length = arv->region()->length();
	}
	else {
		fade_length = arv->region()->last_frame() - pos;
	}
		
	/* mapover the region selection */

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*i);
		
		if (!tmp) {
			continue;
		}
	
		tmp->reset_fade_out_shape_width (fade_length);
	}

	show_verbose_duration_cursor (arv->region()->last_frame() - fade_length, arv->region()->last_frame(), 10);

	drag_info.first_move = false;
}

void
Editor::fade_out_drag_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (drag_info.first_move) return;

	AudioRegionView* arv = static_cast<AudioRegionView*>(drag_info.data);
	nframes64_t pos;
	nframes64_t fade_length;

	if (drag_info.current_pointer_frame > drag_info.pointer_frame_offset) {
		pos = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;
	}
	else {
		pos = 0;
	}

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (pos);
	}

	if (pos > (arv->region()->last_frame() - 64)) {
		fade_length = 64; // this should really be a minimum fade defined somewhere
	}
	else if (pos < arv->region()->position()) {
		fade_length = arv->region()->length();
	}
	else {
		fade_length = arv->region()->last_frame() - pos;
	}

	begin_reversible_command (_("change fade out length"));

	for (RegionSelection::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {

		AudioRegionView* tmp = dynamic_cast<AudioRegionView*> (*i);
		
		if (!tmp) {
			continue;
		}
	
		AutomationList& alist = tmp->audio_region()->fade_out();
		XMLNode &before = alist.get_state();
		
		tmp->audio_region()->set_fade_out_length (fade_length);
		tmp->audio_region()->set_fade_out_active (true);

		XMLNode &after = alist.get_state();
		session->add_command(new MementoCommand<AutomationList>(alist, &before, &after));
	}

	commit_reversible_command ();
}

void
Editor::start_cursor_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	drag_info.item = item;
	drag_info.motion_callback = &Editor::cursor_drag_motion_callback;
	drag_info.finished_callback = &Editor::cursor_drag_finished_callback;

	start_grab (event);

	if ((drag_info.data = (item->get_data ("cursor"))) == 0) {
		fatal << _("programming error: cursor canvas item has no cursor data pointer!") << endmsg;
		/*NOTREACHED*/
	}

	Cursor* cursor = (Cursor *) drag_info.data;

	if (cursor == playhead_cursor) {
		_dragging_playhead = true;
		
		if (session && drag_info.was_rolling) {
			session->request_stop (false, true);
		}

		if (session && session->is_auditioning()) {
			session->cancel_audition ();
		}
	}

	drag_info.pointer_frame_offset = drag_info.grab_frame - cursor->current_frame;	
	
	show_verbose_time_cursor (cursor->current_frame, 10);
}

void
Editor::start_cursor_grab_no_stop (ArdourCanvas::Item* item, GdkEvent* event)
{
	drag_info.item = item;
	drag_info.motion_callback = &Editor::cursor_drag_motion_callback;
	drag_info.finished_callback = &Editor::cursor_drag_finished_ensure_locate_callback;

	start_grab (event);

	if ((drag_info.data = (item->get_data ("cursor"))) == 0) {
		fatal << _("programming error: cursor canvas item has no cursor data pointer!") << endmsg;
		/*NOTREACHED*/
	}

	Cursor* cursor = (Cursor *) drag_info.data;
	nframes64_t where = event_frame (event, 0, 0);

	snap_to(where);
	playhead_cursor->set_position (where);

	if (cursor == playhead_cursor) {
		_dragging_playhead = true;

		if (session && session->is_auditioning()) {
			session->cancel_audition ();
		}
	}

	drag_info.pointer_frame_offset = drag_info.grab_frame - cursor->current_frame;	
	
	show_verbose_time_cursor (cursor->current_frame, 10);
}

void
Editor::cursor_drag_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	Cursor* cursor = (Cursor *) drag_info.data;
	nframes64_t adjusted_frame;
	
	if (drag_info.current_pointer_frame > drag_info.pointer_frame_offset) {
		adjusted_frame = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;
	}
	else {
		adjusted_frame = 0;
	}
	
	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		if (cursor == playhead_cursor) {
			snap_to (adjusted_frame);
		}
	}
	
	if (adjusted_frame == drag_info.last_pointer_frame) return;

	cursor->set_position (adjusted_frame);

	show_verbose_time_cursor (cursor->current_frame, 10);

#ifdef GTKOSX
        flush_canvas ();
#endif
	UpdateAllTransportClocks (cursor->current_frame);

	drag_info.last_pointer_frame = adjusted_frame;
	drag_info.first_move = false;
}

void
Editor::cursor_drag_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	_dragging_playhead = false;

	if (drag_info.first_move) {
		return;
	}
	
	cursor_drag_motion_callback (item, event);
	
	if (item == &playhead_cursor->canvas_item) {
		if (session) {
			session->request_locate (playhead_cursor->current_frame, drag_info.was_rolling);
		}
	} 
}

void
Editor::cursor_drag_finished_ensure_locate_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	_dragging_playhead = false;
	
	cursor_drag_motion_callback (item, event);
	
	if (item == &playhead_cursor->canvas_item) {
		if (session) {
			session->request_locate (playhead_cursor->current_frame, drag_info.was_rolling);
		}
	} 
}

void
Editor::update_marker_drag_item (Location *location)
{
	double x1 = frame_to_pixel (location->start());
	double x2 = frame_to_pixel (location->end());

	if (location->is_mark()) {
	        marker_drag_line_points.front().set_x(x1);
		marker_drag_line_points.back().set_x(x1);
		marker_drag_line->property_points() = marker_drag_line_points;
	} else {
		range_marker_drag_rect->property_x1() = x1;
		range_marker_drag_rect->property_x2() = x2;
	}
}

void
Editor::start_marker_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	Marker* marker;

	if ((marker = static_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	bool is_start;

	Location  *location = find_location_from_marker (marker, is_start);

	drag_info.item = item;
	drag_info.data = marker;
	drag_info.motion_callback = &Editor::marker_drag_motion_callback;
	drag_info.finished_callback = &Editor::marker_drag_finished_callback;

	start_grab (event);

	_dragging_edit_point = true;

	drag_info.pointer_frame_offset = drag_info.grab_frame - (is_start ? location->start() : location->end());	

	update_marker_drag_item (location);

	if (location->is_mark()) {
		// marker_drag_line->show();
		// marker_drag_line->raise_to_top();
	} else {
		range_marker_drag_rect->show();
		//range_marker_drag_rect->raise_to_top();
	}

	if (is_start) {
		show_verbose_time_cursor (location->start(), 10);
	} else {
		show_verbose_time_cursor (location->end(), 10);
	}

	Selection::Operation op = Keyboard::selection_type (event->button.state);

	switch (op) {
	case Selection::Toggle:
		selection->toggle (marker);
		break;
	case Selection::Set:
		if (!selection->selected (marker)) {
			selection->set (marker);
		}
		break;
	case Selection::Extend:
	{
		Locations::LocationList ll;
		list<Marker*> to_add;
		nframes64_t s, e;
		selection->markers.range (s, e);
		s = min (marker->position(), s);
		e = max (marker->position(), e);
		s = min (s, e);
		e = max (s, e);
		if (e < max_frames) {
			++e;
		}
		session->locations()->find_all_between (s, e, ll, Location::Flags (0));
		for (Locations::LocationList::iterator i = ll.begin(); i != ll.end(); ++i) {
			LocationMarkers* lm = find_location_markers (*i);
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
			selection->add (to_add);
		}
		break;
	}
	case Selection::Add:
		selection->add (marker);
		break;
	}

	/* set up copies for us to manipulate during the drag */

	drag_info.clear_copied_locations ();

	for (MarkerSelection::iterator i = selection->markers.begin(); i != selection->markers.end(); ++i) {
		Location  *l = find_location_from_marker (*i, is_start);
		drag_info.copied_locations.push_back (new Location (*l));
	}
}

void
Editor::marker_drag_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	nframes64_t f_delta = 0;
	nframes64_t newframe;
	bool is_start;
	bool move_both = false;
	Marker* dragged_marker = (Marker*) drag_info.data;
	Marker* marker;
	Location  *real_location;
	Location  *copy_location;

	if (drag_info.pointer_frame_offset <= drag_info.current_pointer_frame) {
		newframe = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;
	} else {
		newframe = 0;
	}

	nframes64_t next = newframe;

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (newframe, 0, true);
	}
	
	if (drag_info.current_pointer_frame == drag_info.last_pointer_frame) { 
		return;
	}

	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
		move_both = true;
	}

	MarkerSelection::iterator i;
	list<Location*>::iterator x;

	/* find the marker we're dragging, and compute the delta */

	for (i = selection->markers.begin(), x = drag_info.copied_locations.begin(); 
	     x != drag_info.copied_locations.end() && i != selection->markers.end(); 
	     ++i, ++x) {

		copy_location = *x;
		marker = *i;

		if (marker == dragged_marker) {

			if ((real_location = find_location_from_marker (marker, is_start)) == 0) {
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

	if (i == selection->markers.end()) {
		/* hmm, impossible - we didn't find the dragged marker */
		return;
	}

	/* now move them all */

	for (i = selection->markers.begin(), x = drag_info.copied_locations.begin(); 
	     x != drag_info.copied_locations.end() && i != selection->markers.end(); 
	     ++i, ++x) {

		copy_location = *x;
		marker = *i;

		/* call this to find out if its the start or end */
		
		if ((real_location = find_location_from_marker (marker, is_start)) == 0) {
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
					snap_to (next, 1, true);
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
					snap_to (next, -1, true);
					copy_location->set_start (next);
					copy_location->set_end (newframe);
				}
			}
		}
		update_marker_drag_item (copy_location);

		LocationMarkers* lm = find_location_markers (real_location);

		if (lm) {
			lm->set_position (copy_location->start(), copy_location->end());
		}
	}

	drag_info.last_pointer_frame = drag_info.current_pointer_frame;
	drag_info.first_move = false;

	if (drag_info.copied_locations.empty()) {
		abort();
	}

	if (Profile->get_sae()) {
		edit_point_clock.set (drag_info.copied_locations.front()->start());
	}
	show_verbose_time_cursor (newframe, 10);

#ifdef GTKOSX
        flush_canvas ();
#endif
}

void
Editor::marker_drag_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (drag_info.first_move) {

		/* just a click, do nothing but finish
		   off the selection process
		*/

		Selection::Operation op = Keyboard::selection_type (event->button.state);
		Marker* marker = (Marker *) drag_info.data;

		switch (op) {
		case Selection::Set:
			if (selection->selected (marker) && selection->markers.size() > 1) {
				selection->set (marker);
			}
			break;

		case Selection::Toggle:
		case Selection::Extend:
		case Selection::Add:
			break;
		}
		
		return;
	}

	_dragging_edit_point = false;
	

	begin_reversible_command ( _("move marker") );
	XMLNode &before = session->locations()->get_state();

	MarkerSelection::iterator i;
	list<Location*>::iterator x;
	bool is_start;

	for (i = selection->markers.begin(), x = drag_info.copied_locations.begin(); 
	     x != drag_info.copied_locations.end() && i != selection->markers.end(); 
	     ++i, ++x) {
	
		Location * location = find_location_from_marker ((*i), is_start);
		
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

	XMLNode &after = session->locations()->get_state();
	session->add_command(new MementoCommand<Locations>(*(session->locations()), &before, &after));
	commit_reversible_command ();
	
	marker_drag_line->hide();
	range_marker_drag_rect->hide();
}

void
Editor::start_meter_marker_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	Marker* marker;
	MeterMarker* meter_marker;

	if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: meter marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	meter_marker = dynamic_cast<MeterMarker*> (marker);

	MetricSection& section (meter_marker->meter());

	if (!section.movable()) {
		return;
	}

	drag_info.item = item;
	drag_info.copy = false;
	drag_info.data = marker;
	drag_info.motion_callback = &Editor::meter_marker_drag_motion_callback;
	drag_info.finished_callback = &Editor::meter_marker_drag_finished_callback;

	start_grab (event);

	drag_info.pointer_frame_offset = drag_info.grab_frame - meter_marker->meter().frame();	

	show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
}

void
Editor::start_meter_marker_copy_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	Marker* marker;
	MeterMarker* meter_marker;

	if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: meter marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	meter_marker = dynamic_cast<MeterMarker*> (marker);
	
	// create a dummy marker for visual representation of moving the copy.
	// The actual copying is not done before we reach the finish callback.
	char name[64];
	snprintf (name, sizeof(name), "%g/%g", meter_marker->meter().beats_per_bar(), meter_marker->meter().note_divisor ());
	MeterMarker* new_marker = new MeterMarker(*this, *meter_group, ARDOUR_UI::config()->canvasvar_MeterMarker.get(), name, 
						  *new MeterSection(meter_marker->meter()));

	drag_info.item = &new_marker->the_item();
	drag_info.copy = true;
	drag_info.data = new_marker;
	drag_info.motion_callback = &Editor::meter_marker_drag_motion_callback;
	drag_info.finished_callback = &Editor::meter_marker_drag_finished_callback;

	start_grab (event);

	drag_info.pointer_frame_offset = drag_info.grab_frame - meter_marker->meter().frame();	

	show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
}

void
Editor::meter_marker_drag_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	MeterMarker* marker = (MeterMarker *) drag_info.data;
	nframes64_t adjusted_frame;

	if (drag_info.current_pointer_frame > drag_info.pointer_frame_offset) {
		adjusted_frame = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;
	}
	else {
		adjusted_frame = 0;
	}
	
	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (adjusted_frame);
	}
	
	if (adjusted_frame == drag_info.last_pointer_frame) return;

	marker->set_position (adjusted_frame);
	
	
	drag_info.last_pointer_frame = adjusted_frame;
	drag_info.first_move = false;

	show_verbose_time_cursor (adjusted_frame, 10);
}

void
Editor::meter_marker_drag_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (drag_info.first_move) return;

	meter_marker_drag_motion_callback (drag_info.item, event);
	
	MeterMarker* marker = (MeterMarker *) drag_info.data;
	BBT_Time when;
	
	TempoMap& map (session->tempo_map());
	map.bbt_time (drag_info.last_pointer_frame, when);
	
	if (drag_info.copy == true) {
		begin_reversible_command (_("copy meter mark"));
                XMLNode &before = map.get_state();
		map.add_meter (marker->meter(), when);
		XMLNode &after = map.get_state();
                session->add_command(new MementoCommand<TempoMap>(map, &before, &after));
		commit_reversible_command ();
		
		// delete the dummy marker we used for visual representation of copying.
		// a new visual marker will show up automatically.
		delete marker;
	} else {
		begin_reversible_command (_("move meter mark"));
		XMLNode &before = map.get_state();
		map.move_meter (marker->meter(), when);
		XMLNode &after = map.get_state();
                session->add_command(new MementoCommand<TempoMap>(map, &before, &after));
		commit_reversible_command ();
	}
}

void
Editor::start_tempo_marker_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	Marker* marker;
	TempoMarker* tempo_marker;

	if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: tempo marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((tempo_marker = dynamic_cast<TempoMarker *> (marker)) == 0) {
		fatal << _("programming error: marker for tempo is not a tempo marker!") << endmsg;
		/*NOTREACHED*/
	}

	MetricSection& section (tempo_marker->tempo());

	if (!section.movable()) {
		return;
	}

	drag_info.item = item;
	drag_info.copy = false;
	drag_info.data = marker;
	drag_info.motion_callback = &Editor::tempo_marker_drag_motion_callback;
	drag_info.finished_callback = &Editor::tempo_marker_drag_finished_callback;

	start_grab (event);

	drag_info.pointer_frame_offset = drag_info.grab_frame - tempo_marker->tempo().frame();	
	show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
}

void
Editor::start_tempo_marker_copy_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	Marker* marker;
	TempoMarker* tempo_marker;

	if ((marker = reinterpret_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: tempo marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	if ((tempo_marker = dynamic_cast<TempoMarker *> (marker)) == 0) {
		fatal << _("programming error: marker for tempo is not a tempo marker!") << endmsg;
		/*NOTREACHED*/
	}

	// create a dummy marker for visual representation of moving the copy.
	// The actual copying is not done before we reach the finish callback.
	char name[64];
	snprintf (name, sizeof (name), "%.2f", tempo_marker->tempo().beats_per_minute());
	TempoMarker* new_marker = new TempoMarker(*this, *tempo_group, ARDOUR_UI::config()->canvasvar_TempoMarker.get(), name, 
						  *new TempoSection(tempo_marker->tempo()));

	drag_info.item = &new_marker->the_item();
	drag_info.copy = true;
	drag_info.data = new_marker;
	drag_info.motion_callback = &Editor::tempo_marker_drag_motion_callback;
	drag_info.finished_callback = &Editor::tempo_marker_drag_finished_callback;

	start_grab (event);

	drag_info.pointer_frame_offset = drag_info.grab_frame - tempo_marker->tempo().frame();

	show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
}

void
Editor::tempo_marker_drag_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	TempoMarker* marker = (TempoMarker *) drag_info.data;
	nframes64_t adjusted_frame;
	
	if (drag_info.current_pointer_frame > drag_info.pointer_frame_offset) {
		adjusted_frame = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;
	}
	else {
		adjusted_frame = 0;
	}

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (adjusted_frame);
	}
	
	if (adjusted_frame == drag_info.last_pointer_frame) return;

	/* OK, we've moved far enough to make it worth actually move the thing. */
		
	marker->set_position (adjusted_frame);
	
	show_verbose_time_cursor (adjusted_frame, 10);

	drag_info.last_pointer_frame = adjusted_frame;
	drag_info.first_move = false;
}

void
Editor::tempo_marker_drag_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (drag_info.first_move) return;
	
	tempo_marker_drag_motion_callback (drag_info.item, event);
	
	TempoMarker* marker = (TempoMarker *) drag_info.data;
	BBT_Time when;
	
	TempoMap& map (session->tempo_map());
	map.bbt_time (drag_info.last_pointer_frame, when);

	if (drag_info.copy == true) {
		begin_reversible_command (_("copy tempo mark"));
		XMLNode &before = map.get_state();
		map.add_tempo (marker->tempo(), when);
		XMLNode &after = map.get_state();
		session->add_command (new MementoCommand<TempoMap>(map, &before, &after));
		commit_reversible_command ();
		
		// delete the dummy marker we used for visual representation of copying.
		// a new visual marker will show up automatically.
		delete marker;
	} else {
		begin_reversible_command (_("move tempo mark"));
                XMLNode &before = map.get_state();
		map.move_tempo (marker->tempo(), when);
                XMLNode &after = map.get_state();
                session->add_command (new MementoCommand<TempoMap>(map, &before, &after));
		commit_reversible_command ();
	}
}

void
Editor::remove_gain_control_point (ArdourCanvas::Item*item, GdkEvent* event)
{
	ControlPoint* control_point;

	if ((control_point = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"))) == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	// We shouldn't remove the first or last gain point
	if (control_point->line.is_last_point(*control_point) ||
		control_point->line.is_first_point(*control_point)) {	
		return;
	}

	control_point->line.remove_point (*control_point);
}

void
Editor::remove_control_point (ArdourCanvas::Item*item, GdkEvent* event)
{
	ControlPoint* control_point;

	if ((control_point = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"))) == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	control_point->line.remove_point (*control_point);
}

void
Editor::start_control_point_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	ControlPoint* control_point;
	
	if ((control_point = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"))) == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	drag_info.item = item;
	drag_info.data = control_point;
	drag_info.motion_callback = &Editor::control_point_drag_motion_callback;
	drag_info.finished_callback = &Editor::control_point_drag_finished_callback;

	start_grab (event, fader_cursor);

	// start the grab at the center of the control point so
	// the point doesn't 'jump' to the mouse after the first drag
	drag_info.grab_x = control_point->get_x();
	drag_info.grab_y = control_point->get_y();
	control_point->line.parent_group().i2w(drag_info.grab_x, drag_info.grab_y);
	track_canvas->w2c(drag_info.grab_x, drag_info.grab_y, drag_info.grab_x, drag_info.grab_y);

	drag_info.grab_frame = pixel_to_frame(drag_info.grab_x);

	control_point->line.start_drag (control_point, drag_info.grab_frame, 0);

	float fraction = 1.0 - (control_point->get_y() / control_point->line.height());
	set_verbose_canvas_cursor (control_point->line.get_verbose_cursor_string (fraction), 
				   drag_info.current_pointer_x + 10, drag_info.current_pointer_y + 10);

	show_verbose_canvas_cursor ();
}

void
Editor::control_point_drag_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	ControlPoint* cp = reinterpret_cast<ControlPoint *> (drag_info.data);

	double dx = drag_info.current_pointer_x - drag_info.last_pointer_x;
	double dy = drag_info.current_pointer_y - drag_info.last_pointer_y;

	if (event->button.state & Keyboard::SecondaryModifier) {
		dx *= 0.1;
		dy *= 0.1;
	}

	double cx = drag_info.grab_x + drag_info.cumulative_x_drag + dx;
	double cy = drag_info.grab_y + drag_info.cumulative_y_drag + dy;

	// calculate zero crossing point. back off by .01 to stay on the
	// positive side of zero
	double _unused = 0;
	double zero_gain_y = (1.0 - ZERO_GAIN_FRACTION) * cp->line.height() - .01;
	cp->line.parent_group().i2w(_unused, zero_gain_y);

	// make sure we hit zero when passing through
	if ((cy < zero_gain_y and (cy - dy) > zero_gain_y)
			or (cy > zero_gain_y and (cy - dy) < zero_gain_y)) {
		cy = zero_gain_y;
	}

	if (drag_info.x_constrained) {
		cx = drag_info.grab_x;
	}
	if (drag_info.y_constrained) {
		cy = drag_info.grab_y;
	}

	drag_info.cumulative_x_drag = cx - drag_info.grab_x;
	drag_info.cumulative_y_drag = cy - drag_info.grab_y;

	cp->line.parent_group().w2i (cx, cy);

	cx = max (0.0, cx);
	cy = max (0.0, cy);
	cy = min ((double) cp->line.height(), cy);

	//translate cx to frames
	nframes64_t cx_frames = unit_to_frame (cx);

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier()) && !drag_info.x_constrained) {
		snap_to (cx_frames);
	}

	float fraction = 1.0 - (cy / cp->line.height());

	bool push;

	if (Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier)) {
		push = true;
	} else {
		push = false;
	}

	cp->line.point_drag (*cp, cx_frames , fraction, push);
	
	set_verbose_canvas_cursor_text (cp->line.get_verbose_cursor_string (fraction));

	drag_info.first_move = false;
}

void
Editor::control_point_drag_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	ControlPoint* cp = reinterpret_cast<ControlPoint *> (drag_info.data);

	if (drag_info.first_move) {

		/* just a click */
		
		if ((event->type == GDK_BUTTON_RELEASE) && (event->button.button == 1) && Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
			reset_point_selection ();
		}

	} else {
		control_point_drag_motion_callback (item, event);
	}
	cp->line.end_drag (cp);
}

void
Editor::start_line_grab_from_regionview (ArdourCanvas::Item* item, GdkEvent* event)
{
	switch (mouse_mode) {
	case MouseGain:
		assert(dynamic_cast<AudioRegionView*>(clicked_regionview));
		start_line_grab (dynamic_cast<AudioRegionView*>(clicked_regionview)->get_gain_line(), event);
		break;
	default:
		break;
	}
}

void
Editor::start_line_grab_from_line (ArdourCanvas::Item* item, GdkEvent* event)
{
	AutomationLine* al;
	
	if ((al = reinterpret_cast<AutomationLine*> (item->get_data ("line"))) == 0) {
		fatal << _("programming error: line canvas item has no line pointer!") << endmsg;
		/*NOTREACHED*/
	}

	start_line_grab (al, event);
}

void
Editor::start_line_grab (AutomationLine* line, GdkEvent* event)
{
	double cx;
	double cy;
	nframes64_t frame_within_region;

	/* need to get x coordinate in terms of parent (TimeAxisItemView)
	   origin, and ditto for y.
	*/

	cx = event->button.x;
	cy = event->button.y;

	line->parent_group().w2i (cx, cy);

	frame_within_region = (nframes64_t) floor (cx * frames_per_unit);

	if (!line->control_points_adjacent (frame_within_region, current_line_drag_info.before, 
					    current_line_drag_info.after)) {
		/* no adjacent points */
		return;
	}

	drag_info.item = &line->grab_item();
	drag_info.data = line;
	drag_info.motion_callback = &Editor::line_drag_motion_callback;
	drag_info.finished_callback = &Editor::line_drag_finished_callback;

	start_grab (event, fader_cursor);

	/* store grab start in parent frame */

	drag_info.grab_x = cx;
	drag_info.grab_y = cy;

	double fraction = 1.0 - (cy / line->height());

	line->start_drag (0, drag_info.grab_frame, fraction);
	
	set_verbose_canvas_cursor (line->get_verbose_cursor_string (fraction),
				   drag_info.current_pointer_x + 10, drag_info.current_pointer_y + 10);
	show_verbose_canvas_cursor ();
}

void
Editor::line_drag_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	AutomationLine* line = reinterpret_cast<AutomationLine *> (drag_info.data);

	double dy = drag_info.current_pointer_y - drag_info.last_pointer_y;
	
	if (event->button.state & Keyboard::SecondaryModifier) {
		dy *= 0.1;
	}

	double cy = drag_info.grab_y + drag_info.cumulative_y_drag + dy;

	drag_info.cumulative_y_drag = cy - drag_info.grab_y;

	cy = max (0.0, cy);
	cy = min ((double) line->height(), cy);

	double fraction = 1.0 - (cy / line->height());

	bool push;

	if (Keyboard::modifier_state_contains (event->button.state, Keyboard::PrimaryModifier)) {
		push = false;
	} else {
		push = true;
	}

	line->line_drag (current_line_drag_info.before, current_line_drag_info.after, fraction, push);
	
	set_verbose_canvas_cursor_text (line->get_verbose_cursor_string (fraction));
}

void
Editor::line_drag_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	AutomationLine* line = reinterpret_cast<AutomationLine *> (drag_info.data);
	line_drag_motion_callback (item, event);
	line->end_drag (0);
}

void
Editor::start_region_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (selection->regions.empty() || clicked_regionview == 0) {
		return;
	}

	drag_info.copy = false;
	drag_info.item = item;
	drag_info.data = clicked_regionview;

	if (Config->get_edit_mode() == Splice) {
		drag_info.motion_callback = &Editor::region_drag_splice_motion_callback;
		drag_info.finished_callback = &Editor::region_drag_splice_finished_callback;
	} else {
		drag_info.motion_callback = &Editor::region_drag_motion_callback;
		drag_info.finished_callback = &Editor::region_drag_finished_callback;
	}

	start_grab (event);

	double speed = 1.0;
	TimeAxisView* tvp = clicked_trackview;
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);

	if (tv && tv->is_audio_track()) {
		speed = tv->get_diskstream()->speed();
	}
	
	drag_info.last_frame_position = (nframes64_t) (clicked_regionview->region()->position() / speed);
	drag_info.pointer_frame_offset = drag_info.grab_frame - drag_info.last_frame_position;
 	drag_info.source_trackview = &clicked_regionview->get_time_axis_view();
	drag_info.dest_trackview = drag_info.source_trackview;
	// we want a move threshold
	drag_info.want_move_threshold = true;
	
	show_verbose_time_cursor (drag_info.last_frame_position, 10);

	begin_reversible_command (_("move region(s)"));

	_region_motion_group->raise_to_top ();

	/* sync the canvas to what we think is its current state */
        flush_canvas ();
}

void
Editor::start_region_copy_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (selection->regions.empty() || clicked_regionview == 0) {
		return;
	}

	drag_info.copy = true;
	drag_info.item = item;
	drag_info.data = clicked_regionview;	

	start_grab(event);

	TimeAxisView* tv = &clicked_regionview->get_time_axis_view();
	RouteTimeAxisView* atv = dynamic_cast<RouteTimeAxisView*>(tv);
	double speed = 1.0;

	if (atv && atv->is_audio_track()) {
		speed = atv->get_diskstream()->speed();
	}
	
	drag_info.source_trackview = &clicked_regionview->get_time_axis_view();
	drag_info.dest_trackview = drag_info.source_trackview;
	drag_info.last_frame_position = (nframes64_t) (clicked_regionview->region()->position() / speed);
	drag_info.pointer_frame_offset = drag_info.grab_frame - drag_info.last_frame_position;
	// we want a move threshold
	drag_info.want_move_threshold = true;
	drag_info.motion_callback = &Editor::region_drag_motion_callback;
	drag_info.finished_callback = &Editor::region_drag_finished_callback;
	show_verbose_time_cursor (drag_info.last_frame_position, 10);
	_region_motion_group->raise_to_top ();
}

void
Editor::start_region_brush_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (selection->regions.empty() || clicked_regionview == 0 || Config->get_edit_mode() == Splice) {
		return;
	}

	drag_info.copy = false;
	drag_info.item = item;
	drag_info.data = clicked_regionview;
	drag_info.motion_callback = &Editor::region_drag_motion_callback;
	drag_info.finished_callback = &Editor::region_drag_finished_callback;

	start_grab (event);

	double speed = 1.0;
	TimeAxisView* tvp = clicked_trackview;
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);

	if (tv && tv->is_audio_track()) {
		speed = tv->get_diskstream()->speed();
	}
	
	drag_info.last_frame_position = (nframes64_t) (clicked_regionview->region()->position() / speed);
	drag_info.pointer_frame_offset = drag_info.grab_frame - drag_info.last_frame_position;
	drag_info.source_trackview = &clicked_regionview->get_time_axis_view();
	drag_info.dest_trackview = drag_info.source_trackview;
	// we want a move threshold
	drag_info.want_move_threshold = true;
	drag_info.brushing = true;
	
	begin_reversible_command (_("Drag region brush"));
}

void
Editor::possibly_copy_regions_during_grab (GdkEvent* event)
{
	if (drag_info.copy && drag_info.move_threshold_passed && drag_info.want_move_threshold) {

		drag_info.want_move_threshold = false; // don't copy again

		/* duplicate the regionview(s) and region(s) */

		vector<RegionView*> new_regionviews;
		
		for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin(); i != selection->regions.by_layer().end(); ++i) {
			RegionView* nrv;
			AudioRegionView* arv;
			
			if ((arv = dynamic_cast<AudioRegionView*>(*i)) == 0) {
				/* XXX handle MIDI here */
				continue;
			}

			const boost::shared_ptr<const Region> original = arv->region();
			boost::shared_ptr<Region> region_copy = RegionFactory::create (original);
			boost::shared_ptr<AudioRegion> ar = boost::dynamic_pointer_cast<AudioRegion> (region_copy);

			nrv = new AudioRegionView (*arv, ar);
			nrv->get_canvas_group()->show ();

			new_regionviews.push_back (nrv);
		}

		if (new_regionviews.empty()) {
			return;
		}

		/* reset selection to new regionviews. This will not set selection visual status for 
		   these regionviews since they don't belong to a track, so do that by hand too.
		 */
		
		selection->set (new_regionviews);

		for (vector<RegionView*>::iterator i = new_regionviews.begin(); i != new_regionviews.end(); ++i) {
			(*i)->set_selected (true);
		}

		/* reset drag_info data to reflect the fact that we are dragging the copies */
		
		drag_info.data = new_regionviews.front();

		swap_grab (new_regionviews.front()->get_canvas_group (), 0, event->motion.time);
		/* 
		   sync the canvas to what we think is its current state 
		   without it, the canvas seems to 
		   "forget" to update properly after the upcoming reparent() 
		   ..only if the mouse is in rapid motion at the time of the grab. 
		   something to do with regionview creation raking so long?
		 */
                flush_canvas ();
	}
}

bool
Editor::check_region_drag_possible (AudioTimeAxisView** tv)
{
	/* Which trackview is this ? */

	TimeAxisView* tvp = trackview_by_y_position (drag_info.current_pointer_y);
	(*tv) = dynamic_cast<AudioTimeAxisView*>(tvp);

	/* The region motion is only processed if the pointer is over
	   an audio track.
	*/
	
	if (!(*tv) || !(*tv)->is_audio_track()) {
		/* To make sure we hide the verbose canvas cursor when the mouse is 
		   not held over and audiotrack. 
		*/
		hide_verbose_canvas_cursor ();
		return false;
	}
	
	return true;
}

struct RegionSelectionByPosition {
    bool operator() (RegionView*a, RegionView* b) {
	    return a->region()->position () < b->region()->position();
    }
};

void
Editor::region_drag_splice_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	AudioTimeAxisView* tv;
	
	if (!check_region_drag_possible (&tv)) {
		return;
	}

	if (!drag_info.move_threshold_passed) {
		return;
	}

	int dir;

	if (drag_info.current_pointer_x - drag_info.grab_x > 0) {
		dir = 1;
	} else {
		dir = -1;
	}

	RegionSelection copy (selection->regions);

	RegionSelectionByPosition cmp;
	copy.sort (cmp);

	for (RegionSelection::iterator i = copy.begin(); i != copy.end(); ++i) {

		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*> (&(*i)->get_time_axis_view());

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
			if (drag_info.current_pointer_frame < (*i)->region()->last_frame() + 1) {
				continue;
			}
		} else {
			if (drag_info.current_pointer_frame > (*i)->region()->first_frame()) {
				continue;
			}
		}

		
		playlist->shuffle ((*i)->region(), dir);

		drag_info.grab_x = drag_info.current_pointer_x;
	}
}

void
Editor::region_drag_splice_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
}

void
Editor::region_drag_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	double x_delta;
	double y_delta = 0;
	nframes64_t pending_region_position = 0;
	int32_t pointer_y_span = 0, canvas_pointer_y_span = 0, original_pointer_order;
	int32_t visible_y_high = 0, visible_y_low = 512;  //high meaning higher numbered.. not the height on the screen
	bool clamp_y_axis = false;
	vector<int32_t>  height_list(512) ;
	vector<int32_t>::iterator j;
	AudioTimeAxisView* tv;

	possibly_copy_regions_during_grab (event);

	if (!check_region_drag_possible (&tv)) {
		return;
	}

	original_pointer_order = drag_info.dest_trackview->order;
	
	/************************************************************
                 Y-Delta Computation
	************************************************************/	

	if (drag_info.brushing) {
		clamp_y_axis = true;
		pointer_y_span = 0;
		goto y_axis_done;
	}

	if ((pointer_y_span = (drag_info.dest_trackview->order - tv->order)) != 0) {

		int32_t children = 0, numtracks = 0;
		// XXX hard coding track limit, oh my, so very very bad
		bitset <1024> tracks (0x00ul);
		/* get a bitmask representing the visible tracks */

		for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
			TimeAxisView *tracklist_timeview;
			tracklist_timeview = (*i);
			AudioTimeAxisView* atv2 = dynamic_cast<AudioTimeAxisView*>(tracklist_timeview);
			list<TimeAxisView*> children_list;
	      
			/* zeroes are audio tracks. ones are other types. */
	      
			if (!atv2->hidden()) {
				
				if (visible_y_high < atv2->order) {
					visible_y_high = atv2->order;
				}
				if (visible_y_low > atv2->order) {
					visible_y_low = atv2->order;
				}
		
				if (!atv2->is_audio_track()) {		  		  
					tracks = tracks |= (0x01 << atv2->order);
				}
	
				height_list[atv2->order] = (*i)->current_height();
				children = 1;
				if ((children_list = atv2->get_child_list()).size() > 0) {
					for (list<TimeAxisView*>::iterator j = children_list.begin(); j != children_list.end(); ++j) { 
						tracks = tracks |= (0x01 << (atv2->order + children));
						height_list[atv2->order + children] =  (*j)->current_height();
						numtracks++;
						children++;	
					}
				}
				numtracks++;	    
			}
		}
		/* find the actual span according to the canvas */

		canvas_pointer_y_span = pointer_y_span;
		if (drag_info.dest_trackview->order >= tv->order) {
			int32_t y;
			for (y = tv->order; y < drag_info.dest_trackview->order; y++) {
				if (height_list[y] == 0 ) {
					canvas_pointer_y_span--;
				}
			}
		} else {
			int32_t y;
			for (y = drag_info.dest_trackview->order;y <= tv->order; y++) {
				if (	height_list[y] == 0 ) {
					canvas_pointer_y_span++;
				}
			}
		}

		for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin(); i != selection->regions.by_layer().end(); ++i) {
			RegionView* rv2 = (*i);
			double ix1, ix2, iy1, iy2;
			int32_t n = 0;

			if (rv2->region()->locked()) {
				continue;
			}

			rv2->get_canvas_frame()->get_bounds (ix1, iy1, ix2, iy2);
			rv2->get_canvas_group()->i2w (ix1, iy1);
			iy1 += vertical_adjustment.get_value() - canvas_timebars_vsize;

			TimeAxisView* tvp2 = trackview_by_y_position (iy1);
			RouteTimeAxisView* atv2 = dynamic_cast<RouteTimeAxisView*>(tvp2);

			if (atv2->order != original_pointer_order) {	
				/* this isn't the pointer track */	

				if (canvas_pointer_y_span > 0) {

					/* moving up the canvas */
					if ((atv2->order - canvas_pointer_y_span) >= visible_y_low) {
	
						int32_t visible_tracks = 0;
						while (visible_tracks < canvas_pointer_y_span ) {
							visible_tracks++;
		  
							while (height_list[atv2->order - (visible_tracks - n)] == 0) {
								/* we're passing through a hidden track */
								n--;
							}		  
						}
		 
						if (tracks[atv2->order - (canvas_pointer_y_span - n)] != 0x00) {		  
							clamp_y_axis = true;
						}
		    
					} else {
						clamp_y_axis = true;
					}		  
		  
				} else if (canvas_pointer_y_span < 0) {

					/*moving down the canvas*/

					if ((atv2->order - (canvas_pointer_y_span - n)) <= visible_y_high) { // we will overflow
		    
		    
						int32_t visible_tracks = 0;
		    
						while (visible_tracks > canvas_pointer_y_span ) {
							visible_tracks--;
		      
							while (height_list[atv2->order - (visible_tracks - n)] == 0) {		   
								n++;
							}		 
						}
						if (  tracks[atv2->order - ( canvas_pointer_y_span - n)] != 0x00) {
							clamp_y_axis = true;
			    
						}
					} else {
			  
						clamp_y_axis = true;
					}
				}		
		  
			} else {
		      
				/* this is the pointer's track */
				if ((atv2->order - pointer_y_span) > visible_y_high) { // we will overflow 
					clamp_y_axis = true;
				} else if ((atv2->order - pointer_y_span) < visible_y_low) { // we will underflow
					clamp_y_axis = true;
				}
			}	      
			if (clamp_y_axis) {
				break;
			}
		}

	} else  if (drag_info.dest_trackview == tv) {
		clamp_y_axis = true;
	}	  

  y_axis_done:
	if (!clamp_y_axis) {
		drag_info.dest_trackview = tv;	      
	}
	  
	/************************************************************
	                X DELTA COMPUTATION
	************************************************************/

	/* compute the amount of pointer motion in frames, and where
	   the region would be if we moved it by that much.
	*/
	if ( drag_info.move_threshold_passed ) {

		if (drag_info.current_pointer_frame >= drag_info.pointer_frame_offset) {

			nframes64_t sync_frame;
			nframes64_t sync_offset;
			int32_t sync_dir;

			pending_region_position = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;

			sync_offset = clicked_regionview->region()->sync_offset (sync_dir);

			/* we don't handle a sync point that lies before zero.
			 */
			if (sync_dir >= 0 || (sync_dir < 0 && pending_region_position >= sync_offset)) {
				sync_frame = pending_region_position + (sync_dir*sync_offset);

				/* we snap if the snap modifier is not enabled.
				 */
	    
				if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
					snap_to (sync_frame);	
				}
	    
				pending_region_position = clicked_regionview->region()->adjust_to_sync (sync_frame);

			} else {
				pending_region_position = drag_info.last_frame_position;
			}
	    
		} else {
			pending_region_position = 0;
		}
	  
		if (pending_region_position > max_frames - clicked_regionview->region()->length()) {
			pending_region_position = drag_info.last_frame_position;
		}

		// printf ("3: pending_region_position= %lu    %lu\n", pending_region_position, drag_info.last_frame_position );

		bool x_move_allowed;
		
		if (Config->get_edit_mode() == Lock) {
			x_move_allowed = drag_info.x_constrained;
		} else {
			x_move_allowed = !drag_info.x_constrained;
		}

		if (( pending_region_position != drag_info.last_frame_position) && x_move_allowed ) {

			/* now compute the canvas unit distance we need to move the regionview
			   to make it appear at the new location.
			*/

			if (pending_region_position > drag_info.last_frame_position) {
				x_delta = ((double) (pending_region_position - drag_info.last_frame_position) / frames_per_unit);
			} else {
				x_delta = -((double) (drag_info.last_frame_position - pending_region_position) / frames_per_unit);
				
				//test to make sure that we aren't dragging near 0
				if (selection->regions.by_layer().size() == 1) {  // If a single regionview is being dragged to zero, make sure we go all the way to zero.	
					RegionView* rv2 = *(selection->regions.by_layer().begin());
					double ix1, ix2, iy1, iy2;
					rv2->get_canvas_frame()->get_bounds (ix1, iy1, ix2, iy2);
					rv2->get_canvas_group()->i2w (ix1, iy1);
					double pos = ix1 + horizontal_adjustment.get_value();
					if (-x_delta > pos) {
						pending_region_position = 0;
					}
				} else {  // If any regionview is at zero, we need to know so we can stop further leftward motion.	
					
 					//first find the earliest region in the selection
					RegionView *earliest_rv = selection->regions.by_layer().front();
					for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin(); i != selection->regions.by_layer().end(); ++i) {
						RegionView* rv = (*i);
						if (rv->region()->position() < earliest_rv->region()->position())
							earliest_rv = rv;
					}				
					
					//if the earliest region is near 0, then limit the drag so it doesn't go any farther left
					double ix1, ix2, iy1, iy2;
					earliest_rv->get_canvas_frame()->get_bounds (ix1, iy1, ix2, iy2);
					earliest_rv->get_canvas_group()->i2w (ix1, iy1);
					double pos = ix1 + horizontal_adjustment.get_value();
					if (x_delta < -pos) {
						x_delta = -pos;
						pending_region_position = clicked_regionview->region()->position() - earliest_rv->region()->position();
					}
				}

			}
		
			drag_info.last_frame_position = pending_region_position;

		} else {
			x_delta = 0;
		}

	} else {
		/* threshold not passed */

		x_delta = 0;
	}
	
	/*************************************************************
                        PREPARE TO MOVE
	************************************************************/

	if (x_delta == 0 && (pointer_y_span == 0)) {
		/* haven't reached next snap point, and we're not switching
		   trackviews. nothing to do.
		*/
		return;
	}

	/*************************************************************
                        MOTION								      
	************************************************************/
	bool do_move = true;
	if (drag_info.first_move) {
		if (!drag_info.move_threshold_passed) {
			do_move = false;
		}
	}

	if (do_move) {

		pair<set<boost::shared_ptr<Playlist> >::iterator,bool> insert_result;
		const list<RegionView*>& layered_regions = selection->regions.by_layer();
		
		for (list<RegionView*>::const_iterator i = layered_regions.begin(); i != layered_regions.end(); ++i) {
	    
			RegionView* rv = (*i);
			double ix1, ix2, iy1, iy2;
			int32_t temp_pointer_y_span = pointer_y_span;

			if (rv->region()->locked()) {
				continue;
			}

			/* get item BBox, which will be relative to parent. so we have
			   to query on a child, then convert to world coordinates using
			   the parent.
			*/

			rv->get_canvas_frame()->get_bounds (ix1, iy1, ix2, iy2);
			rv->get_canvas_group()->i2w (ix1, iy1);

			/* for evaluation of the track position of iy1, we have to adjust 
			   to allow for the vertical scrolling adjustment and the height of the timebars.
			*/

			iy1 += get_trackview_group_vertical_offset ();
			if (drag_info.first_move) {

				// hide any dependent views 
	
				rv->get_time_axis_view().hide_dependent_views (*rv);

				/* 
				   reparent to a non scrolling group so that we can keep the 
				   region selection above all time axis views.
				   reparenting means we have to move the rv as the two 
				   parent groups have different coordinates.
				*/

				rv->get_canvas_group()->property_y() = iy1 - 1;
				rv->get_canvas_group()->reparent(*_region_motion_group);

				rv->fake_set_opaque (true);
			}

			TimeAxisView* tvp2 = trackview_by_y_position (iy1);
			AudioTimeAxisView* canvas_atv = dynamic_cast<AudioTimeAxisView*>(tvp2);
			AudioTimeAxisView* temp_atv;

			if ((pointer_y_span != 0) && !clamp_y_axis) {
				y_delta = 0;
				int32_t x = 0;
				for (j = height_list.begin(); j!= height_list.end(); j++) {	
					if (x == canvas_atv->order) {
						/* we found the track the region is on */
						if (x != original_pointer_order) {
							/*this isn't from the same track we're dragging from */
							temp_pointer_y_span = canvas_pointer_y_span;
						}		  
						while (temp_pointer_y_span > 0) {
							/* we're moving up canvas-wise,
							   so  we need to find the next track height
							*/
							if (j != height_list.begin()) {		  
								j--;
							}
							if (x != original_pointer_order) { 
								/* we're not from the dragged track, so ignore hidden tracks. */	      
								if ((*j) == 0) {
									temp_pointer_y_span++;
								}
							}	   
							y_delta -= (*j);	
							temp_pointer_y_span--;	
						}

						while (temp_pointer_y_span < 0) {		  
							y_delta += (*j);
							if (x != original_pointer_order) { 
								if ((*j) == 0) {
									temp_pointer_y_span--;
								}
							}	   
		    
							if (j != height_list.end()) {		      
								j++;
							}
							temp_pointer_y_span++;
						}
						/* find out where we'll be when we move and set height accordingly */
		  
						tvp2 = trackview_by_y_position (iy1 + y_delta);
						temp_atv = dynamic_cast<AudioTimeAxisView*>(tvp2);
						rv->set_height (temp_atv->current_height());
	
						/*   if you un-comment the following, the region colours will follow the track colours whilst dragging,
						     personally, i think this can confuse things, but never mind.
						*/
		  		  
						//const GdkColor& col (temp_atv->view->get_region_color());
						//rv->set_color (const_cast<GdkColor&>(col));
						break;		
					}
					x++;
				}
			}

			if (drag_info.brushing) {
				mouse_brush_insert_region (rv, pending_region_position);
			} else {
				rv->move (x_delta, y_delta);
			}

		} /* foreach region */

	} /* if do_move */

	if (drag_info.first_move && drag_info.move_threshold_passed) {
		cursor_group->raise_to_top();
		drag_info.first_move = false;
	}

	if (x_delta != 0 && !drag_info.brushing) {
		show_verbose_time_cursor (drag_info.last_frame_position, 10);
	}
} 

void
Editor::region_drag_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	bool nocommit = true;
	vector<RegionView*> copies;
	RouteTimeAxisView* source_tv;
	boost::shared_ptr<Diskstream> ds;
	boost::shared_ptr<Playlist> from_playlist;
	vector<RegionView*> new_selection;
	typedef set<boost::shared_ptr<Playlist> > PlaylistSet;
	PlaylistSet modified_playlists;
	PlaylistSet frozen_playlists;
	list <sigc::connection> modified_playlist_connections;
	pair<PlaylistSet::iterator,bool> insert_result, frozen_insert_result;
	nframes64_t drag_delta;
	bool changed_tracks, changed_position;

	/* first_move is set to false if the regionview has been moved in the 
	   motion handler. 
	*/

	if (drag_info.first_move) {
		/* just a click */
		goto out;
	}

	nocommit = false;

	if (Config->get_edit_mode() == Splice && !pre_drag_region_selection.empty()) {
		selection->set (pre_drag_region_selection);
		pre_drag_region_selection.clear ();
	}

	if (drag_info.brushing) {
		/* all changes were made during motion event handlers */
		
		if (drag_info.copy) {
			for (list<RegionView*>::iterator i = selection->regions.begin(); i != selection->regions.end(); ++i) {
				copies.push_back (*i);
			}
		}

		goto out;
	}

	char* op_string;

	/* reverse this here so that we have the correct logic to finalize
	   the drag.
	*/
	
	if (Config->get_edit_mode() == Lock) {
		drag_info.x_constrained = !drag_info.x_constrained;
	}

	if (drag_info.copy) {
		if (drag_info.x_constrained) {
			op_string = _("fixed time region copy");
		} else {
			op_string = _("region copy");
		} 
	} else {
		if (drag_info.x_constrained) {
			op_string = _("fixed time region drag");
		} else {
			op_string = _("region drag");
		}
	}

	begin_reversible_command (op_string);
	changed_position = (drag_info.last_frame_position != (nframes64_t) (clicked_regionview->region()->position()));
	changed_tracks = (trackview_by_y_position (drag_info.current_pointer_y) != &clicked_regionview->get_time_axis_view());

	drag_delta = clicked_regionview->region()->position() - drag_info.last_frame_position;

        flush_canvas ();

	for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin(); i != selection->regions.by_layer().end(); ) {
			
		RegionView* rv = (*i);	    	    
		double ix1, ix2, iy1, iy2;
		rv->get_canvas_frame()->get_bounds (ix1, iy1, ix2, iy2);
		rv->get_canvas_group()->i2w (ix1, iy1);
		iy1 += vertical_adjustment.get_value() - canvas_timebars_vsize;

		TimeAxisView* dest_tv = trackview_by_y_position (iy1);
		AudioTimeAxisView* dest_atv = dynamic_cast<AudioTimeAxisView*>(dest_tv);
		nframes64_t where;

		if (rv->region()->locked()) {
			++i;
			continue;
		}
                
		if (changed_position && !drag_info.x_constrained && (mouse_mode != MouseRange)) {
			where = rv->region()->position() - drag_delta;
		} else {
			where = rv->region()->position();
		}
			
		boost::shared_ptr<Region> new_region;

		if (drag_info.copy) {
			/* we already made a copy */
			new_region = rv->region();

			/* undo the previous hide_dependent_views so that xfades don't
			   disappear on copying regions 
			*/
		
		       	//rv->get_time_axis_view().reveal_dependent_views (*rv);
		
		} else if (changed_tracks) {
			new_region = RegionFactory::create (rv->region());
		}

		if (changed_tracks || drag_info.copy) {

			boost::shared_ptr<Playlist> to_playlist = dest_atv->playlist();

			latest_regionviews.clear ();

			sigc::connection c = dest_atv->view()->RegionViewAdded.connect (mem_fun(*this, &Editor::collect_new_region_view));
			
			insert_result = modified_playlists.insert (to_playlist);
			if (insert_result.second) {
				session->add_command (new MementoCommand<Playlist>(*to_playlist, &to_playlist->get_state(), 0));
			}

			to_playlist->add_region (new_region, where);

			c.disconnect ();
							      
			if (!latest_regionviews.empty()) {
				// XXX why just the first one ? we only expect one
				//dest_atv->reveal_dependent_views (*latest_regionviews.front());
				new_selection.push_back (latest_regionviews.front());
			}

		} else {
			/* 
			   motion on the same track. plonk the previously reparented region 
			   back to its original canvas group (its streamview).
			   No need to do anything for copies as they are fake regions which will be deleted.
			*/

			RouteTimeAxisView* dest_rtv = dynamic_cast<RouteTimeAxisView*> (dest_atv);
			rv->get_canvas_group()->reparent (*dest_rtv->view()->canvas_item());
			rv->get_canvas_group()->property_y() = 0;
		  
			/* just change the model */
			
			boost::shared_ptr<Playlist> playlist = dest_atv->playlist();

			insert_result = modified_playlists.insert (playlist);
			if (insert_result.second) {
				session->add_command (new MementoCommand<Playlist>(*playlist, &playlist->get_state(), 0));
			}
			/* freeze to avoid lots of relayering in the case of a multi-region drag */
			frozen_insert_result = frozen_playlists.insert(playlist);
			if (frozen_insert_result.second) {
				playlist->freeze();
			}

			rv->region()->set_position (where, (void*) this);
		}

		if (changed_tracks && !drag_info.copy) {

			/* get the playlist where this drag started. we can't use rv->region()->playlist()
			   because we may have copied the region and it has not been attached to a playlist.
			*/
			
			assert ((source_tv = dynamic_cast<RouteTimeAxisView*> (&rv->get_time_axis_view())));
			assert ((ds = source_tv->get_diskstream()));
			assert ((from_playlist = ds->playlist()));

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
                                if (mouse_mode != MouseRange) {
                                        session->add_command (new MementoCommand<Playlist>(*from_playlist, &from_playlist->get_state(), 0));
                                }
			}

			from_playlist->remove_region ((rv->region()));
			
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

			if (selection->regions.empty()) {
				break;
			} else { 
				i = selection->regions.by_layer().begin();
			}

		} else {
			++i;
		}
		
		if (drag_info.copy) {
			copies.push_back (rv);
		}
	}
	
	if (new_selection.empty()) {
		if (drag_info.copy) {
			/* the region(view)s that are selected and being dragged around
			   are copies and do not belong to any track. remove them
			   from the selection right here.
			*/
			selection->clear_regions();
		}
	} else {
		/* this will clear any existing selection that would have been
		   cleared in the other clause above
		*/
		selection->set (new_selection);
	}

	for (set<boost::shared_ptr<Playlist> >::iterator p = frozen_playlists.begin(); p != frozen_playlists.end(); ++p) {
		(*p)->thaw();
	}
			
  out:
	if (!nocommit) {
		for (set<boost::shared_ptr<Playlist> >::iterator p = modified_playlists.begin(); p != modified_playlists.end(); ++p) {
			session->add_command (new MementoCommand<Playlist>(*(*p), 0, &(*p)->get_state()));	
		}
		commit_reversible_command ();
	}

	for (vector<RegionView*>::iterator x = copies.begin(); x != copies.end(); ++x) {
		delete *x;
	}

}

void
Editor::region_view_item_click (AudioRegionView& rv, GdkEventButton* event)
{
	/* Either add to or set the set the region selection, unless
	   this is an alignment click (control used)
	*/
	
	if (Keyboard::modifier_state_contains (event->state, Keyboard::PrimaryModifier)) {
		TimeAxisView* tv = &rv.get_time_axis_view();
		AudioTimeAxisView* atv = dynamic_cast<AudioTimeAxisView*>(tv);
		double speed = 1.0;
		if (atv && atv->is_audio_track()) {
			speed = atv->get_diskstream()->speed();
		}

		nframes64_t where = get_preferred_edit_position();

		if (where >= 0) {

			if (Keyboard::modifier_state_equals (event->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::SecondaryModifier))) {
				
				align_region (rv.region(), SyncPoint, (nframes64_t) (where * speed));
				
			} else if (Keyboard::modifier_state_equals (event->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
				
				align_region (rv.region(), End, (nframes64_t) (where * speed));
				
			} else {
				
				align_region (rv.region(), Start, (nframes64_t) (where * speed));
			}
		}
	}
}

void
Editor::show_verbose_time_cursor (nframes64_t frame, double offset, double xpos, double ypos) 
{
	char buf[128];
	SMPTE::Time smpte;
	BBT_Time bbt;
	int hours, mins;
	nframes64_t frame_rate;
	float secs;

	if (session == 0) {
		return;
	}

	AudioClock::Mode m;

	if (Profile->get_sae() || Profile->get_small_screen()) {
		m = ARDOUR_UI::instance()->primary_clock.mode();
	} else {
		m = ARDOUR_UI::instance()->secondary_clock.mode();
	}

	switch (m) {
	case AudioClock::BBT:
		session->bbt_time (frame, bbt);
		snprintf (buf, sizeof (buf), "%02" PRIu32 "|%02" PRIu32 "|%02" PRIu32, bbt.bars, bbt.beats, bbt.ticks);
		break;
		
	case AudioClock::SMPTE:
		session->smpte_time (frame, smpte);
		snprintf (buf, sizeof (buf), "%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32, smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
		break;

	case AudioClock::MinSec:
		/* XXX this is copied from show_verbose_duration_cursor() */
		frame_rate = session->frame_rate();
		hours = frame / (frame_rate * 3600);
		frame = frame % (frame_rate * 3600);
		mins = frame / (frame_rate * 60);
		frame = frame % (frame_rate * 60);
		secs = (float) frame / (float) frame_rate;
		snprintf (buf, sizeof (buf), "%02" PRId32 ":%02" PRId32 ":%.4f", hours, mins, secs);
		break;

	default:
		snprintf (buf, sizeof(buf), "%" PRIi64, frame);
		break;
	}

	if (xpos >= 0 && ypos >=0) {
		set_verbose_canvas_cursor (buf, xpos + offset, ypos + offset);
	}
	else {
		set_verbose_canvas_cursor (buf, drag_info.current_pointer_x + offset - horizontal_adjustment.get_value(), drag_info.current_pointer_y + offset - vertical_adjustment.get_value() + canvas_timebars_vsize);
	}
	show_verbose_canvas_cursor ();
}

void
Editor::show_verbose_duration_cursor (nframes64_t start, nframes64_t end, double offset, double xpos, double ypos) 
{
	char buf[128];
	SMPTE::Time smpte;
	BBT_Time sbbt;
	BBT_Time ebbt;
	int hours, mins;
	nframes64_t distance, frame_rate;
	float secs;
	Meter meter_at_start(session->tempo_map().meter_at(start));

	if (session == 0) {
		return;
	}

	AudioClock::Mode m;

	if (Profile->get_sae() || Profile->get_small_screen()) {
		m = ARDOUR_UI::instance()->primary_clock.mode ();
	} else {
		m = ARDOUR_UI::instance()->secondary_clock.mode ();
	}

	switch (m) {
	case AudioClock::BBT:
		session->bbt_time (start, sbbt);
		session->bbt_time (end, ebbt);

		/* subtract */
		/* XXX this computation won't work well if the
		user makes a selection that spans any meter changes.
		*/

		ebbt.bars -= sbbt.bars;
		if (ebbt.beats >= sbbt.beats) {
			ebbt.beats -= sbbt.beats;
		} else {
			ebbt.bars--;
			ebbt.beats =  int(meter_at_start.beats_per_bar()) + ebbt.beats - sbbt.beats;
		}
		if (ebbt.ticks >= sbbt.ticks) {
			ebbt.ticks -= sbbt.ticks;
		} else {
			ebbt.beats--;
			ebbt.ticks = int(Meter::ticks_per_beat) + ebbt.ticks - sbbt.ticks;
		}
		
		snprintf (buf, sizeof (buf), "%02" PRIu32 "|%02" PRIu32 "|%02" PRIu32, ebbt.bars, ebbt.beats, ebbt.ticks);
		break;
		
	case AudioClock::SMPTE:
		session->smpte_duration (end - start, smpte);
		snprintf (buf, sizeof (buf), "%02" PRId32 ":%02" PRId32 ":%02" PRId32 ":%02" PRId32, smpte.hours, smpte.minutes, smpte.seconds, smpte.frames);
		break;

	case AudioClock::MinSec:
		/* XXX this stuff should be elsewhere.. */
		distance = end - start;
		frame_rate = session->frame_rate();
		hours = distance / (frame_rate * 3600);
		distance = distance % (frame_rate * 3600);
		mins = distance / (frame_rate * 60);
		distance = distance % (frame_rate * 60);
		secs = (float) distance / (float) frame_rate;
		snprintf (buf, sizeof (buf), "%02" PRId32 ":%02" PRId32 ":%.4f", hours, mins, secs);
		break;

	default:
		snprintf (buf, sizeof(buf), "%" PRIi64, end - start);
		break;
	}

	if (xpos >= 0 && ypos >=0) {
		set_verbose_canvas_cursor (buf, xpos + offset, ypos + offset);
	}
	else {
		set_verbose_canvas_cursor (buf, drag_info.current_pointer_x + offset, drag_info.current_pointer_y + offset);
	}

	show_verbose_canvas_cursor ();
}

void
Editor::collect_new_region_view (RegionView* rv)
{
	latest_regionviews.push_back (rv);
}

void
Editor::start_selection_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (clicked_regionview == 0) {
		return;
	}

	/* lets try to create new Region for the selection */

	vector<boost::shared_ptr<AudioRegion> > new_regions;
	create_region_from_selection (new_regions);

	if (new_regions.empty()) {
		return;
	}

	/* XXX fix me one day to use all new regions */
	
	boost::shared_ptr<Region> region (new_regions.front());

	/* add it to the current stream/playlist.

	   tricky: the streamview for the track will add a new regionview. we will
	   catch the signal it sends when it creates the regionview to
	   set the regionview we want to then drag.
	*/
	
	latest_regionviews.clear();
	sigc::connection c = clicked_audio_trackview->view()->RegionViewAdded.connect (mem_fun(*this, &Editor::collect_new_region_view));
	
	/* A selection grab currently creates two undo/redo operations, one for 
	   creating the new region and another for moving it.
	*/

	begin_reversible_command (_("selection grab"));

	boost::shared_ptr<Playlist> playlist = clicked_trackview->playlist();

        XMLNode *before = &(playlist->get_state());
	clicked_trackview->playlist()->add_region (region, selection->time[clicked_selection].start);
        XMLNode *after = &(playlist->get_state());
	session->add_command(new MementoCommand<Playlist>(*playlist, before, after));

	commit_reversible_command ();
	
	c.disconnect ();
	
	if (latest_regionviews.empty()) {
		/* something went wrong */
		return;
	}

	/* we need to deselect all other regionviews, and select this one
	   i'm ignoring undo stuff, because the region creation will take care of it 
	*/
	selection->set (latest_regionviews);
	
	drag_info.item = latest_regionviews.front()->get_canvas_group();
	drag_info.data = latest_regionviews.front();
	drag_info.motion_callback = &Editor::region_drag_motion_callback;
	drag_info.finished_callback = &Editor::region_drag_finished_callback;

	start_grab (event);
	
	drag_info.source_trackview = clicked_trackview;
	drag_info.dest_trackview = drag_info.source_trackview;
	drag_info.last_frame_position = latest_regionviews.front()->region()->position();
	drag_info.pointer_frame_offset = drag_info.grab_frame - drag_info.last_frame_position;
	
	show_verbose_time_cursor (drag_info.last_frame_position, 10);
}

void
Editor::cancel_selection ()
{
        for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
		(*i)->hide_selection ();
	}
	selection->clear ();
	clicked_selection = 0;
}	

void
Editor::start_selection_op (ArdourCanvas::Item* item, GdkEvent* event, SelectionOp op)
{
	nframes64_t start = 0;
	nframes64_t end = 0;

	if (session == 0) {
		return;
	}

	drag_info.item = item;
	drag_info.motion_callback = &Editor::drag_selection;
	drag_info.finished_callback = &Editor::end_selection_op;

	selection_op = op;

	switch (op) {
	case CreateSelection:
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
			drag_info.copy = true;
		} else {
			drag_info.copy = false;
		}
		start_grab (event, selector_cursor);
		break;

	case SelectionStartTrim:
		if (clicked_trackview) {
			clicked_trackview->order_selection_trims (item, true);
		} 
		start_grab (event, trimmer_cursor);
		start = selection->time[clicked_selection].start;
		drag_info.pointer_frame_offset = drag_info.grab_frame - start;	
		break;
		
	case SelectionEndTrim:
		if (clicked_trackview) {
			clicked_trackview->order_selection_trims (item, false);
		}
		start_grab (event, trimmer_cursor);
		end = selection->time[clicked_selection].end;
		drag_info.pointer_frame_offset = drag_info.grab_frame - end;	
		break;

	case SelectionMove:
		start = selection->time[clicked_selection].start;
		start_grab (event);
		drag_info.pointer_frame_offset = drag_info.grab_frame - start;	
		break;
	}

	if (selection_op == SelectionMove) {
		show_verbose_time_cursor(start, 10);	
	} else {
		show_verbose_time_cursor(drag_info.current_pointer_frame, 10);	
	}
}

void
Editor::drag_selection (ArdourCanvas::Item* item, GdkEvent* event)
{
	nframes64_t start = 0;
	nframes64_t end = 0;
	nframes64_t length;
	nframes64_t pending_position;

	if (drag_info.current_pointer_frame > drag_info.pointer_frame_offset) {
		pending_position = drag_info.current_pointer_frame - drag_info.pointer_frame_offset;
	} else {
		pending_position = 0;
	}
	
	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (pending_position);
	}

	/* only alter selection if the current frame is 
	   different from the last frame position (adjusted)
	 */
	
	if (pending_position == drag_info.last_pointer_frame) return;
	
	switch (selection_op) {
	case CreateSelection:
		
		if (drag_info.first_move) {
			snap_to (drag_info.grab_frame);
		}
		
		if (pending_position < drag_info.grab_frame) {
			start = pending_position;
			end = drag_info.grab_frame;
		} else {
			end = pending_position;
			start = drag_info.grab_frame;
		}
		
		/* first drag: Either add to the selection
		   or create a new selection->
		*/
		
		if (drag_info.first_move) {
			
			begin_reversible_command (_("range selection"));
			
			if (drag_info.copy) {
				/* adding to the selection */
				clicked_selection = selection->add (start, end);
				drag_info.copy = false;
			} else {
				/* new selection-> */
				clicked_selection = selection->set (clicked_trackview, start, end);
			}
		} 
		break;
		
	case SelectionStartTrim:
		
		if (drag_info.first_move) {
			begin_reversible_command (_("trim selection start"));
		}
		
		start = selection->time[clicked_selection].start;
		end = selection->time[clicked_selection].end;

		if (pending_position > end) {
			start = end;
		} else {
			start = pending_position;
		}
		break;
		
	case SelectionEndTrim:
		
		if (drag_info.first_move) {
			begin_reversible_command (_("trim selection end"));
		}
		
		start = selection->time[clicked_selection].start;
		end = selection->time[clicked_selection].end;

		if (pending_position < start) {
			end = start;
		} else {
			end = pending_position;
		}
		
		break;
		
	case SelectionMove:
		
		if (drag_info.first_move) {
			begin_reversible_command (_("move selection"));
		}
		
		start = selection->time[clicked_selection].start;
		end = selection->time[clicked_selection].end;
		
		length = end - start;
		
		start = pending_position;
		snap_to (start);
		
		end = start + length;
		
		break;
	}
	
	if (event->button.x >= horizontal_adjustment.get_value() + canvas_width) {
		start_canvas_autoscroll (1, 0);
	}

	if (start != end) {
		selection->replace (clicked_selection, start, end);
	}

	drag_info.last_pointer_frame = pending_position;
	drag_info.first_move = false;

	if (selection_op == SelectionMove) {
		show_verbose_time_cursor(start, 10);	
	} else {
		show_verbose_time_cursor(pending_position, 10);	
	}
}

void
Editor::end_selection_op (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (!drag_info.first_move) {
		drag_selection (item, event);
		/* XXX this is not object-oriented programming at all. ick */
		if (selection->time.consolidate()) {
			selection->TimeChanged ();
		}
		commit_reversible_command ();


		/* XXX what if its a music time selection? */
		if (Config->get_auto_play() || (session->get_play_range() && session->transport_rolling())) {
			session->request_play_range (&selection->time, true);
		}

	} else {
		/* just a click, no pointer movement.*/

		if (Keyboard::no_modifier_keys_pressed (&event->button)) {

			selection->clear_time();

		} 

		if (session->get_play_range () && session->transport_rolling()) {
			session->request_stop (false, false);
		}
	}

	stop_canvas_autoscroll ();
}

void
Editor::start_trim (ArdourCanvas::Item* item, GdkEvent* event)
{
	double speed = 1.0;
	TimeAxisView* tvp = clicked_trackview;
	AudioTimeAxisView* tv = dynamic_cast<AudioTimeAxisView*>(tvp);

	if (tv && tv->is_audio_track()) {
		speed = tv->get_diskstream()->speed();
	}
	
	nframes64_t region_start = (nframes64_t) (clicked_regionview->region()->position() / speed);
	nframes64_t region_end = (nframes64_t) (clicked_regionview->region()->last_frame() / speed);
	nframes64_t region_length = (nframes64_t) (clicked_regionview->region()->length() / speed);

	//drag_info.item = clicked_regionview->get_name_highlight();
	drag_info.item = item;
	drag_info.motion_callback = &Editor::trim_motion_callback;
	drag_info.finished_callback = &Editor::trim_finished_callback;

	start_grab (event, trimmer_cursor);
	
	if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
		trim_op = ContentsTrim;
	} else {
		/* These will get overridden for a point trim.*/
		if (drag_info.current_pointer_frame < (region_start + region_length/2)) {
			/* closer to start */
			trim_op = StartTrim;
		} else if (drag_info.current_pointer_frame > (region_end - region_length/2)) {
			/* closer to end */
			trim_op = EndTrim;
		}
	}

	switch (trim_op) {
	case StartTrim:
		show_verbose_time_cursor(region_start, 10);	
		break;
	case EndTrim:
		show_verbose_time_cursor(region_end, 10);	
		break;
	case ContentsTrim:
		show_verbose_time_cursor(drag_info.current_pointer_frame, 10);	
		break;
	}
}

void
Editor::trim_motion_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	RegionView* rv = clicked_regionview;
	nframes64_t frame_delta = 0;
	bool left_direction;
	bool obey_snap = !Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier());

	/* snap modifier works differently here..
	   its' current state has to be passed to the 
	   various trim functions in order to work properly 
	*/ 

	double speed = 1.0;
	TimeAxisView* tvp = clicked_trackview;
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);
	pair<set<boost::shared_ptr<Playlist> >::iterator,bool> insert_result;

	if (tv && tv->is_audio_track()) {
		speed = tv->get_diskstream()->speed();
	}
	
	if (drag_info.last_pointer_frame > drag_info.current_pointer_frame) {
		left_direction = true;
	} else {
		left_direction = false;
	}

	if (obey_snap) {
		snap_to (drag_info.current_pointer_frame);
	}

	if (drag_info.current_pointer_frame == drag_info.last_pointer_frame) {
		return;
	}

	if (drag_info.first_move) {
	
		string trim_type;

		switch (trim_op) {
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

		begin_reversible_command (trim_type);

		for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin(); i != selection->regions.by_layer().end(); ++i) {
			(*i)->fake_set_opaque(false);			
			(*i)->region()->freeze ();
		
			AudioRegionView* const arv = dynamic_cast<AudioRegionView*>(*i);
			if (arv)
				arv->temporarily_hide_envelope ();

			boost::shared_ptr<Playlist> pl = (*i)->region()->playlist();
			insert_result = motion_frozen_playlists.insert (pl);
			if (insert_result.second) {
                                session->add_command(new MementoCommand<Playlist>(*pl, &pl->get_state(), 0));
			}
		}
	}

	if (left_direction) {
		frame_delta = (drag_info.last_pointer_frame - drag_info.current_pointer_frame);
	} else {
		frame_delta = (drag_info.current_pointer_frame - drag_info.last_pointer_frame);
	}

	switch (trim_op) {		
	case StartTrim:
		if ((left_direction == false) && (drag_info.current_pointer_frame <= rv->region()->first_frame()/speed)) {
			break;
                } else {
			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin(); i != selection->regions.by_layer().end(); ++i) {
				single_start_trim (**i, frame_delta, left_direction, obey_snap);
			}
			break;
		}
		
	case EndTrim:
		if ((left_direction == true) && (drag_info.current_pointer_frame > (nframes64_t) (rv->region()->last_frame()/speed))) {
			break;
		} else {
			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin(); i != selection->regions.by_layer().end(); ++i) {
				single_end_trim (**i, frame_delta, left_direction, obey_snap);
			}
			break;
		}
		
	case ContentsTrim:
		{
			bool swap_direction = false;

			if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
				swap_direction = true;
			}
			
			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin();
			     i != selection->regions.by_layer().end(); ++i)
			{
				single_contents_trim (**i, frame_delta, left_direction, swap_direction, obey_snap);
			}
		}
		break;
	}

	switch (trim_op) {
	case StartTrim:
		show_verbose_time_cursor((nframes64_t) (rv->region()->position()/speed), 10);	
		break;
	case EndTrim:
		show_verbose_time_cursor((nframes64_t) (rv->region()->last_frame()/speed), 10);	
		break;
	case ContentsTrim:
		show_verbose_time_cursor(drag_info.current_pointer_frame, 10);	
		break;
	}

	drag_info.last_pointer_frame = drag_info.current_pointer_frame;
	drag_info.first_move = false;
}

void
Editor::single_contents_trim (RegionView& rv, nframes64_t frame_delta, bool left_direction, bool swap_direction, bool obey_snap)
{
	boost::shared_ptr<Region> region (rv.region());

	if (region->locked()) {
		return;
	}

	nframes64_t new_bound;

	double speed = 1.0;
	TimeAxisView* tvp = clicked_trackview;
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);

	if (tv && tv->is_audio_track()) {
		speed = tv->get_diskstream()->speed();
	}
	
	if (left_direction) {
		if (swap_direction) {
			new_bound = (nframes64_t) (region->position()/speed) + frame_delta;
		} else {
			new_bound = (nframes64_t) (region->position()/speed) - frame_delta;
		}
	} else {
		if (swap_direction) {
			new_bound = (nframes64_t) (region->position()/speed) - frame_delta;
		} else {
			new_bound = (nframes64_t) (region->position()/speed) + frame_delta;
		}
	}

	if (obey_snap) {
		snap_to (new_bound);
	}
	region->trim_start ((nframes64_t) (new_bound * speed), this);	
	rv.region_changed (StartChanged);
}

void
Editor::single_start_trim (RegionView& rv, nframes64_t frame_delta, bool left_direction, bool obey_snap)
{
	boost::shared_ptr<Region> region (rv.region());	

	if (region->locked()) {
		return;
	}

	nframes64_t new_bound;

	double speed = 1.0;
	TimeAxisView* tvp = clicked_trackview;
	AudioTimeAxisView* tv = dynamic_cast<AudioTimeAxisView*>(tvp);

	if (tv && tv->is_audio_track()) {
		speed = tv->get_diskstream()->speed();
	}
	
	if (left_direction) {
		new_bound = (nframes64_t) (region->position()/speed) - frame_delta;
	} else {
		new_bound = (nframes64_t) (region->position()/speed) + frame_delta;
	}

	if (obey_snap) {
		snap_to (new_bound, (left_direction ? 0 : 1));	
	}

	region->trim_front ((nframes64_t) (new_bound * speed), this);

	rv.region_changed (Change (LengthChanged|PositionChanged|StartChanged));
}

void
Editor::single_end_trim (RegionView& rv, nframes64_t frame_delta, bool left_direction, bool obey_snap)
{
	boost::shared_ptr<Region> region (rv.region());

	if (region->locked()) {
		return;
	}

	nframes64_t new_bound;

	double speed = 1.0;
	TimeAxisView* tvp = clicked_trackview;
	AudioTimeAxisView* tv = dynamic_cast<AudioTimeAxisView*>(tvp);

	if (tv && tv->is_audio_track()) {
		speed = tv->get_diskstream()->speed();
	}
	
	if (left_direction) {
		new_bound = (nframes64_t) ((region->last_frame() + 1)/speed) - frame_delta;
	} else {
		new_bound = (nframes64_t) ((region->last_frame() + 1)/speed) + frame_delta;
	}

	if (obey_snap) {
		snap_to (new_bound);
	}
	region->trim_end ((nframes64_t) (new_bound * speed), this);
	rv.region_changed (LengthChanged);
}
	
void
Editor::trim_finished_callback (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (!drag_info.first_move) {
		trim_motion_callback (item, event);
		
		if (!selection->selected (clicked_regionview)) {
			thaw_region_after_trim (*clicked_regionview);		
		} else {
			
			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin();
			     i != selection->regions.by_layer().end(); ++i)
			{
				thaw_region_after_trim (**i);
				(*i)->fake_set_opaque (true);
			}
		}
		
		for (set<boost::shared_ptr<Playlist> >::iterator p = motion_frozen_playlists.begin(); p != motion_frozen_playlists.end(); ++p) {
			//(*p)->thaw ();
                        session->add_command (new MementoCommand<Playlist>(*(*p).get(), 0, &(*p)->get_state()));
                }
		
		motion_frozen_playlists.clear ();

		commit_reversible_command();
	} else {
		/* no mouse movement */
		point_trim (event);
	}
}

void
Editor::point_trim (GdkEvent* event)
{
	RegionView* rv = clicked_regionview;
	nframes64_t new_bound = drag_info.current_pointer_frame;

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (new_bound);
	}

	/* Choose action dependant on which button was pressed */
	switch (event->button.button) {
	case 1:
		trim_op = StartTrim;
		begin_reversible_command (_("Start point trim"));

		if (selection->selected (rv)) {

			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin();
			     i != selection->regions.by_layer().end(); ++i)
			{
				if (!(*i)->region()->locked()) {
					boost::shared_ptr<Playlist> pl = (*i)->region()->playlist();
                                        XMLNode &before = pl->get_state();
					(*i)->region()->trim_front (new_bound, this);	
                                        XMLNode &after = pl->get_state();
                                        session->add_command(new MementoCommand<Playlist>(*pl.get(), &before, &after));
				}
			}

		} else {

			if (!rv->region()->locked()) {
				boost::shared_ptr<Playlist> pl = rv->region()->playlist();
				XMLNode &before = pl->get_state();
				rv->region()->trim_front (new_bound, this);	
                                XMLNode &after = pl->get_state();
				session->add_command(new MementoCommand<Playlist>(*pl.get(), &before, &after));
			}
		}

		commit_reversible_command();
	
		break;
	case 2:
		trim_op = EndTrim;
		begin_reversible_command (_("End point trim"));

		if (selection->selected (rv)) {
			
			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin(); i != selection->regions.by_layer().end(); ++i)
			{
				if (!(*i)->region()->locked()) {
					boost::shared_ptr<Playlist> pl = (*i)->region()->playlist();
					XMLNode &before = pl->get_state();
					(*i)->region()->trim_end (new_bound, this);
					XMLNode &after = pl->get_state();
					session->add_command(new MementoCommand<Playlist>(*pl.get(), &before, &after));
				}
			}

		} else {

			if (!rv->region()->locked()) {
				boost::shared_ptr<Playlist> pl = rv->region()->playlist();
				XMLNode &before = pl->get_state();
				rv->region()->trim_end (new_bound, this);
                                XMLNode &after = pl->get_state();
				session->add_command (new MementoCommand<Playlist>(*pl.get(), &before, &after));
			}
		}

		commit_reversible_command();
	
		break;
	default:
		break;
	}
}

void
Editor::thaw_region_after_trim (RegionView& rv)
{
	boost::shared_ptr<Region> region (rv.region());

	if (region->locked()) {
		return;
	}

	region->thaw (_("trimmed region"));
        XMLNode &after = region->playlist()->get_state();
	session->add_command (new MementoCommand<Playlist>(*(region->playlist()), 0, &after));

	AudioRegionView* arv = dynamic_cast<AudioRegionView*>(&rv);
	if (arv)
		arv->unhide_envelope ();
}

void
Editor::hide_marker (ArdourCanvas::Item* item, GdkEvent* event)
{
	Marker* marker;
	bool is_start;

	if ((marker = static_cast<Marker *> (item->get_data ("marker"))) == 0) {
		fatal << _("programming error: marker canvas item has no marker object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	Location* location = find_location_from_marker (marker, is_start);	
	location->set_hidden (true, this);
}


void
Editor::start_range_markerbar_op (ArdourCanvas::Item* item, GdkEvent* event, RangeMarkerOp op)
{
	if (session == 0) {
		return;
	}

	drag_info.item = item;
	drag_info.motion_callback = &Editor::drag_range_markerbar_op;
	drag_info.finished_callback = &Editor::end_range_markerbar_op;

	range_marker_op = op;

	if (!temp_location) {
		temp_location = new Location;
	}
	
	switch (op) {
	case CreateRangeMarker:
	case CreateTransportMarker:
	case CreateCDMarker:
	
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::TertiaryModifier)) {
			drag_info.copy = true;
		} else {
			drag_info.copy = false;
		}
		start_grab (event, selector_cursor);
		break;
	}

	show_verbose_time_cursor(drag_info.current_pointer_frame, 10);	
	
}

void
Editor::drag_range_markerbar_op (ArdourCanvas::Item* item, GdkEvent* event)
{
	nframes64_t start = 0;
	nframes64_t end = 0;
	ArdourCanvas::SimpleRect *crect;

	switch (range_marker_op) {
	case CreateRangeMarker:
		crect = range_bar_drag_rect;
		break;
	case CreateTransportMarker:
		crect = transport_bar_drag_rect;
		break;
	case CreateCDMarker:
		crect = cd_marker_bar_drag_rect;
		break;
	default:
		cerr << "Error: unknown range marker op passed to Editor::drag_range_markerbar_op ()" << endl;
		return;
		break;
	}
	
	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (drag_info.current_pointer_frame);
	}

	/* only alter selection if the current frame is 
	   different from the last frame position.
	 */
	
	if (drag_info.current_pointer_frame == drag_info.last_pointer_frame) return;
	
	switch (range_marker_op) {
	case CreateRangeMarker:
	case CreateTransportMarker:
	case CreateCDMarker:
		if (drag_info.first_move) {
			snap_to (drag_info.grab_frame);
		}
		
		if (drag_info.current_pointer_frame < drag_info.grab_frame) {
			start = drag_info.current_pointer_frame;
			end = drag_info.grab_frame;
		} else {
			end = drag_info.current_pointer_frame;
			start = drag_info.grab_frame;
		}
		
		/* first drag: Either add to the selection
		   or create a new selection.
		*/
		
		if (drag_info.first_move) {
			
			temp_location->set (start, end);
			
			crect->show ();

			update_marker_drag_item (temp_location);
			range_marker_drag_rect->show();
			//range_marker_drag_rect->raise_to_top();
			
		} 
		break;		
	}
	
	if (event->button.x >= horizontal_adjustment.get_value() + canvas_width) {
		start_canvas_autoscroll (1, 0);
	}
	
	if (start != end) {
		temp_location->set (start, end);

		double x1 = frame_to_pixel (start);
		double x2 = frame_to_pixel (end);
		crect->property_x1() = x1;
		crect->property_x2() = x2;

		update_marker_drag_item (temp_location);
	}

	drag_info.last_pointer_frame = drag_info.current_pointer_frame;
	drag_info.first_move = false;

	show_verbose_time_cursor(drag_info.current_pointer_frame, 10);	
	
}

void
Editor::end_range_markerbar_op (ArdourCanvas::Item* item, GdkEvent* event)
{
	Location * newloc = 0;
	string rangename;
	int flags;
	
	if (!drag_info.first_move) {
		drag_range_markerbar_op (item, event);

		switch (range_marker_op) {
		case CreateRangeMarker:
		case CreateCDMarker:
		    {
			begin_reversible_command (_("new range marker"));
                        XMLNode &before = session->locations()->get_state();
			session->locations()->next_available_name(rangename,"unnamed");
			if (range_marker_op == CreateCDMarker) {
				flags =  Location::IsRangeMarker|Location::IsCDMarker;
				cd_marker_bar_drag_rect->hide();
			}
			else {
				flags =  Location::IsRangeMarker;
				range_bar_drag_rect->hide();
			}
			newloc = new Location(temp_location->start(), temp_location->end(), rangename, (Location::Flags) flags);
			session->locations()->add (newloc, true);
                        XMLNode &after = session->locations()->get_state();
			session->add_command(new MementoCommand<Locations>(*(session->locations()), &before, &after));
			commit_reversible_command ();
			
			range_marker_drag_rect->hide();
			break;
		    }

		case CreateTransportMarker:
			// popup menu to pick loop or punch
			new_transport_marker_context_menu (&event->button, item);
			
			break;
		}
	} else {
		/* just a click, no pointer movement. remember that context menu stuff was handled elsewhere */

		if (Keyboard::no_modifier_keys_pressed (&event->button) && range_marker_op != CreateCDMarker) {

			nframes64_t start;
			nframes64_t end;

			start = session->locations()->first_mark_before (drag_info.grab_frame);
			end = session->locations()->first_mark_after (drag_info.grab_frame);
			
			if (end == max_frames) {
				end = session->current_end_frame ();
			}

			if (start == 0) {
				start = session->current_start_frame ();
			}

			switch (mouse_mode) {
			case MouseObject:
				/* find the two markers on either side and then make the selection from it */
				select_all_within (start, end, 0.0f, FLT_MAX, track_views, Selection::Set);
				break;

			case MouseRange:
				/* find the two markers on either side of the click and make the range out of it */
				selection->set (0, start, end);
				break;

			default:
				break;
			}
		} 
	}

	stop_canvas_autoscroll ();
}



void
Editor::start_mouse_zoom (ArdourCanvas::Item* item, GdkEvent* event)
{
	drag_info.item = item;
	drag_info.motion_callback = &Editor::drag_mouse_zoom;
	drag_info.finished_callback = &Editor::end_mouse_zoom;

	start_grab (event, zoom_cursor);

	show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
}

void
Editor::drag_mouse_zoom (ArdourCanvas::Item* item, GdkEvent* event)
{
	nframes64_t start;
	nframes64_t end;

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (drag_info.current_pointer_frame);
		
		if (drag_info.first_move) {
			snap_to (drag_info.grab_frame);
		}
	}
		
	if (drag_info.current_pointer_frame == drag_info.last_pointer_frame) return;

	/* base start and end on initial click position */
	if (drag_info.current_pointer_frame < drag_info.grab_frame) {
		start = drag_info.current_pointer_frame;
		end = drag_info.grab_frame;
	} else {
		end = drag_info.current_pointer_frame;
		start = drag_info.grab_frame;
	}
	
	if (start != end) {

		if (drag_info.first_move) {
			zoom_rect->show();
			zoom_rect->raise_to_top();
		}

		reposition_zoom_rect(start, end);

		drag_info.last_pointer_frame = drag_info.current_pointer_frame;
		drag_info.first_move = false;

		show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
	}
}

void
Editor::end_mouse_zoom (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (!drag_info.first_move) {
		drag_mouse_zoom (item, event);
		
		if (drag_info.grab_frame < drag_info.last_pointer_frame) {
			temporal_zoom_by_frame (drag_info.grab_frame, drag_info.last_pointer_frame, "mouse zoom");
		} else {
			temporal_zoom_by_frame (drag_info.last_pointer_frame, drag_info.grab_frame, "mouse zoom");
		}		
	} else {
		temporal_zoom_to_frame (false, drag_info.grab_frame);
		/*
		temporal_zoom_step (false);
		center_screen (drag_info.grab_frame);
		*/
	}

	zoom_rect->hide();
}

void
Editor::reposition_zoom_rect (nframes64_t start, nframes64_t end)
{
	double x1 = frame_to_pixel (start);
	double x2 = frame_to_pixel (end);
	double y2 = full_canvas_height - 1.0;

	zoom_rect->property_x1() = x1;
	zoom_rect->property_y1() = 1.0;
	zoom_rect->property_x2() = x2;
	zoom_rect->property_y2() = y2;
}

void
Editor::start_rubberband_select (ArdourCanvas::Item* item, GdkEvent* event)
{
	drag_info.item = item;
	drag_info.motion_callback = &Editor::drag_rubberband_select;
	drag_info.finished_callback = &Editor::end_rubberband_select;

	start_grab (event, cross_hair_cursor);

	show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
}

void
Editor::drag_rubberband_select (ArdourCanvas::Item* item, GdkEvent* event)
{
	nframes64_t start;
	nframes64_t end;
	double y1;
	double y2;

	/* use a bigger drag threshold than the default */

	if (abs ((int) (drag_info.current_pointer_frame - drag_info.grab_frame)) < 8) {
		return;
	}

 	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier()) && Config->get_rubberbanding_snaps_to_grid()) {
 		if (drag_info.first_move) {
 			snap_to (drag_info.grab_frame);
		} 
		snap_to (drag_info.current_pointer_frame);
 	}

	/* base start and end on initial click position */

	if (drag_info.current_pointer_frame < drag_info.grab_frame) {
		start = drag_info.current_pointer_frame;
		end = drag_info.grab_frame;
	} else {
		end = drag_info.current_pointer_frame;
		start = drag_info.grab_frame;
	}

	if (drag_info.current_pointer_y < drag_info.grab_y) {
		y1 = drag_info.current_pointer_y;
		y2 = drag_info.grab_y;
	} else {
		y2 = drag_info.current_pointer_y;
		y1 = drag_info.grab_y;
	}

	
	if (start != end || y1 != y2) {

		double x1 = frame_to_pixel (start);
		double x2 = frame_to_pixel (end);
		
		rubberband_rect->property_x1() = x1;
		rubberband_rect->property_y1() = y1;
		rubberband_rect->property_x2() = x2;
		rubberband_rect->property_y2() = y2;

		rubberband_rect->show();
		rubberband_rect->raise_to_top();
		
		drag_info.last_pointer_frame = drag_info.current_pointer_frame;
		drag_info.first_move = false;

		show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
	}
}

void
Editor::end_rubberband_select (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (!drag_info.first_move) {

		drag_rubberband_select (item, event);

		double y1,y2;
		if (drag_info.current_pointer_y < drag_info.grab_y) {
			y1 = drag_info.current_pointer_y;
			y2 = drag_info.grab_y;
		} else {
			y2 = drag_info.current_pointer_y;
			y1 = drag_info.grab_y;
		}


		Selection::Operation op = Keyboard::selection_type (event->button.state);
		bool commit;

		begin_reversible_command (_("rubberband selection"));

		if (drag_info.grab_frame < drag_info.last_pointer_frame) {
			commit = select_all_within (drag_info.grab_frame, drag_info.last_pointer_frame, y1, y2, track_views, op);
		} else {
			commit = select_all_within (drag_info.last_pointer_frame, drag_info.grab_frame, y1, y2, track_views, op);
		}		

		if (commit) {
			commit_reversible_command ();
		}
		
	} else {
		if (!getenv("ARDOUR_SAE")) {
			selection->clear_tracks();
		}
		selection->clear_regions();
		selection->clear_points ();
		selection->clear_lines ();
	}

	rubberband_rect->hide();
}


gint
Editor::mouse_rename_region (ArdourCanvas::Item* item, GdkEvent* event)
{
	using namespace Gtkmm2ext;

	ArdourPrompter prompter (false);

	prompter.set_prompt (_("Name for region:"));
	prompter.set_initial_text (clicked_regionview->region()->name());
	prompter.add_button (_("Rename"), Gtk::RESPONSE_ACCEPT);
	prompter.set_response_sensitive (Gtk::RESPONSE_ACCEPT, false);
	prompter.show_all ();
	switch (prompter.run ()) {
	case Gtk::RESPONSE_ACCEPT:
        string str;
		prompter.get_result(str);
		if (str.length()) {
			clicked_regionview->region()->set_name (str);
		}
		break;
	}
	return true;
}

void
Editor::start_time_fx (ArdourCanvas::Item* item, GdkEvent* event)
{
	drag_info.item = item;
	drag_info.motion_callback = &Editor::time_fx_motion;
	drag_info.finished_callback = &Editor::end_time_fx;

	start_grab (event);

	show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
}

void
Editor::time_fx_motion (ArdourCanvas::Item *item, GdkEvent* event)
{
	RegionView* rv = clicked_regionview;

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (drag_info.current_pointer_frame);
	}

	if (drag_info.current_pointer_frame == drag_info.last_pointer_frame) {
		return;
	}

	if (drag_info.current_pointer_frame > rv->region()->position()) {
		rv->get_time_axis_view().show_timestretch (rv->region()->position(), drag_info.current_pointer_frame);
	}

	drag_info.last_pointer_frame = drag_info.current_pointer_frame;
	drag_info.first_move = false;

	show_verbose_time_cursor (drag_info.current_pointer_frame, 10);
}

void
Editor::end_time_fx (ArdourCanvas::Item* item, GdkEvent* event)
{
	clicked_regionview->get_time_axis_view().hide_timestretch ();

 	if (drag_info.first_move) {
		return;
	}

	if (drag_info.last_pointer_frame < clicked_regionview->region()->position()) {
		/* backwards drag of the left edge - not usable */
		return;
	}
	
	nframes64_t newlen = drag_info.last_pointer_frame - clicked_regionview->region()->position();
#ifdef USE_RUBBERBAND
	float percentage = (float) ((double) newlen / (double) clicked_regionview->region()->length());
#else
	float percentage = (float) ((double) newlen - (double) clicked_regionview->region()->length()) / ((double) newlen) * 100.0f;
#endif	

	begin_reversible_command (_("timestretch"));

	// XXX how do timeFX on multiple regions ?

	RegionSelection rs;
	rs.add (clicked_regionview);

	if (time_stretch (rs, percentage) == 0) {
		session->commit_reversible_command ();
	}
}

void
Editor::mouse_brush_insert_region (RegionView* rv, nframes64_t pos)
{
	/* no brushing without a useful snap setting */

	// FIXME
	AudioRegionView* arv = dynamic_cast<AudioRegionView*>(rv);
	assert(arv);

	switch (snap_mode) {
	case SnapMagnetic:
		return; /* can't work because it allows region to be placed anywhere */
	default:
		break; /* OK */
	}

	switch (snap_type) {
	case SnapToMark:
		return;

	default:
		break;
	}

	/* don't brush a copy over the original */
	
	if (pos == rv->region()->position()) {
		return;
	}

	RouteTimeAxisView* atv = dynamic_cast<RouteTimeAxisView*>(&arv->get_time_axis_view());

	if (atv == 0 || !atv->is_audio_track()) {
		return;
	}

	boost::shared_ptr<Playlist> playlist = atv->playlist();
	double speed = atv->get_diskstream()->speed();
	
        XMLNode &before = playlist->get_state();
	playlist->add_region (boost::dynamic_pointer_cast<AudioRegion> (RegionFactory::create (arv->audio_region())), (nframes64_t) (pos * speed));
        XMLNode &after = playlist->get_state();
	session->add_command(new MementoCommand<Playlist>(*playlist.get(), &before, &after));
	
	// playlist is frozen, so we have to update manually
	
	playlist->Modified(); /* EMIT SIGNAL */
}

gint
Editor::track_height_step_timeout ()
{
	if (get_microseconds() - last_track_height_step_timestamp < 250000) {
		current_stepping_trackview = 0;
		return false;
	}
	return true;
}

