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
#include "pbd/enumwriter.h"
#include "pbd/memento_command.h"
#include "pbd/basename.h"
#include "pbd/stateful_diff_command.h"

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/utils.h"
#include "gtkmm2ext/tearoff.h"

#include "ardour_ui.h"
#include "actions.h"
#include "canvas-note.h"
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
#include "automation_region_view.h"
#include "edit_note_dialog.h"
#include "mouse_cursors.h"
#include "editor_cursors.h"
#include "verbose_cursor.h"

#include "ardour/audioregion.h"
#include "ardour/operations.h"
#include "ardour/playlist.h"
#include "ardour/profile.h"
#include "ardour/region_factory.h"
#include "ardour/route.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include <bitset>

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;
using namespace Gtk;
using namespace Editing;
using Gtkmm2ext::Keyboard;

bool
Editor::mouse_frame (framepos_t& where, bool& in_track_canvas) const
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

framepos_t
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

Gdk::Cursor*
Editor::which_grabber_cursor ()
{
	Gdk::Cursor* c = _cursors->grabber;

	if (_internal_editing) {
		switch (mouse_mode) {
		case MouseDraw:
			c = _cursors->midi_pencil;
			break;

		case MouseObject:
			c = _cursors->grabber_note;
			break;

		case MouseTimeFX:
			c = _cursors->midi_resize;
			break;

		default:
			break;
		}

	} else {

		switch (_edit_point) {
		case EditAtMouse:
			c = _cursors->grabber_edit_point;
			break;
		default:
			boost::shared_ptr<Movable> m = _movable.lock();
			if (m && m->locked()) {
				c = _cursors->speaker;
			}
			break;
		}
	}

	return c;
}

void
Editor::set_current_trimmable (boost::shared_ptr<Trimmable> t)
{
	boost::shared_ptr<Trimmable> st = _trimmable.lock();

	if (!st || st == t) {
		_trimmable = t;
		set_canvas_cursor ();
	}
}

void
Editor::set_current_movable (boost::shared_ptr<Movable> m)
{
	boost::shared_ptr<Movable> sm = _movable.lock();

	if (!sm || sm != m) {
		_movable = m;
		set_canvas_cursor ();
	}
}

void
Editor::set_canvas_cursor ()
{
	switch (mouse_mode) {
	case MouseRange:
		current_canvas_cursor = _cursors->selector;
		break;

	case MouseObject:
		current_canvas_cursor = which_grabber_cursor();
		break;

	case MouseDraw:
		current_canvas_cursor = _cursors->midi_pencil;
		break;

	case MouseGain:
		current_canvas_cursor = _cursors->cross_hair;
		break;

	case MouseZoom:
		if (Keyboard::the_keyboard().key_is_down (GDK_Control_L)) {
			current_canvas_cursor = _cursors->zoom_out;
		} else {
			current_canvas_cursor = _cursors->zoom_in;
		}
		break;

	case MouseTimeFX:
		current_canvas_cursor = _cursors->time_fx; // just use playhead
		break;

	case MouseAudition:
		current_canvas_cursor = _cursors->speaker;
		break;
	}

	switch (_join_object_range_state) {
	case JOIN_OBJECT_RANGE_NONE:
		break;
	case JOIN_OBJECT_RANGE_OBJECT:
		current_canvas_cursor = which_grabber_cursor ();
		break;
	case JOIN_OBJECT_RANGE_RANGE:
		current_canvas_cursor = _cursors->selector;
		break;
	}

	/* up-down cursor as a cue that automation can be dragged up and down when in join object/range mode */
	if (smart_mode_action->get_active()) {
		double x, y;
		get_pointer_position (x, y);
		ArdourCanvas::Item* i = track_canvas->get_item_at (x, y);
		if (i && i->property_parent() && (*i->property_parent()).get_data (X_("timeselection"))) {
			pair<TimeAxisView*, int> tvp = trackview_by_y_position (_last_motion_y + vertical_adjustment.get_value() - canvas_timebars_vsize);
			if (dynamic_cast<AutomationTimeAxisView*> (tvp.first)) {
				current_canvas_cursor = _cursors->up_down;
			}
		}
	}

	set_canvas_cursor (current_canvas_cursor, true);
}

void
Editor::set_mouse_mode (MouseMode m, bool force)
{
	if (_drags->active ()) {
		return;
	}

	if (!force && m == mouse_mode) {
		return;
	}

	Glib::RefPtr<Action> act;

	switch (m) {
	case MouseRange:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-range"));
		break;

	case MouseObject:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-object"));
		break;

	case MouseDraw:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-draw"));
		break;

	case MouseGain:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-gain"));
		break;

	case MouseZoom:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-zoom"));
		break;

	case MouseTimeFX:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-timefx"));
		break;

	case MouseAudition:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-audition"));
		break;
	}

	assert (act);

	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	assert (tact);

	/* go there and back to ensure that the toggled handler is called to set up mouse_mode */
	tact->set_active (false);
	tact->set_active (true);

	MouseModeChanged (); /* EMIT SIGNAL */
}

void
Editor::mouse_mode_toggled (MouseMode m)
{
	Glib::RefPtr<Action> act;
	Glib::RefPtr<ToggleAction> tact;

	switch (m) {
	case MouseRange:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-range"));
		break;

	case MouseObject:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-object"));
		break;

	case MouseDraw:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-draw"));
		break;

	case MouseGain:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-gain"));
		break;

	case MouseZoom:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-zoom"));
		break;

	case MouseTimeFX:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-timefx"));
		break;

	case MouseAudition:
		act = ActionManager::get_action (X_("MouseMode"), X_("set-mouse-mode-audition"));
		break;
	}

	assert (act);

	tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
	assert (tact);

	if (!tact->get_active()) {
		/* this was just the notification that the old mode has been
		 * left. we'll get called again with the new mode active in a
		 * jiffy.
		 */
		return;
	}

	switch (m) {
	case MouseDraw:
		act = ActionManager::get_action (X_("MouseMode"), X_("toggle-internal-edit"));
		tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		tact->set_active (true);
		break;
	default:
		break;
	}

	mouse_mode = m;

	instant_save ();

	if (!internal_editing()) {
		if (mouse_mode != MouseRange && mouse_mode != MouseGain && _join_object_range_state == JOIN_OBJECT_RANGE_NONE) {

			/* in all modes except range, gain and joined object/range, hide the range selection,
			   show the object (region) selection.
			*/

			for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
				(*i)->hide_selection ();
			}

		} else {

			/*
			  in range or object/range mode, show the range selection.
			*/

			for (TrackSelection::iterator i = selection->tracks.begin(); i != selection->tracks.end(); ++i) {
				(*i)->show_selection (selection->time);
			}
		}
	}

	set_canvas_cursor ();
	set_gain_envelope_visibility ();

	MouseModeChanged (); /* EMIT SIGNAL */
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
		if (next) set_mouse_mode (MouseDraw);
		else set_mouse_mode (MouseObject);
		break;

	case MouseDraw:
		if (next) set_mouse_mode (MouseZoom);
		else set_mouse_mode (MouseRange);
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
				set_mouse_mode (MouseDraw);
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

