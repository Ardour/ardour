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
#include "evoral/Event.hpp"
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
#include "note.h"
#include "hit.h"
#include "patch_change.h"
#include "sys_ex.h"
#include "ui_config.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace PBD;
using namespace Editing;
using namespace std;
using Gtkmm2ext::Keyboard;

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
	, _region_relative_time_converter_double(r->session().tempo_map(), r->position())
	, _active_notes(0)
	, _note_group (new ArdourCanvas::Container (group))
	, _note_diff_command (0)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1.0)
	, _step_edit_cursor_position (0.0)
	, _channel_selection_scoped_note (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _grabbed_keyboard (false)
	, _entered (false)
	, _entered_note (0)
	, _mouse_changed_selection (false)
{
	CANVAS_DEBUG_NAME (_note_group, string_compose ("note group for %1", get_item_name()));

	_patch_change_outline = UIConfiguration::instance().color ("midi patch change outline");
	_patch_change_fill = UIConfiguration::instance().color_mod ("midi patch change fill", "midi patch change fill");

	_note_group->raise_to_top();
	PublicEditor::DropDownKeys.connect (sigc::mem_fun (*this, &MidiRegionView::drop_down_keys));

	Config->ParameterChanged.connect (*this, invalidator (*this), boost::bind (&MidiRegionView::parameter_changed, this, _1), gui_context());
	connect_to_diskstream ();
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
	, _region_relative_time_converter_double(r->session().tempo_map(), r->position())
	, _active_notes(0)
	, _note_group (new ArdourCanvas::Container (group))
	, _note_diff_command (0)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1.0)
	, _step_edit_cursor_position (0.0)
	, _channel_selection_scoped_note (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _grabbed_keyboard (false)
	, _entered (false)
	, _entered_note (0)
	, _mouse_changed_selection (false)
{
	CANVAS_DEBUG_NAME (_note_group, string_compose ("note group for %1", get_item_name()));

	_patch_change_outline = UIConfiguration::instance().color ("midi patch change outline");
	_patch_change_fill = UIConfiguration::instance().color_mod ("midi patch change fill", "midi patch change fill");

	_note_group->raise_to_top();

	PublicEditor::DropDownKeys.connect (sigc::mem_fun (*this, &MidiRegionView::drop_down_keys));

	connect_to_diskstream ();
}

void
MidiRegionView::parameter_changed (std::string const & p)
{
	if (p == "display-first-midi-bank-as-zero") {
		if (_enable_display) {
			redisplay_model();
		}
	} else if (p == "color-regions-using-track-color") {
		set_colors ();
	}
}

MidiRegionView::MidiRegionView (const MidiRegionView& other)
	: sigc::trackable(other)
	, RegionView (other)
	, _current_range_min(0)
	, _current_range_max(0)
	, _region_relative_time_converter(other.region_relative_time_converter())
	, _source_relative_time_converter(other.source_relative_time_converter())
	, _region_relative_time_converter_double(other.region_relative_time_converter_double())
	, _active_notes(0)
	, _note_group (new ArdourCanvas::Container (get_canvas_group()))
	, _note_diff_command (0)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1.0)
	, _step_edit_cursor_position (0.0)
	, _channel_selection_scoped_note (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _grabbed_keyboard (false)
	, _entered (false)
	, _entered_note (0)
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
	, _region_relative_time_converter_double(other.region_relative_time_converter_double())
	, _active_notes(0)
	, _note_group (new ArdourCanvas::Container (get_canvas_group()))
	, _note_diff_command (0)
	, _ghost_note(0)
	, _step_edit_cursor (0)
	, _step_edit_cursor_width (1.0)
	, _step_edit_cursor_position (0.0)
	, _channel_selection_scoped_note (0)
	, _mouse_state(None)
	, _pressed_button(0)
	, _optimization_iterator (_events.end())
	, _list_editor (0)
	, _no_sound_notes (false)
	, _last_display_zoom (0)
	, _last_event_x (0)
	, _last_event_y (0)
	, _grabbed_keyboard (false)
	, _entered (false)
	, _entered_note (0)
	, _mouse_changed_selection (false)
{
	init (true);
}

void
MidiRegionView::init (bool wfd)
{
	PublicEditor::DropDownKeys.connect (sigc::mem_fun (*this, &MidiRegionView::drop_down_keys));

	if (wfd) {
		Glib::Threads::Mutex::Lock lm(midi_region()->midi_source(0)->mutex());
		midi_region()->midi_source(0)->load_model(lm);
	}

	_model = midi_region()->midi_source(0)->model();
	_enable_display = false;
	fill_color_name = "midi frame base";

	RegionView::init (false);

	//set_height (trackview.current_height());

	region_muted ();
	region_sync_changed ();
	region_resized (ARDOUR::bounds_change);
	//region_locked ();

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
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &MidiRegionView::parameter_changed));
	connect_to_diskstream ();
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
	enter_internal (ev->state);

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
		if (!trackview.editor().internal_editing()) {
			/* Switched out of internal editing mode while entered.
			Only necessary for leave as a mouse_mode_change over a region
			automatically triggers an enter event. */
			leave_internal();
		}
		else if (trackview.editor().current_mouse_mode() == MouseContent) {
			// hide cursor and ghost note after changing to internal edit mode
			remove_ghost_note ();

			/* XXX This is problematic as the function is executed for every region
			and only for one region _entered_note can be true. Still it's
			necessary as to hide the verbose cursor when we're changing from
			draw mode to internal edit mode. These lines are the reason why
			in some situations no verbose cursor is shown when we enter internal
			edit mode over a note. */
			if (!_entered_note) {
				hide_verbose_cursor ();
			}
		}
	}
}

