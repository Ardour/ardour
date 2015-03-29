/*
    Copyright (C) 2001-2011 Paul Davis
    Author: David Robillard

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

#include <cmath>
#include <algorithm>
#include <ostream>

#include <gtkmm.h>

#include "gtkmm2ext/gtk_ui.h"

#include <sigc++/signal.h>

#include "midi++/midnam_patch.h"

#include "pbd/memento_command.h"
#include "pbd/stateful_diff_command.h"

#include "ardour/midi_model.h"
#include "ardour/midi_playlist.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/midi_track.h"
#include "ardour/operations.h"
#include "ardour/session.h"

#include "evoral/Parameter.hpp"
#include "evoral/MIDIEvent.hpp"
#include "evoral/Control.hpp"
#include "evoral/midi_util.h"

#include "canvas/debug.h"
#include "canvas/text.h"

#include "automation_region_view.h"
#include "automation_time_axis.h"
#include "control_point.h"
#include "debug.h"
#include "editor.h"
#include "editor_drag.h"
#include "ghostregion.h"
#include "gui_thread.h"
#include "item_counts.h"
#include "keyboard.h"
#include "midi_channel_dialog.h"
#include "midi_cut_buffer.h"
#include "midi_list_editor.h"
#include "midi_region_view.h"
#include "midi_streamview.h"
#include "midi_time_axis.h"
#include "midi_util.h"
#include "midi_velocity_dialog.h"
#include "mouse_cursors.h"
#include "note_player.h"
#include "paste_context.h"
#include "public_editor.h"
#include "route_time_axis.h"
#include "rgb_macros.h"
#include "selection.h"
#include "streamview.h"
#include "patch_change_dialog.h"
#include "verbose_cursor.h"
#include "ardour_ui.h"
#include "note.h"
#include "hit.h"
#include "patch_change.h"
#include "sys_ex.h"

#include "i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace std;
using Gtkmm2ext::Keyboard;

PBD::Signal1<void, MidiRegionView *> MidiRegionView::SelectionCleared;

#define MIDI_BP_ZERO ((Config->get_first_midi_bank_is_zero())?0:1)

MidiRegionView::MidiRegionView (ArdourCanvas::Container*      parent,
                                RouteTimeAxisView&            tv,
                                boost::shared_ptr<MidiRegion> r,
                                double                        spu,
                                uint32_t                      basic_color)
	: RegionView (parent, tv, r, spu, basic_color)
	, _current_range_min(0)
	, _current_range_max(0)
	, _region_relative_time_converter(r->session().tempo_map(), r->position())
	, _source_relative_time_converter(r->session().tempo_map(), r->position() - r->start())
	, _active_notes(0)
	, _note_group (new ArdourCanvas::Container (group))
	, _note_diff_command (0)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1.0)
	, _step_edit_cursor_position (0.0)
	, _channel_selection_scoped_note (0)
	, _temporary_note_group (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _grabbed_keyboard (false)
	, _entered (false)
	, _mouse_changed_selection (false)
{
	CANVAS_DEBUG_NAME (_note_group, string_compose ("note group for %1", get_item_name()));
	_note_group->raise_to_top();
	PublicEditor::DropDownKeys.connect (sigc::mem_fun (*this, &MidiRegionView::drop_down_keys));

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&MidiRegionView::parameter_changed, this, _1), gui_context());
	connect_to_diskstream ();

	SelectionCleared.connect (_selection_cleared_connection, invalidator (*this), boost::bind (&MidiRegionView::selection_cleared, this, _1), gui_context ());

	PublicEditor& editor (trackview.editor());
	editor.get_selection().ClearMidiNoteSelection.connect (_clear_midi_selection_connection, invalidator (*this), boost::bind (&MidiRegionView::clear_midi_selection, this), gui_context ());
}

MidiRegionView::MidiRegionView (ArdourCanvas::Container*      parent,
                                RouteTimeAxisView&            tv,
                                boost::shared_ptr<MidiRegion> r,
                                double                        spu,
                                uint32_t                      basic_color,
                                bool                          recording,
                                TimeAxisViewItem::Visibility  visibility)
	: RegionView (parent, tv, r, spu, basic_color, recording, visibility)
	, _current_range_min(0)
	, _current_range_max(0)
	, _region_relative_time_converter(r->session().tempo_map(), r->position())
	, _source_relative_time_converter(r->session().tempo_map(), r->position() - r->start())
	, _active_notes(0)
	, _note_group (new ArdourCanvas::Container (group))
	, _note_diff_command (0)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1.0)
	, _step_edit_cursor_position (0.0)
	, _channel_selection_scoped_note (0)
	, _temporary_note_group (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _grabbed_keyboard (false)
	, _entered (false)
	, _mouse_changed_selection (false)
{
	CANVAS_DEBUG_NAME (_note_group, string_compose ("note group for %1", get_item_name()));
	_note_group->raise_to_top();

	PublicEditor::DropDownKeys.connect (sigc::mem_fun (*this, &MidiRegionView::drop_down_keys));

	connect_to_diskstream ();

	SelectionCleared.connect (_selection_cleared_connection, invalidator (*this), boost::bind (&MidiRegionView::selection_cleared, this, _1), gui_context ());

	PublicEditor& editor (trackview.editor());
	editor.get_selection().ClearMidiNoteSelection.connect (_clear_midi_selection_connection, invalidator (*this), boost::bind (&MidiRegionView::clear_midi_selection, this), gui_context ());
}

void
MidiRegionView::parameter_changed (std::string const & p)
{
	if (p == "display-first-midi-bank-as-zero") {
		if (_enable_display) {
			redisplay_model();
		}
	}
}

MidiRegionView::MidiRegionView (const MidiRegionView& other)
	: sigc::trackable(other)
	, RegionView (other)
	, _current_range_min(0)
	, _current_range_max(0)
	, _region_relative_time_converter(other.region_relative_time_converter())
	, _source_relative_time_converter(other.source_relative_time_converter())
	, _active_notes(0)
	, _note_group (new ArdourCanvas::Container (get_canvas_group()))
	, _note_diff_command (0)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1.0)
	, _step_edit_cursor_position (0.0)
	, _channel_selection_scoped_note (0)
	, _temporary_note_group (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _grabbed_keyboard (false)
	, _entered (false)
	, _mouse_changed_selection (false)
{
	init (false);
}

MidiRegionView::MidiRegionView (const MidiRegionView& other, boost::shared_ptr<MidiRegion> region)
	: RegionView (other, boost::shared_ptr<Region> (region))
	, _current_range_min(0)
	, _current_range_max(0)
	, _region_relative_time_converter(other.region_relative_time_converter())
	, _source_relative_time_converter(other.source_relative_time_converter())
	, _active_notes(0)
	, _note_group (new ArdourCanvas::Container (get_canvas_group()))
	, _note_diff_command (0)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1.0)
	, _step_edit_cursor_position (0.0)
	, _channel_selection_scoped_note (0)
	, _temporary_note_group (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _sort_needed (true)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _grabbed_keyboard (false)
	, _entered (false)
	, _mouse_changed_selection (false)
{
	init (true);
}

void
MidiRegionView::init (bool wfd)
{
	PublicEditor::DropDownKeys.connect (sigc::mem_fun (*this, &MidiRegionView::drop_down_keys));

	NoteBase::NoteBaseDeleted.connect (note_delete_connection, MISSING_INVALIDATOR,
					   boost::bind (&MidiRegionView::maybe_remove_deleted_note_from_selection, this, _1),
					   gui_context());
	
	if (wfd) {
		Glib::Threads::Mutex::Lock lm(midi_region()->midi_source(0)->mutex());
		midi_region()->midi_source(0)->load_model(lm);
	}

	_model = midi_region()->midi_source(0)->model();
	_enable_display = false;
	fill_color_name = "midi frame base";

	RegionView::init (false);

	set_height (trackview.current_height());

	region_muted ();
	region_sync_changed ();
	region_resized (ARDOUR::bounds_change);
	region_locked ();

	set_colors ();

	_enable_display = true;
	if (_model) {
		if (wfd) {
			display_model (_model);
		}
	}

	reset_width_dependent_items (_pixel_width);

	group->raise_to_top();

	midi_view()->midi_track()->playback_filter().ChannelModeChanged.connect (_channel_mode_changed_connection, invalidator (*this),
								       boost::bind (&MidiRegionView::midi_channel_mode_changed, this),
								       gui_context ());

	instrument_info().Changed.connect (_instrument_changed_connection, invalidator (*this),
					   boost::bind (&MidiRegionView::instrument_settings_changed, this), gui_context());

	trackview.editor().SnapChanged.connect(snap_changed_connection, invalidator(*this),
	                                       boost::bind (&MidiRegionView::snap_changed, this),
	                                       gui_context());

	trackview.editor().MouseModeChanged.connect(_mouse_mode_connection, invalidator (*this),
	                                            boost::bind (&MidiRegionView::mouse_mode_changed, this),
	                                            gui_context ());

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&MidiRegionView::parameter_changed, this, _1), gui_context());
	connect_to_diskstream ();

	SelectionCleared.connect (_selection_cleared_connection, invalidator (*this), boost::bind (&MidiRegionView::selection_cleared, this, _1), gui_context ());

	PublicEditor& editor (trackview.editor());
	editor.get_selection().ClearMidiNoteSelection.connect (_clear_midi_selection_connection, invalidator (*this), boost::bind (&MidiRegionView::clear_midi_selection, this), gui_context ());
}

InstrumentInfo&
MidiRegionView::instrument_info () const
{
	RouteUI* route_ui = dynamic_cast<RouteUI*> (&trackview);
	return route_ui->route()->instrument_info();
}

const boost::shared_ptr<ARDOUR::MidiRegion>
MidiRegionView::midi_region() const
{
	return boost::dynamic_pointer_cast<ARDOUR::MidiRegion>(_region);
}

void
MidiRegionView::connect_to_diskstream ()
{
	midi_view()->midi_track()->DataRecorded.connect(
		*this, invalidator(*this),
		boost::bind (&MidiRegionView::data_recorded, this, _1),
		gui_context());
}

bool
MidiRegionView::canvas_group_event(GdkEvent* ev)
{
	if (in_destructor || _recregion) {
		return false;
	}

	if (!trackview.editor().internal_editing()) {
		// not in internal edit mode, so just act like a normal region
		return RegionView::canvas_group_event (ev);
	}

	bool r;

	switch (ev->type) {
	case GDK_ENTER_NOTIFY:
		_last_event_x = ev->crossing.x;
		_last_event_y = ev->crossing.y;
		enter_notify(&ev->crossing);
		// set entered_regionview (among other things)
		return RegionView::canvas_group_event (ev);

	case GDK_LEAVE_NOTIFY:
		_last_event_x = ev->crossing.x;
		_last_event_y = ev->crossing.y;
		leave_notify(&ev->crossing);
		// reset entered_regionview (among other things)
		return RegionView::canvas_group_event (ev);

	case GDK_SCROLL:
		if (scroll (&ev->scroll)) {
			return true;
		}
		break;

	case GDK_KEY_PRESS:
		return key_press (&ev->key);

	case GDK_KEY_RELEASE:
		return key_release (&ev->key);

	case GDK_BUTTON_PRESS:
		return button_press (&ev->button);

	case GDK_BUTTON_RELEASE:
		r = button_release (&ev->button);
		return r;

	case GDK_MOTION_NOTIFY:
		_last_event_x = ev->motion.x;
		_last_event_y = ev->motion.y;
		return motion (&ev->motion);

	default:
		break;
	}

	return RegionView::canvas_group_event (ev);
}

bool
MidiRegionView::enter_notify (GdkEventCrossing* ev)
{
	enter_internal();

	_entered = true;
	return false;
}

bool
MidiRegionView::leave_notify (GdkEventCrossing*)
{
	leave_internal();

	_entered = false;
	return false;
}

void
MidiRegionView::mouse_mode_changed ()
{
	// Adjust frame colour (become more transparent for internal tools)
	set_frame_color();

	if (_entered) {
		if (trackview.editor().internal_editing()) {
			// Switched in to internal editing mode while entered
			enter_internal();
		} else {
			// Switched out of internal editing mode while entered
			leave_internal();
		}
	}
}

void
MidiRegionView::enter_internal()
{
	if (trackview.editor().current_mouse_mode() == MouseDraw && _mouse_state != AddDragging) {
		// Show ghost note under pencil
		create_ghost_note(_last_event_x, _last_event_y);
	}

	if (!_selection.empty()) {
		// Grab keyboard for moving selected notes with arrow keys
		Keyboard::magic_widget_grab_focus();
		_grabbed_keyboard = true;
	}

	// Lower frame handles below notes so they don't steal events
	if (frame_handle_start) {
		frame_handle_start->lower_to_bottom();
	}
	if (frame_handle_end) {
		frame_handle_end->lower_to_bottom();
	}
}

void
MidiRegionView::leave_internal()
{
	trackview.editor().verbose_cursor()->hide ();
	remove_ghost_note ();

	if (_grabbed_keyboard) {
		Keyboard::magic_widget_drop_focus();
		_grabbed_keyboard = false;
	}

	// Raise frame handles above notes so they catch events
	if (frame_handle_start) {
		frame_handle_start->raise_to_top();
	}
	if (frame_handle_end) {
		frame_handle_end->raise_to_top();
	}
}

bool
MidiRegionView::button_press (GdkEventButton* ev)
{
	if (ev->button != 1) {
		return false;
	}

	Editor* editor = dynamic_cast<Editor *> (&trackview.editor());
	MouseMode m = editor->current_mouse_mode();

	if (m == MouseContent && Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier())) {
		_press_cursor_ctx = CursorContext::create(*editor, editor->cursors()->midi_pencil);
	}

	if (_mouse_state != SelectTouchDragging) {
		
		_pressed_button = ev->button;
		_mouse_state = Pressed;
		
		return true;
	}

	_pressed_button = ev->button;
	_mouse_changed_selection = false;

	return true;
}

bool
MidiRegionView::button_release (GdkEventButton* ev)
{
	double event_x, event_y;

	if (ev->button != 1) {
		return false;
	}

	event_x = ev->x;
	event_y = ev->y;

	group->canvas_to_item (event_x, event_y);
	group->ungrab ();

	PublicEditor& editor = trackview.editor ();

	_press_cursor_ctx.reset();

	switch (_mouse_state) {
	case Pressed: // Clicked

		switch (editor.current_mouse_mode()) {
		case MouseRange:
			/* no motion occured - simple click */
			clear_selection ();
			_mouse_changed_selection = true;
			break;

		case MouseContent:
		case MouseTimeFX:
			{
				clear_selection();
				_mouse_changed_selection = true;

				if (Keyboard::is_insert_note_event(ev)) {

					double event_x, event_y;

					event_x = ev->x;
					event_y = ev->y;
					group->canvas_to_item (event_x, event_y);

					Evoral::Beats beats = get_grid_beats(editor.pixel_to_sample(event_x));

					/* Shorten the length by 1 tick so that we can add a new note at the next
					   grid snap without it overlapping this one.
					*/
					beats -= Evoral::Beats::tick();

					create_note_at (editor.pixel_to_sample (event_x), event_y, beats, true);
				}

				break;
			}
		case MouseDraw:
			{
				Evoral::Beats beats = get_grid_beats(editor.pixel_to_sample(event_x));

				/* Shorten the length by 1 tick so that we can add a new note at the next
				   grid snap without it overlapping this one.
				*/
				beats -= Evoral::Beats::tick();
				
				create_note_at (editor.pixel_to_sample (event_x), event_y, beats, true);

				break;
			}
		default:
			break;
		}

		_mouse_state = None;
		break;

	case SelectRectDragging:
	case AddDragging:
		editor.drags()->end_grab ((GdkEvent *) ev);
		_mouse_state = None;
		create_ghost_note (ev->x, ev->y);
		break;


	default:
		break;
	}

	if (_mouse_changed_selection) {
		trackview.editor().begin_reversible_selection_op (X_("Mouse Selection Change"));
		trackview.editor().commit_reversible_selection_op ();
	}

	return false;
}