bool
Editor::toggle_internal_editing_from_double_click (GdkEvent* event)
{
	if (_drags->active()) {
		_drags->end_grab (event);
	} 

	ActionManager::do_action ("MouseMode", "toggle-internal-edit");

	/* prevent reversion of edit cursor on button release */
	
	pre_press_cursor = 0;

	return true;
}

void
Editor::button_selection (ArdourCanvas::Item* /*item*/, GdkEvent* event, ItemType item_type)
{
 	/* in object/audition/timefx/gain-automation mode,
	   any button press sets the selection if the object
	   can be selected. this is a bit of hack, because
	   we want to avoid this if the mouse operation is a
	   region alignment.

	   note: not dbl-click or triple-click

	   Also note that there is no region selection in internal edit mode, otherwise
	   for operations operating on the selection (e.g. cut) it is not obvious whether
	   to cut notes or regions.
	*/

	if (((mouse_mode != MouseObject) &&
	     (_join_object_range_state != JOIN_OBJECT_RANGE_OBJECT) &&
	     (mouse_mode != MouseAudition || item_type != RegionItem) &&
	     (mouse_mode != MouseTimeFX || item_type != RegionItem) &&
	     (mouse_mode != MouseGain) &&
	     (mouse_mode != MouseRange) &&
	     (mouse_mode != MouseDraw)) ||
	    ((event->type != GDK_BUTTON_PRESS && event->type != GDK_BUTTON_RELEASE) || event->button.button > 3) ||
	    (internal_editing() && mouse_mode != MouseTimeFX)) {

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

	Selection::Operation op = ArdourKeyboard::selection_type (event->button.state);
	bool press = (event->type == GDK_BUTTON_PRESS);

	switch (item_type) {
	case RegionItem:
		if (!doing_range_stuff()) {
			set_selected_regionview_from_click (press, op);
		}
		
		if (press) {
			if (doing_range_stuff()) {
				/* don't change the selection unless the
				   clicked track is not currently selected. if
				   so, "collapse" the selection to just this
				   track
				*/
				if (!selection->selected (clicked_axisview)) {
					set_selected_track_as_side_effect (Selection::Set);
				}
			}
		}
		break;

 	case RegionViewNameHighlight:
 	case RegionViewName:
	case LeftFrameHandle:
	case RightFrameHandle:
		if (doing_object_stuff() || (mouse_mode != MouseRange && mouse_mode != MouseObject)) {
			set_selected_regionview_from_click (press, op);
		} else if (event->type == GDK_BUTTON_PRESS) {
			set_selected_track_as_side_effect (op);
		}
		break;

	case FadeInHandleItem:
	case FadeInItem:
	case FadeOutHandleItem:
	case FadeOutItem:
	case StartCrossFadeItem:
	case EndCrossFadeItem:
		if (doing_object_stuff() || (mouse_mode != MouseRange && mouse_mode != MouseObject)) {
			set_selected_regionview_from_click (press, op);
		} else if (event->type == GDK_BUTTON_PRESS) {
			set_selected_track_as_side_effect (op);
		}
		break;

	case ControlPointItem:
		set_selected_track_as_side_effect (op);
		if (doing_object_stuff() || (mouse_mode != MouseRange && mouse_mode != MouseObject)) {
			set_selected_control_point_from_click (press, op);
		}
		break;

	case StreamItem:
		/* for context click, select track */
		if (event->button.button == 3) {
			selection->clear_tracks ();
			set_selected_track_as_side_effect (op);
		}
		break;

	case AutomationTrackItem:
		set_selected_track_as_side_effect (op);
		break;

	default:
		break;
	}
}

bool
Editor::button_press_handler_1 (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	/* single mouse clicks on any of these item types operate
	   independent of mouse mode, mostly because they are
	   not on the main track canvas or because we want
	   them to be modeless.
	*/

	switch (item_type) {
	case PlayheadCursorItem:
		_drags->set (new CursorDrag (this, item, true), event);
		return true;

	case MarkerItem:
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {
			hide_marker (item, event);
		} else {
			_drags->set (new MarkerDrag (this, item), event);
		}
		return true;

	case TempoMarkerItem:
	{
		TempoMarker* m = reinterpret_cast<TempoMarker*> (item->get_data ("marker"));
		assert (m);
		if (m->tempo().movable ()) {
			_drags->set (
				new TempoMarkerDrag (
					this,
					item,
					Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)
					),
				event
				);
			return true;
		} else {
			return false;
		}
	}

	case MeterMarkerItem:
	{
		MeterMarker* m = reinterpret_cast<MeterMarker*> (item->get_data ("marker"));
		assert (m);
		if (m->meter().movable ()) {
			_drags->set (
				new MeterMarkerDrag (
					this,
					item,
					Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)
					),
				event
				);
			return true;
		} else {
			return false;
		}
	}

	case MarkerBarItem:
	case TempoBarItem:
	case MeterBarItem:
		if (!Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
			_drags->set (new CursorDrag (this, &playhead_cursor->canvas_item, false), event);
		}
		return true;
		break;


	case RangeMarkerBarItem:
		if (!Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
			_drags->set (new CursorDrag (this, &playhead_cursor->canvas_item, false), event);
		} else {
			_drags->set (new RangeMarkerBarDrag (this, item, RangeMarkerBarDrag::CreateRangeMarker), event);
		}
		return true;
		break;

	case CdMarkerBarItem:
		if (!Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
			_drags->set (new CursorDrag (this, &playhead_cursor->canvas_item, false), event);
		} else {
			_drags->set (new RangeMarkerBarDrag (this, item, RangeMarkerBarDrag::CreateCDMarker), event);
		}
		return true;
		break;

	case TransportMarkerBarItem:
		if (!Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
			_drags->set (new CursorDrag (this, &playhead_cursor->canvas_item, false), event);
		} else {
			_drags->set (new RangeMarkerBarDrag (this, item, RangeMarkerBarDrag::CreateTransportMarker), event);
		}
		return true;
		break;

	default:
		break;
	}

	if (_join_object_range_state == JOIN_OBJECT_RANGE_OBJECT) {
		/* special case: allow trim of range selections in joined object mode;
		   in theory eff should equal MouseRange in this case, but it doesn't
		   because entering the range selection canvas item results in entered_regionview
		   being set to 0, so update_join_object_range_location acts as if we aren't
		   over a region.
		*/
		if (item_type == StartSelectionTrimItem) {
			_drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionStartTrim), event);
		} else if (item_type == EndSelectionTrimItem) {
			_drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionEndTrim), event);
		}
	}

	Editing::MouseMode eff = effective_mouse_mode ();

	/* special case: allow drag of region fade in/out in object mode with join object/range enabled */
	if (item_type == FadeInHandleItem || item_type == FadeOutHandleItem) {
		eff = MouseObject;
	}

	switch (eff) {
	case MouseRange:
		switch (item_type) {
		case StartSelectionTrimItem:
			_drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionStartTrim), event);
			break;

		case EndSelectionTrimItem:
			_drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionEndTrim), event);
			break;

		case SelectionItem:
			if (Keyboard::modifier_state_contains
			    (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier))) {
				// contains and not equals because I can't use alt as a modifier alone.
				start_selection_grab (item, event);
			} else if (Keyboard::modifier_state_equals (event->button.state, Keyboard::SecondaryModifier)) {
				/* grab selection for moving */
				_drags->set (new SelectionDrag (this, item, SelectionDrag::SelectionMove), event);
			} else {
				double const y = event->button.y + vertical_adjustment.get_value() - canvas_timebars_vsize;
				pair<TimeAxisView*, int> tvp = trackview_by_y_position (y);
				if (tvp.first) {
					AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (tvp.first);
					if (smart_mode_action->get_active() && atv) {
						/* smart "join" mode: drag automation */
						_drags->set (new AutomationRangeDrag (this, atv, selection->time), event, _cursors->up_down);
					} else {
						/* this was debated, but decided the more common action was to
						   make a new selection */
						_drags->set (new SelectionDrag (this, item, SelectionDrag::CreateSelection), event);
					}
				}
			}
			break;

		case StreamItem:
			if (internal_editing()) {
				if (dynamic_cast<MidiTimeAxisView*> (clicked_axisview)) {
					_drags->set (new RegionCreateDrag (this, item, clicked_axisview), event);
					return true;
				} 
			} else {
				_drags->set (new SelectionDrag (this, item, SelectionDrag::CreateSelection), event);
				return true;
			}
			break;

		case RegionViewNameHighlight:
			if (!clicked_regionview->region()->locked()) {
				RegionSelection s = get_equivalent_regions (selection->regions, Properties::edit.property_id);
				_drags->set (new TrimDrag (this, item, clicked_regionview, s.by_layer()), event);
				return true;
			}
			break;

		case LeftFrameHandle:
		case RightFrameHandle:
			if (!internal_editing() && doing_object_stuff() && !clicked_regionview->region()->locked()) {
				RegionSelection s = get_equivalent_regions (selection->regions, Properties::edit.property_id);
				_drags->set (new TrimDrag (this, item, clicked_regionview, s.by_layer()), event);
				return true;
			}
			break;

		default:
			if (!internal_editing()) {
				_drags->set (new SelectionDrag (this, item, SelectionDrag::CreateSelection), event);
			}
		}
		return true;
		break;

	case MouseDraw:
		switch (item_type) {
		case NoteItem:
			if (internal_editing()) {
				/* trim notes if we're in internal edit mode and near the ends of the note */
				ArdourCanvas::CanvasNote* cn = dynamic_cast<ArdourCanvas::CanvasNote*> (item);
				if (cn && cn->big_enough_to_trim() && cn->mouse_near_ends()) {
					_drags->set (new NoteResizeDrag (this, item), event, current_canvas_cursor);
				} else {
					_drags->set (new NoteDrag (this, item), event);
				}
				return true;
			}
			break;

		default:
			break;
		}
		break;

	case MouseObject:
		switch (item_type) {
		case NoteItem:
			if (internal_editing()) {
				ArdourCanvas::CanvasNoteEvent* cn = dynamic_cast<ArdourCanvas::CanvasNoteEvent*> (item);
				if (cn->mouse_near_ends()) {
					_drags->set (new NoteResizeDrag (this, item), event, current_canvas_cursor);
				} else {
					_drags->set (new NoteDrag (this, item), event);
				}
				return true;
			}
			break;

		default:
			break;
		}

		if (Keyboard::modifier_state_contains (event->button.state, Keyboard::ModifierMask(Keyboard::PrimaryModifier|Keyboard::SecondaryModifier)) &&
		    event->type == GDK_BUTTON_PRESS) {

			_drags->set (new EditorRubberbandSelectDrag (this, item), event);

		} else if (event->type == GDK_BUTTON_PRESS) {

			switch (item_type) {
			case FadeInHandleItem:
			{
				RegionSelection s = get_equivalent_regions (selection->regions, Properties::edit.property_id);
				_drags->set (new FadeInDrag (this, item, reinterpret_cast<RegionView*> (item->get_data("regionview")), s), event, _cursors->fade_in);
				return true;
			}

			case FadeOutHandleItem:
			{
				RegionSelection s = get_equivalent_regions (selection->regions, Properties::edit.property_id);
				_drags->set (new FadeOutDrag (this, item, reinterpret_cast<RegionView*> (item->get_data("regionview")), s), event, _cursors->fade_out);
				return true;
			}

			case StartCrossFadeItem:
				_drags->set (new CrossfadeEdgeDrag (this, reinterpret_cast<AudioRegionView*>(item->get_data("regionview")), item, true), event, 0);
				break;

			case EndCrossFadeItem:
				_drags->set (new CrossfadeEdgeDrag (this, reinterpret_cast<AudioRegionView*>(item->get_data("regionview")), item, false), event, 0);
				break;

			case FeatureLineItem:
			{
				if (Keyboard::modifier_state_contains (event->button.state, Keyboard::TertiaryModifier)) {
					remove_transient(item);
					return true;
				}

				_drags->set (new FeatureLineDrag (this, item), event);
				return true;
				break;
			}

			case RegionItem:
				if (dynamic_cast<AutomationRegionView*> (clicked_regionview)) {
					/* click on an automation region view; do nothing here and let the ARV's signal handler
					   sort it out.
					*/
					break;
				}

				if (internal_editing ()) {
					if (event->type == GDK_2BUTTON_PRESS && event->button.button == 1) {
						Glib::RefPtr<Action> act = ActionManager::get_action (X_("MouseMode"), X_("toggle-internal-edit"));
						act->activate ();
					}
					break;
				}

				/* click on a normal region view */
				if (Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)) {
					add_region_copy_drag (item, event, clicked_regionview);
				} else if (Keyboard::the_keyboard().key_is_down (GDK_b)) {
					add_region_brush_drag (item, event, clicked_regionview);
				} else {
					add_region_drag (item, event, clicked_regionview);
				}


				if (!internal_editing() && (_join_object_range_state == JOIN_OBJECT_RANGE_RANGE && !selection->regions.empty())) {
					_drags->add (new SelectionDrag (this, clicked_axisview->get_selection_rect (clicked_selection)->rect, SelectionDrag::SelectionMove));
				}

				_drags->start_grab (event);
				break;

			case RegionViewNameHighlight:
			case LeftFrameHandle:
                        case RightFrameHandle:
				if (!clicked_regionview->region()->locked()) {
					RegionSelection s = get_equivalent_regions (selection->regions, Properties::edit.property_id);
					_drags->set (new TrimDrag (this, item, clicked_regionview, s.by_layer()), event);
					return true;
				}
				break;

			case RegionViewName:
			{
				/* rename happens on edit clicks */
				RegionSelection s = get_equivalent_regions (selection->regions, Properties::edit.property_id);
				_drags->set (new TrimDrag (this, clicked_regionview->get_name_highlight(), clicked_regionview, s.by_layer()), event);
				return true;
				break;
			}

			case ControlPointItem:
				_drags->set (new ControlPointDrag (this, item), event);
				return true;
				break;

			case AutomationLineItem:
				_drags->set (new LineDrag (this, item), event);
				return true;
				break;

			case StreamItem:
				if (internal_editing()) {
					if (dynamic_cast<MidiTimeAxisView*> (clicked_axisview)) {
						_drags->set (new RegionCreateDrag (this, item, clicked_axisview), event);
					}
					return true;
				} else {
					_drags->set (new EditorRubberbandSelectDrag (this, item), event);
				}
				break;

			case AutomationTrackItem:
			{
				TimeAxisView* parent = clicked_axisview->get_parent ();
				AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (clicked_axisview);
				assert (atv);
				if (parent && dynamic_cast<MidiTimeAxisView*> (parent) && atv->show_regions ()) {

					RouteTimeAxisView* p = dynamic_cast<RouteTimeAxisView*> (parent);
					assert (p);
					boost::shared_ptr<Playlist> pl = p->track()->playlist ();
					if (pl->n_regions() == 0) {
						/* Parent has no regions; create one so that we have somewhere to put automation */
						_drags->set (new RegionCreateDrag (this, item, parent), event);
					} else {
						/* See if there's a region before the click that we can extend, and extend it if so */
						framepos_t const t = event_frame (event);
						boost::shared_ptr<Region> prev = pl->find_next_region (t, End, -1);
						if (!prev) {
							_drags->set (new RegionCreateDrag (this, item, parent), event);
						} else {
							prev->set_length (t - prev->position ());
						}
					}
				} else {
					/* rubberband drag to select automation points */
					_drags->set (new EditorRubberbandSelectDrag (this, item), event);
				}
				break;
			}

			case SelectionItem:
			{
				if (smart_mode_action->get_active()) {
					/* we're in "smart" joined mode, and we've clicked on a Selection */
					double const y = event->button.y + vertical_adjustment.get_value() - canvas_timebars_vsize;
					pair<TimeAxisView*, int> tvp = trackview_by_y_position (y);
					if (tvp.first) {
						/* if we're over an automation track, start a drag of its data */
						AutomationTimeAxisView* atv = dynamic_cast<AutomationTimeAxisView*> (tvp.first);
						if (atv) {
							_drags->set (new AutomationRangeDrag (this, atv, selection->time), event, _cursors->up_down);
						}

						/* if we're over a track and a region, and in the `object' part of a region,
						   put a selection around the region and drag both
						*/
						RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tvp.first);
						if (rtv && _join_object_range_state == JOIN_OBJECT_RANGE_OBJECT) {
							boost::shared_ptr<Track> t = boost::dynamic_pointer_cast<Track> (rtv->route ());
							if (t) {
								boost::shared_ptr<Playlist> pl = t->playlist ();
								if (pl) {

									boost::shared_ptr<Region> r = pl->top_region_at (event_frame (event));
									if (r) {
										RegionView* rv = rtv->view()->find_view (r);
										clicked_selection = select_range (rv->region()->position(), 
														  rv->region()->last_frame()+1);
										_drags->add (new SelectionDrag (this, item, SelectionDrag::SelectionMove));
										list<RegionView*> rvs;
										rvs.push_back (rv);
										_drags->add (new RegionMoveDrag (this, item, rv, rvs, false, false));
										_drags->start_grab (event);
									}
								}
							}
						}
					}
				}
				break;
			}

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
		case GainLineItem:
			_drags->set (new LineDrag (this, item), event);
			return true;

		case ControlPointItem:
			_drags->set (new ControlPointDrag (this, item), event);
			return true;
			break;

		case SelectionItem:
		{
			AudioRegionView* arv = dynamic_cast<AudioRegionView *> (clicked_regionview);
			if (arv) {
				_drags->set (new AutomationRangeDrag (this, arv, selection->time), event, _cursors->up_down);
				_drags->start_grab (event);
			}
			return true;
			break;
		}

		case AutomationLineItem:
			_drags->set (new LineDrag (this, item), event);
			break;
			
		default:
			break;
		}
		return true;
		break;

	case MouseZoom:
		if (event->type == GDK_BUTTON_PRESS) {
			_drags->set (new MouseZoomDrag (this, item), event);
		}

		return true;
		break;

	case MouseTimeFX:
		if (internal_editing() && item_type == NoteItem) {
			/* drag notes if we're in internal edit mode */
			_drags->set (new NoteResizeDrag (this, item), event, current_canvas_cursor);
			return true;
		} else if (clicked_regionview) {
			/* do time-FX  */
			_drags->set (new TimeFXDrag (this, item, clicked_regionview, selection->regions.by_layer()), event);
			return true;
		}
		break;

	case MouseAudition:
		_drags->set (new ScrubDrag (this, item), event);
		scrub_reversals = 0;
		scrub_reverse_distance = 0;
		last_scrub_x = event->button.x;
		scrubbing_direction = 0;
		set_canvas_cursor (_cursors->transparent);
		return true;
		break;

	default:
		break;
	}

	return false;
}

