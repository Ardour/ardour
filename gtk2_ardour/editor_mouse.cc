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

#include "pbd/error.h"
#include <gtkmm2ext/utils.h>
#include <gtkmm2ext/tearoff.h>
#include "pbd/memento_command.h"
#include "pbd/basename.h"

#include "ardour_ui.h"
#include "editor.h"
#include "time_axis_view.h"
#include "audio_time_axis.h"
#include "audio_region_view.h"
#include "midi_region_view.h"
#include "marker.h"
#include "streamview.h"
#include "region_gain_line.h"
#include "automation_time_axis.h"
#include "control_point.h"
#include "prompter.h"
#include "utils.h"
#include "selection.h"
#include "keyboard.h"
#include "editing.h"
#include "rgb_macros.h"
#include "control_point_dialog.h"
#include "editor_drag.h"

#include "ardour/types.h"
#include "ardour/profile.h"
#include "ardour/route.h"
#include "ardour/audio_track.h"
#include "ardour/audio_diskstream.h"
#include "ardour/midi_diskstream.h"
#include "ardour/playlist.h"
#include "ardour/audioplaylist.h"
#include "ardour/audioregion.h"
#include "ardour/midi_region.h"
#include "ardour/dB.h"
#include "ardour/utils.h"
#include "ardour/region_factory.h"
#include "ardour/source_factory.h"

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
Editor::event_frame (GdkEvent const * event, double* pcx, double* pcy) const
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
	
	case MouseNote:
		if (mouse_note_button.get_active()) {
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
	
	case MouseNote:
		set_midi_edit_cursor (current_midi_edit_mode());
		break;
	}

	if (is_drawable()) {
	        track_canvas->get_window()->set_cursor(*current_canvas_cursor);
	}
}

void
Editor::set_mouse_mode (MouseMode m, bool force)
{
	if (_drag) {
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
	
	case MouseNote:
		mouse_note_button.set_active (true);
		set_midi_edit_cursor (current_midi_edit_mode());
		break;
	}

	if (midi_tools_tearoff) {
		if (mouse_mode == MouseNote) {
			midi_tools_tearoff->show();
		} else {
			midi_tools_tearoff->hide();
		}
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
	
	case MouseNote:
		if (next) set_mouse_mode (MouseObject);
		else set_mouse_mode (MouseAudition);
		break;
	}
}

void
Editor::midi_edit_mode_toggled (MidiEditMode m)
{
	if (ignore_midi_edit_mode_toggle) {
		return;
	}

	switch (m) {
	case MidiEditPencil:
		if (midi_tool_pencil_button.get_active())
			set_midi_edit_mode (m);
		break;

	case MidiEditSelect:
		if (midi_tool_select_button.get_active())
			set_midi_edit_mode (m);
		break;

	case MidiEditResize:
		if (midi_tool_resize_button.get_active())
			set_midi_edit_mode (m);
		break;

	case MidiEditErase:
		if (midi_tool_erase_button.get_active())
			set_midi_edit_mode (m);
		break;

	default:
		break;
	}

	set_midi_edit_cursor(m);
}	


void
Editor::set_midi_edit_mode (MidiEditMode m, bool force)
{
	if (_drag) {
		return;
	}

	if (!force && m == midi_edit_mode) {
		return;
	}
	
	midi_edit_mode = m;

	instant_save ();
	
	ignore_midi_edit_mode_toggle = true;

	switch (midi_edit_mode) {
	case MidiEditPencil:
		midi_tool_pencil_button.set_active (true);
		break;

	case MidiEditSelect:
		midi_tool_select_button.set_active (true);
		break;

	case MidiEditResize:
		midi_tool_resize_button.set_active (true);
		break;

	case MidiEditErase:
		midi_tool_erase_button.set_active (true);
		break;
	}

	ignore_midi_edit_mode_toggle = false;

	set_midi_edit_cursor (current_midi_edit_mode());

	if (is_drawable()) {
		track_canvas->get_window()->set_cursor(*current_canvas_cursor);
	}
}