bool
MidiRegionView::motion (GdkEventMotion* ev)
{
	PublicEditor& editor = trackview.editor ();

	if (!_ghost_note && editor.current_mouse_mode() == MouseContent &&
	    Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier()) &&
	    _mouse_state != AddDragging) {

		create_ghost_note (ev->x, ev->y);

	} else if (_ghost_note && editor.current_mouse_mode() == MouseContent &&
	           Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier())) {

		update_ghost_note (ev->x, ev->y);

	} else if (_ghost_note && editor.current_mouse_mode() == MouseContent) {

		remove_ghost_note ();
		editor.verbose_cursor()->hide ();

	} else if (_ghost_note && editor.current_mouse_mode() == MouseDraw) {

		update_ghost_note (ev->x, ev->y);
	}

	/* any motion immediately hides velocity text that may have been visible */

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->hide_velocity ();
	}

	switch (_mouse_state) {
	case Pressed:

		if (_pressed_button == 1) {
			
			MouseMode m = editor.current_mouse_mode();
			
			if (m == MouseDraw || (m == MouseContent && Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier()))) {
				editor.drags()->set (new NoteCreateDrag (dynamic_cast<Editor *> (&editor), group, this), (GdkEvent *) ev);
				_mouse_state = AddDragging;
				remove_ghost_note ();
				editor.verbose_cursor()->hide ();
				return true;
			} else if (m == MouseContent) {
				editor.drags()->set (new MidiRubberbandSelectDrag (dynamic_cast<Editor *> (&editor), this), (GdkEvent *) ev);
				if (!Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
					clear_selection ();
					_mouse_changed_selection = true;
				}
				_mouse_state = SelectRectDragging;
				return true;
			} else if (m == MouseRange) {
				editor.drags()->set (new MidiVerticalSelectDrag (dynamic_cast<Editor *> (&editor), this), (GdkEvent *) ev);
				_mouse_state = SelectVerticalDragging;
				return true;
			}
		}

		return false;

	case SelectRectDragging:
	case SelectVerticalDragging:
	case AddDragging:
		editor.drags()->motion_handler ((GdkEvent *) ev, false);
		break;
		
	case SelectTouchDragging:
		return false;

	default:
		break;

	}

	/* we may be dragging some non-note object (eg. patch-change, sysex) 
	 */

	return editor.drags()->motion_handler ((GdkEvent *) ev, false);
}


bool
MidiRegionView::scroll (GdkEventScroll* ev)
{
	if (_selection.empty()) {
		return false;
	}

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		/* XXX: bit of a hack; allow PrimaryModifier scroll through so that
		   it still works for zoom.
		*/
		return false;
	}

	trackview.editor().verbose_cursor()->hide ();

	bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
	bool together = Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier);

	if (ev->direction == GDK_SCROLL_UP) {
		change_velocities (true, fine, false, together);
	} else if (ev->direction == GDK_SCROLL_DOWN) {
		change_velocities (false, fine, false, together);
	} else {
		/* left, right: we don't use them */
		return false;
	}

	return true;
}

bool
MidiRegionView::key_press (GdkEventKey* ev)
{
	/* since GTK bindings are generally activated on press, and since
	   detectable auto-repeat is the name of the game and only sends
	   repeated presses, carry out key actions at key press, not release.
	*/

	bool unmodified = Keyboard::no_modifier_keys_pressed (ev);
	
	if (unmodified && (ev->keyval == GDK_Alt_L || ev->keyval == GDK_Alt_R)) {
		_mouse_state = SelectTouchDragging;
		return true;

	} else if (ev->keyval == GDK_Escape && unmodified) {
		clear_selection();
		_mouse_state = None;

	} else if (ev->keyval == GDK_comma || ev->keyval == GDK_period) {

		bool start = (ev->keyval == GDK_comma);
		bool end = (ev->keyval == GDK_period);
		bool shorter = Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier);
		bool fine = Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);

		change_note_lengths (fine, shorter, Evoral::Beats(), start, end);

		return true;

	} else if ((ev->keyval == GDK_BackSpace || ev->keyval == GDK_Delete) && unmodified) {

		if (_selection.empty()) {
			return false;
		}

		delete_selection();
		return true;

	} else if (ev->keyval == GDK_Tab || ev->keyval == GDK_ISO_Left_Tab) {

		trackview.editor().begin_reversible_selection_op (X_("Select Adjacent Note"));

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
			goto_previous_note (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier));
		} else {
			goto_next_note (Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier));
		}

		trackview.editor().commit_reversible_selection_op();

		return true;

	} else if (ev->keyval == GDK_Up) {

		bool allow_smush = Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier);
		bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
		bool together = Keyboard::modifier_state_contains (ev->state, Keyboard::Level4Modifier);

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
			change_velocities (true, fine, allow_smush, together);
		} else {
			transpose (true, fine, allow_smush);
		}
		return true;

	} else if (ev->keyval == GDK_Down) {

		bool allow_smush = Keyboard::modifier_state_contains (ev->state, Keyboard::TertiaryModifier);
		bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
		bool together = Keyboard::modifier_state_contains (ev->state, Keyboard::Level4Modifier);

		if (Keyboard::modifier_state_contains (ev->state, Keyboard::PrimaryModifier)) {
			change_velocities (false, fine, allow_smush, together);
		} else {
			transpose (false, fine, allow_smush);
		}
		return true;

	} else if (ev->keyval == GDK_Left) {

		bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
		nudge_notes (false, fine);
		return true;

	} else if (ev->keyval == GDK_Right) {

		bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
		nudge_notes (true, fine);
		return true;

	} else if (ev->keyval == GDK_c && unmodified) {
		channel_edit ();
		return true;

	} else if (ev->keyval == GDK_v && unmodified) {
		velocity_edit ();
		return true;
	}

	return false;
}

bool
MidiRegionView::key_release (GdkEventKey* ev)
{
	if ((_mouse_state == SelectTouchDragging) && (ev->keyval == GDK_Alt_L || ev->keyval == GDK_Alt_R)) {
		_mouse_state = None;
		return true;
	}
	return false;
}

void
MidiRegionView::channel_edit ()
{
	if (_selection.empty()) {
		return;
	}

	/* pick a note somewhat at random (since Selection is a set<>) to
	 * provide the "current" channel for the dialog.
	 */

	uint8_t current_channel = (*_selection.begin())->note()->channel ();
	MidiChannelDialog channel_dialog (current_channel);
	int ret = channel_dialog.run ();

	switch (ret) {
	case Gtk::RESPONSE_OK:
		break;
	default:
		return;
	}

	uint8_t new_channel = channel_dialog.active_channel ();

	start_note_diff_command (_("channel edit"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_channel (*i, new_channel);
		i = next;
	}

	apply_diff ();
}

void
MidiRegionView::velocity_edit ()
{
	if (_selection.empty()) {
		return;
	}
	
	/* pick a note somewhat at random (since Selection is a set<>) to
	 * provide the "current" velocity for the dialog.
	 */

	uint8_t current_velocity = (*_selection.begin())->note()->velocity ();
	MidiVelocityDialog velocity_dialog (current_velocity);
	int ret = velocity_dialog.run ();

	switch (ret) {
	case Gtk::RESPONSE_OK:
		break;
	default:
		return;
	}

	uint8_t new_velocity = velocity_dialog.velocity ();

	start_note_diff_command (_("velocity edit"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_velocity (*i, new_velocity, false);
		i = next;
	}

	apply_diff ();
}

void
MidiRegionView::show_list_editor ()
{
	if (!_list_editor) {
		_list_editor = new MidiListEditor (trackview.session(), midi_region(), midi_view()->midi_track());
	}
	_list_editor->present ();
}

/** Add a note to the model, and the view, at a canvas (click) coordinate.
 * \param t time in frames relative to the position of the region
 * \param y vertical position in pixels
 * \param length duration of the note in beats
 * \param snap_t true to snap t to the grid, otherwise false.
 */
void
MidiRegionView::create_note_at (framepos_t t, double y, Evoral::Beats length, bool snap_t)
{
	if (length < 2 * DBL_EPSILON) {
		return;
	}

	MidiTimeAxisView* const mtv  = dynamic_cast<MidiTimeAxisView*>(&trackview);
	MidiStreamView* const   view = mtv->midi_view();

	// Start of note in frames relative to region start
	if (snap_t) {
		framecnt_t grid_frames;
		t = snap_frame_to_grid_underneath (t, grid_frames);
	}

	const MidiModel::TimeType beat_time = region_frames_to_region_beats(
		t + _region->start());

	const double  note     = view->y_to_note(y);
	const uint8_t chan     = mtv->get_channel_for_add();
	const uint8_t velocity = get_velocity_for_add(beat_time);

	const boost::shared_ptr<NoteType> new_note(
		new NoteType (chan, beat_time, length, (uint8_t)note, velocity));

	if (_model->contains (new_note)) {
		return;
	}

	view->update_note_range(new_note->note());

	start_note_diff_command(_("add note"));

	clear_selection ();
	note_diff_add_note (new_note, true, false);

	apply_diff();

	play_midi_note (new_note);
}

void
MidiRegionView::clear_events (bool with_selection_signal)
{
	clear_selection (with_selection_signal);

	MidiGhostRegion* gr;
	for (std::vector<GhostRegion*>::iterator g = ghosts.begin(); g != ghosts.end(); ++g) {
		if ((gr = dynamic_cast<MidiGhostRegion*>(*g)) != 0) {
			gr->clear_events();
		}
	}

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		delete *i;
	}

	_events.clear();
	_patch_changes.clear();
	_sys_exes.clear();
	_optimization_iterator = _events.end();
}

void
MidiRegionView::display_model(boost::shared_ptr<MidiModel> model)
{
	_model = model;

	content_connection.disconnect ();
	_model->ContentsChanged.connect (content_connection, invalidator (*this), boost::bind (&MidiRegionView::redisplay_model, this), gui_context());
	/* Don't signal as nobody else needs to know until selection has been altered. */
	clear_events (false);

	if (_enable_display) {
		redisplay_model();
	}
}

void
MidiRegionView::start_note_diff_command (string name)
{
	if (!_note_diff_command) {
		trackview.editor().begin_reversible_command (name);
		_note_diff_command = _model->new_note_diff_command (name);
	}
}

void
MidiRegionView::note_diff_add_note (const boost::shared_ptr<NoteType> note, bool selected, bool show_velocity)
{
	if (_note_diff_command) {
		_note_diff_command->add (note);
	}
	if (selected) {
		_marked_for_selection.insert(note);
	}
	if (show_velocity) {
		_marked_for_velocity.insert(note);
	}
}

void
MidiRegionView::note_diff_remove_note (NoteBase* ev)
{
	if (_note_diff_command && ev->note()) {
		_note_diff_command->remove(ev->note());
	}
}

void
MidiRegionView::note_diff_add_change (NoteBase* ev,
                                      MidiModel::NoteDiffCommand::Property property,
                                      uint8_t val)
{
	if (_note_diff_command) {
		_note_diff_command->change (ev->note(), property, val);
	}
}

void
MidiRegionView::note_diff_add_change (NoteBase* ev,
                                      MidiModel::NoteDiffCommand::Property property,
                                      Evoral::Beats val)
{
	if (_note_diff_command) {
		_note_diff_command->change (ev->note(), property, val);
	}
}

void
MidiRegionView::apply_diff (bool as_subcommand)
{
	bool add_or_remove;
	bool commit = false;

	if (!_note_diff_command) {
		return;
	}

	if ((add_or_remove = _note_diff_command->adds_or_removes())) {
		// Mark all selected notes for selection when model reloads
		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			_marked_for_selection.insert((*i)->note());
		}
	}

	midi_view()->midi_track()->midi_playlist()->region_edited(
		_region, _note_diff_command);

	if (as_subcommand) {
		_model->apply_command_as_subcommand (*trackview.session(), _note_diff_command);
	} else {
		_model->apply_command (*trackview.session(), _note_diff_command);
		commit = true;
	}

	_note_diff_command = 0;

	if (add_or_remove) {
		_marked_for_selection.clear();
	}

	_marked_for_velocity.clear();
	if (commit) {
		trackview.editor().commit_reversible_command ();
	}
}