bool
Editor::button_press_handler_2 (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	Editing::MouseMode const eff = effective_mouse_mode ();
	switch (eff) {
	case MouseObject:
		switch (item_type) {
		case RegionItem:
			if (internal_editing ()) {
				/* no region drags in internal edit mode */
				return false;
			}

			if (Keyboard::modifier_state_contains (event->button.state, Keyboard::CopyModifier)) {
				add_region_copy_drag (item, event, clicked_regionview);
			} else {
				add_region_drag (item, event, clicked_regionview);
			}
			_drags->start_grab (event);
			return true;
			break;
		case ControlPointItem:
			_drags->set (new ControlPointDrag (this, item), event);
			return true;
			break;

		default:
			break;
		}

		switch (item_type) {
		case RegionViewNameHighlight:
                        _drags->set (new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer()), event);
                        return true;
                        break;

                case LeftFrameHandle:
                case RightFrameHandle:
			if (!internal_editing ()) {
				_drags->set (new TrimDrag (this, item, clicked_regionview, selection->regions.by_layer()), event);
			}
			return true;
			break;

		case RegionViewName:
			_drags->set (new TrimDrag (this, clicked_regionview->get_name_highlight(), clicked_regionview, selection->regions.by_layer()), event);
			return true;
			break;

		default:
			break;
		}

		break;

	case MouseDraw:
		return false;

	case MouseRange:
		/* relax till release */
		return true;
		break;


	case MouseZoom:
		if (Keyboard::modifier_state_equals (event->button.state, Keyboard::PrimaryModifier)) {
			temporal_zoom_to_frame (false, event_frame (event));
		} else {
			temporal_zoom_to_frame (true, event_frame(event));
		}
		return true;
		break;

	default:
		break;
	}

	return false;
}
   