void
MidiRegionView::enter_internal (uint32_t state)
{
	if (trackview.editor().current_mouse_mode() == MouseDraw && _mouse_state != AddDragging) {
		// Show ghost note under pencil
		create_ghost_note(_last_event_x, _last_event_y, state);
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
	hide_verbose_cursor ();
	remove_ghost_note ();
	_entered_note = 0;

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

		if (m == MouseDraw || (m == MouseContent && Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier()))) {

			if (midi_view()->note_mode() == Percussive) {
				editor->drags()->set (new HitCreateDrag (dynamic_cast<Editor *> (editor), group, this), (GdkEvent *) ev);
			} else {
				editor->drags()->set (new NoteCreateDrag (dynamic_cast<Editor *> (editor), group, this), (GdkEvent *) ev);
			}

			_mouse_state = AddDragging;
			remove_ghost_note ();
			hide_verbose_cursor ();
		} else {
			_mouse_state = Pressed;
		}

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
			/* no motion occurred - simple click */
			clear_editor_note_selection ();
			_mouse_changed_selection = true;
			break;

		case MouseContent:
		case MouseTimeFX:
			{
				_mouse_changed_selection = true;
				clear_editor_note_selection ();

				break;
			}
		case MouseDraw:
			break;

		default:
			break;
		}

		_mouse_state = None;
		break;

	case AddDragging:
		/* Don't a ghost note when we added a note - wait until motion to avoid visual confusion.
		   we don't want one when we were drag-selecting either. */
	case SelectRectDragging:
		editor.drags()->end_grab ((GdkEvent *) ev);
		_mouse_state = None;
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

	if (!_entered_note) {

		if (_mouse_state == AddDragging) {
			if (_ghost_note) {
				remove_ghost_note ();
			}

		} else if (!_ghost_note && editor.current_mouse_mode() == MouseContent &&
		    Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier()) &&
		    _mouse_state != AddDragging) {

			create_ghost_note (ev->x, ev->y, ev->state);

		} else if (_ghost_note && editor.current_mouse_mode() == MouseContent &&
			   Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier())) {

			update_ghost_note (ev->x, ev->y, ev->state);

		} else if (_ghost_note && editor.current_mouse_mode() == MouseContent) {

			remove_ghost_note ();
			hide_verbose_cursor ();

		} else if (editor.current_mouse_mode() == MouseDraw) {

			if (_ghost_note) {
				update_ghost_note (ev->x, ev->y, ev->state);
			}
			else {
				create_ghost_note (ev->x, ev->y, ev->state);
			}
		}
	}

	/* any motion immediately hides velocity text that may have been visible */

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->hide_velocity ();
	}

	switch (_mouse_state) {
	case Pressed:

		if (_pressed_button == 1) {

			MouseMode m = editor.current_mouse_mode();

			if (m == MouseContent && !Keyboard::modifier_state_contains (ev->state, Keyboard::insert_note_modifier())) {
				editor.drags()->set (new MidiRubberbandSelectDrag (dynamic_cast<Editor *> (&editor), this), (GdkEvent *) ev);
				if (!Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
					clear_editor_note_selection ();
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

	if (Keyboard::modifier_state_equals (ev->state, Keyboard::PrimaryModifier) ||
	    Keyboard::modifier_state_equals (ev->state, Keyboard::TertiaryModifier)) {
		/* XXX: bit of a hack; allow PrimaryModifier and TertiaryModifier scroll
		 * through so that it still works for navigation.
		*/
		return false;
	}

	hide_verbose_cursor ();

	bool fine = !Keyboard::modifier_state_contains (ev->state, Keyboard::SecondaryModifier);
	Keyboard::ModifierMask mask_together(Keyboard::PrimaryModifier|Keyboard::TertiaryModifier);
	bool together = Keyboard::modifier_state_contains (ev->state, mask_together);

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

		if (_mouse_state != AddDragging) {
			_mouse_state = SelectTouchDragging;
		}

		return true;

	} else if (ev->keyval == GDK_Escape && unmodified) {
		clear_editor_note_selection ();
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
MidiRegionView::create_note_at (framepos_t t, double y, Evoral::Beats length, uint32_t state, bool shift_snap)
{
	if (length < 2 * DBL_EPSILON) {
		return;
	}

	MidiTimeAxisView* const mtv  = dynamic_cast<MidiTimeAxisView*>(&trackview);
	MidiStreamView* const   view = mtv->midi_view();
	boost::shared_ptr<MidiRegion> mr  = boost::dynamic_pointer_cast<MidiRegion> (_region);

	if (!mr) {
		return;
	}

	// Start of note in frames relative to region start
	const int32_t divisions = trackview.editor().get_grid_music_divisions (state);
	Evoral::Beats beat_time = snap_frame_to_grid_underneath (t, divisions, shift_snap);

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

	clear_editor_note_selection ();
	note_diff_add_note (new_note, true, false);

	apply_diff();

	play_midi_note (new_note);
}

void
MidiRegionView::clear_events ()
{
	// clear selection without signaling
	clear_selection_internal ();

	MidiGhostRegion* gr;
	for (std::vector<GhostRegion*>::iterator g = ghosts.begin(); g != ghosts.end(); ++g) {
		if ((gr = dynamic_cast<MidiGhostRegion*>(*g)) != 0) {
			gr->clear_events();
		}
	}


	_note_group->clear (true);
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
	clear_events();

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
MidiRegionView::apply_diff (bool as_subcommand, bool was_copy)
{
	bool add_or_remove;
	bool commit = false;

	if (!_note_diff_command) {
		return;
	}

	if (!was_copy && (add_or_remove = _note_diff_command->adds_or_removes())) {
		// Mark all selected notes for selection when model reloads
		for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
			_marked_for_selection.insert((*i)->note());
		}
	}

	midi_view()->midi_track()->midi_playlist()->region_edited (_region, _note_diff_command);

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
	clear_editor_note_selection();
}

NoteBase*
MidiRegionView::find_canvas_note (boost::shared_ptr<NoteType> note)
{

	if (_optimization_iterator != _events.end()) {
		++_optimization_iterator;
	}

	if (_optimization_iterator != _events.end() && _optimization_iterator->first == note) {
		return _optimization_iterator->second;
	}

	_optimization_iterator = _events.find (note);
	if (_optimization_iterator != _events.end()) {
		return _optimization_iterator->second;
	}

	return 0;
}

/** This version finds any canvas note matching the supplied note. */
NoteBase*
MidiRegionView::find_canvas_note (Evoral::event_id_t id)
{
	Events::iterator it;

	for (it = _events.begin(); it != _events.end(); ++it) {
		if (it->first->id() == id) {
			return it->second;
		}
	}

	return 0;
}

boost::shared_ptr<PatchChange>
MidiRegionView::find_canvas_patch_change (MidiModel::PatchChangePtr p)
{
	PatchChanges::const_iterator f = _patch_changes.find (p);

	if (f != _patch_changes.end()) {
		return f->second;
	}

	return boost::shared_ptr<PatchChange>();
}

boost::shared_ptr<SysEx>
MidiRegionView::find_canvas_sys_ex (MidiModel::SysExPtr s)
{
	SysExes::const_iterator f = _sys_exes.find (s);

	if (f != _sys_exes.end()) {
		return f->second;
	}

	return boost::shared_ptr<SysEx>();
}

void
MidiRegionView::get_events (Events& e, Evoral::Sequence<Evoral::Beats>::NoteOperator op, uint8_t val, int chan_mask)
{
	MidiModel::Notes notes;
	_model->get_notes (notes, op, val, chan_mask);

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {
		NoteBase* cne = find_canvas_note (*n);
		if (cne) {
			e.insert (make_pair (*n, cne));
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
				if (i->second->note()->length() > 0) {
					update_note(i->second);
				}
			}
			_last_display_zoom = zoom;
		}
		return;
	}

	if (!_model) {
		return;
	}

	for (_optimization_iterator = _events.begin(); _optimization_iterator != _events.end(); ++_optimization_iterator) {
		_optimization_iterator->second->invalidate();
	}

	bool empty_when_starting = _events.empty();
	_optimization_iterator = _events.begin();
	MidiModel::Notes missing_notes;
	Note* sus = NULL;
	Hit*  hit = NULL;

	MidiModel::ReadLock lock(_model->read_lock());
	MidiModel::Notes& notes (_model->notes());

	NoteBase* cne;
	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {

		boost::shared_ptr<NoteType> note (*n);
		bool visible;

		if (note_in_region_range (note, visible)) {
			if (!empty_when_starting && (cne = find_canvas_note (note)) != 0) {
				cne->validate ();
				if (visible) {
					cne->show ();
				} else {
					cne->hide ();
				}
			} else {
				missing_notes.insert (note);
			}
		}
	}

	if (!empty_when_starting) {
		MidiModel::Notes::iterator f;
		for (Events::iterator i = _events.begin(); i != _events.end(); ) {

			NoteBase* cne = i->second;

			/* remove note items that are no longer valid */
			if (!cne->valid()) {

				for (vector<GhostRegion*>::iterator j = ghosts.begin(); j != ghosts.end(); ++j) {
					MidiGhostRegion* gr = dynamic_cast<MidiGhostRegion*> (*j);
					if (gr) {
						gr->remove_note (cne);
					}
				}

				delete cne;
				i = _events.erase (i);

			} else {
				bool visible = cne->item()->visible();

				if ((sus = dynamic_cast<Note*>(cne))) {

					if (visible) {
						update_sustained (sus);
					}

				} else if ((hit = dynamic_cast<Hit*>(cne))) {

					if (visible) {
						update_hit (hit);
					}

				}
				++i;
			}
		}
	}

	for (MidiModel::Notes::iterator n = missing_notes.begin(); n != missing_notes.end(); ++n) {
		boost::shared_ptr<NoteType> note (*n);
		NoteBase* cne;
		bool visible;

		if (note_in_region_range (note, visible)) {
			if (visible) {
				cne = add_note (note, true);
			} else {
				cne = add_note (note, false);
			}
		} else {
			cne = add_note (note, false);
		}

		for (set<Evoral::event_id_t>::iterator it = _pending_note_selection.begin(); it != _pending_note_selection.end(); ++it) {
			if ((*it) == note->id()) {
				add_to_selection (cne);
			}
		}
	}

	for (vector<GhostRegion*>::iterator j = ghosts.begin(); j != ghosts.end(); ++j) {
		MidiGhostRegion* gr = dynamic_cast<MidiGhostRegion*> (*j);
		if (gr && !gr->trackview.hidden()) {
			gr->redisplay_model ();
		}
	}

	display_sysexes();
	display_patch_changes ();

	_marked_for_selection.clear ();
	_marked_for_velocity.clear ();
	_pending_note_selection.clear ();

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
		boost::shared_ptr<PatchChange> p;

		if ((*i)->channel() != channel) {
			continue;
		}

		if ((p = find_canvas_patch_change (*i)) != 0) {

			const framecnt_t region_frames = source_beats_to_region_frames ((*i)->time());

			if (region_frames < 0 || region_frames >= _region->length()) {
				p->hide();
			} else {
				const double x = trackview.editor().sample_to_pixel (region_frames);
				const string patch_name = instrument_info().get_patch_name ((*i)->bank(), (*i)->program(), channel);
				p->canvas_item()->set_position (ArdourCanvas::Duple (x, 1.0));
				p->set_text (patch_name);

				p->show();
			}

		} else {
			const string patch_name = instrument_info().get_patch_name ((*i)->bank(), (*i)->program(), channel);
			add_canvas_patch_change (*i, patch_name, active_channel);
		}
	}
}

void
MidiRegionView::display_sysexes()
{
	bool have_periodic_system_messages = false;
	bool display_periodic_messages = true;

	if (!UIConfiguration::instance().get_never_display_periodic_midi()) {

		for (MidiModel::SysExes::const_iterator i = _model->sysexes().begin(); i != _model->sysexes().end(); ++i) {
			if ((*i)->is_spp() || (*i)->is_mtc_quarter() || (*i)->is_mtc_full()) {
				have_periodic_system_messages = true;
				break;
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
		MidiModel::SysExPtr sysex_ptr = *i;
		Evoral::Beats time = sysex_ptr->time();

		if ((*i)->is_spp() || (*i)->is_mtc_quarter() || (*i)->is_mtc_full()) {
			if (!display_periodic_messages) {
				continue;
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
		boost::shared_ptr<SysEx> sysex = find_canvas_sys_ex (sysex_ptr);

		if (!sysex) {
			sysex = boost::shared_ptr<SysEx>(
				new SysEx (*this, _note_group, text, height, x, 1.0, sysex_ptr));
			_sys_exes.insert (make_pair (sysex_ptr, sysex));
		} else {
			sysex->set_height (height);
			sysex->item().set_position (ArdourCanvas::Duple (x, 1.0));
		}

		// Show unless message is beyond the region bounds
		if (time - _region->start() >= _region->length() || time < _region->start()) {
			sysex->hide();
		} else {
			sysex->show();
		}
	}
}

MidiRegionView::~MidiRegionView ()
{
	in_destructor = true;

	hide_verbose_cursor ();

	delete _list_editor;

	RegionViewGoingAway (this); /* EMIT_SIGNAL */

	if (_active_notes) {
		end_write();
	}
	_entered_note = 0;
	clear_events ();

	delete _note_group;
	delete _note_diff_command;
	delete _step_edit_cursor;
}

void
MidiRegionView::region_resized (const PropertyChange& what_changed)
{
	RegionView::region_resized(what_changed); // calls RegionView::set_duration()

	if (what_changed.contains (ARDOUR::Properties::position)) {
		_region_relative_time_converter.set_origin_b(_region->position());
		_region_relative_time_converter_double.set_origin_b(_region->position());
		/* reset_width dependent_items() redisplays model */

	}

	if (what_changed.contains (ARDOUR::Properties::start) ||
	    what_changed.contains (ARDOUR::Properties::position)) {
		_source_relative_time_converter.set_origin_b (_region->position() - _region->start());
	}
	/* catch end and start trim so we can update the view*/
	if (!what_changed.contains (ARDOUR::Properties::start) &&
	    what_changed.contains (ARDOUR::Properties::length)) {
		enable_display (true);
	} else if (what_changed.contains (ARDOUR::Properties::start) &&
	    what_changed.contains (ARDOUR::Properties::length)) {
		enable_display (true);
	}
}

void
MidiRegionView::reset_width_dependent_items (double pixel_width)
{
	RegionView::reset_width_dependent_items(pixel_width);

	if (_enable_display) {
		redisplay_model();
	}

	bool hide_all = false;
	PatchChanges::iterator x = _patch_changes.begin();
	if (x != _patch_changes.end()) {
		hide_all = x->second->width() >= _pixel_width;
	}

	if (hide_all) {
		for (; x != _patch_changes.end(); ++x) {
			x->second->hide();
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
		(*x).second->set_height (midi_stream_view()->contents_height());
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

	redisplay_model ();
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
		ghost = new MidiGhostRegion (*this, *mtv->midi_view(), trackview, unit_position);
	} else {
		ghost = new MidiGhostRegion (*this, tv, trackview, unit_position);
	}

	ghost->set_colors ();
	ghost->set_height ();
	ghost->set_duration (_region->length() / samples_per_pixel);

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		ghost->add_note(i->second);
	}

	ghosts.push_back (ghost);
	enable_display (true);
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
	if (_no_sound_notes || !UIConfiguration::instance().get_sound_midi_notes()) {
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
	if (_no_sound_notes || !UIConfiguration::instance().get_sound_midi_notes()) {
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
	const boost::shared_ptr<ARDOUR::MidiRegion> midi_reg = midi_region();

	/* must compare double explicitly as Beats::operator< rounds to ppqn */
	const bool outside = (note->time().to_double() < midi_reg->start_beats() ||
			      note->time().to_double() >= midi_reg->start_beats() + midi_reg->length_beats());

	visible = (note->note() >= _current_range_min) &&
		(note->note() <= _current_range_max);

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
	TempoMap& map (trackview.session()->tempo_map());
	const boost::shared_ptr<ARDOUR::MidiRegion> mr = midi_region();
	boost::shared_ptr<NoteType> note = ev->note();

	const double session_source_start = _region->quarter_note() - mr->start_beats();
	const framepos_t note_start_frames = map.frame_at_quarter_note (note->time().to_double() + session_source_start) - _region->position();

	const double x0 = trackview.editor().sample_to_pixel (note_start_frames);
	double x1;
	const double y0 = 1 + floor(note_to_y(note->note()));
	double y1;

	/* trim note display to not overlap the end of its region */
	if (note->length().to_double() > 0.0) {
		double note_end_time = note->end_time().to_double();

		if (note_end_time > mr->start_beats() + mr->length_beats()) {
			note_end_time = mr->start_beats() + mr->length_beats();
		}

		const framepos_t note_end_frames = map.frame_at_quarter_note (session_source_start + note_end_time) - _region->position();

		x1 = std::max(1., trackview.editor().sample_to_pixel (note_end_frames)) - 1;
	} else {
		x1 = std::max(1., trackview.editor().sample_to_pixel (_region->length())) - 1;
	}

	y1 = y0 + std::max(1., floor(note_height()) - 1);

	ev->set (ArdourCanvas::Rect (x0, y0, x1, y1));

	if (!note->length()) {
		if (_active_notes && note->note() < 128) {
			Note* const old_rect = _active_notes[note->note()];
			if (old_rect) {
				/* There is an active note on this key, so we have a stuck
				   note.  Finish the old rectangle here. */
				old_rect->set_x1 (x1);
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
	const uint32_t base_col = ev->base_color();
	ev->set_fill_color(base_col);
	ev->set_outline_color(ev->calculate_outline(base_col, ev->selected()));

}

void
MidiRegionView::update_hit (Hit* ev, bool update_ghost_regions)
{
	boost::shared_ptr<NoteType> note = ev->note();

	const double note_time_qn = note->time().to_double() + (_region->quarter_note() - midi_region()->start_beats());
	const framepos_t note_start_frames = trackview.session()->tempo_map().frame_at_quarter_note (note_time_qn) - _region->position();

	const double x = trackview.editor().sample_to_pixel(note_start_frames);
	const double diamond_size = std::max(1., floor(note_height()) - 2.);
	const double y = 1.5 + floor(note_to_y(note->note())) + diamond_size * .5;

	// see DnD note in MidiRegionView::apply_note_range() above
	if (y <= 0 || y >= _height) {
		ev->hide();
	} else {
		ev->show();
	}

	ev->set_position (ArdourCanvas::Duple (x, y));
	ev->set_height (diamond_size);

	// Update color in case velocity has changed
	const uint32_t base_col = ev->base_color();
	ev->set_fill_color(base_col);
	ev->set_outline_color(ev->calculate_outline(base_col, ev->selected()));

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

		Note* ev_rect = new Note (*this, _note_group, note); // XXX may leak

		update_sustained (ev_rect);

		event = ev_rect;

	} else if (midi_view()->note_mode() == Percussive) {

		const double diamond_size = std::max(1., floor(note_height()) - 2.);

		Hit* ev_diamond = new Hit (*this, _note_group, diamond_size, note); // XXX may leak

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
		_events.insert (make_pair (event->note(), event));

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
		/* XX sets length in beats from audio space. make musical */
		_region->set_length (end_frame - _region->position(), 0);
	}

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	MidiStreamView* const view = mtv->midi_view();

	view->update_note_range(new_note->note());

	_marked_for_selection.clear ();

	start_note_diff_command (_("step add"));

	clear_editor_note_selection ();
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
				patch,
				_patch_change_outline,
				_patch_change_fill)
		);

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

	_patch_changes.insert (make_pair (patch, patch_change));
}

void
MidiRegionView::remove_canvas_patch_change (PatchChange* pc)
{
	/* remove the canvas item */
	for (PatchChanges::iterator x = _patch_changes.begin(); x != _patch_changes.end(); ++x) {
		if (x->second->patch() == pc->patch()) {
			_patch_changes.erase (x);
			break;
		}
	}
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

	remove_canvas_patch_change (&pc);
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

	for (PatchChanges::iterator x = _patch_changes.begin(); x != _patch_changes.end(); ++x) {
		if (x->second->patch() == old_change) {
			_patch_changes.erase (x);
			break;
		}
	}

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

	remove_canvas_patch_change (pc);
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
MidiRegionView::note_deleted (NoteBase* cne)
{
	if (_entered_note && cne == _entered_note) {
		_entered_note = 0;
	}

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

	if (trackview.editor().drags()->active()) {
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

	hide_verbose_cursor ();
}

void
MidiRegionView::delete_note (boost::shared_ptr<NoteType> n)
{
	start_note_diff_command (_("delete note"));
	_note_diff_command->remove (n);
	apply_diff ();

	hide_verbose_cursor ();
}

void
MidiRegionView::clear_editor_note_selection ()
{
	DEBUG_TRACE(DEBUG::Selection, "MRV::clear_editor_note_selection\n");
	PublicEditor& editor(trackview.editor());
	editor.get_selection().clear_midi_notes();
}

void
MidiRegionView::clear_selection ()
{
	clear_selection_internal();
	PublicEditor& editor(trackview.editor());
	editor.get_selection().remove(this);
}

void
MidiRegionView::clear_selection_internal ()
{
	DEBUG_TRACE(DEBUG::Selection, "MRV::clear_selection_internal\n");

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->set_selected(false);
		(*i)->hide_velocity();
	}
	_selection.clear();

	if (_entered) {
		// Clearing selection entirely, ungrab keyboard
		Keyboard::magic_widget_drop_focus();
		_grabbed_keyboard = false;
	}
}

void
MidiRegionView::unique_select(NoteBase* ev)
{
	clear_editor_note_selection();
	add_to_selection(ev);
}

void
MidiRegionView::select_all_notes ()
{
	clear_editor_note_selection ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		add_to_selection (i->second);
	}
}

void
MidiRegionView::select_range (framepos_t start, framepos_t end)
{
	clear_editor_note_selection ();

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		framepos_t t = source_beats_to_absolute_frames(i->first->time());
		if (t >= start && t <= end) {
			add_to_selection (i->second);
		}
	}
}

void
MidiRegionView::invert_selection ()
{
	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if (i->second->selected()) {
			remove_from_selection(i->second);
		} else {
			add_to_selection (i->second);
		}
	}
}

/** Used for selection undo/redo.
    The requested notes most likely won't exist in the view until the next model redisplay.
*/
void
MidiRegionView::select_notes (list<Evoral::event_id_t> notes)
{
	NoteBase* cne;
	list<Evoral::event_id_t>::iterator n;

	for (n = notes.begin(); n != notes.end(); ++n) {
		if ((cne = find_canvas_note(*n)) != 0) {
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
		clear_editor_note_selection ();

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
		clear_editor_note_selection();
		add_to_selection (ev);
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

			if ((i->first->time() >= earliest && i->first->end_time() <= latest) ||
			    (i->first->time() <= earliest && i->first->end_time() >= latest)) {
				add_to_selection (i->second);
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
		if (i->second->x0() < x1 && i->second->x1() > x0 && i->second->y0() < y1 && i->second->y1() > y0) {
			// Rectangles intersect
			if (!i->second->selected()) {
				add_to_selection (i->second);
			}
		} else if (i->second->selected() && !extend) {
			// Rectangles do not intersect
			remove_from_selection (i->second);
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
		if ((i->second->y1() >= y1 && i->second->y1() <= y2)) {
			// within y- (note-) range
			if (!i->second->selected()) {
				add_to_selection (i->second);
			}
		} else if (i->second->selected() && !extend) {
			remove_from_selection (i->second);
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

Evoral::Beats
MidiRegionView::earliest_in_selection ()
{
	Evoral::Beats earliest = Evoral::MaxBeats;

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		if ((*i)->note()->time() < earliest) {
			earliest = (*i)->note()->time();
		}
	}

	return earliest;
}

void
MidiRegionView::move_selection(double dx_qn, double dy, double cumulative_dy)
{
	typedef vector<boost::shared_ptr<NoteType> > PossibleChord;
	Editor* editor = dynamic_cast<Editor*> (&trackview.editor());
	TempoMap& tmap (editor->session()->tempo_map());
	PossibleChord to_play;
	Evoral::Beats earliest = earliest_in_selection();

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		NoteBase* n = *i;
		if (n->note()->time() == earliest) {
			to_play.push_back (n->note());
		}
		double const note_time_qn = session_relative_qn (n->note()->time().to_double());
		double const dx = editor->sample_to_pixel_unrounded (tmap.frame_at_quarter_note (note_time_qn + dx_qn))
			- n->item()->item_to_canvas (ArdourCanvas::Duple (n->x0(), 0)).x;

		(*i)->move_event(dx, dy);

		/* update length */
		if (midi_view()->note_mode() == Sustained) {
			Note* sus = dynamic_cast<Note*> (*i);
			double const len_dx = editor->sample_to_pixel_unrounded (
				tmap.frame_at_quarter_note (note_time_qn + dx_qn + n->note()->length().to_double()));

			sus->set_x1 (n->item()->canvas_to_item (ArdourCanvas::Duple (len_dx, 0)).x);
		}
	}

	if (dy && !_selection.empty() && !_no_sound_notes && UIConfiguration::instance().get_sound_midi_notes()) {

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

NoteBase*
MidiRegionView::copy_selection (NoteBase* primary)
{
	_copy_drag_events.clear ();

	if (_selection.empty()) {
		return 0;
	}

	NoteBase* note;
	NoteBase* ret = 0;

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		boost::shared_ptr<NoteType> g (new NoteType (*((*i)->note())));
		if (midi_view()->note_mode() == Sustained) {
			Note* n = new Note (*this, _note_group, g);
			update_sustained (n, false);
			note = n;
		} else {
			Hit* h = new Hit (*this, _note_group, 10, g);
			update_hit (h, false);
			note = h;
		}

		if ((*i) == primary) {
			ret = note;
		}

		_copy_drag_events.push_back (note);
	}

	return ret;
}

void
MidiRegionView::move_copies (double dx_qn, double dy, double cumulative_dy)
{
	typedef vector<boost::shared_ptr<NoteType> > PossibleChord;
	Editor* editor = dynamic_cast<Editor*> (&trackview.editor());
	TempoMap& tmap (editor->session()->tempo_map());
	PossibleChord to_play;
	Evoral::Beats earliest = earliest_in_selection();

	for (CopyDragEvents::iterator i = _copy_drag_events.begin(); i != _copy_drag_events.end(); ++i) {
		NoteBase* n = *i;
		if (n->note()->time() == earliest) {
			to_play.push_back (n->note());
		}
		double const note_time_qn = session_relative_qn (n->note()->time().to_double());
		double const dx = editor->sample_to_pixel_unrounded (tmap.frame_at_quarter_note (note_time_qn + dx_qn))
			- n->item()->item_to_canvas (ArdourCanvas::Duple (n->x0(), 0)).x;

		(*i)->move_event(dx, dy);

		if (midi_view()->note_mode() == Sustained) {
			Note* sus = dynamic_cast<Note*> (*i);
			double const len_dx = editor->sample_to_pixel_unrounded (
				tmap.frame_at_quarter_note (note_time_qn + dx_qn + n->note()->length().to_double()));

			sus->set_x1 (n->item()->canvas_to_item (ArdourCanvas::Duple (len_dx, 0)).x);
		}
	}

	if (dy && !_copy_drag_events.empty() && !_no_sound_notes && UIConfiguration::instance().get_sound_midi_notes()) {

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
MidiRegionView::note_dropped(NoteBase *, double d_qn, int8_t dnote, bool copy)
{
	uint8_t lowest_note_in_selection  = 127;
	uint8_t highest_note_in_selection = 0;
	uint8_t highest_note_difference   = 0;

	if (!copy) {
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

			Evoral::Beats new_time = Evoral::Beats ((*i)->note()->time().to_double() + d_qn);

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
	} else {

		clear_editor_note_selection ();

		for (CopyDragEvents::iterator i = _copy_drag_events.begin(); i != _copy_drag_events.end(); ++i) {
			uint8_t pitch = (*i)->note()->note();
			lowest_note_in_selection  = std::min(lowest_note_in_selection,  pitch);
			highest_note_in_selection = std::max(highest_note_in_selection, pitch);
		}

		// Make sure the note pitch does not exceed the MIDI standard range
		if (highest_note_in_selection + dnote > 127) {
			highest_note_difference = highest_note_in_selection - 127;
		}

		start_note_diff_command (_("copy notes"));

		for (CopyDragEvents::iterator i = _copy_drag_events.begin(); i != _copy_drag_events.end() ; ++i) {

			/* update time */
			Evoral::Beats new_time = Evoral::Beats ((*i)->note()->time().to_double() + d_qn);

			if (new_time < 0) {
				continue;
			}

			(*i)->note()->set_time (new_time);

			/* update pitch */

			uint8_t original_pitch = (*i)->note()->note();
			uint8_t new_pitch      = original_pitch + dnote - highest_note_difference;

			(*i)->note()->set_note (new_pitch);

			// keep notes in standard midi range
			clamp_to_0_127(new_pitch);

			lowest_note_in_selection  = std::min(lowest_note_in_selection,  new_pitch);
			highest_note_in_selection = std::max(highest_note_in_selection, new_pitch);

			note_diff_add_note ((*i)->note(), true);

			delete *i;
		}

		_copy_drag_events.clear ();
	}

	apply_diff (false, copy);

	// care about notes being moved beyond the upper/lower bounds on the canvas
	if (lowest_note_in_selection  < midi_stream_view()->lowest_note() ||
	    highest_note_in_selection > midi_stream_view()->highest_note()) {
		midi_stream_view()->set_note_range (MidiStreamView::ContentsRange);
	}
}

/** @param x Pixel relative to the region position.
 *  @param ensure_snap defaults to false. true = snap always, ignoring snap mode and magnetic snap.
 *  Used for inverting the snap logic with key modifiers and snap delta calculation.
 *  @return Snapped frame relative to the region position.
 */
framepos_t
MidiRegionView::snap_pixel_to_sample(double x, bool ensure_snap)
{
	PublicEditor& editor (trackview.editor());
	return snap_frame_to_frame (editor.pixel_to_sample (x), ensure_snap).frame;
}

/** @param x Pixel relative to the region position.
 *  @param ensure_snap defaults to false. true = ignore magnetic snap and snap mode (used for snap delta calculation).
 *  @return Snapped pixel relative to the region position.
 */
double
MidiRegionView::snap_to_pixel(double x, bool ensure_snap)
{
	return (double) trackview.editor().sample_to_pixel(snap_pixel_to_sample(x, ensure_snap));
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

double
MidiRegionView::region_frames_to_region_beats_double (framepos_t frames) const
{
	return _region_relative_time_converter_double.from(frames);
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
				UIConfiguration::instance().color ("midi note selected"),
				128);

			// make the resize preview notes more transparent and bright
			fill_color = UINT_INTERPOLATE(fill_color, 0xFFFFFF40, 0.5);

			// calculate color based on note velocity
			resize_rect->set_fill_color (UINT_INTERPOLATE(
				NoteBase::meter_style_fill_color(note->note()->velocity(), note->selected()),
				fill_color,
				0.85));

			resize_rect->set_outline_color (NoteBase::calculate_outline (
								UIConfiguration::instance().color ("midi note selected")));

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
 * @param snap_delta snap offset of the primary note in pixels. used in SnapRelative SnapDelta mode.
 * @param with_snap true if snap is to be used to determine the position, false if no snap is to be used.
 */
void
MidiRegionView::update_resizing (NoteBase* primary, bool at_front, double delta_x, bool relative, double snap_delta, bool with_snap)
{
	TempoMap& tmap (trackview.session()->tempo_map());
	bool cursor_set = false;
	bool const ensure_snap = trackview.editor().snap_mode () != SnapMagnetic;

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		ArdourCanvas::Rectangle* resize_rect = (*i)->resize_rect;
		Note* canvas_note = (*i)->note;
		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x0() + delta_x + snap_delta;
			} else {
				current_x = primary->x0() + delta_x + snap_delta;
			}
		} else {
			if (relative) {
				current_x = canvas_note->x1() + delta_x + snap_delta;
			} else {
				current_x = primary->x1() + delta_x + snap_delta;
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
			if (with_snap) {
				resize_rect->set_x0 (snap_to_pixel (current_x, ensure_snap) - snap_delta);
			} else {
				resize_rect->set_x0 (current_x - snap_delta);
			}
			resize_rect->set_x1 (canvas_note->x1());
		} else {
			if (with_snap) {
				resize_rect->set_x1 (snap_to_pixel (current_x, ensure_snap) - snap_delta);
			} else {
				resize_rect->set_x1 (current_x - snap_delta);
			}
			resize_rect->set_x0 (canvas_note->x0());
		}

		if (!cursor_set) {
			/* Convert snap delta from pixels to beats. */
			framepos_t snap_delta_samps = trackview.editor().pixel_to_sample (snap_delta);
			double snap_delta_beats = 0.0;
			int sign = 1;

			/* negative beat offsets aren't allowed */
			if (snap_delta_samps > 0) {
				snap_delta_beats = region_frames_to_region_beats_double (snap_delta_samps);
			} else if (snap_delta_samps < 0) {
				snap_delta_beats = region_frames_to_region_beats_double ( - snap_delta_samps);
				sign = -1;
			}

			double  snapped_x;
			int32_t divisions = 0;

			if (with_snap) {
				snapped_x = snap_pixel_to_sample (current_x, ensure_snap);
				divisions = trackview.editor().get_grid_music_divisions (0);
			} else {
				snapped_x = trackview.editor ().pixel_to_sample (current_x);
			}
			const Evoral::Beats beats = Evoral::Beats (tmap.exact_beat_at_frame (snapped_x + midi_region()->position(), divisions)
								     - midi_region()->beat()) + midi_region()->start_beats();

			Evoral::Beats len         = Evoral::Beats();

			if (at_front) {
				if (beats < canvas_note->note()->end_time()) {
					len = canvas_note->note()->time() - beats + (sign * snap_delta_beats);
					len += canvas_note->note()->length();
				}
			} else {
				if (beats >= canvas_note->note()->time()) {
					len = beats - canvas_note->note()->time() - (sign * snap_delta_beats);
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
MidiRegionView::commit_resizing (NoteBase* primary, bool at_front, double delta_x, bool relative, double snap_delta, bool with_snap)
{
	_note_diff_command = _model->new_note_diff_command (_("resize notes"));
	TempoMap& tmap (trackview.session()->tempo_map());

	/* XX why doesn't snap_pixel_to_sample() handle this properly? */
	bool const ensure_snap = trackview.editor().snap_mode () != SnapMagnetic;

	for (std::vector<NoteResizeData *>::iterator i = _resize_data.begin(); i != _resize_data.end(); ++i) {
		Note*  canvas_note = (*i)->note;
		ArdourCanvas::Rectangle*  resize_rect = (*i)->resize_rect;

		/* Get the new x position for this resize, which is in pixels relative
		 * to the region position.
		 */

		double current_x;

		if (at_front) {
			if (relative) {
				current_x = canvas_note->x0() + delta_x + snap_delta;
			} else {
				current_x = primary->x0() + delta_x + snap_delta;
			}
		} else {
			if (relative) {
				current_x = canvas_note->x1() + delta_x + snap_delta;
			} else {
				current_x = primary->x1() + delta_x + snap_delta;
			}
		}

		if (current_x < 0) {
			current_x = 0;
		}
		if (current_x > trackview.editor().sample_to_pixel(_region->length())) {
			current_x = trackview.editor().sample_to_pixel(_region->length());
		}

		/* Convert snap delta from pixels to beats with sign. */
		framepos_t snap_delta_samps = trackview.editor().pixel_to_sample (snap_delta);
		double snap_delta_beats = 0.0;
		int sign = 1;

		if (snap_delta_samps > 0) {
			snap_delta_beats = region_frames_to_region_beats_double (snap_delta_samps);
		} else if (snap_delta_samps < 0) {
			snap_delta_beats = region_frames_to_region_beats_double ( - snap_delta_samps);
			sign = -1;
		}

		uint32_t divisions = 0;
		/* Convert the new x position to a frame within the source */
		framepos_t current_fr;
		if (with_snap) {
			current_fr = snap_pixel_to_sample (current_x, ensure_snap);
			divisions = trackview.editor().get_grid_music_divisions (0);
		} else {
			current_fr = trackview.editor().pixel_to_sample (current_x);
		}

		/* and then to beats */
		const double e_qaf = tmap.exact_qn_at_frame (current_fr + midi_region()->position(), divisions);
		const double quarter_note_start = _region->quarter_note() - midi_region()->start_beats();
		const Evoral::Beats x_beats = Evoral::Beats (e_qaf - quarter_note_start);

		if (at_front && x_beats < canvas_note->note()->end_time()) {
			note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::StartTime, x_beats - (sign * snap_delta_beats));
			Evoral::Beats len = canvas_note->note()->time() - x_beats + (sign * snap_delta_beats);
			len += canvas_note->note()->length();

			if (!!len) {
				note_diff_add_change (canvas_note, MidiModel::NoteDiffCommand::Length, len);
			}
		}

		if (!at_front) {
			Evoral::Beats len = std::max(Evoral::Beats(1 / 512.0),
						     x_beats - canvas_note->note()->time() - (sign * snap_delta_beats));
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
		delta = Evoral::Beats(trackview.session()->tempo_map().meter_at_frame (ref_point).divisions_per_bar());

	} else if (trackview.editor().snap_mode() == Editing::SnapOff) {

		/* grid is off - use nudge distance */

		framepos_t       unused;
		const framecnt_t distance = trackview.editor().get_nudge_distance (ref_point, unused);
		delta = region_frames_to_region_beats (fabs ((double)distance));

	} else {

		/* use grid */

		MusicFrame next_pos (ref_point, 0);
		if (forward) {
			if (max_framepos - 1 < next_pos.frame) {
				next_pos.frame += 1;
			}
		} else {
			if (next_pos.frame == 0) {
				return;
			}
			next_pos.frame -= 1;
		}

		trackview.editor().snap_to (next_pos, (forward ? RoundUpAlways : RoundDownAlways), false);
		const framecnt_t distance = ref_point - next_pos.frame;
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
	_entered_note = ev;

	Editor* editor = dynamic_cast<Editor*>(&trackview.editor());

	if (_mouse_state == SelectTouchDragging) {

		note_selected (ev, true);

	} else if (editor->current_mouse_mode() == MouseContent) {

		remove_ghost_note ();
		show_verbose_cursor (ev->note ());

	} else if (editor->current_mouse_mode() == MouseDraw) {

		remove_ghost_note ();
		show_verbose_cursor (ev->note ());
	}
}

void
MidiRegionView::note_left (NoteBase*)
{
	_entered_note = 0;

	for (Selection::iterator i = _selection.begin(); i != _selection.end(); ++i) {
		(*i)->hide_velocity ();
	}

	hide_verbose_cursor ();
}

void
MidiRegionView::patch_entered (PatchChange* p)
{
	ostringstream s;
	s << _("Bank ") << (p->patch()->bank() + MIDI_BP_ZERO) << '\n'
	  << instrument_info().get_patch_name_without (p->patch()->bank(), p->patch()->program(), p->patch()->channel()) << '\n'
	  << _("Channel ") << ((int) p->patch()->channel() + 1);
	show_verbose_cursor (s.str(), 10, 20);
	p->item().grab_focus();
}

void
MidiRegionView::patch_left (PatchChange *)
{
	hide_verbose_cursor ();
	/* focus will transfer back via the enter-notify event sent to this
	 * midi region view.
	 */
}

void
MidiRegionView::sysex_entered (SysEx* p)
{
	// ostringstream s;
	// CAIROCANVAS
	// need a way to extract text from p->_flag->_text
	// s << p->text();
	// show_verbose_cursor (s.str(), 10, 20);
	p->item().grab_focus();
}

void
MidiRegionView::sysex_left (SysEx *)
{
	hide_verbose_cursor ();
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
		return UIConfiguration::instance().color_mod ("selected region base", mod_name);
	} else if ((!UIConfiguration::instance().get_show_name_highlight() || high_enough_for_name) &&
	           !UIConfiguration::instance().get_color_regions_using_track_color()) {
		return UIConfiguration::instance().color_mod ("midi frame base", mod_name);
	}
	return UIConfiguration::instance().color_mod (fill_color, mod_name);
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
		i->second->on_channel_selection_change (mask);
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
MidiRegionView::paste (framepos_t pos, const ::Selection& selection, PasteContext& ctx, const int32_t sub_num)
{
	bool commit = false;
	// Paste notes, if available
	MidiNoteSelection::const_iterator m = selection.midi_notes.get_nth(ctx.counts.n_notes());
	if (m != selection.midi_notes.end()) {
		ctx.counts.increase_n_notes();
		if (!(*m)->empty()) {
			commit = true;
		}
		paste_internal(pos, ctx.count, ctx.times, **m);
	}

	// Paste control points to automation children, if available
	typedef RouteTimeAxisView::AutomationTracks ATracks;
	const ATracks& atracks = midi_view()->automation_tracks();
	for (ATracks::const_iterator a = atracks.begin(); a != atracks.end(); ++a) {
		if (a->second->paste(pos, selection, ctx, sub_num)) {
			if(!commit) {
				trackview.editor().begin_reversible_command (Operations::paste);
			}
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
	const Evoral::Beats quarter_note     = absolute_frames_to_source_beats(pos) + paste_offset;
	Evoral::Beats       end_point     = Evoral::Beats();

	DEBUG_TRACE (DEBUG::CutNPaste, string_compose ("Paste data spans from %1 to %2 (%3) ; paste pos beats = %4 (based on %5 - %6)\n",
	                                               first_time,
	                                               last_time,
	                                               duration, pos, _region->position(),
	                                               quarter_note));

	clear_editor_note_selection ();

	for (int n = 0; n < (int) times; ++n) {

		for (Notes::const_iterator i = mcb.notes().begin(); i != mcb.notes().end(); ++i) {

			boost::shared_ptr<NoteType> copied_note (new NoteType (*((*i).get())));
			copied_note->set_time (quarter_note + copied_note->time() - first_time);
			copied_note->set_id (Evoral::next_event_id());

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
		/* we probably need to get the snap modifier somehow to make this correct for non-musical use */
		_region->set_length (end_frame - _region->position(), trackview.editor().get_grid_music_divisions (0));
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
MidiRegionView::goto_next_note (bool add_to_selection)
{
	bool use_next = false;

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	uint16_t const channel_mask = mtv->midi_track()->get_playback_channel_mask();
	NoteBase* first_note = 0;

	MidiModel::ReadLock lock(_model->read_lock());
	MidiModel::Notes& notes (_model->notes());

	for (MidiModel::Notes::iterator n = notes.begin(); n != notes.end(); ++n) {
		NoteBase* cne = 0;
		if ((cne = find_canvas_note (*n))) {

			if (!first_note && (channel_mask & (1 << (*n)->channel()))) {
				first_note = cne;
			}

			if (cne->selected()) {
				use_next = true;
				continue;
			} else if (use_next) {
				if (channel_mask & (1 << (*n)->channel())) {
					if (!add_to_selection) {
						unique_select (cne);
					} else {
						note_selected (cne, true, false);
					}

					return;
				}
			}
		}
	}

	/* use the first one */

	if (!_events.empty() && first_note) {
		unique_select (first_note);
	}
}

void
MidiRegionView::goto_previous_note (bool add_to_selection)
{
	bool use_next = false;

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	uint16_t const channel_mask = mtv->midi_track()->get_playback_channel_mask ();
	NoteBase* last_note = 0;

	MidiModel::ReadLock lock(_model->read_lock());
	MidiModel::Notes& notes (_model->notes());

	for (MidiModel::Notes::reverse_iterator n = notes.rbegin(); n != notes.rend(); ++n) {
		NoteBase* cne = 0;
		if ((cne = find_canvas_note (*n))) {

			if (!last_note && (channel_mask & (1 << (*n)->channel()))) {
				last_note = cne;
			}

			if (cne->selected()) {
				use_next = true;
				continue;

			} else if (use_next) {
				if (channel_mask & (1 << (*n)->channel())) {
					if (!add_to_selection) {
						unique_select (cne);
					} else {
						note_selected (cne, true, false);
					}

					return;
				}
			}
		}
	}

	/* use the last one */

	if (!_events.empty() && last_note) {
		unique_select (last_note);
	}
}

void
MidiRegionView::selection_as_notelist (Notes& selected, bool allow_all_if_none_selected)
{
	bool had_selected = false;

	/* we previously time sorted events here, but Notes is a multiset sorted by time */

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		if (i->second->selected()) {
			selected.insert (i->first);
			had_selected = true;
		}
	}

	if (allow_all_if_none_selected && !had_selected) {
		for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
			selected.insert (i->first);
		}
	}
}

void
MidiRegionView::update_ghost_note (double x, double y, uint32_t state)
{
	x = std::max(0.0, x);

	MidiTimeAxisView* const mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);

	_last_ghost_x = x;
	_last_ghost_y = y;

	_note_group->canvas_to_item (x, y);

	PublicEditor& editor = trackview.editor ();

	framepos_t const unsnapped_frame = editor.pixel_to_sample (x);

	const int32_t divisions = editor.get_grid_music_divisions (state);
	const bool shift_snap = midi_view()->note_mode() != Percussive;
	const Evoral::Beats snapped_beats = snap_frame_to_grid_underneath (unsnapped_frame, divisions, shift_snap);

	/* prevent Percussive mode from displaying a ghost hit at region end */
	if (!shift_snap && snapped_beats >= midi_region()->start_beats() + midi_region()->length_beats()) {
		_ghost_note->hide();
		hide_verbose_cursor ();
		return;
	}

	/* ghost note may have been snapped before region */
	if (_ghost_note && snapped_beats.to_double() < 0.0) {
		_ghost_note->hide();
		return;

	} else if (_ghost_note) {
		_ghost_note->show();
	}

	/* calculate time in beats relative to start of source */
	const Evoral::Beats length = get_grid_beats(unsnapped_frame + _region->position());

	_ghost_note->note()->set_time (snapped_beats);
	_ghost_note->note()->set_length (length);
	_ghost_note->note()->set_note (y_to_note (y));
	_ghost_note->note()->set_channel (mtv->get_channel_for_add ());
	_ghost_note->note()->set_velocity (get_velocity_for_add (snapped_beats));
	/* the ghost note does not appear in ghost regions, so pass false in here */
	update_note (_ghost_note, false);

	show_verbose_cursor (_ghost_note->note ());
}

void
MidiRegionView::create_ghost_note (double x, double y, uint32_t state)
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
	update_ghost_note (x, y, state);
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
MidiRegionView::hide_verbose_cursor ()
{
	trackview.editor().verbose_cursor()->hide ();
	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	if (mtv) {
		mtv->set_note_highlight (NO_MIDI_NOTE);
	}
}

void
MidiRegionView::snap_changed ()
{
	if (!_ghost_note) {
		return;
	}

	create_ghost_note (_last_ghost_x, _last_ghost_y, 0);
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

	double note = y_to_note(y);
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
		if (_selection.insert (i->second).second) {
			i->second->set_selected (true);
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

	_patch_change_outline = UIConfiguration::instance().color ("midi patch change outline");
	_patch_change_fill = UIConfiguration::instance().color_mod ("midi patch change fill", "midi patch change fill");

	for (Events::iterator i = _events.begin(); i != _events.end(); ++i) {
		i->second->set_selected (i->second->selected()); // will change color
	}

	/* XXX probably more to do here */
}

void
MidiRegionView::enable_display (bool yn)
{
	RegionView::enable_display (yn);
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
		_step_edit_cursor->set_x1 (_step_edit_cursor->x0() + trackview.editor().sample_to_pixel (
						   region_beats_to_region_frames (_step_edit_cursor_position + beats)
						   - region_beats_to_region_frames (_step_edit_cursor_position)));
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
		const Evoral::Event<MidiBuffer::TimeType>& ev = *i;

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
	/* We used to eparent the note group to the region view's parent, so that it didn't change.
	   now we update it.
	*/
}

void
MidiRegionView::trim_front_ending ()
{
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

std::string
MidiRegionView::get_note_name (boost::shared_ptr<NoteType> n, uint8_t note_value) const
{
	using namespace MIDI::Name;
	std::string name;

	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	if (mtv) {
		boost::shared_ptr<MasterDeviceNames> device_names(mtv->get_device_names());
		if (device_names) {
			MIDI::Name::PatchPrimaryKey patch_key;
			get_patch_key_at(n->time(), n->channel(), patch_key);
			name = device_names->note_name(mtv->gui_property(X_("midnam-custom-device-mode")),
			                               n->channel(),
			                               patch_key.bank(),
			                               patch_key.program(),
			                               note_value);
		}
	}

	char buf[128];
	snprintf (buf, sizeof (buf), "%d %s\nCh %d Vel %d",
	          (int) note_value,
	          name.empty() ? ParameterDescriptor::midi_note_name (note_value).c_str() : name.c_str(),
	          (int) n->channel() + 1,
	          (int) n->velocity());

	return buf;
}

void
MidiRegionView::show_verbose_cursor_for_new_note_value(boost::shared_ptr<NoteType> current_note,
                                                       uint8_t new_value) const
{
	MidiTimeAxisView* mtv = dynamic_cast<MidiTimeAxisView*>(&trackview);
	if (mtv) {
		mtv->set_note_highlight (new_value);
	}

	show_verbose_cursor(get_note_name(current_note, new_value), 10, 20);
}

void
MidiRegionView::show_verbose_cursor (boost::shared_ptr<NoteType> n) const
{
	show_verbose_cursor_for_new_note_value(n, n->note());
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
 *  @param divisions beat division to snap given by Editor::get_grid_music_divisions() where
 *  bar is -1, 0 is audio samples and a positive integer is beat subdivisions.
 *  @return beat duration of p snapped to the grid subdivision underneath it.
 */
Evoral::Beats
MidiRegionView::snap_frame_to_grid_underneath (framepos_t p, int32_t divisions, bool shift_snap) const
{
	TempoMap& map (trackview.session()->tempo_map());
	double eqaf = map.exact_qn_at_frame (p + _region->position(), divisions);

	if (divisions != 0 && shift_snap) {
		const double qaf = map.quarter_note_at_frame (p + _region->position());
		/* Hack so that we always snap to the note that we are over, instead of snapping
		   to the next one if we're more than halfway through the one we're over.
		*/
		const Evoral::Beats grid_beats = get_grid_beats (p + _region->position());
		const double rem = eqaf - qaf;
		if (rem >= 0.0) {
			eqaf -= grid_beats.to_double();
		}
	}
	const double session_start_off = _region->quarter_note() - midi_region()->start_beats();

	return Evoral::Beats (eqaf - session_start_off);
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
	Evoral::Beats beats   = editor.get_grid_type_as_beats (success, pos);
	if (!success) {
		beats = Evoral::Beats(1);
	}
	return beats;
}
uint8_t
MidiRegionView::y_to_note (double y) const
{
	int const n = ((contents_height() - y) / contents_height() * (double)(_current_range_max - _current_range_min + 1))
		+ _current_range_min;

	if (n < 0) {
		return 0;
	} else if (n > 127) {
		return 127;
	}

	/* min due to rounding and/or off-by-one errors */
	return min ((uint8_t) n, _current_range_max);
}

double
MidiRegionView::note_to_y(uint8_t note) const
{
	return contents_height() - (note + 1 - _current_range_min) * note_height() + 1;
}

double
MidiRegionView::session_relative_qn (double qn) const
{
	return qn + (region()->quarter_note() - midi_region()->start_beats());
}