void
MidiRegionView::abort_command()
{
	delete _note_diff_command;
	_note_diff_command = 0;
	clear_selection();
}

NoteBase*
MidiRegionView::find_canvas_note (boost::shared_ptr<NoteType> note)
{
	if (_optimization_iterator != _events.end()) {
		++_optimization_iterator;
	}

	if (_optimization_iterator != _events.end() && (*_optimization_iterator)->note() == note) {
		return *_optimization_iterator;
	}

	for (_optimization_iterator = _events.begin(); _optimization_iterator != _events.end(); ++_optimization_iterator) {
		if ((*_optimization_iterator)->note() == note) {
			return *_optimization_iterator;
		}
	}

	return 0;
}

/** This version finds any canvas note matching the supplied note. */
NoteBase*
MidiRegionView::find_canvas_note (NoteType note)
{
	Events::iterator it;

	for (it = _events.begin(); it != _events.end(); ++it) {
		if (*((*it)->note()) == note) {
			return *it;
		}
	}

	return 0;
}

void
MidiRegionView::get_events (Events& e, Evoral::Sequence<Evoral::Beats>::NoteOperator op, uint8_t val, int chan_mask)
{
	MidiModel::Notes notes;
	_model->get_notes (notes, op, val, chan_mask);

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {
		NoteBase* cne = find_canvas_note (*n);
		if (cne) {
			e.push_back (cne);
		}
	}
}

void
MidiRegionView::redisplay_model()
{
	if (_active_notes) {
		// Currently recording
		const framecnt_t zoom = trackview.editor().get_current_zoom();
		if (zoom != _last_display_zoom) {
			/* Update resolved canvas notes to reflect changes in zoom without
			   touching model.  Leave active notes (with length 0) alone since
			   they are being extended. */
			for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
				if ((*i)->note()->length() > 0) {
					update_note(*i);
				}
			}
			_last_display_zoom = zoom;
		}
		return;
	}

	if (!_model) {
		return;
	}

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		(*i)->invalidate ();
	}

	MidiModel::ReadLock lock(_model->read_lock());

	MidiModel::Notes& notes (_model->notes());
	_optimization_iterator = _events.begin();

	bool empty_when_starting = _events.empty();

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		boost::shared_ptr<NoteType> note (*n);
		NoteBase* cne;
		bool visible;

		if (note_in_region_range (note, visible)) {
			
			if (!empty_when_starting && (cne = find_canvas_note (note)) != 0) {

				cne->validate ();
				update_note (cne);

				if (visible) {
					cne->show ();
				} else {
					cne->hide ();
				}

			} else {

				cne = add_note (note, visible);
			}

			set<boost::shared_ptr<NoteType> >::iterator it;
			for (it = _pending_note_selection.begin(); it != _pending_note_selection.end(); ++it) {
				if (*(*it) == *note) {
					add_to_selection (cne);
				}
			}

		} else {
			
			if (!empty_when_starting && (cne = find_canvas_note (note)) != 0) {
				cne->validate ();
				cne->hide ();
			}
		}
	}

	/* remove note items that are no longer valid */

	if (!empty_when_starting) {
		for (Events::iterator i = _events.begin(); i != _events.end(); ) {
			if (!(*i)->valid ()) {
				
				for (vector<GhostRegion*>::iterator j = ghosts.begin(); j != ghosts.end(); ++j) {
					MidiGhostRegion* gr = dynamic_cast<MidiGhostRegion*> (*j);
					if (gr) {
						gr->remove_note (*i);
					}
				}
				
				delete *i;
				i = _events.erase (i);
				
			} else {
				++i;
			}
		}
	}

	_patch_changes.clear();
	_sys_exes.clear();

	display_sysexes();
	display_patch_changes ();

	_marked_for_selection.clear ();
	_marked_for_velocity.clear ();
	_pending_note_selection.clear ();

	/* we may have caused _events to contain things out of order (e.g. if a note
	   moved earlier or later). we don't generally need them in time order, but
	   make a note that a sort is required for those cases that require it.
	*/

	_sort_needed = true;
}

void
MidiRegionView::display_patch_changes ()
{
	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	uint16_t chn_mask = mtv->midi_track()->get_playback_channel_mask();

	for (uint8_t i = 0; i < 16; ++i) {
		display_patch_changes_on_channel (i, chn_mask & (1 << i));
	}
}

/** @param active_channel true to display patch changes fully, false to display
 * them `greyed-out' (as on an inactive channel)
 */
void
MidiRegionView::display_patch_changes_on_channel (uint8_t channel, bool active_channel)
{
	for (MidiModel::PatchChanges::const_iterator i = _model->patch_changes().begin(); i != _model->patch_changes().end(); ++i) {

		if ((*i)->channel() != channel) {
			continue;
		}

		const string patch_name = instrument_info().get_patch_name ((*i)->bank(), (*i)->program(), channel);
		add_canvas_patch_change (*i, patch_name, active_channel);
	}
}

void
MidiRegionView::display_sysexes()
{
	bool have_periodic_system_messages = false;
	bool display_periodic_messages = true;

	if (!ARDOUR_UI::config()->get_never_display_periodic_midi()) {

		for (MidiModel::SysExes::const_iterator i = _model->sysexes().begin(); i != _model->sysexes().end(); ++i) {
			const boost::shared_ptr<const Evoral::MIDIEvent<Evoral::Beats> > mev = 
				boost::static_pointer_cast<const Evoral::MIDIEvent<Evoral::Beats> > (*i);
			
			if (mev) {
				if (mev->is_spp() || mev->is_mtc_quarter() || mev->is_mtc_full()) {
					have_periodic_system_messages = true;
					break;
				}
			}
		}
		
		if (have_periodic_system_messages) {
			double zoom = trackview.editor().get_current_zoom (); // frames per pixel
			
			/* get an approximate value for the number of samples per video frame */
			
			double video_frame = trackview.session()->frame_rate() * (1.0/30);
			
			/* if we are zoomed out beyond than the cutoff (i.e. more
			 * frames per pixel than frames per 4 video frames), don't
			 * show periodic sysex messages.
			 */
			
			if (zoom > (video_frame*4)) {
				display_periodic_messages = false;
			} 
		}
	} else {
		display_periodic_messages = false;
	}

	for (MidiModel::SysExes::const_iterator i = _model->sysexes().begin(); i != _model->sysexes().end(); ++i) {

		const boost::shared_ptr<const Evoral::MIDIEvent<Evoral::Beats> > mev = 
			boost::static_pointer_cast<const Evoral::MIDIEvent<Evoral::Beats> > (*i);

		Evoral::Beats time = (*i)->time();

		if (mev) {
			if (mev->is_spp() || mev->is_mtc_quarter() || mev->is_mtc_full()) {
				if (!display_periodic_messages) {
					continue;
				}
			}
		}

		ostringstream str;
		str << hex;
		for (uint32_t b = 0; b < (*i)->size(); ++b) {
			str << int((*i)->buffer()[b]);
			if (b != (*i)->size() -1) {
				str << " ";
			}
		}
		string text = str.str();

		const double x = trackview.editor().sample_to_pixel(source_beats_to_region_frames(time));

		double height = midi_stream_view()->contents_height();

		// CAIROCANVAS: no longer passing *i (the sysex event) to the
		// SysEx canvas object!!!

		boost::shared_ptr<SysEx> sysex = boost::shared_ptr<SysEx>(
			new SysEx (*this, _note_group, text, height, x, 1.0));

		// Show unless message is beyond the region bounds
		if (time - _region->start() >= _region->length() || time < _region->start()) {
			sysex->hide();
		} else {
			sysex->show();
		}

		_sys_exes.push_back(sysex);
	}
}

MidiRegionView::~MidiRegionView ()
{
	in_destructor = true;

	trackview.editor().verbose_cursor()->hide ();

	note_delete_connection.disconnect ();

	delete _list_editor;

	RegionViewGoingAway (this); /* EMIT_SIGNAL */

	if (_active_notes) {
		end_write();
	}

	_selection_cleared_connection.disconnect ();

	_selection.clear();
	clear_events (false);

	delete _note_group;
	delete _note_diff_command;
	delete _step_edit_cursor;
	delete _temporary_note_group;
}

void
MidiRegionView::region_resized (const PropertyChange& what_changed)
{
	RegionView::region_resized(what_changed);

	if (what_changed.contains (ARDOUR::Properties::position)) {
		_region_relative_time_converter.set_origin_b(_region->position());
		set_duration(_region->length(), 0);
		if (_enable_display) {
			redisplay_model();
		}
	}

	if (what_changed.contains (ARDOUR::Properties::start) ||
	    what_changed.contains (ARDOUR::Properties::position)) {
		_source_relative_time_converter.set_origin_b (_region->position() - _region->start());
	}
}

void
MidiRegionView::reset_width_dependent_items (double pixel_width)
{
	RegionView::reset_width_dependent_items(pixel_width);

	if (_enable_display) {
		redisplay_model();
	}

	for (PatchChanges::iterator x = _patch_changes.begin(); x != _patch_changes.end(); ++x) {
		if ((*x)->canvas_item()->width() >= _pixel_width) {
			(*x)->hide();
		} else {
			(*x)->show();
		}
	}

	move_step_edit_cursor (_step_edit_cursor_position);
	set_step_edit_cursor_width (_step_edit_cursor_width);
}

void
MidiRegionView::set_height (double height)
{
	double old_height = _height;
	RegionView::set_height(height);

	apply_note_range (midi_stream_view()->lowest_note(),
			  midi_stream_view()->highest_note(),
			  height != old_height);

	if (name_text) {
		name_text->raise_to_top();
	}

	for (PatchChanges::iterator x = _patch_changes.begin(); x != _patch_changes.end(); ++x) {
		(*x)->set_height (midi_stream_view()->contents_height());
	}

	if (_step_edit_cursor) {
		_step_edit_cursor->set_y1 (midi_stream_view()->contents_height());
	}
}


/** Apply the current note range from the stream view
 * by repositioning/hiding notes as necessary
 */
void
MidiRegionView::apply_note_range (uint8_t min, uint8_t max, bool force)
{
	if (!_enable_display) {
		return;
	}

	if (!force && _current_range_min == min && _current_range_max == max) {
		return;
	}

	_current_range_min = min;
	_current_range_max = max;

	for (Events::const_iterator i = _events.begin(); i != _events.end(); ++i) {
		NoteBase* event = *i;
		boost::shared_ptr<NoteType> note (event->note());

		if (note->note() < _current_range_min ||
		    note->note() > _current_range_max) {
			event->hide();
		} else {
			event->show();
		}

		if (Note* cnote = dynamic_cast<Note*>(event)) {

			const double y0 = 1. + floor (midi_stream_view()->note_to_y(note->note()));
			const double y1 = y0 + std::max(1., floor(midi_stream_view()->note_height()) - 1.);

			if (y0 < 0 || y1 >= _height) {
				/* During DnD, the region uses the 'old/current'
				 * midi_stream_view()'s range and its position/height calculation.
				 *
				 * Ideally DnD would decouple the midi_stream_view() for the
				 * region(s) being dragged and set it to the target's range
				 * (or in case of the drop-zone, FullRange).
				 * but I don't see how this can be done without major rework.
				 *
				 * For now, just prevent visual bleeding of events in case
				 * the target-track is smaller.
				 */
				event->hide();
				continue;
			}
			cnote->set_y0 (y0);
			cnote->set_y1 (y1);

		} else if (Hit* chit = dynamic_cast<Hit*>(event)) {
			update_hit (chit);
		}
	}
}

GhostRegion*
MidiRegionView::add_ghost (TimeAxisView& tv)
{
	double unit_position = _region->position () / samples_per_pixel;
	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(&tv);
	MidiGhostRegion* ghost;

	if (mtv && mtv->midi_view()) {
		/* if ghost is inserted into midi track, use a dedicated midi ghost canvas group
		   to allow having midi notes on top of note lines and waveforms.
		*/
		ghost = new MidiGhostRegion (*mtv->midi_view(), trackview, unit_position);
	} else {
		ghost = new MidiGhostRegion (tv, trackview, unit_position);
	}

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		ghost->add_note(*i);
	}

	ghost->set_height ();
	ghost->set_duration (_region->length() / samples_per_pixel);
	ghosts.push_back (ghost);

	GhostRegion::CatchDeletion.connect (*this, invalidator (*this), boost::bind (&RegionView::remove_ghost, this, _1), gui_context());

	return ghost;
}


/** Begin tracking note state for successive calls to add_event
 */
void
MidiRegionView::begin_write()
{
	if (_active_notes) {
		delete[] _active_notes;
	}
	_active_notes = new Note*[128];
	for (unsigned i = 0; i < 128; ++i) {
		_active_notes[i] = 0;
	}
}


/** Destroy note state for add_event
 */
void
MidiRegionView::end_write()
{
	delete[] _active_notes;
	_active_notes = 0;
	_marked_for_selection.clear();
	_marked_for_velocity.clear();
}


/** Resolve an active MIDI note (while recording).
 */