bool
Editor::button_press_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	if (event->type != GDK_BUTTON_PRESS) {
		return false;
	}

	Glib::RefPtr<Gdk::Window> canvas_window = const_cast<Editor*>(this)->track_canvas->get_window();

	if (canvas_window) {
		Glib::RefPtr<const Gdk::Window> pointer_window;
		int x, y;
		double wx, wy;
		Gdk::ModifierType mask;

		pointer_window = canvas_window->get_pointer (x, y, mask);

		if (pointer_window == track_canvas->get_bin_window()) {
			track_canvas->window_to_world (x, y, wx, wy);
		}
	}

        pre_press_cursor = current_canvas_cursor;
	
	track_canvas->grab_focus();

	if (_session && _session->actively_recording()) {
		return true;
	}



	if (internal_editing()) {
		bool leave_internal_edit_mode = false;

		switch (item_type) {
		case NoteItem:
			break;

		case RegionItem:
			if (!dynamic_cast<MidiRegionView*> (clicked_regionview) && !dynamic_cast<AutomationRegionView*> (clicked_regionview)) {
				leave_internal_edit_mode = true;
			}
			break;

		case PlayheadCursorItem:
		case MarkerItem:
		case TempoMarkerItem:
		case MeterMarkerItem:
		case MarkerBarItem:
		case TempoBarItem:
		case MeterBarItem:
		case RangeMarkerBarItem:
		case CdMarkerBarItem:
		case TransportMarkerBarItem:
			/* button press on these events never does anything to
			   change the editing mode.
			*/
			break;
			
		case StreamItem:
			leave_internal_edit_mode = true;
			break;

		default:
			break;
		}
		
		if (leave_internal_edit_mode) {
			ActionManager::do_action ("MouseMode", "toggle-internal-edit");
		}
	}

	button_selection (item, event, item_type);

	if (!_drags->active () &&
	    (Keyboard::is_delete_event (&event->button) ||
	     Keyboard::is_context_menu_event (&event->button) ||
	     Keyboard::is_edit_event (&event->button))) {

		/* handled by button release */
		return true;
	}

	switch (event->button.button) {
	case 1:
		return button_press_handler_1 (item, event, item_type);
		break;

	case 2:
		return button_press_handler_2 (item, event, item_type);
		break;

	case 3:
		break;

	default:
                return button_press_dispatch (&event->button);
		break;

	}

	return false;
}