void
Editor::set_midi_edit_cursor (MidiEditMode m)
{
	switch (midi_edit_mode) {
	case MidiEditPencil:
		current_canvas_cursor = midi_pencil_cursor;
		break;

	case MidiEditSelect:
		current_canvas_cursor = midi_select_cursor;
		break;

	case MidiEditResize:
		current_canvas_cursor = midi_resize_cursor;
		break;

	case MidiEditErase:
		current_canvas_cursor = midi_erase_cursor;
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
			set_selected_track_as_side_effect ();
		}
		break;
		
 	case RegionViewNameHighlight:
 	case RegionViewName:
		if (mouse_mode != MouseRange) {
			set_selected_regionview_from_click (press, op, true);
		} else if (event->type == GDK_BUTTON_PRESS) {
			set_selected_track_as_side_effect ();
		}
		break;


	case FadeInHandleItem:
	case FadeInItem:
	case FadeOutHandleItem:
	case FadeOutItem:
		if (mouse_mode != MouseRange) {
			set_selected_regionview_from_click (press, op, true);
		} else if (event->type == GDK_BUTTON_PRESS) {
			set_selected_track_as_side_effect ();
		}
		break;

	case ControlPointItem:
		set_selected_track_as_side_effect ();
		if (mouse_mode != MouseRange) {
			set_selected_control_point_from_click (op, false);
		}
		break;
		
	case StreamItem:
		/* for context click or range selection, select track */
		if (event->button.button == 3) {
			set_selected_track_as_side_effect ();
		} else if (event->type == GDK_BUTTON_PRESS && mouse_mode == MouseRange) {
			set_selected_track_as_side_effect ();
		}
		break;
		    
	case AutomationTrackItem:
		set_selected_track_as_side_effect (true);
		break;
		
	default:
		break;
	}
}

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

	if (_drag == 0 &&
	    (Keyboard::is_delete_event (&event->button) ||
	     Keyboard::is_context_menu_event (&event->button) ||
	     Keyboard::is_edit_event (&event->button))) {
		
		/* handled by button release */
		return true;
	}

	switch (event->button.button) {
	case 1:

		if (event->type == GDK_BUTTON_PRESS) {

			if (_drag) {
				_drag->item()->ungrab (event->button.time);
			}

			/* single mouse clicks on any of these item types operate
			   independent of mouse mode, mostly because they are
			   not on the main track canvas or because we want
			   them to be modeless.
			*/
			
			switch (item_type) {
			case PlayheadCursorItem:
				assert (_drag == 0);
				_drag = new CursorDrag (this, item, true);
				_drag->start_grab (event);
				return true;

			case MarkerItem:
				if (Keyboard::modifier_state_equals (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
					hide_marker (item, event);
				} else {
					assert (_drag == 0);
					_drag = new MarkerDrag (this, item);
					_drag->start_grab (event);
				}
				return true;

			case TempoMarkerItem:
				assert (_drag == 0);
				_drag = new TempoMarkerDrag (
					this,
					item,
					Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)
					);
				_drag->start_grab (event);
				return true;

			case MeterMarkerItem:
				assert (_drag == 0);

				_drag = new MeterMarkerDrag (
					this,
					item, 
					Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)
					);
				
				_drag->start_grab (event);
				return true;

			case MarkerBarItem:
			case TempoBarItem:
			case MeterBarItem:
				if (!Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
					assert (_drag == 0);
					_drag = new CursorDrag (this, &playhead_cursor->canvas_item, false);
					_drag->start_grab (event);
				}
				return true;
				break;

				
			case RangeMarkerBarItem:
				assert (_drag == 0);
				if (!Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {		
					_drag = new CursorDrag (this, &playhead_cursor->canvas_item, false);
				} else {
					_drag = new RangeMarkerBarDrag (this, item, RangeMarkerBarDrag::CreateRangeMarker); 
				}	
				_drag->start_grab (event);
				return true;
				break;

			case CdMarkerBarItem:
				assert (_drag == 0);
				if (!Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
					_drag = new CursorDrag (this, &playhead_cursor->canvas_item, false);
				} else {
					_drag = new RangeMarkerBarDrag (this, item, RangeMarkerBarDrag::CreateCDMarker);
				}
				_drag->start_grab (event);
				return true;
				break;

			case TransportMarkerBarItem:
				assert (_drag == 0);
				if (!Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
					_drag = new CursorDrag (this, &playhead_cursor->canvas_item, false);
				} else {
					_drag = new RangeMarkerBarDrag (this, item, RangeMarkerBarDrag::CreateTransportMarker);
				}
				_drag->start_grab (event);
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
				assert (_drag == 0);
				_drag = new SelectionDrag (this, item, SelectionDrag::SelectionStartTrim);
				_drag->start_grab (event);
				break;
				
			case EndSelectionTrimItem:
				assert (_drag == 0);
				_drag = new SelectionDrag (this, item, SelectionDrag::SelectionEndTrim);
				_drag->start_grab (event);
				break;

			case SelectionItem:
				if (Keyboard::modifier_state_contains 
				    (event->button.state, Keyboard::ModifierMask(Keyboard::SecondaryModifier))) {
					// contains and not equals because I can't use alt as a modifier alone.
					start_selection_grab (item, event);
				} else if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
					/* grab selection for moving */
					assert (_drag == 0);
					_drag = new SelectionDrag (this, item, SelectionDrag::SelectionMove);
					_drag->start_grab (event);
				} else {
					/* this was debated, but decided the more common action was to
					   make a new selection */
					assert (_drag == 0);
					_drag = new SelectionDrag (this, item, SelectionDrag::CreateSelection);
					_drag->start_grab (event);
				}
				break;

			default:
				assert (_drag == 0);
				_drag = new SelectionDrag (this, item, SelectionDrag::CreateSelection);
				_drag->start_grab (event);
			}
			return true;
			break;
			
		case MouseObject:
			if (Keyboard::modifier_state_contains (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)) &&
			    event->type == GDK_BUTTON_PRESS) {

				assert (_drag == 0);
				_drag = new RubberbandSelectDrag (this, item);
				_drag->start_grab (event);

			} else if (event->type == GDK_BUTTON_PRESS) {

				switch (item_type) {
				case FadeInHandleItem:
					assert (_drag == 0);
					_drag = new FadeInDrag (this, item, reinterpret_cast<RegionView*> (item->get_data("regionview")), selection->regions);
					_drag->start_grab (event);
					return true;
					
				case FadeOutHandleItem:
					assert (_drag == 0);
					_drag = new FadeOutDrag (this, item, reinterpret_cast<RegionView*> (item->get_data("regionview")), selection->regions);
					_drag->start_grab (event);
					return true;

				case RegionItem:
					if (Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)) {
						start_region_copy_grab (item, event, clicked_regionview);
					} else if (Keyboard::the_keyboard().key_is_down (GDK_b)) {
						start_region_brush_grab (item, event, clicked_regionview);
					} else {
						start_region_grab (item, event, clicked_regionview);
					}
					break;
					
				case RegionViewNameHighlight:
					assert (_drag == 0);
					_drag = new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer());
					_drag->start_grab (event);
					return true;
					break;
					
				case RegionViewName:
					/* rename happens on edit clicks */
					assert (_drag == 0);
					_drag = new TrimDrag (this, clicked_regionview->get_name_highlight(), clicked_regionview, selection->regions.by_layer());
					_drag->start_grab (event);
					return true;
					break;

				case ControlPointItem:
					assert (_drag == 0);
					_drag = new ControlPointDrag (this, item);
					_drag->start_grab (event);
					return true;
					break;
					
				case AutomationLineItem:
					assert (_drag == 0);
					_drag = new LineDrag (this, item);
					_drag->start_grab (event);
					return true;
					break;

				case StreamItem:
				case AutomationTrackItem:
					assert (_drag == 0);
					_drag = new RubberbandSelectDrag (this, item);
					_drag->start_grab (event);
					break;
					