void
MidiRegionView::resolve_note(uint8_t note, Evoral::Beats end_time)
{
	if (midi_view()->note_mode() != Sustained) {
		return;
	}

	if (_active_notes && _active_notes[note]) {
		/* Set note length so update_note() works.  Note this is a local note
		   for recording, not from a model, so we can safely mess with it. */
		_active_notes[note]->note()->set_length(
			end_time - _active_notes[note]->note()->time());

		/* End time is relative to the region being recorded. */
		const framepos_t end_time_frames = region_beats_to_region_frames(end_time);

		_active_notes[note]->set_x1 (trackview.editor().sample_to_pixel(end_time_frames));
		_active_notes[note]->set_outline_all ();
		_active_notes[note] = 0;
	}
}


/** Extend active notes to rightmost edge of region (if length is changed)
 */
void
MidiRegionView::extend_active_notes()
{
	if (!_active_notes) {
		return;
	}

	for (unsigned i = 0; i < 128; ++i) {
		if (_active_notes[i]) {
			_active_notes[i]->set_x1(
				trackview.editor().sample_to_pixel(_region->length()));
		}
	}
}

void
MidiRegionView::play_midi_note(boost::shared_ptr<NoteType> note)
{
	if (_no_sound_notes || !ARDOUR_UI::config()->get_sound_midi_notes()) {
		return;
	}

	RouteUI* route_ui = dynamic_cast<RouteUI*> (&trackview);

	if (!route_ui || !route_ui->midi_track()) {
		return;
	}

	NotePlayer* np = new NotePlayer (route_ui->midi_track ());
	np->add (note);
	np->play ();

	/* NotePlayer deletes itself */
}

void
MidiRegionView::start_playing_midi_note(boost::shared_ptr<NoteType> note)
{
	const std::vector< boost::shared_ptr<NoteType> > notes(1, note);
	start_playing_midi_chord(notes);
}

void
MidiRegionView::start_playing_midi_chord (vector<boost::shared_ptr<NoteType> > notes)
{
	if (_no_sound_notes || !ARDOUR_UI::config()->get_sound_midi_notes()) {
		return;
	}

	RouteUI* route_ui = dynamic_cast<RouteUI*> (&trackview);

	if (!route_ui || !route_ui->midi_track()) {
		return;
	}

	NotePlayer* player = new NotePlayer (route_ui->midi_track());

	for (vector<boost::shared_ptr<NoteType> >::iterator n = notes.begin(); n != notes.end(); ++n) {
		player->add (*n);
	}

	player->play ();
}


bool
MidiRegionView::note_in_region_range (const boost::shared_ptr<NoteType> note, bool& visible) const
{
	/* This is imprecise due to all the conversion conversion involved, so only
	   hide notes if they seem to start more than one tick before the start. */
	const framecnt_t tick_frames       = Evoral::Beats::tick().to_ticks(trackview.session()->frame_rate());
	const framepos_t note_start_frames = source_beats_to_region_frames (note->time());
	const bool       outside           = ((note_start_frames <= -tick_frames) ||
	                                      (note_start_frames >= _region->length()));

	visible = (note->note() >= midi_stream_view()->lowest_note()) &&
		(note->note() <= midi_stream_view()->highest_note());

	return !outside;
}

void
MidiRegionView::update_note (NoteBase* note, bool update_ghost_regions)
{
	Note* sus = NULL;
	Hit*  hit = NULL;
	if ((sus = dynamic_cast<Note*>(note))) {
		update_sustained(sus, update_ghost_regions);
	} else if ((hit = dynamic_cast<Hit*>(note))) {
		update_hit(hit, update_ghost_regions);
	}
}

/** Update a canvas note's size from its model note.
 *  @param ev Canvas note to update.
 *  @param update_ghost_regions true to update the note in any ghost regions that we have, otherwise false.
 */
void
MidiRegionView::update_sustained (Note* ev, bool update_ghost_regions)
{
	boost::shared_ptr<NoteType> note = ev->note();
	const double x = trackview.editor().sample_to_pixel (source_beats_to_region_frames (note->time()));
	const double y0 = 1 + floor(midi_stream_view()->note_to_y(note->note()));

	ev->set_x0 (x);
	ev->set_y0 (y0);

	/* trim note display to not overlap the end of its region */

	if (note->length() > 0) {
		const framepos_t note_end_frames = min (source_beats_to_region_frames (note->end_time()), _region->length());
		ev->set_x1 (trackview.editor().sample_to_pixel (note_end_frames));
	} else {
		ev->set_x1 (trackview.editor().sample_to_pixel (_region->length()));
	}

	ev->set_y1 (y0 + std::max(1., floor(midi_stream_view()->note_height()) - 1));

	if (!note->length()) {
		if (_active_notes && note->note() < 128) {
			Note* const old_rect = _active_notes[note->note()];
			if (old_rect) {
				/* There is an active note on this key, so we have a stuck
				   note.  Finish the old rectangle here. */
				old_rect->set_x1 (x);
				old_rect->set_outline_all ();
			}
			_active_notes[note->note()] = ev;
		}
		/* outline all but right edge */
		ev->set_outline_what (ArdourCanvas::Rectangle::What (
					      ArdourCanvas::Rectangle::TOP|
					      ArdourCanvas::Rectangle::LEFT|
					      ArdourCanvas::Rectangle::BOTTOM));
	} else {
		/* outline all edges */
		ev->set_outline_all ();
	}

	// Update color in case velocity has changed
	ev->set_fill_color(ev->base_color());
	ev->set_outline_color(ev->calculate_outline(ev->base_color(), ev->selected()));

	if (update_ghost_regions) {
		for (std::vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
			MidiGhostRegion* gr = dynamic_cast<MidiGhostRegion*> (*i);
			if (gr) {
				gr->update_note (ev);
			}
		}
	}
}

void
MidiRegionView::update_hit (Hit* ev, bool update_ghost_regions)
{
	boost::shared_ptr<NoteType> note = ev->note();

	const framepos_t note_start_frames = source_beats_to_region_frames(note->time());
	const double x = trackview.editor().sample_to_pixel(note_start_frames);
	const double diamond_size = std::max(1., floor(midi_stream_view()->note_height()) - 2.);
	const double y = 1.5 + floor(midi_stream_view()->note_to_y(note->note())) + diamond_size * .5;

	// see DnD note in MidiRegionView::apply_note_range() above
	if (y <= 0 || y >= _height) {
		ev->hide();
	} else {
		ev->show();
	}

	ev->set_position (ArdourCanvas::Duple (x, y));
	ev->set_height (diamond_size);

	// Update color in case velocity has changed
	ev->set_fill_color(ev->base_color());
	ev->set_outline_color(ev->calculate_outline(ev->base_color(), ev->selected()));

	if (update_ghost_regions) {
		for (std::vector<GhostRegion*>::iterator i = ghosts.begin(); i != ghosts.end(); ++i) {
			MidiGhostRegion* gr = dynamic_cast<MidiGhostRegion*> (*i);
			if (gr) {
				gr->update_note (ev);
			}
		}
	}
}

/** Add a MIDI note to the view (with length).
 *
 * If in sustained mode, notes with length 0 will be considered active
 * notes, and resolve_note should be called when the corresponding note off
 * event arrives, to properly display the note.
 */
NoteBase*
MidiRegionView::add_note(const boost::shared_ptr<NoteType> note, bool visible)
{
	NoteBase* event = 0;

	if (midi_view()->note_mode() == Sustained) {

		Note* ev_rect = new Note (*this, _note_group, note);

		update_sustained (ev_rect);

		event = ev_rect;

	} else if (midi_view()->note_mode() == Percussive) {

		const double diamond_size = std::max(1., floor(midi_stream_view()->note_height()) - 2.);

		Hit* ev_diamond = new Hit (*this, _note_group, diamond_size, note);

		update_hit (ev_diamond);

		event = ev_diamond;

	} else {
		event = 0;
	}

	if (event) {
		MidiGhostRegion* gr;

		for (std::vector<GhostRegion*>::iterator g = ghosts.begin(); g != ghosts.end(); ++g) {
			if ((gr = dynamic_cast<MidiGhostRegion*>(*g)) != 0) {
				gr->add_note(event);
			}
		}

		if (_marked_for_selection.find(note) != _marked_for_selection.end()) {
			note_selected(event, true);
		}

		if (_marked_for_velocity.find(note) != _marked_for_velocity.end()) {
			event->show_velocity();
		}

		event->on_channel_selection_change (get_selected_channels());
		_events.push_back(event);

		if (visible) {
			event->show();
		} else {
			event->hide ();
		}
	}

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	MidiStreamView* const view = mtv->midi_view();

	view->update_note_range (note->note());
	return event;
}

void
MidiRegionView::step_add_note (uint8_t channel, uint8_t number, uint8_t velocity,
                               Evoral::Beats pos, Evoral::Beats len)
{
	boost::shared_ptr<NoteType> new_note (new NoteType (channel, pos, len, number, velocity));

	/* potentially extend region to hold new note */

	framepos_t end_frame = source_beats_to_absolute_frames (new_note->end_time());
	framepos_t region_end = _region->last_frame();

	if (end_frame > region_end) {
		_region->set_length (end_frame - _region->position());
	}

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	MidiStreamView* const view = mtv->midi_view();

	view->update_note_range(new_note->note());

	_marked_for_selection.clear ();

	start_note_diff_command (_("step add"));

	clear_selection ();
	note_diff_add_note (new_note, true, false);

	apply_diff();

	// last_step_edit_note = new_note;
}

void
MidiRegionView::step_sustain (Evoral::Beats beats)
{
	change_note_lengths (false, false, beats, false, true);
}

/** Add a new patch change flag to the canvas.
 * @param patch the patch change to add
 * @param the text to display in the flag
 * @param active_channel true to display the flag as on an active channel, false to grey it out for an inactive channel.
 */
void
MidiRegionView::add_canvas_patch_change (MidiModel::PatchChangePtr patch, const string& displaytext, bool /*active_channel*/)
{
	framecnt_t region_frames = source_beats_to_region_frames (patch->time());
	const double x = trackview.editor().sample_to_pixel (region_frames);

	double const height = midi_stream_view()->contents_height();

	// CAIROCANVAS: active_channel info removed from PatcChange constructor
	// so we need to do something more sophisticated to keep its color
	// appearance (MidiPatchChangeFill/MidiPatchChangeInactiveChannelFill)
	// up to date.

	boost::shared_ptr<PatchChange> patch_change = boost::shared_ptr<PatchChange>(
		new PatchChange(*this, group,
				displaytext,
				height,
				x, 1.0,
				instrument_info(),
				patch));

	if (patch_change->item().width() < _pixel_width) {
		// Show unless patch change is beyond the region bounds
		if (region_frames < 0 || region_frames >= _region->length()) {
			patch_change->hide();
		} else {
			patch_change->show();
		}
	} else {
		patch_change->hide ();
	}

	_patch_changes.push_back (patch_change);
}

MIDI::Name::PatchPrimaryKey
MidiRegionView::patch_change_to_patch_key (MidiModel::PatchChangePtr p)
{
	return MIDI::Name::PatchPrimaryKey (p->program(), p->bank());
}

/// Return true iff @p pc applies to the given time on the given channel.
static bool
patch_applies (const ARDOUR::MidiModel::constPatchChangePtr pc, Evoral::Beats time, uint8_t channel)
{
	return pc->time() <= time && pc->channel() == channel;
}
	
void 
MidiRegionView::get_patch_key_at (Evoral::Beats time, uint8_t channel, MIDI::Name::PatchPrimaryKey& key) const
{
	// The earliest event not before time
	MidiModel::PatchChanges::iterator i = _model->patch_change_lower_bound (time);

	// Go backwards until we find the latest PC for this channel, or the start
	while (i != _model->patch_changes().begin() &&
	       (i == _model->patch_changes().end() ||
	        !patch_applies(*i, time, channel))) {
		--i;
	}

	if (i != _model->patch_changes().end() && patch_applies(*i, time, channel)) {
		key.set_bank((*i)->bank());
		key.set_program((*i)->program ());
	} else {
		key.set_bank(0);
		key.set_program(0);
	}
}

void
MidiRegionView::change_patch_change (PatchChange& pc, const MIDI::Name::PatchPrimaryKey& new_patch)
{
	string name = _("alter patch change");
	trackview.editor().begin_reversible_command (name);
	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (name);

	if (pc.patch()->program() != new_patch.program()) {
		c->change_program (pc.patch (), new_patch.program());
	}

	int const new_bank = new_patch.bank();
	if (pc.patch()->bank() != new_bank) {
		c->change_bank (pc.patch (), new_bank);
	}

	_model->apply_command (*trackview.session(), c);
	trackview.editor().commit_reversible_command ();

	_patch_changes.clear ();
	display_patch_changes ();
}

void
MidiRegionView::change_patch_change (MidiModel::PatchChangePtr old_change, const Evoral::PatchChange<Evoral::Beats> & new_change)
{
	string name = _("alter patch change");
	trackview.editor().begin_reversible_command (name);
	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (name);

	if (old_change->time() != new_change.time()) {
		c->change_time (old_change, new_change.time());
	}

	if (old_change->channel() != new_change.channel()) {
		c->change_channel (old_change, new_change.channel());
	}

	if (old_change->program() != new_change.program()) {
		c->change_program (old_change, new_change.program());
	}

	if (old_change->bank() != new_change.bank()) {
		c->change_bank (old_change, new_change.bank());
	}

	_model->apply_command (*trackview.session(), c);
	trackview.editor().commit_reversible_command ();

	_patch_changes.clear ();
	display_patch_changes ();
}

/** Add a patch change to the region.
 *  @param t Time in frames relative to region position
 *  @param patch Patch to add; time and channel are ignored (time is converted from t, and channel comes from
 *  MidiTimeAxisView::get_channel_for_add())
 */