bool
Editor::button_press_dispatch (GdkEventButton* ev)
{
        /* this function is intended only for buttons 4 and above.
         */

        Gtkmm2ext::MouseButton b (ev->state, ev->button);
        return button_bindings->activate (b, Gtkmm2ext::Bindings::Press);
}

bool
Editor::button_release_dispatch (GdkEventButton* ev)
{
        /* this function is intended only for buttons 4 and above.
         */

        Gtkmm2ext::MouseButton b (ev->state, ev->button);
        return button_bindings->activate (b, Gtkmm2ext::Bindings::Release);
}

bool
Editor::button_release_handler (ArdourCanvas::Item* item, GdkEvent* event, ItemType item_type)
{
	framepos_t where = event_frame (event, 0, 0);
	AutomationTimeAxisView* atv = 0;

        if (pre_press_cursor) {
                set_canvas_cursor (pre_press_cursor);
                pre_press_cursor = 0;
        }

	/* no action if we're recording */

	if (_session && _session->actively_recording()) {
		return true;
	}

	/* see if we're finishing a drag */

	bool were_dragging = false;
	if (_drags->active ()) {
		bool const r = _drags->end_grab (event);
		if (r) {
			/* grab dragged, so do nothing else */
			return true;
		}

		were_dragging = true;
	}

        update_region_layering_order_editor ();

	/* edit events get handled here */

	if (!_drags->active () && Keyboard::is_edit_event (&event->button)) {
		switch (item_type) {
		case RegionItem:
			show_region_properties ();
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

		case NoteItem:
		{
			ArdourCanvas::CanvasNoteEvent* e = dynamic_cast<ArdourCanvas::CanvasNoteEvent*> (item);
			assert (e);
			edit_notes (e->region_view().selection ());
			break;
		}

		default:
			break;
		}
		return true;
	}

	/* context menu events get handled here */

	if (Keyboard::is_context_menu_event (&event->button)) {

		context_click_event = *event;

		if (!_drags->active ()) {

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

			case StartCrossFadeItem:
				popup_xfade_in_context_menu (1, event->button.time, item, item_type);
				break;

			case EndCrossFadeItem:
				popup_xfade_out_context_menu (1, event->button.time, item, item_type);
				break;

			case StreamItem:
				popup_track_context_menu (1, event->button.time, item_type, false);
				break;

			case RegionItem:
			case RegionViewNameHighlight:
			case LeftFrameHandle:
			case RightFrameHandle:
			case RegionViewName:
				popup_track_context_menu (1, event->button.time, item_type, false);
				break;

			case SelectionItem:
				popup_track_context_menu (1, event->button.time, item_type, true);
				break;
				
			case AutomationTrackItem:
				popup_track_context_menu (1, event->button.time, item_type, false);
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
				tempo_or_meter_marker_context_menu (&event->button, item);
				break;

			case MeterMarkerItem:
				tempo_or_meter_marker_context_menu (&event->button, item);
				break;

			case CrossfadeViewItem:
				popup_track_context_menu (1, event->button.time, item_type, false);
				break;

			case ControlPointItem:
				popup_control_point_context_menu (item, event);
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

	Editing::MouseMode const eff = effective_mouse_mode ();

	if (!_drags->active () && Keyboard::is_delete_event (&event->button)) {

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
			if (eff == MouseObject) {
				remove_clicked_region ();
			}
			break;

		case ControlPointItem:
			remove_control_point (item);
			break;

		case NoteItem:
			remove_midi_note (item, event);
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
				snap_to_with_modifier (where, event, 0, true);
				mouse_add_new_marker (where);
			}
			return true;

		case CdMarkerBarItem:
			if (!_dragging_playhead) {
				// if we get here then a dragged range wasn't done
				snap_to_with_modifier (where, event, 0, true);
				mouse_add_new_marker (where, true);
			}
			return true;

		case TempoBarItem:
			if (!_dragging_playhead) {
				snap_to_with_modifier (where, event);
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

		switch (eff) {
		case MouseObject:
			switch (item_type) {
			case AutomationTrackItem:
				atv = dynamic_cast<AutomationTimeAxisView*>(clicked_axisview);
				if (atv) {
					atv->add_automation_event (event, where, event->button.y);
				}
				return true;
				break;
			default:
				break;
			}
			break;

		case MouseGain:
			switch (item_type) {
			case RegionItem:
			{
				/* check that we didn't drag before releasing, since
				   its really annoying to create new control
				   points when doing this.
				*/
				AudioRegionView* arv = dynamic_cast<AudioRegionView*> (clicked_regionview);
				if (!were_dragging && arv) {
					arv->add_gain_point_event (item, event);
				}
				return true;
				break;
			}

			case AutomationTrackItem:
				dynamic_cast<AutomationTimeAxisView*>(clicked_axisview)->
					add_automation_event (event, where, event->button.y);
				return true;
				break;
			default:
				break;
			}
			break;

		case MouseAudition:
			set_canvas_cursor (current_canvas_cursor);
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
				_session->request_transport_speed (0.0);
			}
			break;

		default:
			break;

		}

                /* do any (de)selection operations that should occur on button release */
                button_selection (item, event, item_type);
		return true;
		break;


	case 2:
		switch (eff) {

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

		case MouseDraw:
			return true;
			
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
        bool ret = true;

	switch (item_type) {
	case ControlPointItem:
		if (mouse_mode == MouseGain || mouse_mode == MouseObject) {
			cp = static_cast<ControlPoint*>(item->get_data ("control_point"));
			cp->set_visible (true);

			double at_x, at_y;
			at_x = cp->get_x();
			at_y = cp->get_y ();
			cp->i2w (at_x, at_y);
			at_x += 10.0;
			at_y += 10.0;

			fraction = 1.0 - (cp->get_y() / cp->line().height());

			if (is_drawable() && !_drags->active ()) {
			        set_canvas_cursor (_cursors->fader);
			}

			_verbose_cursor->set (cp->line().get_verbose_cursor_string (fraction), at_x, at_y);
			_verbose_cursor->show ();
		}
		break;

	case GainLineItem:
		if (mouse_mode == MouseGain) {
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			if (line)
				line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_EnteredGainLine.get();
			if (is_drawable()) {
				set_canvas_cursor (_cursors->fader);
			}
		}
		break;

	case AutomationLineItem:
		if (mouse_mode == MouseGain || mouse_mode == MouseObject) {
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			if (line) {
				line->property_fill_color_rgba() = ARDOUR_UI::config()->canvasvar_EnteredAutomationLine.get();
			}
			if (is_drawable()) {
				set_canvas_cursor (_cursors->fader);
			}
		}
		break;

	case RegionViewNameHighlight:
		if (is_drawable() && doing_object_stuff() && entered_regionview) {
			set_canvas_cursor_for_region_view (event->crossing.x, entered_regionview);
			_over_region_trim_target = true;
		}
		break;

	case LeftFrameHandle:
	case RightFrameHandle:
		if (is_drawable() && doing_object_stuff() && !internal_editing() && entered_regionview) {
			set_canvas_cursor_for_region_view (event->crossing.x, entered_regionview);
		}
                break;

	case StartSelectionTrimItem:
#ifdef WITH_CMT
	case ImageFrameHandleStartItem:
	case MarkerViewHandleStartItem:
#endif
		if (is_drawable()) {
			set_canvas_cursor (_cursors->left_side_trim);
		}
		break;
	case EndSelectionTrimItem:
#ifdef WITH_CMT
	case ImageFrameHandleEndItem:
	case MarkerViewHandleEndItem:
#endif
		if (is_drawable()) {
			set_canvas_cursor (_cursors->right_side_trim);
		}
		break;

	case PlayheadCursorItem:
		if (is_drawable()) {
			switch (_edit_point) {
			case EditAtMouse:
				set_canvas_cursor (_cursors->grabber_edit_point);
				break;
			default:
				set_canvas_cursor (_cursors->grabber);
				break;
			}
		}
		break;

	case RegionViewName:

		/* when the name is not an active item, the entire name highlight is for trimming */

		if (!reinterpret_cast<RegionView *> (item->get_data ("regionview"))->name_active()) {
			if (mouse_mode == MouseObject && is_drawable()) {
				set_canvas_cursor_for_region_view (event->crossing.x, entered_regionview);
				_over_region_trim_target = true;
			}
		}
		break;


	case AutomationTrackItem:
		if (is_drawable()) {
			Gdk::Cursor *cursor;
			switch (mouse_mode) {
			case MouseRange:
				cursor = _cursors->selector;
				break;
			case MouseZoom:
	 			cursor = _cursors->zoom_in;
				break;
			default:
				cursor = _cursors->cross_hair;
				break;
			}

			set_canvas_cursor (cursor);

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
			set_canvas_cursor (_cursors->timebar);
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
			set_canvas_cursor (_cursors->timebar);
		}
		break;

	case FadeInHandleItem:
		if (mouse_mode == MouseObject && !internal_editing()) {
			ArdourCanvas::SimpleRect *rect = dynamic_cast<ArdourCanvas::SimpleRect *> (item);
			if (rect) {
				rect->property_fill_color_rgba() = 0xBBBBBBAA;
			}
			set_canvas_cursor (_cursors->fade_in);
		}
		break;

	case FadeOutHandleItem:
		if (mouse_mode == MouseObject && !internal_editing()) {
			ArdourCanvas::SimpleRect *rect = dynamic_cast<ArdourCanvas::SimpleRect *> (item);
			if (rect) {
				rect->property_fill_color_rgba() = 0xBBBBBBAA;
			}
			set_canvas_cursor (_cursors->fade_out);
		}
		break;
	case FeatureLineItem:
		{
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			line->property_fill_color_rgba() = 0xFF0000FF;
		}
		break;
	case SelectionItem:
		if (smart_mode_action->get_active()) {
			set_canvas_cursor ();
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

	return ret;
}

bool
Editor::leave_handler (ArdourCanvas::Item* item, GdkEvent*, ItemType item_type)
{
	AutomationLine* al;
	ControlPoint* cp;
	Marker *marker;
	Location *loc;
	RegionView* rv;
	bool is_start;
	bool ret = true;

	switch (item_type) {
	case ControlPointItem:
		cp = reinterpret_cast<ControlPoint*>(item->get_data ("control_point"));
		if (cp->line().the_list()->interpolation() != AutomationList::Discrete) {
			if (cp->line().npoints() > 1 && !cp->get_selected()) {
				cp->set_visible (false);
			}
		}

		if (is_drawable()) {
			set_canvas_cursor (current_canvas_cursor);
		}

		_verbose_cursor->hide ();
		break;

	case RegionViewNameHighlight:
	case LeftFrameHandle:
	case RightFrameHandle:
	case StartSelectionTrimItem:
	case EndSelectionTrimItem:
	case PlayheadCursorItem:

#ifdef WITH_CMT
	case ImageFrameHandleStartItem:
	case ImageFrameHandleEndItem:
	case MarkerViewHandleStartItem:
	case MarkerViewHandleEndItem:
#endif

		_over_region_trim_target = false;

		if (is_drawable()) {
			set_canvas_cursor (current_canvas_cursor);
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
			set_canvas_cursor (current_canvas_cursor);
		}
		break;

	case RegionViewName:
		/* see enter_handler() for notes */
		_over_region_trim_target = false;

		if (!reinterpret_cast<RegionView *> (item->get_data ("regionview"))->name_active()) {
			if (is_drawable() && mouse_mode == MouseObject) {
				set_canvas_cursor (current_canvas_cursor);
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
			set_canvas_cursor (current_canvas_cursor);
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
			set_canvas_cursor (current_canvas_cursor);
		}

		break;

	case FadeInHandleItem:
	case FadeOutHandleItem:
		rv = static_cast<RegionView*>(item->get_data ("regionview"));
		{
			ArdourCanvas::SimpleRect *rect = dynamic_cast<ArdourCanvas::SimpleRect *> (item);
			if (rect) {
				rect->property_fill_color_rgba() = rv->get_fill_color();
			}
		}
		set_canvas_cursor (current_canvas_cursor);
		break;

	case AutomationTrackItem:
		if (is_drawable()) {
			set_canvas_cursor (current_canvas_cursor);
			clear_entered_track = true;
			Glib::signal_idle().connect (sigc::mem_fun(*this, &Editor::left_automation_track));
		}
		break;
	case FeatureLineItem:
		{
			ArdourCanvas::Line *line = dynamic_cast<ArdourCanvas::Line *> (item);
			line->property_fill_color_rgba() = (guint) ARDOUR_UI::config()->canvasvar_ZeroLine.get();;
		}
		break;

	default:
		break;
	}

	return ret;
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
Editor::scrub (framepos_t frame, double current_x)
{
	double delta;

	if (scrubbing_direction == 0) {
		/* first move */
		_session->request_locate (frame, false);
		_session->request_transport_speed (0.1);
		scrubbing_direction = 1;

	} else {

		if (last_scrub_x > current_x) {

			/* pointer moved to the left */

			if (scrubbing_direction > 0) {

				/* we reversed direction to go backwards */

				scrub_reversals++;
				scrub_reverse_distance += (int) (last_scrub_x - current_x);

			} else {

				/* still moving to the left (backwards) */

				scrub_reversals = 0;
				scrub_reverse_distance = 0;

				delta = 0.01 * (last_scrub_x - current_x);
				_session->request_transport_speed_nonzero (_session->transport_speed() - delta);
			}

		} else {
			/* pointer moved to the right */

			if (scrubbing_direction < 0) {
				/* we reversed direction to go forward */

				scrub_reversals++;
				scrub_reverse_distance += (int) (current_x - last_scrub_x);

			} else {
				/* still moving to the right */

				scrub_reversals = 0;
				scrub_reverse_distance = 0;

				delta = 0.01 * (current_x - last_scrub_x);
				_session->request_transport_speed_nonzero (_session->transport_speed() + delta);
			}
		}

		/* if there have been more than 2 opposite motion moves detected, or one that moves
		   back more than 10 pixels, reverse direction
		*/

		if (scrub_reversals >= 2 || scrub_reverse_distance > 10) {

			if (scrubbing_direction > 0) {
				/* was forwards, go backwards */
				_session->request_transport_speed (-0.1);
				scrubbing_direction = -1;
			} else {
				/* was backwards, go forwards */
				_session->request_transport_speed (0.1);
				scrubbing_direction = 1;
			}

			scrub_reverse_distance = 0;
			scrub_reversals = 0;
		}
	}

	last_scrub_x = current_x;
}

bool
Editor::motion_handler (ArdourCanvas::Item* /*item*/, GdkEvent* event, bool from_autoscroll)
{
	_last_motion_y = event->motion.y;

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

	if (_session && _session->actively_recording()) {
		/* Sorry. no dragging stuff around while we record */
		return true;
	}

	JoinObjectRangeState const old = _join_object_range_state;
	update_join_object_range_location (event->motion.x, event->motion.y);
	if (_join_object_range_state != old) {
		set_canvas_cursor ();
	}

	if (_over_region_trim_target) {
		set_canvas_cursor_for_region_view (event->motion.x, entered_regionview);
	}

	bool handled = false;
	if (_drags->active ()) {
		handled = _drags->motion_handler (event, from_autoscroll);
	}

	if (!handled) {
		return false;
	}

	track_canvas_motion (event);
	return true;
}

bool
Editor::can_remove_control_point (ArdourCanvas::Item* item)
{
	ControlPoint* control_point;

	if ((control_point = reinterpret_cast<ControlPoint *> (item->get_data ("control_point"))) == 0) {
		fatal << _("programming error: control point canvas item has no control point object pointer!") << endmsg;
		/*NOTREACHED*/
	}

	AutomationLine& line = control_point->line ();
	if (dynamic_cast<AudioRegionGainLine*> (&line)) {
		/* we shouldn't remove the first or last gain point in region gain lines */
		if (line.is_last_point(*control_point) || line.is_first_point(*control_point)) {
			return false;
		}
	}

	return true;
}

void
Editor::remove_control_point (ArdourCanvas::Item* item)
{
	if (!can_remove_control_point (item)) {
		return;
	}

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
Editor::edit_notes (MidiRegionView::Selection const & s)
{
	if (s.empty ()) {
		return;
	}
	
	EditNoteDialog d (&(*s.begin())->region_view(), s);
	d.set_position (Gtk::WIN_POS_MOUSE);
	ensure_float (d);

	d.run ();
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
			speed = rtv->track()->speed();
		}

		framepos_t where = get_preferred_edit_position();

		if (where >= 0) {

			if (Keyboard::modifier_state_equals (event->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::SecondaryModifier))) {

				align_region (rv.region(), SyncPoint, (framepos_t) (where * speed));

			} else if (Keyboard::modifier_state_equals (event->state, Keyboard::ModifierMask (Keyboard::PrimaryModifier|Keyboard::TertiaryModifier))) {

				align_region (rv.region(), End, (framepos_t) (where * speed));

			} else {

				align_region (rv.region(), Start, (framepos_t) (where * speed));
			}
		}
	}
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
Editor::point_trim (GdkEvent* event, framepos_t new_bound)
{
	RegionView* rv = clicked_regionview;

	/* Choose action dependant on which button was pressed */
	switch (event->button.button) {
	case 1:
		begin_reversible_command (_("start point trim"));

		if (selection->selected (rv)) {
			for (list<RegionView*>::const_iterator i = selection->regions.by_layer().begin();
			     i != selection->regions.by_layer().end(); ++i)
			{
				if (!(*i)->region()->locked()) {
					(*i)->region()->clear_changes ();
					(*i)->region()->trim_front (new_bound);
					_session->add_command(new StatefulDiffCommand ((*i)->region()));
				}
			}

		} else {
			if (!rv->region()->locked()) {
				rv->region()->clear_changes ();
				rv->region()->trim_front (new_bound);
				_session->add_command(new StatefulDiffCommand (rv->region()));
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
					(*i)->region()->clear_changes();
					(*i)->region()->trim_end (new_bound);
					_session->add_command(new StatefulDiffCommand ((*i)->region()));
				}
			}

		} else {

			if (!rv->region()->locked()) {
				rv->region()->clear_changes ();
				rv->region()->trim_end (new_bound);
				_session->add_command (new StatefulDiffCommand (rv->region()));
			}
		}

		commit_reversible_command();

		break;
	default:
		break;
	}
}

void
Editor::hide_marker (ArdourCanvas::Item* item, GdkEvent* /*event*/)
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
Editor::reposition_zoom_rect (framepos_t start, framepos_t end)
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
Editor::mouse_rename_region (ArdourCanvas::Item* /*item*/, GdkEvent* /*event*/)
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
Editor::mouse_brush_insert_region (RegionView* rv, framepos_t pos)
{
	/* no brushing without a useful snap setting */

	switch (_snap_mode) {
	case SnapMagnetic:
		return; /* can't work because it allows region to be placed anywhere */
	default:
		break; /* OK */
	}

	switch (_snap_type) {
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
	double speed = rtv->track()->speed();

	playlist->clear_changes ();
	boost::shared_ptr<Region> new_region (RegionFactory::create (rv->region(), true));
	playlist->add_region (new_region, (framepos_t) (pos * speed));
	_session->add_command (new StatefulDiffCommand (playlist));

	// playlist is frozen, so we have to update manually XXX this is disgusting

	playlist->RegionAdded (new_region); /* EMIT SIGNAL */
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
Editor::add_region_drag (ArdourCanvas::Item* item, GdkEvent*, RegionView* region_view)
{
	assert (region_view);

	if (!region_view->region()->playlist()) {
		return;
	}

	_region_motion_group->raise_to_top ();

	if (Config->get_edit_mode() == Splice) {
		_drags->add (new RegionSpliceDrag (this, item, region_view, selection->regions.by_layer()));
	} else {
		RegionSelection s = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);
		_drags->add (new RegionMoveDrag (this, item, region_view, s.by_layer(), false, false));
	}

	/* sync the canvas to what we think is its current state */
	update_canvas_now();
}

void
Editor::add_region_copy_drag (ArdourCanvas::Item* item, GdkEvent*, RegionView* region_view)
{
	assert (region_view);

	if (!region_view->region()->playlist()) {
		return;
	}

	_region_motion_group->raise_to_top ();

	RegionSelection s = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);
	_drags->add (new RegionMoveDrag (this, item, region_view, s.by_layer(), false, true));
}

void
Editor::add_region_brush_drag (ArdourCanvas::Item* item, GdkEvent*, RegionView* region_view)
{
	assert (region_view);

	if (!region_view->region()->playlist()) {
		return;
	}

	if (Config->get_edit_mode() == Splice) {
		return;
	}

	RegionSelection s = get_equivalent_regions (selection->regions, ARDOUR::Properties::edit.property_id);
	_drags->add (new RegionMoveDrag (this, item, region_view, s.by_layer(), true, false));

	begin_reversible_command (Operations::drag_region_brush);
}

/** Start a grab where a time range is selected, track(s) are selected, and the
 *  user clicks and drags a region with a modifier in order to create a new region containing
 *  the section of the clicked region that lies within the time range.
 */
void
Editor::start_selection_grab (ArdourCanvas::Item* /*item*/, GdkEvent* event)
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
	sigc::connection c = clicked_routeview->view()->RegionViewAdded.connect (sigc::mem_fun(*this, &Editor::collect_new_region_view));

	/* A selection grab currently creates two undo/redo operations, one for
	   creating the new region and another for moving it.
	*/

	begin_reversible_command (Operations::selection_grab);

	boost::shared_ptr<Playlist> playlist = clicked_axisview->playlist();

	playlist->clear_changes ();
	clicked_routeview->playlist()->add_region (region, selection->time[clicked_selection].start);
	_session->add_command(new StatefulDiffCommand (playlist));

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

	_drags->set (new RegionMoveDrag (this, latest_regionviews.front()->get_canvas_group(), latest_regionviews.front(), latest_regionviews, false, false), event);
}

void
Editor::escape ()
{
	if (_drags->active ()) {
		_drags->abort ();
	} else {
		selection->clear ();
	}
}

void
Editor::set_internal_edit (bool yn)
{
	if (_internal_editing == yn) {
		return;
	}

	_internal_editing = yn;
	
	if (yn) {
                pre_internal_mouse_mode = mouse_mode;
		pre_internal_snap_type = _snap_type;
		pre_internal_snap_mode = _snap_mode;

                for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
                        (*i)->enter_internal_edit_mode ();
                }

		set_snap_to (internal_snap_type);
		set_snap_mode (internal_snap_mode);

	} else {

		internal_snap_mode = _snap_mode;
		internal_snap_type = _snap_type;

                for (TrackViewList::iterator i = track_views.begin(); i != track_views.end(); ++i) {
                        (*i)->leave_internal_edit_mode ();
                }

                if (mouse_mode == MouseDraw && pre_internal_mouse_mode != MouseDraw) {
                        /* we were drawing .. flip back to something sensible */
                        set_mouse_mode (pre_internal_mouse_mode);
                }

		set_snap_to (pre_internal_snap_type);
		set_snap_mode (pre_internal_snap_mode);
	}
	
	set_canvas_cursor ();
}

/** Update _join_object_range_state which indicate whether we are over the top or bottom half of a region view,
 *  used by the `join object/range' tool mode.
 */
void
Editor::update_join_object_range_location (double /*x*/, double y)
{
	/* XXX: actually, this decides based on whether the mouse is in the top
	   or bottom half of a the waveform part RouteTimeAxisView;

	   Note that entered_{track,regionview} is not always setup (e.g. if
	   the mouse is over a TimeSelection), and to get a Region
	   that we're over requires searching the playlist.
	*/

	if (!smart_mode_action->get_active() || (mouse_mode != MouseRange && mouse_mode != MouseObject)) {
		_join_object_range_state = JOIN_OBJECT_RANGE_NONE;
		return;
	}

	if (mouse_mode == MouseObject) {
		_join_object_range_state = JOIN_OBJECT_RANGE_OBJECT;
	} else if (mouse_mode == MouseRange) {
		_join_object_range_state = JOIN_OBJECT_RANGE_RANGE;
	}

	/* XXX: maybe we should make entered_track work in all cases, rather than resorting to this */
	pair<TimeAxisView*, int> tvp = trackview_by_y_position (y + vertical_adjustment.get_value() - canvas_timebars_vsize);

	if (tvp.first) {

		RouteTimeAxisView* rtv = dynamic_cast<RouteTimeAxisView*> (tvp.first);
		if (rtv) {

			double cx = 0;
			double cy = y;
			rtv->canvas_display()->w2i (cx, cy);

			double const c = cy / (rtv->view()->child_height() - TimeAxisViewItem::NAME_HIGHLIGHT_SIZE);
			double d;
			double const f = modf (c, &d);

			_join_object_range_state = f < 0.5 ? JOIN_OBJECT_RANGE_RANGE : JOIN_OBJECT_RANGE_OBJECT;
		}
	}
}

Editing::MouseMode
Editor::effective_mouse_mode () const
{
	if (_join_object_range_state == JOIN_OBJECT_RANGE_OBJECT) {
		return MouseObject;
	} else if (_join_object_range_state == JOIN_OBJECT_RANGE_RANGE) {
		return MouseRange;
	}

	return mouse_mode;
}

void
Editor::remove_midi_note (ArdourCanvas::Item* item, GdkEvent *)
{
	ArdourCanvas::CanvasNoteEvent* e = dynamic_cast<ArdourCanvas::CanvasNoteEvent*> (item);
	assert (e);

	e->region_view().delete_note (e->note ());
}

void
Editor::set_canvas_cursor_for_region_view (double x, RegionView* rv)
{
	assert (rv);

	ArdourCanvas::Group* g = rv->get_canvas_group ();
	ArdourCanvas::Group* p = g->get_parent_group ();

	/* Compute x in region view parent coordinates */
	double dy = 0;
	p->w2i (x, dy);

	double x1, x2, y1, y2;
	g->get_bounds (x1, y1, x2, y2);

	/* Halfway across the region */
	double const h = (x1 + x2) / 2;

	Trimmable::CanTrim ct = rv->region()->can_trim ();
	if (x <= h) {
		if (ct & Trimmable::FrontTrimEarlier) {
			set_canvas_cursor (_cursors->left_side_trim);
		} else {
			set_canvas_cursor (_cursors->left_side_trim_right_only);
		}
	} else {
		if (ct & Trimmable::EndTrimLater) {
			set_canvas_cursor (_cursors->right_side_trim);
		} else {
			set_canvas_cursor (_cursors->right_side_trim_left_only);
		}
	}
}

/** Obtain the pointer position in world coordinates */
void
Editor::get_pointer_position (double& x, double& y) const
{
	int px, py;
	track_canvas->get_pointer (px, py);
	track_canvas->window_to_world (px, py, x, y);
}