#ifdef WITH_CMT
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
				case MarkerViewItem:
					start_markerview_grab(item, event) ;
					break ;
				case ImageFrameItem:
					start_imageframe_grab(item, event) ;
					break ;
#endif

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
				assert (_drag == 0);
				_drag = new RegionGainDrag (this, item);
				_drag->start_grab (event, current_canvas_cursor);
				break;

			case GainLineItem:
				assert (_drag == 0);
				_drag = new LineDrag (this, item);
				_drag->start_grab (event);
				return true;

			case ControlPointItem:
				assert (_drag == 0);
				_drag = new ControlPointDrag (this, item);
				_drag->start_grab (event);
				return true;
				break;

			default:
				break;
			}
			return true;
			break;

			switch (item_type) {
			case ControlPointItem:
				assert (_drag == 0);
				_drag = new ControlPointDrag (this, item);
				_drag->start_grab (event);
				break;

			case AutomationLineItem:
				assert (_drag == 0);
				_drag = new LineDrag (this, item);
				_drag->start_grab (event);
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
				assert (_drag == 0);
				_drag = new MouseZoomDrag (this, item);
				_drag->start_grab (event);
			}

			return true;
			break;

		case MouseTimeFX:
			if (item_type == RegionItem) {
				assert (_drag == 0);
				_drag = new TimeFXDrag (this, item, clicked_regionview, selection->regions.by_layer());
				_drag->start_grab (event);
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

		case MouseNote:
			assert (_drag == 0);
			_drag = new RegionCreateDrag (this, item, clicked_axisview);
			_drag->start_grab (event);
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
						start_region_copy_grab (item, event, clicked_regionview);
					} else {
						start_region_grab (item, event, clicked_regionview);
					}
					return true;
					break;
				case ControlPointItem:
					assert (_drag == 0);
					_drag = new ControlPointDrag (this, item);
					_drag->start_grab (event);
					return true;
					break;
					
				default:
					break;
				}
			}
			
			
			switch (item_type) {
			case RegionViewNameHighlight:
				assert (_drag == 0);
				_drag = new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer());
				_drag->start_grab (event);
				return true;
				break;
				
			case RegionViewName:
				assert (_drag == 0);
				_drag = new TrimDrag (this, clicked_regionview->get_name_highlight(), clicked_regionview, selection->regions.by_layer());
				_drag->start_grab (event);
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

	bool were_dragging = false;
	if (_drag) {
		bool const r = _drag->end_grab (event);
		delete _drag;
		_drag = 0;
		if (r) {
			/* grab dragged, so do nothing else */
			return true;
		}

		were_dragging = true;
	}
	
	button_selection (item, event, item_type);

	/* edit events get handled here */

	if (_drag == 0 && Keyboard::is_edit_event (&event->button)) {
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

		case ControlPointItem:
			edit_control_point (item);
			break;

		default:
			break;
		}
		return true;
	}

	/* context menu events get handled here */

	if (Keyboard::is_context_menu_event (&event->button)) {

		if (_drag == 0) {

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

#ifdef WITH_CMT
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
#endif
				
			default:
				break;
			}

			return true;
		}
	}

	/* delete events get handled here */

	if (_drag == 0 && Keyboard::is_delete_event (&event->button)) {

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
			
		case ControlPointItem:
			if (mouse_mode == MouseGain) {
				remove_gain_control_point (item, event);
			} else {
				remove_control_point (item, event);
			}
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
		case AutomationLineItem:
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
				atv = dynamic_cast<AutomationTimeAxisView*>(clicked_routeview);
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
				if (were_dragging) {
					dynamic_cast<AudioRegionView*>(clicked_regionview)->add_gain_point_event (item, event);
				}
				return true;
				break;
				
			case AutomationTrackItem:
				dynamic_cast<AutomationTimeAxisView*>(clicked_axisview)->
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
				session->request_transport_speed (0.0);
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
	case ControlPointItem:
		if (mouse_mode == MouseGain || mouse_mode == MouseObject) {
			cp = static_cast<ControlPoint*>(item->get_data ("control_point"));
			cp->set_visible (true);

			double at_x, at_y;
			at_x = cp->get_x();
			at_y = cp->get_y ();
			cp->item()->i2w (at_x, at_y);
			at_x += 10.0;
			at_y += 10.0;

			fraction = 1.0 - (cp->get_y() / cp->line().height());

			if (is_drawable() && !_scrubbing) {
			        track_canvas->get_window()->set_cursor (*fader_cursor);
			}

			last_item_entered_n++;
			set_verbose_canvas_cursor (cp->line().get_verbose_cursor_string (fraction), at_x, at_y);
			if (last_item_entered_n < 10) {
				show_verbose_canvas_cursor ();
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
			
	case AutomationLineItem:
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

#ifdef WITH_CMT
	case ImageFrameHandleStartItem:
	case ImageFrameHandleEndItem:
	case MarkerViewHandleStartItem:
	case MarkerViewHandleEndItem:
#endif

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
	case AutomationLineItem:
	case ControlPointItem:
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
	case ControlPointItem:
		cp = reinterpret_cast<ControlPoint*>(item->get_data ("control_point"));
		if (cp->line().the_list()->interpolation() != AutomationList::Discrete) {
			if (cp->line().npoints() > 1 && !cp->selected()) {
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

#ifdef WITH_CMT
	case ImageFrameHandleStartItem:
	case ImageFrameHandleEndItem:
	case MarkerViewHandleStartItem:
	case MarkerViewHandleEndItem:
#endif

		if (is_drawable()) {
			track_canvas->get_window()->set_cursor (*current_canvas_cursor);
		}
		break;

	case GainLineItem:
	case AutomationLineItem:
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
		session->request_locate (_drag->current_pointer_frame(), false);
		session->request_transport_speed (0.1);
		scrubbing_direction = 1;
		
	} else {
		
		if (last_scrub_x > _drag->current_pointer_x()) {
			
			/* pointer moved to the left */
			
			if (scrubbing_direction > 0) {
				
				/* we reversed direction to go backwards */
				
				scrub_reversals++;
				scrub_reverse_distance += (int) (last_scrub_x - _drag->current_pointer_x());
				
			} else {
				
				/* still moving to the left (backwards) */
				
				scrub_reversals = 0;
				scrub_reverse_distance = 0;
				
				delta = 0.01 * (last_scrub_x - _drag->current_pointer_x());
				session->request_transport_speed (session->transport_speed() - delta);
			}
			
		} else {
			/* pointer moved to the right */
			
			if (scrubbing_direction < 0) {
				/* we reversed direction to go forward */
				
				scrub_reversals++;
				scrub_reverse_distance += (int) (_drag->current_pointer_x() - last_scrub_x);
				
			} else {
				/* still moving to the right */
				
				scrub_reversals = 0;
				scrub_reverse_distance = 0;
				
				delta = 0.01 * (_drag->current_pointer_x() - last_scrub_x);
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
	
	last_scrub_x = _drag->current_pointer_x();
}

bool
Editor::motion_handler (ArdourCanvas::Item* item, GdkEvent* event, bool from_autoscroll)
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

	bool handled = false;
	if (_drag) {
		handled = _drag->motion_handler (event, from_autoscroll);
	}

	switch (mouse_mode) {
	case MouseAudition:
		if (_scrubbing) {
			scrub ();
		}
		break;

	default:
		break;
	}

	if (!handled) {
		return false;
	}

	track_canvas_motion (event);
	return true;
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
	if (control_point->line().is_last_point(*control_point) ||
		control_point->line().is_first_point(*control_point)) {	
		return;
	}

	control_point->line().remove_point (*control_point);
}

void
Editor::remove_control_point (ArdourCanvas::Item* item, GdkEvent* event)
{
	ControlPoint* control_point;

	if ((control_point = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"))) == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	control_point->line().remove_point (*control_point);
}

void
Editor::edit_control_point (ArdourCanvas::Item* item)
{
	ControlPoint* p = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"));

	if (p == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	ControlPointDialog d (p);
	d.set_position (Gtk::WIN_POS_MOUSE);
	ensure_float (d);

	if (d.run () != RESPONSE_ACCEPT) {
		return;
	}

	p->line().modify_point_y (*p, d.get_y_fraction ());
}


void
Editor::visible_order_range (int* low, int* high) const
{
	*low = TimeAxisView::max_order ();
	*high = 0;
	
	for (TrackViewList::const_iterator i = track_views.begin(); i != track_views.end(); ++i) {

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (*i);
		
		if (!rtv->hidden()) {
			
			if (*high < rtv->order()) {
				*high = rtv->order ();
			}
			
			if (*low > rtv->order()) {
				*low = rtv->order ();
			}
		}
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
		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(tv);
		double speed = 1.0;
		if (rtv && rtv->is_track()) {
			speed = rtv->get_diskstream()->speed();
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
		set_verbose_canvas_cursor (buf, _drag->current_pointer_x() + offset - horizontal_adjustment.get_value(), _drag->current_pointer_y() + offset - vertical_adjustment.get_value() + canvas_timebars_vsize);
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
		set_verbose_canvas_cursor (buf, _drag->current_pointer_x() + offset, _drag->current_pointer_y() + offset);
	}

	show_verbose_canvas_cursor ();
}

void
Editor::collect_new_region_view (RegionView* rv)
{
	latest_regionviews.push_back (rv);
}

void
Editor::collect_and_select_new_region_view (RegionView* rv)
{
 	selection->add(rv);
	latest_regionviews.push_back (rv);
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
Editor::single_contents_trim (RegionView& rv, nframes64_t frame_delta, bool left_direction, bool swap_direction, bool obey_snap)
{
	boost::shared_ptr<Region> region (rv.region());

	if (region->locked()) {
		return;
	}

	nframes64_t new_bound;

	double speed = 1.0;
	TimeAxisView* tvp = clicked_axisview;
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);

	if (tv && tv->is_track()) {
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
Editor::single_start_trim (RegionView& rv, nframes64_t frame_delta, bool left_direction, bool obey_snap, bool no_overlap)
{
	boost::shared_ptr<Region> region (rv.region());	

	if (region->locked()) {
		return;
	}

	nframes64_t new_bound;

	double speed = 1.0;
	TimeAxisView* tvp = clicked_axisview;
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);

	if (tv && tv->is_track()) {
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
	
	nframes64_t pre_trim_first_frame = region->first_frame();

	region->trim_front ((nframes64_t) (new_bound * speed), this);
  
	if (no_overlap) {
		//Get the next region on the left of this region and shrink/expand it.
		boost::shared_ptr<Playlist> playlist (region->playlist());
		boost::shared_ptr<Region> region_left = playlist->find_next_region (pre_trim_first_frame, End, 0);
		
		bool regions_touching = false;

		if (region_left != 0 && (pre_trim_first_frame == region_left->last_frame() + 1)){
		    regions_touching = true;
		}

		//Only trim region on the left if the first frame has gone beyond the left region's last frame.
		if (region_left != 0 && 
			(region_left->last_frame() > region->first_frame() || regions_touching)) 
		{
			region_left->trim_end(region->first_frame(), this);
		}
	}

	

	rv.region_changed (Change (LengthChanged|PositionChanged|StartChanged));
}

void
Editor::single_end_trim (RegionView& rv, nframes64_t frame_delta, bool left_direction, bool obey_snap, bool no_overlap)
{
	boost::shared_ptr<Region> region (rv.region());

	if (region->locked()) {
		return;
	}

	nframes64_t new_bound;

	double speed = 1.0;
	TimeAxisView* tvp = clicked_axisview;
	RouteTimeAxisView* tv = dynamic_cast<RouteTimeAxisView*>(tvp);

	if (tv && tv->is_track()) {
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

	nframes64_t pre_trim_last_frame = region->last_frame();

	region->trim_end ((nframes64_t) (new_bound * speed), this);

	if (no_overlap) {
		//Get the next region on the right of this region and shrink/expand it.
		boost::shared_ptr<Playlist> playlist (region->playlist());
		boost::shared_ptr<Region> region_right = playlist->find_next_region (pre_trim_last_frame, Start, 1);

		bool regions_touching = false;

		if (region_right != 0 && (pre_trim_last_frame == region_right->first_frame() - 1)){
		    regions_touching = true;
		}

		//Only trim region on the right if the last frame has gone beyond the right region's first frame.
		if (region_right != 0 &&
			(region_right->first_frame() < region->last_frame() || regions_touching)) 
		{
			region_right->trim_front(region->last_frame() + 1, this);
		}
		
		rv.region_changed (Change (LengthChanged|PositionChanged|StartChanged));
	}
	else {
		rv.region_changed (LengthChanged);
	}
}


void
Editor::point_trim (GdkEvent* event)
{
	RegionView* rv = clicked_regionview;

	nframes64_t new_bound = _drag->current_pointer_frame();

	if (!Keyboard::modifier_state_contains (event->button.state, Keyboard::snap_modifier())) {
		snap_to (new_bound);
	}

	/* Choose action dependant on which button was pressed */
	switch (event->button.button) {
	case 1:
		begin_reversible_command (_("Start point trim"));

		if (selection->selected (rv)) {
			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin();
			     i != selection->regions.by_layer().end(); ++i)
			{
				if ( (*i) == NULL){
				    cerr << "region view contains null region" << endl;
				}

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
Editor::mouse_brush_insert_region (RegionView* rv, nframes64_t pos)
{
	/* no brushing without a useful snap setting */

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

	RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*>(&rv->get_time_axis_view());

	if (rtv == 0 || !rtv->is_track()) {
		return;
	}

	boost::shared_ptr<Playlist> playlist = rtv->playlist();
	double speed = rtv->get_diskstream()->speed();
	
	XMLNode &before = playlist->get_state();
	playlist->add_region (RegionFactory::create (rv->region()), (nframes64_t) (pos * speed));
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

void
Editor::start_region_grab (ArdourCanvas::Item* item, GdkEvent* event, RegionView* region_view)
{
	assert (region_view);

	_region_motion_group->raise_to_top ();
	
	assert (_drag == 0);
	
	if (Config->get_edit_mode() == Splice) {
		_drag = new RegionSpliceDrag (this, item, region_view, selection->regions.by_layer());
	} else {
		_drag = new RegionMoveDrag (this, item, region_view, selection->regions.by_layer(), false, false);
	}
	
	_drag->start_grab (event);

	begin_reversible_command (_("move region(s)"));

	/* sync the canvas to what we think is its current state */
	track_canvas->update_now();
}

void
Editor::start_region_copy_grab (ArdourCanvas::Item* item, GdkEvent* event, RegionView* region_view)
{
	assert (region_view);
	assert (_drag == 0);
	
	_region_motion_group->raise_to_top ();
	_drag = new RegionMoveDrag (this, item, region_view, selection->regions.by_layer(), false, true);
	_drag->start_grab(event);
}

void
Editor::start_region_brush_grab (ArdourCanvas::Item* item, GdkEvent* event, RegionView* region_view)
{
	assert (region_view);
	assert (_drag == 0);
	
	if (Config->get_edit_mode() == Splice) {
		return;
	}

	_drag = new RegionMoveDrag (this, item, region_view, selection->regions.by_layer(), true, false);
	_drag->start_grab (event);
	
	begin_reversible_command (_("Drag region brush"));
}

/** Start a grab where a time range is selected, track(s) are selected, and the
 *  user clicks and drags a region with a modifier in order to create a new region containing
 *  the section of the clicked region that lies within the time range.
 */
void
Editor::start_selection_grab (ArdourCanvas::Item* item, GdkEvent* event)
{
	if (clicked_regionview == 0) {
		return;
	}

	/* lets try to create new Region for the selection */

	vector<boost::shared_ptr<Region> > new_regions;
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
	sigc::connection c = clicked_routeview->view()->RegionViewAdded.connect (mem_fun(*this, &Editor::collect_new_region_view));
	
	/* A selection grab currently creates two undo/redo operations, one for 
	   creating the new region and another for moving it.
	*/

	begin_reversible_command (_("selection grab"));

	boost::shared_ptr<Playlist> playlist = clicked_axisview->playlist();

	XMLNode *before = &(playlist->get_state());
	clicked_routeview->playlist()->add_region (region, selection->time[clicked_selection].start);
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
	
	assert (_drag == 0);
	_drag = new RegionMoveDrag (this, latest_regionviews.front()->get_canvas_group(), latest_regionviews.front(), latest_regionviews, false, false);
	_drag->start_grab (event);
}

void
Editor::break_drag ()
{
	if (_drag) {
		_drag->break_drag ();
	}
}