void
MidiRegionView::add_patch_change (framecnt_t t, Evoral::PatchChange<Evoral::Beats> const & patch)
{
	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	string name = _("add patch change");

	trackview.editor().begin_reversible_command (name);
	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (name);
	c->add (MidiModel::PatchChangePtr (
		        new Evoral::PatchChange<Evoral::Beats> (
			        absolute_frames_to_source_beats (_region->position() + t),
				mtv->get_channel_for_add(), patch.program(), patch.bank()
				)
			)
		);

	_model->apply_command (*trackview.session(), c);
	trackview.editor().commit_reversible_command ();

	_patch_changes.clear ();
	display_patch_changes ();
}

void
MidiRegionView::move_patch_change (PatchChange& pc, Evoral::Beats t)
{
	trackview.editor().begin_reversible_command (_("move patch change"));
	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (_("move patch change"));
	c->change_time (pc.patch (), t);
	_model->apply_command (*trackview.session(), c);
	trackview.editor().commit_reversible_command ();

	_patch_changes.clear ();
	display_patch_changes ();
}

void
MidiRegionView::delete_patch_change (PatchChange* pc)
{
	trackview.editor().begin_reversible_command (_("delete patch change"));
	MidiModel::PatchChangeDiffCommand* c = _model->new_patch_change_diff_command (_("delete patch change"));
	c->remove (pc->patch ());
	_model->apply_command (*trackview.session(), c);
	trackview.editor().commit_reversible_command ();

	_patch_changes.clear ();
	display_patch_changes ();
}

void
MidiRegionView::step_patch (PatchChange& patch, bool bank, int delta)
{
	MIDI::Name::PatchPrimaryKey key = patch_change_to_patch_key(patch.patch());
	if (bank) {
		key.set_bank(key.bank() + delta);
	} else {
		key.set_program(key.program() + delta);
	}
	change_patch_change(patch, key);
}

void
MidiRegionView::maybe_remove_deleted_note_from_selection (NoteBase* cne)
{
	if (_selection.empty()) {
		return;
	}

	_selection.erase (cne);
}

void
MidiRegionView::delete_selection()
{
	if (_selection.empty()) {
		return;
	}

	start_note_diff_command (_("delete selection"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->selected()) {
			_note_diff_command->remove((*i)->note());
		}
	}

	_selection.clear();

	apply_diff ();
}

void
MidiRegionView::delete_note (boost::shared_ptr<NoteType> n)
{
	start_note_diff_command (_("delete note"));
	_note_diff_command->remove (n);
	apply_diff ();

	trackview.editor().verbose_cursor()->hide ();
}

void
MidiRegionView::clear_selection_except (NoteBase* ev, bool signal)
{
 	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		if ((*i) != ev) {
			Selection::iterator tmp = i;
			++tmp;

			(*i)->set_selected (false);
			(*i)->hide_velocity ();
			_selection.erase (i);

			i = tmp;
		} else {
			++i;
		}
	}

	if (!ev && _entered) {
		// Clearing selection entirely, ungrab keyboard
		Keyboard::magic_widget_drop_focus();
		_grabbed_keyboard = false;
	}

	/* this does not change the status of this regionview w.r.t the editor
	   selection.
	*/

	if (signal) {
		SelectionCleared (this); /* EMIT SIGNAL */
	}
}

void
MidiRegionView::unique_select(NoteBase* ev)
{
	const bool selection_was_empty = _selection.empty();

	clear_selection_except (ev);

	/* don't bother with checking to see if we should remove this
	   regionview from the editor selection, since we're about to add
	   another note, and thus put/keep this regionview in the editor
	   selection anyway.
	*/

	if (!ev->selected()) {
		add_to_selection (ev);
		if (selection_was_empty && _entered) {
			// Grab keyboard for moving notes with arrow keys
			Keyboard::magic_widget_grab_focus();
			_grabbed_keyboard = true;
		}
	}
}

void
MidiRegionView::select_all_notes ()
{
	clear_selection ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		add_to_selection (*i);
	}
}

void
MidiRegionView::select_range (framepos_t start, framepos_t end)
{
	clear_selection ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		framepos_t t = source_beats_to_absolute_frames((*i)->note()->time());
		if (t >= start && t <= end) {
			add_to_selection (*i);
		}
	}
}

void
MidiRegionView::invert_selection ()
{
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((*i)->selected()) {
			remove_from_selection(*i);
		} else {
			add_to_selection (*i);
		}
	}
}

/** Used for selection undo/redo.
    The requested notes most likely won't exist in the view until the next model redisplay.
*/
void
MidiRegionView::select_notes (list<boost::shared_ptr<NoteType> > notes)
{
	NoteBase* cne;
	list<boost::shared_ptr<NoteType> >::iterator n;

	for (n = notes.begin(); n != notes.end(); ++n) {
		if ((cne = find_canvas_note(*(*n))) != 0) {
			add_to_selection (cne);
		} else {
			_pending_note_selection.insert(*n);
		}
	}
}

void
MidiRegionView::select_matching_notes (uint8_t notenum, uint16_t channel_mask, bool add, bool extend)
{
	bool have_selection = !_selection.empty();
	uint8_t low_note = 127;
	uint8_t high_note = 0;
	MidiModel::Notes& notes (_model->notes());
	_optimization_iterator = _events.begin();
	
	if (extend && !have_selection) {
		extend = false;
	}

	/* scan existing selection to get note range */
	
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->note()->note() < low_note) {
			low_note = (*i)->note()->note();
		}
		if ((*i)->note()->note() > high_note) {
			high_note = (*i)->note()->note();
		}
	}
	
	if (!add) {
		clear_selection ();

		if (!extend && (low_note == high_note) && (high_note == notenum)) {
			/* only note previously selected is the one we are
			 * reselecting. treat this as cancelling the selection.
			 */
			return;
		}
	}

	if (extend) {
		low_note = min (low_note, notenum);
		high_note = max (high_note, notenum);
	}

	_no_sound_notes = true;

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		boost::shared_ptr<NoteType> note (*n);
		NoteBase* cne;
		bool select = false;

		if (((1 << note->channel()) & channel_mask) != 0) {
			if (extend) {
				if ((note->note() >= low_note && note->note() <= high_note)) {
					select = true;
				}
			} else if (note->note() == notenum) {
				select = true;
			}
		}

		if (select) {
			if ((cne = find_canvas_note (note)) != 0) {
				// extend is false because we've taken care of it,
				// since it extends by time range, not pitch.
				note_selected (cne, add, false);
			}
		}

		add = true; // we need to add all remaining matching notes, even if the passed in value was false (for "set")

	}

	_no_sound_notes = false;
}

void
MidiRegionView::toggle_matching_notes (uint8_t notenum, uint16_t channel_mask)
{
	MidiModel::Notes& notes (_model->notes());
	_optimization_iterator = _events.begin();

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		boost::shared_ptr<NoteType> note (*n);
		NoteBase* cne;

		if (note->note() == notenum && (((0x0001 << note->channel()) & channel_mask) != 0)) {
			if ((cne = find_canvas_note (note)) != 0) {
				if (cne->selected()) {
					note_deselected (cne);
				} else {
					note_selected (cne, true, false);
				}
			}
		}
	}
}

void
MidiRegionView::note_selected (NoteBase* ev, bool add, bool extend)
{
	if (!add) {
		clear_selection_except (ev);
		if (!_selection.empty()) {
			PublicEditor& editor (trackview.editor());
			editor.get_selection().add (this);
		}
	}

	if (!extend) {

		if (!ev->selected()) {
			add_to_selection (ev);
		}

	} else {
		/* find end of latest note selected, select all between that and the start of "ev" */

		Evoral::Beats earliest = Evoral::MaxBeats;
		Evoral::Beats latest   = Evoral::Beats();

		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if ((*i)->note()->end_time() > latest) {
				latest = (*i)->note()->end_time();
			}
			if ((*i)->note()->time() < earliest) {
				earliest = (*i)->note()->time();
			}
		}

		if (ev->note()->end_time() > latest) {
			latest = ev->note()->end_time();
		}

		if (ev->note()->time() < earliest) {
			earliest = ev->note()->time();
		}

		for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {

			/* find notes entirely within OR spanning the earliest..latest range */

			if (((*i)->note()->time() >= earliest && (*i)->note()->end_time() <= latest) ||
			    ((*i)->note()->time() <= earliest && (*i)->note()->end_time() >= latest)) {
				add_to_selection (*i);
			}

		}
	}
}

void
MidiRegionView::note_deselected(NoteBase* ev)
{
	remove_from_selection (ev);
}

void
MidiRegionView::update_drag_selection(framepos_t start, framepos_t end, double gy0, double gy1, bool extend)
{
	PublicEditor& editor = trackview.editor();

	// Convert to local coordinates
	const framepos_t p  = _region->position();
	const double     y  = midi_view()->y_position();
	const double     x0 = editor.sample_to_pixel(max((framepos_t)0, start - p));
	const double     x1 = editor.sample_to_pixel(max((framepos_t)0, end - p));
	const double     y0 = max(0.0, gy0 - y);
	const double     y1 = max(0.0, gy1 - y);

	// TODO: Make this faster by storing the last updated selection rect, and only
	// adjusting things that are in the area that appears/disappeared.
	// We probably need a tree to be able to find events in O(log(n)) time.

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((*i)->x0() < x1 && (*i)->x1() > x0 && (*i)->y0() < y1 && (*i)->y1() > y0) {
			// Rectangles intersect
			if (!(*i)->selected()) {
				add_to_selection (*i);
			}
		} else if ((*i)->selected() && !extend) {
			// Rectangles do not intersect
			remove_from_selection (*i);
		}
	}

	typedef RouteTimeAxisView::AutomationTracks ATracks;
	typedef std::list<Selectable*>              Selectables;

	/* Add control points to selection. */
	const ATracks& atracks = midi_view()->automation_tracks();
	Selectables    selectables;
	editor.get_selection().clear_points();
	for (ATracks::const_iterator a = atracks.begin(); a != atracks.end(); ++a) {
		a->second->get_selectables(start, end, gy0, gy1, selectables);
		for (Selectables::const_iterator s = selectables.begin(); s != selectables.end(); ++s) {
			ControlPoint* cp = dynamic_cast<ControlPoint*>(*s);
			if (cp) {
				editor.get_selection().add(cp);
			}
		}
		a->second->set_selected_points(editor.get_selection().points);
	}
}

void
MidiRegionView::update_vertical_drag_selection (double y1, double y2, bool extend)
{
	if (y1 > y2) {
		swap (y1, y2);
	}

	// TODO: Make this faster by storing the last updated selection rect, and only
	// adjusting things that are in the area that appears/disappeared.
	// We probably need a tree to be able to find events in O(log(n)) time.

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if (((*i)->y1() >= y1 && (*i)->y1() <= y2)) {
			// within y- (note-) range
			if (!(*i)->selected()) {
				add_to_selection (*i);
			}
		} else if ((*i)->selected() && !extend) {
			remove_from_selection (*i);
		}
	}
}

void
MidiRegionView::remove_from_selection (NoteBase* ev)
{
	Selection::iterator i = _selection.find (ev);

	if (i != _selection.end()) {
		_selection.erase (i);
		if (_selection.empty() && _grabbed_keyboard) {
			// Ungrab keyboard
			Keyboard::magic_widget_drop_focus();
			_grabbed_keyboard = false;
		}
	}

	ev->set_selected (false);
	ev->hide_velocity ();

	if (_selection.empty()) {
		PublicEditor& editor (trackview.editor());
		editor.get_selection().remove (this);
	}
}

void
MidiRegionView::add_to_selection (NoteBase* ev)
{
	const bool selection_was_empty = _selection.empty();

	if (_selection.insert (ev).second) {
		ev->set_selected (true);
		start_playing_midi_note ((ev)->note());
		if (selection_was_empty && _entered) {
			// Grab keyboard for moving notes with arrow keys
			Keyboard::magic_widget_grab_focus();
			_grabbed_keyboard = true;
		}
	}

	if (selection_was_empty) {
		PublicEditor& editor (trackview.editor());
		editor.get_selection().add (this);
	}
}

void
MidiRegionView::move_selection(double dx, double dy, double cumulative_dy)
{
	typedef vector<boost::shared_ptr<NoteType> > PossibleChord;
	PossibleChord to_play;
	Evoral::Beats earliest = Evoral::MaxBeats;

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->note()->time() < earliest) {
			earliest = (*i)->note()->time();
		}
	}

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->note()->time() == earliest) {
			to_play.push_back ((*i)->note());
		}
		(*i)->move_event(dx, dy);
	}

	if (dy && !_selection.empty() && !_no_sound_notes && ARDOUR_UI::config()->get_sound_midi_notes()) {

		if (to_play.size() > 1) {

			PossibleChord shifted;

			for (PossibleChord::iterator n = to_play.begin(); n != to_play.end(); ++n) {
				boost::shared_ptr<NoteType> moved_note (new NoteType (**n));
				moved_note->set_note (moved_note->note() + cumulative_dy);
				shifted.push_back (moved_note);
			}

			start_playing_midi_chord (shifted);

		} else if (!to_play.empty()) {

			boost::shared_ptr<NoteType> moved_note (new NoteType (*to_play.front()));
			moved_note->set_note (moved_note->note() + cumulative_dy);
			start_playing_midi_note (moved_note);
		}
	}
}

void
MidiRegionView::note_dropped(NoteBase *, frameoffset_t dt, int8_t dnote)
{
	uint8_t lowest_note_in_selection  = 127;
	uint8_t highest_note_in_selection = 0;
	uint8_t highest_note_difference   = 0;

	// find highest and lowest notes first

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		uint8_t pitch = (*i)->note()->note();
		lowest_note_in_selection  = std::min(lowest_note_in_selection,  pitch);
		highest_note_in_selection = std::max(highest_note_in_selection, pitch);
	}

	/*
	  cerr << "dnote: " << (int) dnote << endl;
	  cerr << "lowest note (streamview): " << int(midi_stream_view()->lowest_note())
	  << " highest note (streamview): " << int(midi_stream_view()->highest_note()) << endl;
	  cerr << "lowest note (selection): " << int(lowest_note_in_selection) << " highest note(selection): "
	  << int(highest_note_in_selection) << endl;
	  cerr << "selection size: " << _selection.size() << endl;
	  cerr << "Highest note in selection: " << (int) highest_note_in_selection << endl;
	*/

	// Make sure the note pitch does not exceed the MIDI standard range
	if (highest_note_in_selection + dnote > 127) {
		highest_note_difference = highest_note_in_selection - 127;
	}

	start_note_diff_command (_("move notes"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end() ; ++i) {
		
		framepos_t new_frames = source_beats_to_absolute_frames ((*i)->note()->time()) + dt;
		Evoral::Beats new_time = absolute_frames_to_source_beats (new_frames);

		if (new_time < 0) {
			continue;
		}

		note_diff_add_change (*i, MidiModel::NoteDiffCommand::StartTime, new_time);

		uint8_t original_pitch = (*i)->note()->note();
		uint8_t new_pitch      = original_pitch + dnote - highest_note_difference;

		// keep notes in standard midi range
		clamp_to_0_127(new_pitch);

		lowest_note_in_selection  = std::min(lowest_note_in_selection,  new_pitch);
		highest_note_in_selection = std::max(highest_note_in_selection, new_pitch);

		note_diff_add_change (*i, MidiModel::NoteDiffCommand::NoteNumber, new_pitch);
	}

	apply_diff();

	// care about notes being moved beyond the upper/lower bounds on the canvas
	if (lowest_note_in_selection  < midi_stream_view()->lowest_note() ||
	    highest_note_in_selection > midi_stream_view()->highest_note()) {
		midi_stream_view()->set_note_range(MidiStreamView::ContentsRange);
	}
}

/** @param x Pixel relative to the region position.
 *  @return Snapped frame relative to the region position.
 */
framepos_t
MidiRegionView::snap_pixel_to_sample(double x)
{
	PublicEditor& editor (trackview.editor());
	return snap_frame_to_frame (editor.pixel_to_sample (x));
}

/** @param x Pixel relative to the region position.
 *  @return Snapped pixel relative to the region position.
 */
double
MidiRegionView::snap_to_pixel(double x)
{
	return (double) trackview.editor().sample_to_pixel(snap_pixel_to_sample(x));
}

double
MidiRegionView::get_position_pixels()
{
	framepos_t region_frame = get_position();
	return trackview.editor().sample_to_pixel(region_frame);
}

double
MidiRegionView::get_end_position_pixels()
{
	framepos_t frame = get_position() + get_duration ();
	return trackview.editor().sample_to_pixel(frame);
}

framepos_t
MidiRegionView::source_beats_to_absolute_frames(Evoral::Beats beats) const
{
	/* the time converter will return the frame corresponding to `beats'
	   relative to the start of the source. The start of the source
	   is an implied position given by region->position - region->start
	*/
	const framepos_t source_start = _region->position() - _region->start();
	return  source_start +  _source_relative_time_converter.to (beats);
}

Evoral::Beats
MidiRegionView::absolute_frames_to_source_beats(framepos_t frames) const
{
	/* the `frames' argument needs to be converted into a frame count
	   relative to the start of the source before being passed in to the
	   converter.
	*/
	const framepos_t source_start = _region->position() - _region->start();
	return  _source_relative_time_converter.from (frames - source_start);
}

framepos_t
MidiRegionView::region_beats_to_region_frames(Evoral::Beats beats) const
{
	return _region_relative_time_converter.to(beats);
}

Evoral::Beats
MidiRegionView::region_frames_to_region_beats(framepos_t frames) const
{
	return _region_relative_time_converter.from(frames);
}

void
MidiRegionView::begin_resizing (bool /*at_front*/)
{
	_resize_data.clear();

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		Note *note = dynamic_cast<Note*> (*i);

		// only insert CanvasNotes into the map
		if (note) {
			NoteResizeData *resize_data = new NoteResizeData();
			resize_data->note = note;

			// create a new SimpleRect from the note which will be the resize preview
			ArdourCanvas::Rectangle *resize_rect = new ArdourCanvas::Rectangle (_note_group, 
											    ArdourCanvas::Rect (note->x0(), note->y0(), note->x0(), note->y1()));

			// calculate the colors: get the color settings
			uint32_t fill_color = UINT_RGBA_CHANGE_A(
				ARDOUR_UI::config()->color ("midi note selected"),
				128);

			// make the resize preview notes more transparent and bright
			fill_color = UINT_INTERPOLATE(fill_color, 0xFFFFFF40, 0.5);

			// calculate color based on note velocity
			resize_rect->set_fill_color (UINT_INTERPOLATE(
				NoteBase::meter_style_fill_color(note->note()->velocity(), note->selected()),
				fill_color,
				0.85));

			resize_rect->set_outline_color (NoteBase::calculate_outline (
								ARDOUR_UI::config()->color ("midi note selected")));

			resize_data->resize_rect = resize_rect;
			_resize_data.push_back(resize_data);
		}
	}
}

/** Update resizing notes while user drags.
 * @param primary `primary' note for the drag; ie the one that is used as the reference in non-relative mode.
 * @param at_front which end of the note (true == note on, false == note off)
 * @param delta_x change in mouse position since the start of the drag
 * @param relative true if relative resizing is taking place, false if absolute resizing.  This only makes
 * a difference when multiple notes are being resized; in relative mode, each note's length is changed by the
 * amount of the drag.  In non-relative mode, all selected notes are set to have the same start or end point
 * as the \a primary note.
 */
void
MidiRegionView::update_resizing (NoteBase* primary, bool at_front, double delta_x, bool relative)
{
	bool cursor_set = false;

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		ArdourCanvas::Rectangle* resize_rect = (*i)->resize_rect;
		Note* canvas_note = (*i)->note;
		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x0() + delta_x;
			} else {
				current_x = primary->x0() + delta_x;
			}
		} else {
			if (relative) {
				current_x = canvas_note->x1() + delta_x;
			} else {
				current_x = primary->x1() + delta_x;
			}
		}

		if (current_x < 0) {
			// This works even with snapping because RegionView::snap_frame_to_frame()
			// snaps forward if the snapped sample is before the beginning of the region
			current_x = 0;
		}
		if (current_x > trackview.editor().sample_to_pixel(_region->length())) {
			current_x = trackview.editor().sample_to_pixel(_region->length());
		}

		if (at_front) {
			resize_rect->set_x0 (snap_to_pixel(current_x));
			resize_rect->set_x1 (canvas_note->x1());
		} else {
			resize_rect->set_x1 (snap_to_pixel(current_x));
			resize_rect->set_x0 (canvas_note->x0());
		}

		if (!cursor_set) {
			const double  snapped_x = snap_pixel_to_sample (current_x);
			Evoral::Beats beats     = region_frames_to_region_beats (snapped_x);
			Evoral::Beats len       = Evoral::Beats();

			if (at_front) {
				if (beats < canvas_note->note()->end_time()) {
					len = canvas_note->note()->time() - beats;
					len += canvas_note->note()->length();
				}
			} else {
				if (beats >= canvas_note->note()->time()) {
					len = beats - canvas_note->note()->time();
				}
			}

			len = std::max(Evoral::Beats(1 / 512.0), len);

			char buf[16];
			snprintf (buf, sizeof (buf), "%.3g beats", len.to_double());
			show_verbose_cursor (buf, 0, 0);

			cursor_set = true;
		}

	}
}


/** Finish resizing notes when the user releases the mouse button.
 *  Parameters the same as for \a update_resizing().
 */
void
MidiRegionView::commit_resizing (NoteBase* primary, bool at_front, double delta_x, bool relative)
{
	_note_diff_command = _model->new_note_diff_command (_("resize notes"));

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		Note*  canvas_note = (*i)->note;
		ArdourCanvas::Rectangle*  resize_rect = (*i)->resize_rect;

		/* Get the new x position for this resize, which is in pixels relative
		 * to the region position.
		 */
		
		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x0() + delta_x;
			} else {
				current_x = primary->x0() + delta_x;
			}
		} else {
			if (relative) {
				current_x = canvas_note->x1() + delta_x;
			} else {
				current_x = primary->x1() + delta_x;
			}
		}

		if (current_x < 0) {
			current_x = 0;
		}
		if (current_x > trackview.editor().sample_to_pixel(_region->length())) {
			current_x = trackview.editor().sample_to_pixel(_region->length());
		}

		/* Convert that to a frame within the source */
		current_x = snap_pixel_to_sample (current_x) + _region->start ();

		/* and then to beats */
		const Evoral::Beats x_beats = region_frames_to_region_beats (current_x);

		if (at_front && x_beats < canvas_note->note()->end_time()) {
			note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::StartTime, x_beats);

			Evoral::Beats len = canvas_note->note()->time() - x_beats;
			len += canvas_note->note()->length();

			if (!!len) {
				note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::Length, len);
			}
		}

		if (!at_front) {
			const Evoral::Beats len = std::max(Evoral::Beats(1 / 512.0),
			                                   x_beats - canvas_note->note()->time());
			note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::Length, len);
		}

		delete resize_rect;
		delete (*i);
	}

	_resize_data.clear();
	apply_diff(true);
}

void
MidiRegionView::abort_resizing ()
{
	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		delete (*i)->resize_rect;
		delete *i;
	}

	_resize_data.clear ();
}

void
MidiRegionView::change_note_velocity(NoteBase* event, int8_t velocity, bool relative)
{
	uint8_t new_velocity;

	if (relative) {
		new_velocity = event->note()->velocity() + velocity;
		clamp_to_0_127(new_velocity);
	} else {
		new_velocity = velocity;
	}

	event->set_selected (event->selected()); // change color

	note_diff_add_change (event, MidiModel::NoteDiffCommand::Velocity, new_velocity);
}

void
MidiRegionView::change_note_note (NoteBase* event, int8_t note, bool relative)
{
	uint8_t new_note;

	if (relative) {
		new_note = event->note()->note() + note;
	} else {
		new_note = note;
	}

	clamp_to_0_127 (new_note);
	note_diff_add_change (event, MidiModel::NoteDiffCommand::NoteNumber, new_note);
}

void
MidiRegionView::trim_note (NoteBase* event, Evoral::Beats front_delta, Evoral::Beats end_delta)
{
	bool change_start = false;
	bool change_length = false;
	Evoral::Beats new_start;
	Evoral::Beats new_length;

	/* NOTE: the semantics of the two delta arguments are slightly subtle:

	   front_delta: if positive - move the start of the note later in time (shortening it)
	   if negative - move the start of the note earlier in time (lengthening it)

	   end_delta:   if positive - move the end of the note later in time (lengthening it)
	   if negative - move the end of the note earlier in time (shortening it)
	*/

	if (!!front_delta) {
		if (front_delta < 0) {

			if (event->note()->time() < -front_delta) {
				new_start = Evoral::Beats();
			} else {
				new_start = event->note()->time() + front_delta; // moves earlier
			}

			/* start moved toward zero, so move the end point out to where it used to be.
			   Note that front_delta is negative, so this increases the length.
			*/

			new_length = event->note()->length() - front_delta;
			change_start = true;
			change_length = true;

		} else {

			Evoral::Beats new_pos = event->note()->time() + front_delta;

			if (new_pos < event->note()->end_time()) {
				new_start = event->note()->time() + front_delta;
				/* start moved toward the end, so move the end point back to where it used to be */
				new_length = event->note()->length() - front_delta;
				change_start = true;
				change_length = true;
			}
		}

	}

	if (!!end_delta) {
		bool can_change = true;
		if (end_delta < 0) {
			if (event->note()->length() < -end_delta) {
				can_change = false;
			}
		}

		if (can_change) {
			new_length = event->note()->length() + end_delta;
			change_length = true;
		}
	}

	if (change_start) {
		note_diff_add_change (event, MidiModel::NoteDiffCommand::StartTime, new_start);
	}

	if (change_length) {
		note_diff_add_change (event, MidiModel::NoteDiffCommand::Length, new_length);
	}
}

void
MidiRegionView::change_note_channel (NoteBase* event, int8_t chn, bool relative)
{
	uint8_t new_channel;

	if (relative) {
		if (chn < 0.0) {
			if (event->note()->channel() < -chn) {
				new_channel = 0;
			} else {
				new_channel = event->note()->channel() + chn;
			}
		} else {
			new_channel = event->note()->channel() + chn;
		}
	} else {
		new_channel = (uint8_t) chn;
	}

	note_diff_add_change (event, MidiModel::NoteDiffCommand::Channel, new_channel);
}

void
MidiRegionView::change_note_time (NoteBase* event, Evoral::Beats delta, bool relative)
{
	Evoral::Beats new_time;

	if (relative) {
		if (delta < 0.0) {
			if (event->note()->time() < -delta) {
				new_time = Evoral::Beats();
			} else {
				new_time = event->note()->time() + delta;
			}
		} else {
			new_time = event->note()->time() + delta;
		}
	} else {
		new_time = delta;
	}

	note_diff_add_change (event, MidiModel::NoteDiffCommand::StartTime, new_time);
}

void
MidiRegionView::change_note_length (NoteBase* event, Evoral::Beats t)
{
	note_diff_add_change (event, MidiModel::NoteDiffCommand::Length, t);
}

void
MidiRegionView::change_velocities (bool up, bool fine, bool allow_smush, bool all_together)
{
	int8_t delta;
	int8_t value = 0;

	if (_selection.empty()) {
		return;
	}

	if (fine) {
		delta = 1;
	} else {
		delta = 10;
	}

	if (!up) {
		delta = -delta;
	}

	if (!allow_smush) {
		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if ((*i)->note()->velocity() < -delta || (*i)->note()->velocity() + delta > 127) {
				goto cursor_label;
			}
		}
	}

	start_note_diff_command (_("change velocities"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end();) {
		Selection::iterator next = i;
		++next;

		if (all_together) {
			if (i == _selection.begin()) {
				change_note_velocity (*i, delta, true);
				value = (*i)->note()->velocity() + delta;
			} else {
				change_note_velocity (*i, value, false);
			}

		} else {
			change_note_velocity (*i, delta, true);
		}

		i = next;
	}

	apply_diff();

  cursor_label:
	if (!_selection.empty()) {
		char buf[24];
		snprintf (buf, sizeof (buf), "Vel %d",
		          (int) (*_selection.begin())->note()->velocity());
		show_verbose_cursor (buf, 10, 10);
	}
}


void
MidiRegionView::transpose (bool up, bool fine, bool allow_smush)
{
	if (_selection.empty()) {
		return;
	}

	int8_t delta;

	if (fine) {
		delta = 1;
	} else {
		delta = 12;
	}

	if (!up) {
		delta = -delta;
	}

	if (!allow_smush) {
		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			if (!up) {
				if ((int8_t) (*i)->note()->note() + delta <= 0) {
					return;
				}
			} else {
				if ((int8_t) (*i)->note()->note() + delta > 127) {
					return;
				}
			}
		}
	}

	start_note_diff_command (_("transpose"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_note (*i, delta, true);
		i = next;
	}

	apply_diff ();
}

void
MidiRegionView::change_note_lengths (bool fine, bool shorter, Evoral::Beats delta, bool start, bool end)
{
	if (!delta) {
		if (fine) {
			delta = Evoral::Beats(1.0/128.0);
		} else {
			/* grab the current grid distance */
			delta = get_grid_beats(_region->position());
		}
	}

	if (shorter) {
		delta = -delta;
	}

	start_note_diff_command (_("change note lengths"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;

		/* note the negation of the delta for start */

		trim_note (*i,
		           (start ? -delta : Evoral::Beats()),
		           (end   ? delta  : Evoral::Beats()));
		i = next;
	}

	apply_diff ();

}

void
MidiRegionView::nudge_notes (bool forward, bool fine)
{
	if (_selection.empty()) {
		return;
	}

	/* pick a note as the point along the timeline to get the nudge distance.
	   its not necessarily the earliest note, so we may want to pull the notes out
	   into a vector and sort before using the first one.
	*/

	const framepos_t ref_point = source_beats_to_absolute_frames ((*(_selection.begin()))->note()->time());
	Evoral::Beats    delta;

	if (!fine) {

		/* non-fine, move by 1 bar regardless of snap */
		delta = Evoral::Beats(trackview.session()->tempo_map().meter_at(ref_point).divisions_per_bar());

	} else if (trackview.editor().snap_mode() == Editing::SnapOff) {

		/* grid is off - use nudge distance */

		framepos_t       unused;
		const framecnt_t distance = trackview.editor().get_nudge_distance (ref_point, unused);
		delta = region_frames_to_region_beats (fabs ((double)distance));

	} else {

		/* use grid */

		framepos_t next_pos = ref_point;

		if (forward) {
			if (max_framepos - 1 < next_pos) {
				next_pos += 1;
			}
		} else {
			if (next_pos == 0) {
				return;
			}
			next_pos -= 1;
		}

		trackview.editor().snap_to (next_pos, (forward ? RoundUpAlways : RoundDownAlways), false);
		const framecnt_t distance = ref_point - next_pos;
		delta = region_frames_to_region_beats (fabs ((double)distance));
	}

	if (!delta) {
		return;
	}

	if (!forward) {
		delta = -delta;
	}

	start_note_diff_command (_("nudge"));

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ) {
		Selection::iterator next = i;
		++next;
		change_note_time (*i, delta, true);
		i = next;
	}

	apply_diff ();
}

void
MidiRegionView::change_channel(uint8_t channel)
{
	start_note_diff_command(_("change channel"));
	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		note_diff_add_change (*i, MidiModel::NoteDiffCommand::Channel, channel);
	}

	apply_diff();
}


void
MidiRegionView::note_entered(NoteBase* ev)
{
	Editor* editor = dynamic_cast<Editor*>(&trackview.editor());

	if (_mouse_state == SelectTouchDragging) {
		note_selected (ev, true);
	} else if (editor->current_mouse_mode() == MouseContent) {
		show_verbose_cursor (ev->note ());
	} else if (editor->current_mouse_mode() == MouseDraw) {
		show_verbose_cursor (ev->note ());
	}
}

void
MidiRegionView::note_left (NoteBase*)
{
	Editor* editor = dynamic_cast<Editor*>(&trackview.editor());

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->hide_velocity ();
	}

	editor->verbose_cursor()->hide ();
}

void
MidiRegionView::patch_entered (PatchChange* p)
{
	ostringstream s;
	/* XXX should get patch name if we can */
	s << _("Bank ") << (p->patch()->bank() + MIDI_BP_ZERO) << '\n' 
	  << _("Program ") << ((int) p->patch()->program()) + MIDI_BP_ZERO << '\n' 
	  << _("Channel ") << ((int) p->patch()->channel() + 1);
	show_verbose_cursor (s.str(), 10, 20);
	p->item().grab_focus();
}

void
MidiRegionView::patch_left (PatchChange *)
{
	trackview.editor().verbose_cursor()->hide ();
	/* focus will transfer back via the enter-notify event sent to this
	 * midi region view.
	 */
}

void
MidiRegionView::sysex_entered (SysEx* p)
{
	ostringstream s;
	// CAIROCANVAS
	// need a way to extract text from p->_flag->_text
	// s << p->text();
	// show_verbose_cursor (s.str(), 10, 20);
	p->item().grab_focus();
}

void
MidiRegionView::sysex_left (SysEx *)
{
	trackview.editor().verbose_cursor()->hide ();
	/* focus will transfer back via the enter-notify event sent to this
	 * midi region view.
	 */
}

void
MidiRegionView::note_mouse_position (float x_fraction, float /*y_fraction*/, bool can_set_cursor)
{
	Editor* editor = dynamic_cast<Editor*>(&trackview.editor());
	Editing::MouseMode mm = editor->current_mouse_mode();
	bool trimmable = (mm == MouseContent || mm == MouseTimeFX || mm == MouseDraw);

	Editor::EnterContext* ctx = editor->get_enter_context(NoteItem);
	if (can_set_cursor && ctx) {
		if (trimmable && x_fraction > 0.0 && x_fraction < 0.2) {
			ctx->cursor_ctx->change(editor->cursors()->left_side_trim);
		} else if (trimmable && x_fraction >= 0.8 && x_fraction < 1.0) {
			ctx->cursor_ctx->change(editor->cursors()->right_side_trim);
		} else {
			ctx->cursor_ctx->change(editor->cursors()->grabber_note);
		}
	}
}

uint32_t
MidiRegionView::get_fill_color() const
{
	const std::string mod_name = (_dragging ? "dragging region" :
	                              trackview.editor().internal_editing() ? "editable region" :
	                              "midi frame base");
	if (_selected) {
		return ARDOUR_UI::config()->color_mod ("selected region base", mod_name);
	} else if ((!ARDOUR_UI::config()->get_show_name_highlight() || high_enough_for_name) &&
	           !ARDOUR_UI::config()->get_color_regions_using_track_color()) {
		return ARDOUR_UI::config()->color_mod ("midi frame base", mod_name);
	}
	return ARDOUR_UI::config()->color_mod (fill_color, mod_name);
}

void
MidiRegionView::midi_channel_mode_changed ()
{
	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	uint16_t mask = mtv->midi_track()->get_playback_channel_mask();
	ChannelMode mode = mtv->midi_track()->get_playback_channel_mode ();

	if (mode == ForceChannel) {
		mask = 0xFFFF; // Show all notes as active (below)
	}

	// Update notes for selection
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		(*i)->on_channel_selection_change (mask);
	}

	_patch_changes.clear ();
	display_patch_changes ();
}

void
MidiRegionView::instrument_settings_changed ()
{
	redisplay_model();
}

void
MidiRegionView::cut_copy_clear (Editing::CutCopyOp op)
{
	if (_selection.empty()) {
		return;
	}

	PublicEditor& editor (trackview.editor());

	switch (op) {
	case Delete:
		/* XXX what to do ? */
		break;
	case Cut:
	case Copy:
		editor.get_cut_buffer().add (selection_as_cut_buffer());
		break;
	default:
		break;
	}

	if (op != Copy) {

		start_note_diff_command();

		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			switch (op) {
			case Copy:
				break;
			case Delete:
			case Cut:
			case Clear:
				note_diff_remove_note (*i);
				break;
			}
		}

		apply_diff();
	}
}

MidiCutBuffer*
MidiRegionView::selection_as_cut_buffer () const
{
	Notes notes;

	for (Selection::const_iterator i = _selection.begin(); i != _selection.end(); ++i) {
		NoteType* n = (*i)->note().get();
		notes.insert (boost::shared_ptr<NoteType> (new NoteType (*n)));
	}

	MidiCutBuffer* cb = new MidiCutBuffer (trackview.session());
	cb->set (notes);

	return cb;
}

/** This method handles undo */
bool
MidiRegionView::paste (framepos_t pos, const ::Selection& selection, PasteContext& ctx)
{
	bool commit = false;
	// Paste notes, if available
	MidiNoteSelection::const_iterator m = selection.midi_notes.get_nth(ctx.counts.n_notes());
	if (m != selection.midi_notes.end()) {
		ctx.counts.increase_n_notes();
		if (!(*m)->empty()) { commit = true; }
		paste_internal(pos, ctx.count, ctx.times, **m);
	}

	// Paste control points to automation children, if available
	typedef RouteTimeAxisView::AutomationTracks ATracks;
	const ATracks& atracks = midi_view()->automation_tracks();
	for (ATracks::const_iterator a = atracks.begin(); a != atracks.end(); ++a) {
		if (a->second->paste(pos, selection, ctx)) {
			commit = true;
		}
	}

	if (commit) {
		trackview.editor().commit_reversible_command ();
	}
	return true;
}

/** This method handles undo */
void
MidiRegionView::paste_internal (framepos_t pos, unsigned paste_count, float times, const MidiCutBuffer& mcb)
{
	if (mcb.empty()) {
		return;
	}

	start_note_diff_command (_("paste"));

	const Evoral::Beats snap_beats    = get_grid_beats(pos);
	const Evoral::Beats first_time    = (*mcb.notes().begin())->time();
	const Evoral::Beats last_time     = (*mcb.notes().rbegin())->end_time();
	const Evoral::Beats duration      = last_time - first_time;
	const Evoral::Beats snap_duration = duration.snap_to(snap_beats);
	const Evoral::Beats paste_offset  = snap_duration * paste_count;
	const Evoral::Beats pos_beats     = absolute_frames_to_source_beats(pos) + paste_offset;
	Evoral::Beats       end_point     = Evoral::Beats();

	DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("Paste data spans from %1 to %2 (%3) ; paste pos beats = %4 (based on %5 - %6)\n",
	                                               first_time,
	                                               last_time,
	                                               duration, pos, _region->position(),
	                                               pos_beats));

	clear_selection ();

	for (int n = 0; n < (int) times; ++n) {

		for (Notes::const_iterator i = mcb.notes().begin(); i != mcb.notes().end(); ++i) {

			boost::shared_ptr<NoteType> copied_note (new NoteType (*((*i).get())));
			copied_note->set_time (pos_beats + copied_note->time() - first_time);

			/* make all newly added notes selected */

			note_diff_add_note (copied_note, true);
			end_point = copied_note->end_time();
		}
	}

	/* if we pasted past the current end of the region, extend the region */

	framepos_t end_frame = source_beats_to_absolute_frames (end_point);
	framepos_t region_end = _region->position() + _region->length() - 1;

	if (end_frame > region_end) {

		DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("Paste extended region from %1 to %2\n", region_end, end_frame));

		_region->clear_changes ();
		_region->set_length (end_frame - _region->position());
		trackview.session()->add_command (new StatefulDiffCommand (_region));
	}

	apply_diff (true);
}

struct EventNoteTimeEarlyFirstComparator {
	bool operator() (NoteBase* a, NoteBase* b) {
		return a->note()->time() < b->note()->time();
	}
};

void
MidiRegionView::time_sort_events ()
{
	if (!_sort_needed) {
		return;
	}

	EventNoteTimeEarlyFirstComparator cmp;
	_events.sort (cmp);

	_sort_needed = false;
}

void
MidiRegionView::goto_next_note (bool add_to_selection)
{
	bool use_next = false;

	if (_events.back()->selected()) {
		return;
	}

	time_sort_events ();

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	uint16_t const channel_mask = mtv->midi_track()->get_playback_channel_mask();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((*i)->selected()) {
			use_next = true;
			continue;
		} else if (use_next) {
			if (channel_mask & (1 << (*i)->note()->channel())) {
				if (!add_to_selection) {
					unique_select (*i);
				} else {
					note_selected (*i, true, false);
				}
				return;
			}
		}
	}

	/* use the first one */

	if (!_events.empty() && (channel_mask & (1 << _events.front()->note()->channel ()))) {
		unique_select (_events.front());
	}
}

void
MidiRegionView::goto_previous_note (bool add_to_selection)
{
	bool use_next = false;

	if (_events.front()->selected()) {
		return;
	}

	time_sort_events ();

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	uint16_t const channel_mask = mtv->midi_track()->get_playback_channel_mask ();

	for (Events::reverse_iterator i = _events.rbegin(); i != _events.rend(); ++i) {
		if ((*i)->selected()) {
			use_next = true;
			continue;
		} else if (use_next) {
			if (channel_mask & (1 << (*i)->note()->channel())) {
				if (!add_to_selection) {
					unique_select (*i);
				} else {
					note_selected (*i, true, false);
				}
				return;
			}
		}
	}

	/* use the last one */

	if (!_events.empty() && (channel_mask & (1 << (*_events.rbegin())->note()->channel ()))) {
		unique_select (*(_events.rbegin()));
	}
}

void
MidiRegionView::selection_as_notelist (Notes& selected, bool allow_all_if_none_selected)
{
	bool had_selected = false;

	time_sort_events ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if ((*i)->selected()) {
			selected.insert ((*i)->note());
			had_selected = true;
		}
	}

	if (allow_all_if_none_selected && !had_selected) {
		for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
			selected.insert ((*i)->note());
		}
	}
}

void
MidiRegionView::update_ghost_note (double x, double y)
{
	x = std::max(0.0, x);

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);

	_last_ghost_x = x;
	_last_ghost_y = y;

	_note_group->canvas_to_item (x, y);

	PublicEditor& editor = trackview.editor ();
	
	framepos_t const unsnapped_frame = editor.pixel_to_sample (x);
	framecnt_t grid_frames;
	framepos_t const f = snap_frame_to_grid_underneath (unsnapped_frame, grid_frames);

	/* calculate time in beats relative to start of source */
	const Evoral::Beats length = get_grid_beats(unsnapped_frame);
	const Evoral::Beats time   = std::max(
		Evoral::Beats(),
		absolute_frames_to_source_beats (f + _region->position ()));

	_ghost_note->note()->set_time (time);
	_ghost_note->note()->set_length (length);
	_ghost_note->note()->set_note (midi_stream_view()->y_to_note (y));
	_ghost_note->note()->set_channel (mtv->get_channel_for_add ());
	_ghost_note->note()->set_velocity (get_velocity_for_add (time));

	/* the ghost note does not appear in ghost regions, so pass false in here */
	update_note (_ghost_note, false);

	show_verbose_cursor (_ghost_note->note ());
}

void
MidiRegionView::create_ghost_note (double x, double y)
{
	remove_ghost_note ();

	boost::shared_ptr<NoteType> g (new NoteType);
	if (midi_view()->note_mode() == Sustained) {
		_ghost_note = new Note (*this, _note_group, g);
	} else {
		_ghost_note = new Hit (*this, _note_group, 10, g);
	}
	_ghost_note->set_ignore_events (true);
	_ghost_note->set_outline_color (0x000000aa);
	update_ghost_note (x, y);
	_ghost_note->show ();

	show_verbose_cursor (_ghost_note->note ());
}

void
MidiRegionView::remove_ghost_note ()
{
	delete _ghost_note;
	_ghost_note = 0;
}

void
MidiRegionView::snap_changed ()
{
	if (!_ghost_note) {
		return;
	}

	create_ghost_note (_last_ghost_x, _last_ghost_y);
}

void
MidiRegionView::drop_down_keys ()
{
	_mouse_state = None;
}

void
MidiRegionView::maybe_select_by_position (GdkEventButton* ev, double /*x*/, double y)
{
	/* XXX: This is dead code.  What was it for? */

	double note = midi_stream_view()->y_to_note(y);
	Events e;
	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);

	uint16_t chn_mask = mtv->midi_track()->get_playback_channel_mask();

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
		get_events (e, Evoral::Sequence<Evoral::Beats>::PitchGreaterThanOrEqual, (uint8_t) floor (note), chn_mask);
	} else if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier)) {
		get_events (e, Evoral::Sequence<Evoral::Beats>::PitchLessThanOrEqual, (uint8_t) floor (note), chn_mask);
	} else {
		return;
	}

	bool add_mrv_selection = false;

	if (_selection.empty()) {
		add_mrv_selection = true;
	}

	for (Events::iterator i = e.begin(); i != e.end(); ++i) {
		if (_selection.insert (*i).second) {
			(*i)->set_selected (true);
		}
	}

	if (add_mrv_selection) {
		PublicEditor& editor (trackview.editor());
		editor.get_selection().add (this);
	}
}

void
MidiRegionView::color_handler ()
{
	RegionView::color_handler ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		(*i)->set_selected ((*i)->selected()); // will change color
	}

	/* XXX probably more to do here */
}

void
MidiRegionView::enable_display (bool yn)
{
	RegionView::enable_display (yn);
	if (yn) {
		redisplay_model ();
	}
}

void
MidiRegionView::show_step_edit_cursor (Evoral::Beats pos)
{
	if (_step_edit_cursor == 0) {
		ArdourCanvas::Item* const group = get_canvas_group();

		_step_edit_cursor = new ArdourCanvas::Rectangle (group);
		_step_edit_cursor->set_y0 (0);
		_step_edit_cursor->set_y1 (midi_stream_view()->contents_height());
		_step_edit_cursor->set_fill_color (RGBA_TO_UINT (45,0,0,90));
		_step_edit_cursor->set_outline_color (RGBA_TO_UINT (85,0,0,90));
	}

	move_step_edit_cursor (pos);
	_step_edit_cursor->show ();
}

void
MidiRegionView::move_step_edit_cursor (Evoral::Beats pos)
{
	_step_edit_cursor_position = pos;

	if (_step_edit_cursor) {
		double pixel = trackview.editor().sample_to_pixel (region_beats_to_region_frames (pos));
		_step_edit_cursor->set_x0 (pixel);
		set_step_edit_cursor_width (_step_edit_cursor_width);
	}
}

void
MidiRegionView::hide_step_edit_cursor ()
{
	if (_step_edit_cursor) {
		_step_edit_cursor->hide ();
	}
}

void
MidiRegionView::set_step_edit_cursor_width (Evoral::Beats beats)
{
	_step_edit_cursor_width = beats;

	if (_step_edit_cursor) {
		_step_edit_cursor->set_x1 (_step_edit_cursor->x0() + trackview.editor().sample_to_pixel (region_beats_to_region_frames (beats)));
	}
}

/** Called when a diskstream on our track has received some data.  Update the view, if applicable.
 *  @param w Source that the data will end up in.
 */
void
MidiRegionView::data_recorded (boost::weak_ptr<MidiSource> w)
{
	if (!_active_notes) {
		/* we aren't actively being recorded to */
		return;
	}

	boost::shared_ptr<MidiSource> src = w.lock ();
	if (!src || src != midi_region()->midi_source()) {
		/* recorded data was not destined for our source */
		return;
	}

	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*> (&trackview);

	boost::shared_ptr<MidiBuffer> buf = mtv->midi_track()->get_gui_feed_buffer ();

	framepos_t back = max_framepos;

	for (MidiBuffer::iterator i = buf->begin(); i != buf->end(); ++i) {
		Evoral::MIDIEvent<MidiBuffer::TimeType> const ev (*i, false);

		if (ev.is_channel_event()) {
			if (get_channel_mode() == FilterChannels) {
				if (((uint16_t(1) << ev.channel()) & get_selected_channels()) == 0) {
					continue;
				}
			}
		}

		/* convert from session frames to source beats */
		Evoral::Beats const time_beats = _source_relative_time_converter.from(
			ev.time() - src->timeline_position() + _region->start());

		if (ev.type() == MIDI_CMD_NOTE_ON) {
			boost::shared_ptr<NoteType> note (
				new NoteType (ev.channel(), time_beats, Evoral::Beats(), ev.note(), ev.velocity()));

			add_note (note, true);

			/* fix up our note range */
			if (ev.note() < _current_range_min) {
				midi_stream_view()->apply_note_range (ev.note(), _current_range_max, true);
			} else if (ev.note() > _current_range_max) {
				midi_stream_view()->apply_note_range (_current_range_min, ev.note(), true);
			}

		} else if (ev.type() == MIDI_CMD_NOTE_OFF) {
			resolve_note (ev.note (), time_beats);
		}

		back = ev.time ();
	}

	midi_stream_view()->check_record_layers (region(), back);
}

void
MidiRegionView::trim_front_starting ()
{
	/* Reparent the note group to the region view's parent, so that it doesn't change
	   when the region view is trimmed.
	*/
	_temporary_note_group = new ArdourCanvas::Container (group->parent ());
	_temporary_note_group->move (group->position ());
	_note_group->reparent (_temporary_note_group);
}

void
MidiRegionView::trim_front_ending ()
{
	_note_group->reparent (group);
	delete _temporary_note_group;
	_temporary_note_group = 0;

	if (_region->start() < 0) {
		/* Trim drag made start time -ve; fix this */
		midi_region()->fix_negative_start ();
	}
}

void
MidiRegionView::edit_patch_change (PatchChange* pc)
{
	PatchChangeDialog d (&_source_relative_time_converter, trackview.session(), *pc->patch (), instrument_info(), Gtk::Stock::APPLY, true);

	int response = d.run();

	switch (response) {
	case Gtk::RESPONSE_ACCEPT:
		break;
	case Gtk::RESPONSE_REJECT:
		delete_patch_change (pc);
		return;
	default:
		return;
	}

	change_patch_change (pc->patch(), d.patch ());
}

void
MidiRegionView::delete_sysex (SysEx* /*sysex*/)
{
	// CAIROCANVAS
	// sysyex object doesn't have a pointer to a sysex event
	// MidiModel::SysExDiffCommand* c = _model->new_sysex_diff_command (_("delete sysex"));
	// c->remove (sysex->sysex());
	// _model->apply_command (*trackview.session(), c);

	//_sys_exes.clear ();
	// display_sysexes();
}

void
MidiRegionView::show_verbose_cursor (boost::shared_ptr<NoteType> n) const
{
	using namespace MIDI::Name;

	std::string name;

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	if (mtv) {
		boost::shared_ptr<MasterDeviceNames> device_names(mtv->get_device_names());
		if (device_names) {
			MIDI::Name::PatchPrimaryKey patch_key;
			get_patch_key_at(n->time(), n->channel(), patch_key);
			name = device_names->note_name(mtv->gui_property(X_("midnam-custom-device-mode")),
			                               n->channel(),
			                               patch_key.bank(),
			                               patch_key.program(),
			                               n->note());
		}
	}

	char buf[128];
	snprintf (buf, sizeof (buf), "%d %s\nCh %d Vel %d",
	          (int) n->note (),
	          name.empty() ? Evoral::midi_note_name (n->note()).c_str() : name.c_str(),
	          (int) n->channel() + 1,
	          (int) n->velocity());

	show_verbose_cursor(buf, 10, 20);
}

void
MidiRegionView::show_verbose_cursor (string const & text, double xoffset, double yoffset) const
{
	trackview.editor().verbose_cursor()->set (text);
	trackview.editor().verbose_cursor()->show ();
	trackview.editor().verbose_cursor()->set_offset (ArdourCanvas::Duple (xoffset, yoffset));
}

uint8_t
MidiRegionView::get_velocity_for_add (MidiModel::TimeType time) const
{
	if (_model->notes().empty()) {
		return 0x40;  // No notes, use default
	}

	MidiModel::Notes::const_iterator m = _model->note_lower_bound(time);
	if (m == _model->notes().begin()) {
		// Before the start, use the velocity of the first note
		return (*m)->velocity();
	} else if (m == _model->notes().end()) {
		// Past the end, use the velocity of the last note
		--m;
		return (*m)->velocity();
	}

	// Interpolate velocity of surrounding notes
	MidiModel::Notes::const_iterator n = m;
	--n;

	const double frac = ((time - (*n)->time()).to_double() /
	                     ((*m)->time() - (*n)->time()).to_double());

	return (*n)->velocity() + (frac * ((*m)->velocity() - (*n)->velocity()));
}

/** @param p A session framepos.
 *  @param grid_frames Filled in with the number of frames that a grid interval is at p.
 *  @return p snapped to the grid subdivision underneath it.
 */
framepos_t
MidiRegionView::snap_frame_to_grid_underneath (framepos_t p, framecnt_t& grid_frames) const
{
	PublicEditor& editor = trackview.editor ();
	
	const Evoral::Beats grid_beats = get_grid_beats(p);

	grid_frames = region_beats_to_region_frames (grid_beats);

	/* Hack so that we always snap to the note that we are over, instead of snapping
	   to the next one if we're more than halfway through the one we're over.
	*/
	if (editor.snap_mode() == SnapNormal && p >= grid_frames / 2) {
		p -= grid_frames / 2;
	}

	return snap_frame_to_frame (p);
}

/** Called when the selection has been cleared in any MidiRegionView.
 *  @param rv MidiRegionView that the selection was cleared in.
 */
void
MidiRegionView::selection_cleared (MidiRegionView* rv)
{
	if (rv == this) {
		return;
	}

	/* Clear our selection in sympathy; but don't signal the fact */
	clear_selection (false);
}

ChannelMode
MidiRegionView::get_channel_mode () const
{
	RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (&trackview);
	return rtav->midi_track()->get_playback_channel_mode();
}

uint16_t
MidiRegionView::get_selected_channels () const
{
	RouteTimeAxisView* rtav = dynamic_cast<RouteTimeAxisView*> (&trackview);
	return rtav->midi_track()->get_playback_channel_mask();
}


Evoral::Beats
MidiRegionView::get_grid_beats(framepos_t pos) const
{
	PublicEditor& editor  = trackview.editor();
	bool          success = false;
	Evoral::Beats beats   = editor.get_grid_type_as_beats(success, pos);
	if (!success) {
		beats = Evoral::Beats(1);
	}
	return beats;
}
