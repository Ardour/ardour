/*
 * Copyright (C) 2023 Paul Davis <paul@linuxaudiosystems.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>

#include "pbd/error.h"
#include "pbd/stacktrace.h"
#include "pbd/unwind.h"

#include "ardour/legatize.h"
#include "ardour/midi_region.h"
#include "ardour/midi_source.h"
#include "ardour/rc_configuration.h"
#include "ardour/transpose.h"
#include "ardour/quantize.h"

#include "gtkmm2ext/bindings.h"

#include "widgets/tooltips.h"

#include "actions.h"
#include "ardour_ui.h"
#include "automation_line.h"
#include "control_point.h"
#include "edit_note_dialog.h"
#include "editing_context.h"
#include "editing_convert.h"
#include "editor_drag.h"
#include "grid_lines.h"
#include "gui_thread.h"
#include "keyboard.h"
#include "midi_region_view.h"
#include "note_base.h"
#include "quantize_dialog.h"
#include "rc_option_editor.h"
#include "selection.h"
#include "selection_memento.h"
#include "transform_dialog.h"
#include "transpose_dialog.h"
#include "verbose_cursor.h"

#include "pbd/i18n.h"

using namespace ARDOUR;
using namespace Editing;
using namespace Glib;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace PBD;
using namespace Temporal;
using namespace ArdourWidgets;
using std::string;

sigc::signal<void> EditingContext::DropDownKeys;
Gtkmm2ext::Bindings* EditingContext::button_bindings = nullptr;
Glib::RefPtr<Gtk::ActionGroup> EditingContext::_midi_actions;
Glib::RefPtr<Gtk::ActionGroup> EditingContext::_common_actions;
std::vector<std::string> EditingContext::grid_type_strings;
MouseCursors* EditingContext::_cursors = nullptr;
EditingContext* EditingContext::_current_editing_context = nullptr;

static const gchar *_grid_type_strings[] = {
	N_("No Grid"),
	N_("Bar"),
	N_("1/4 Note"),
	N_("1/8 Note"),
	N_("1/16 Note"),
	N_("1/32 Note"),
	N_("1/64 Note"),
	N_("1/128 Note"),
	N_("1/3 (8th triplet)"), // or "1/12" ?
	N_("1/6 (16th triplet)"),
	N_("1/12 (32nd triplet)"),
	N_("1/24 (64th triplet)"),
	N_("1/5 (8th quintuplet)"),
	N_("1/10 (16th quintuplet)"),
	N_("1/20 (32nd quintuplet)"),
	N_("1/7 (8th septuplet)"),
	N_("1/14 (16th septuplet)"),
	N_("1/28 (32nd septuplet)"),
	N_("Timecode"),
	N_("MinSec"),
	N_("CD Frames"),
	0
};


static const gchar *_zoom_focus_strings[] = {
	N_("Left"),
	N_("Right"),
	N_("Center"),
	N_("Playhead"),
	N_("Mouse"),
	N_("Edit point"),
	0
};

Editing::GridType EditingContext::_draw_length (GridTypeNone);
int EditingContext::_draw_velocity (DRAW_VEL_AUTO);
int EditingContext::_draw_channel (DRAW_CHAN_AUTO);
sigc::signal<void> EditingContext::DrawLengthChanged;
sigc::signal<void> EditingContext::DrawVelocityChanged;
sigc::signal<void> EditingContext::DrawChannelChanged;

Glib::RefPtr<Gtk::Action> EditingContext::undo_action;
Glib::RefPtr<Gtk::Action> EditingContext::redo_action;
Glib::RefPtr<Gtk::Action> EditingContext::alternate_redo_action;
Glib::RefPtr<Gtk::Action> EditingContext::alternate_alternate_redo_action;

EditingContext::EditingContext (std::string const & name)
	: rubberband_rect (0)
	, _name (name)
	, within_track_canvas (false)
	, pre_internal_grid_type (GridTypeBeat)
	, pre_internal_snap_mode (SnapOff)
	, internal_grid_type (GridTypeBeat)
	, internal_snap_mode (SnapOff)
	, _grid_type (GridTypeBeat)
	, _snap_mode (SnapOff)
	, _timeline_origin (0.)
	, play_note_selection_button (ArdourButton::default_elements)
	, follow_playhead_button (_("F"), ArdourButton::Text, true)
	, visible_channel_label (_("MIDI Channel"))
	, _drags (new DragManager (this))
	, _leftmost_sample (0)
	, _playhead_cursor (nullptr)
	, _snapped_cursor (nullptr)
	, _follow_playhead (false)
	, selection (new Selection (this, true))
	, cut_buffer (new Selection (this, false))
	, _selection_memento (new SelectionMemento())
	, _verbose_cursor (nullptr)
	, samples_per_pixel (2048)
	, _zoom_focus (ZoomFocusPlayhead)
	, bbt_ruler_scale (bbt_show_many)
	, bbt_bars (0)
	, bbt_bar_helper_on (0)
	, _track_canvas_width (0)
	, _visible_canvas_width (0)
	, _visible_canvas_height (0)
	, quantize_dialog (nullptr)
	, vertical_adjustment (0.0, 0.0, 10.0, 400.0)
	, horizontal_adjustment (0.0, 0.0, 1e16)
	, bindings (nullptr)
	, mouse_mode (MouseObject)
	, visual_change_queued (false)
	, autoscroll_horizontal_allowed (false)
	, autoscroll_vertical_allowed (false)
	, autoscroll_cnt (0)
	, _mouse_changed_selection (false)
	, entered_marker (nullptr)
	, entered_track (nullptr)
	, entered_regionview (nullptr)
	, clear_entered_track (false)
	, grid_lines (nullptr)
	, time_line_group (nullptr)
{
	using namespace Gtk::Menu_Helpers;

	if (!button_bindings) {
		button_bindings = new Bindings ("editor-mouse");

		XMLNode* node = button_settings();
		if (node) {
			for (XMLNodeList::const_iterator i = node->children().begin(); i != node->children().end(); ++i) {
				button_bindings->load_operation (**i);
			}
		}
	}

	if (grid_type_strings.empty()) {
		grid_type_strings =  I18N (_grid_type_strings);
	}

	if (zoom_focus_strings.empty()) {
		zoom_focus_strings = I18N (_zoom_focus_strings);
	}

	zoom_focus_selector.set_name ("zoom button");

	snap_mode_button.set_text (_("Snap"));
	snap_mode_button.set_name ("mouse mode button");
	snap_mode_button.signal_button_press_event().connect (sigc::mem_fun (*this, &EditingContext::snap_mode_button_clicked), false);

	if (!_cursors) {
		_cursors = new MouseCursors;
		_cursors->set_cursor_set (UIConfiguration::instance().get_icon_set());
		std::cerr << "Set cursor set to " << UIConfiguration::instance().get_icon_set() << std::endl;
	}

	DrawLengthChanged.connect (sigc::mem_fun (*this, &EditingContext::draw_length_changed));
	DrawVelocityChanged.connect (sigc::mem_fun (*this, &EditingContext::draw_velocity_changed));
	DrawChannelChanged.connect (sigc::mem_fun (*this, &EditingContext::draw_channel_changed));

	set_tooltip (draw_length_selector, _("Note Length to Draw (AUTO uses the current Grid setting)"));
	set_tooltip (draw_velocity_selector, _("Note Velocity to Draw (AUTO uses the nearest note's velocity)"));
	set_tooltip (draw_channel_selector, _("Note Channel to Draw (AUTO uses the nearest note's channel)"));
	set_tooltip (grid_type_selector, _("Grid Mode"));
	set_tooltip (snap_mode_button, _("Snap Mode\n\nRight-click to visit Snap preferences."));
	set_tooltip (zoom_focus_selector, _("Zoom Focus"));

	set_tooltip (play_note_selection_button, _("Play notes when selected"));
	set_tooltip (note_mode_button, _("Switch between sustained and percussive mode"));
	set_tooltip (follow_playhead_button, _("Scroll automatically to keep playhead visible"));
	/* Leave tip for full zoom button to derived class */
	set_tooltip (visible_channel_selector, _("Select visible MIDI channel"));

	play_note_selection_button.signal_clicked.connect (sigc::mem_fun (*this, &EditingContext::play_note_selection_clicked));
	note_mode_button.signal_clicked.connect (sigc::mem_fun (*this, &EditingContext::note_mode_clicked));
	follow_playhead_button.signal_clicked.connect (sigc::mem_fun (*this, &EditingContext::follow_playhead_clicked));
	full_zoom_button.signal_clicked.connect (sigc::mem_fun (*this, &EditingContext::full_zoom_clicked));

	zoom_in_button.set_name ("zoom button");
	zoom_in_button.set_icon (ArdourIcon::ZoomIn);

	zoom_out_button.set_name ("zoom button");
	zoom_out_button.set_icon (ArdourIcon::ZoomOut);

	full_zoom_button.set_name ("zoom button");
	full_zoom_button.set_icon (ArdourIcon::ZoomFull);

	selection->PointsChanged.connect (sigc::mem_fun(*this, &EditingContext::point_selection_changed));

	for (int i = 0; i < 16; i++) {
		char buf[4];
		sprintf(buf, "%d", i+1);
		visible_channel_selector.AddMenuElem (MenuElem (buf, [this,i]() { set_visible_channel (i); }));
	}

	/* handle escape */

	ARDOUR_UI::instance()->Escape.connect (escape_connection, MISSING_INVALIDATOR, std::bind (&EditingContext::escape, this), gui_context());

	Config->ParameterChanged.connect (parameter_connections, MISSING_INVALIDATOR, std::bind (&EditingContext::parameter_changed, this, _1), gui_context());
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &EditingContext::ui_parameter_changed));

	std::function<void (string)> pc (std::bind (&EditingContext::ui_parameter_changed, this, _1));
	UIConfiguration::instance().map_parameters (pc);
}

EditingContext::~EditingContext()
{
	delete grid_lines;
}

void
EditingContext::ui_parameter_changed (string parameter)
{
	if (parameter == "sound-midi-notes") {
		if (UIConfiguration::instance().get_sound_midi_notes()) {
			play_note_selection_button.set_active_state (Gtkmm2ext::ExplicitActive);
		} else {
			play_note_selection_button.set_active_state (Gtkmm2ext::Off);
		}
	}
}


void
EditingContext::parameter_changed (string parameter)
{
}

void
EditingContext::set_session (ARDOUR::Session* s)
{
	SessionHandlePtr::set_session (s);
}

void
EditingContext::set_selected_midi_region_view (MidiRegionView& mrv)
{
	/* clear note selection in all currently selected MidiRegionViews */

	if (get_selection().regions.contains (&mrv) && get_selection().regions.size() == 1) {
		/* Nothing to do */
		return;
	}

	midi_action (&MidiRegionView::clear_note_selection);
	get_selection().set (&mrv);
}

void
EditingContext::register_common_actions (Bindings* common_bindings)
{
	if (_common_actions) {
		return;
	}

	_common_actions = ActionManager::create_action_group (common_bindings, X_("Editing"));

	reg_sens (_common_actions, "temporal-zoom-out", _("Zoom Out"), []() { current_editing_context()->temporal_zoom_step (true); });
	reg_sens (_common_actions, "temporal-zoom-in", _("Zoom In"), []() { current_editing_context()->temporal_zoom_step (false); });

	undo_action = reg_sens (_common_actions, "undo", S_("Command|Undo"), []() { current_editing_context()->undo(1U) ; });
	redo_action = reg_sens (_common_actions, "redo", _("Redo"), []() { current_editing_context()->redo(1U) ; });
	alternate_redo_action = reg_sens (_common_actions, "alternate-redo", _("Redo"), []() { current_editing_context()->redo(1U) ; });
	alternate_alternate_redo_action = reg_sens (_common_actions, "alternate-alternate-redo", _("Redo"), []() { current_editing_context()->redo(1U) ; });

	reg_sens (_common_actions, "editor-delete", _("Delete"), []() { current_editing_context()->delete_() ; });
	reg_sens (_common_actions, "alternate-editor-delete", _("Delete"), []() { current_editing_context()->delete_() ; });

	reg_sens (_common_actions, "editor-cut", _("Cut"), []() { current_editing_context()->cut() ; });
	reg_sens (_common_actions, "editor-copy", _("Copy"), []() { current_editing_context()->copy() ; });
	reg_sens (_common_actions, "editor-paste", _("Paste"), []() { current_editing_context()->keyboard_paste() ; });

	RadioAction::Group mouse_mode_group;

	ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-object", _("Grab (Object Tool)"), []() { current_editing_context()->mouse_mode_toggled (Editing::MouseObject); });
	ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-range", _("Range Tool"), []() { current_editing_context()->mouse_mode_toggled (Editing::MouseRange); });
	ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-draw", _("Note Drawing Tool"), []() { current_editing_context()->mouse_mode_toggled (Editing::MouseDraw); });
	ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-timefx", _("Time FX Tool"), []() { current_editing_context()->mouse_mode_toggled (Editing::MouseTimeFX); });
	ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-grid", _("Grid Tool"), []() { current_editing_context()->mouse_mode_toggled (Editing::MouseGrid); });
	ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-content", _("Internal Edit (Content Tool)"), []() { current_editing_context()->mouse_mode_toggled (Editing::MouseContent); });
	ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-cut", _("Cut Tool"), []() { current_editing_context()->mouse_mode_toggled (Editing::MouseCut); });

	Glib::RefPtr<ActionGroup> zoom_actions = ActionManager::create_action_group (common_bindings, X_("Zoom"));
	RadioAction::Group zoom_group;

	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-left", _("Zoom Focus Left"), []() { current_editing_context()->zoom_focus_chosen (Editing::ZoomFocusLeft); });
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-right", _("Zoom Focus Right"), []() { current_editing_context()->zoom_focus_chosen (Editing::ZoomFocusRight); });
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-center", _("Zoom Focus Center"), []() { current_editing_context()->zoom_focus_chosen (Editing::ZoomFocusCenter); });
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-playhead", _("Zoom Focus Playhead"), []() { current_editing_context()->zoom_focus_chosen (Editing::ZoomFocusPlayhead); });
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-mouse", _("Zoom Focus Mouse"), []() { current_editing_context()->zoom_focus_chosen (Editing::ZoomFocusMouse); });
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-edit", _("Zoom Focus Edit Point"), []() { current_editing_context()->zoom_focus_chosen (Editing::ZoomFocusEdit); });

	ActionManager::register_action (zoom_actions, X_("cycle-zoom-focus"), _("Next Zoom Focus"), []() { current_editing_context()->cycle_zoom_focus(); });
}

void
EditingContext::register_midi_actions (Bindings* midi_bindings)
{
	/* These actions are all singletons, defined globally for all EditingContexts */

	if (_midi_actions) {
		return;
	}

	_midi_actions = ActionManager::create_action_group (midi_bindings, X_("Notes"));

	/* two versions to allow same action for Delete and Backspace */

	ActionManager::register_action (_midi_actions, X_("clear-selection"), _("Clear Note Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::clear_note_selection); });
	ActionManager::register_action (_midi_actions, X_("invert-selection"), _("Invert Note Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::invert_selection); });
	ActionManager::register_action (_midi_actions, X_("extend-selection"), _("Extend Note Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::extend_selection); });
	ActionManager::register_action (_midi_actions, X_("duplicate-selection"), _("Duplicate Note Selection"), []() { current_editing_context()->midi_action (&MidiRegionView::duplicate_selection); });

	/* Lengthen */

	ActionManager::register_action (_midi_actions, X_("move-starts-earlier-fine"), _("Move Note Start Earlier (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_starts_earlier_fine); });
	ActionManager::register_action (_midi_actions, X_("move-starts-earlier"), _("Move Note Start Earlier"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_starts_earlier); });
	ActionManager::register_action (_midi_actions, X_("move-ends-later-fine"), _("Move Note Ends Later (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_ends_later_fine); });
	ActionManager::register_action (_midi_actions, X_("move-ends-later"), _("Move Note Ends Later"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_ends_later); });

	/* Shorten */

	ActionManager::register_action (_midi_actions, X_("move-starts-later-fine"), _("Move Note Start Later (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_starts_later_fine); });
	ActionManager::register_action (_midi_actions, X_("move-starts-later"), _("Move Note Start Later"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_starts_later); });
	ActionManager::register_action (_midi_actions, X_("move-ends-earlier-fine"), _("Move Note Ends Earlier (fine)"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_ends_earlier_fine); });
	ActionManager::register_action (_midi_actions, X_("move-ends-earlier"), _("Move Note Ends Earlier"), []() { current_editing_context()->midi_action (&MidiRegionView::move_note_ends_earlier); });


	/* Alt versions allow bindings for both Tab and ISO_Left_Tab, if desired */

	ActionManager::register_action (_midi_actions, X_("select-next"), _("Select Next"), []() { current_editing_context()->midi_action (&MidiView::select_next_note); });
	ActionManager::register_action (_midi_actions, X_("alt-select-next"), _("Select Next (alternate)"), []() { current_editing_context()->midi_action (&MidiView::select_next_note); });
	ActionManager::register_action (_midi_actions, X_("select-previous"), _("Select Previous"), []() { current_editing_context()->midi_action (&MidiView::select_previous_note); });
	ActionManager::register_action (_midi_actions, X_("alt-select-previous"), _("Select Previous (alternate)"), []() { current_editing_context()->midi_action (&MidiView::select_previous_note); });
	ActionManager::register_action (_midi_actions, X_("add-select-next"), _("Add Next to Selection"), []() { current_editing_context()->midi_action (&MidiView::add_select_next_note); });
	ActionManager::register_action (_midi_actions, X_("alt-add-select-next"), _("Add Next to Selection (alternate)"), []() { current_editing_context()->midi_action (&MidiView::add_select_next_note); });
	ActionManager::register_action (_midi_actions, X_("add-select-previous"), _("Add Previous to Selection"), []() { current_editing_context()->midi_action (&MidiView::add_select_previous_note); });
	ActionManager::register_action (_midi_actions, X_("alt-add-select-previous"), _("Add Previous to Selection (alternate)"), []() { current_editing_context()->midi_action (&MidiView::add_select_previous_note); });

	ActionManager::register_action (_midi_actions, X_("increase-velocity"), _("Increase Velocity"), []() { current_editing_context()->midi_action (&MidiView::increase_note_velocity); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine"), _("Increase Velocity (fine)"), []() { current_editing_context()->midi_action (&MidiView::increase_note_velocity_fine); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-smush"), _("Increase Velocity (allow mush)"), []() { current_editing_context()->midi_action (&MidiView::increase_note_velocity_smush); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-together"), _("Increase Velocity (non-relative)"), []() { current_editing_context()->midi_action (&MidiView::increase_note_velocity_together); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-smush"), _("Increase Velocity (fine, allow mush)"), []() { current_editing_context()->midi_action (&MidiView::increase_note_velocity_fine_smush); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-together"), _("Increase Velocity (fine, non-relative)"), []() { current_editing_context()->midi_action (&MidiView::increase_note_velocity_fine_together); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-smush-together"), _("Increase Velocity (maintain ratios, allow mush)"), []() { current_editing_context()->midi_action (&MidiView::increase_note_velocity_smush_together); });
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-smush-together"), _("Increase Velocity (fine, allow mush, non-relative)"), []() { current_editing_context()->midi_action (&MidiView::increase_note_velocity_fine_smush_together); });

	ActionManager::register_action (_midi_actions, X_("decrease-velocity"), _("Decrease Velocity"), []() { current_editing_context()->midi_action (&MidiView::decrease_note_velocity); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine"), _("Decrease Velocity (fine)"), []() { current_editing_context()->midi_action (&MidiView::decrease_note_velocity_fine); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-smush"), _("Decrease Velocity (allow mush)"), []() { current_editing_context()->midi_action (&MidiView::decrease_note_velocity_smush); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-together"), _("Decrease Velocity (non-relative)"), []() { current_editing_context()->midi_action (&MidiView::decrease_note_velocity_together); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-smush"), _("Decrease Velocity (fine, allow mush)"), []() { current_editing_context()->midi_action (&MidiView::decrease_note_velocity_fine_smush); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-together"), _("Decrease Velocity (fine, non-relative)"), []() { current_editing_context()->midi_action (&MidiView::decrease_note_velocity_fine_together); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-smush-together"), _("Decrease Velocity (maintain ratios, allow mush)"), []() { current_editing_context()->midi_action (&MidiView::decrease_note_velocity_smush_together); });
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-smush-together"), _("Decrease Velocity (fine, allow mush, non-relative)"), []() { current_editing_context()->midi_action (&MidiView::decrease_note_velocity_fine_smush_together); });

	ActionManager::register_action (_midi_actions, X_("transpose-up-octave"), _("Transpose Up (octave)"), []() { current_editing_context()->midi_action (&MidiView::transpose_up_octave); });
	ActionManager::register_action (_midi_actions, X_("transpose-up-octave-smush"), _("Transpose Up (octave, allow mush)"), []() { current_editing_context()->midi_action (&MidiView::transpose_up_octave_smush); });
	ActionManager::register_action (_midi_actions, X_("transpose-up-semitone"), _("Transpose Up (semitone)"), []() { current_editing_context()->midi_action (&MidiView::transpose_up_tone); });
	ActionManager::register_action (_midi_actions, X_("transpose-up-semitone-smush"), _("Transpose Up (semitone, allow mush)"), []() { current_editing_context()->midi_action (&MidiView::transpose_up_octave_smush); });

	ActionManager::register_action (_midi_actions, X_("transpose-down-octave"), _("Transpose Down (octave)"), []() { current_editing_context()->midi_action (&MidiView::transpose_down_octave); });
	ActionManager::register_action (_midi_actions, X_("transpose-down-octave-smush"), _("Transpose Down (octave, allow mush)"), []() { current_editing_context()->midi_action (&MidiView::transpose_down_octave_smush); });
	ActionManager::register_action (_midi_actions, X_("transpose-down-semitone"), _("Transpose Down (semitone)"), []() { current_editing_context()->midi_action (&MidiView::transpose_down_tone); });
	ActionManager::register_action (_midi_actions, X_("transpose-down-semitone-smush"), _("Transpose Down (semitone, allow mush)"), []() { current_editing_context()->midi_action (&MidiView::transpose_down_octave_smush); });

	ActionManager::register_action (_midi_actions, X_("nudge-later"), _("Nudge Notes Later (grid)"), []() { current_editing_context()->midi_action (&MidiView::nudge_notes_later); });
	ActionManager::register_action (_midi_actions, X_("nudge-later-fine"), _("Nudge Notes Later (1/4 grid)"), []() { current_editing_context()->midi_action (&MidiView::nudge_notes_later_fine); });
	ActionManager::register_action (_midi_actions, X_("nudge-earlier"), _("Nudge Notes Earlier (grid)"), []() { current_editing_context()->midi_action (&MidiView::nudge_notes_earlier); });
	ActionManager::register_action (_midi_actions, X_("nudge-earlier-fine"), _("Nudge Notes Earlier (1/4 grid)"), []() { current_editing_context()->midi_action (&MidiView::nudge_notes_earlier_fine); });

	ActionManager::register_action (_midi_actions, X_("split-notes-grid"), _("Split Selected Notes on grid boundaries"), []() { current_editing_context()->midi_action (&MidiView::split_notes_grid); });
	ActionManager::register_action (_midi_actions, X_("split-notes-more"), _("Split Selected Notes into more pieces"), []() { current_editing_context()->midi_action (&MidiView::split_notes_more); });
	ActionManager::register_action (_midi_actions, X_("split-notes-less"), _("Split Selected Notes into less pieces"), []() { current_editing_context()->midi_action (&MidiView::split_notes_less); });
	ActionManager::register_action (_midi_actions, X_("join-notes"), _("Join Selected Notes"), []() { current_editing_context()->midi_action (&MidiView::join_notes); });

	ActionManager::register_action (_midi_actions, X_("edit-channels"), _("Edit Note Channels"), []() { current_editing_context()->midi_action (&MidiView::channel_edit); });
	ActionManager::register_action (_midi_actions, X_("edit-velocities"), _("Edit Note Velocities"), []() { current_editing_context()->midi_action (&MidiView::velocity_edit); });

	ActionManager::register_action (_midi_actions, X_("quantize-selected-notes"), _("Quantize Selected Notes"), []() { current_editing_context()->midi_action ( &MidiView::quantize_selected_notes); });

	Glib::RefPtr<ActionGroup> length_actions = ActionManager::create_action_group (midi_bindings, X_("DrawLength"));
	RadioAction::Group draw_length_group;

	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-thirtyseconds"),  grid_type_strings[(int)GridTypeBeatDiv32].c_str(), []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv32); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentyeighths"),  grid_type_strings[(int)GridTypeBeatDiv28].c_str(), []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv28); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentyfourths"),  grid_type_strings[(int)GridTypeBeatDiv24].c_str(), []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv24); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentieths"),     grid_type_strings[(int)GridTypeBeatDiv20].c_str(), []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv20); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-asixteenthbeat"), grid_type_strings[(int)GridTypeBeatDiv16].c_str(), []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv16); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-fourteenths"),    grid_type_strings[(int)GridTypeBeatDiv14].c_str(), []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv14); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twelfths"),       grid_type_strings[(int)GridTypeBeatDiv12].c_str(), []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv12); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-tenths"),         grid_type_strings[(int)GridTypeBeatDiv10].c_str(), []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv10); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-eighths"),        grid_type_strings[(int)GridTypeBeatDiv8].c_str(),  []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv8); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-sevenths"),       grid_type_strings[(int)GridTypeBeatDiv7].c_str(),  []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv7); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-sixths"),         grid_type_strings[(int)GridTypeBeatDiv6].c_str(),  []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv6); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-fifths"),         grid_type_strings[(int)GridTypeBeatDiv5].c_str(),  []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv5); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-quarters"),       grid_type_strings[(int)GridTypeBeatDiv4].c_str(),  []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv4); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-thirds"),         grid_type_strings[(int)GridTypeBeatDiv3].c_str(),  []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv3); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-halves"),         grid_type_strings[(int)GridTypeBeatDiv2].c_str(),  []() { EditingContext::draw_length_action_method (Editing::GridTypeBeatDiv2); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-beat"),           grid_type_strings[(int)GridTypeBeat].c_str(),      []() { EditingContext::draw_length_action_method (Editing::GridTypeBeat); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-bar"),            grid_type_strings[(int)GridTypeBar].c_str(),       []() { EditingContext::draw_length_action_method (Editing::GridTypeBar); });
	ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-auto"),           _("Auto"),                                         []() { EditingContext::draw_length_action_method (DRAW_LEN_AUTO); });

	Glib::RefPtr<ActionGroup> velocity_actions = ActionManager::create_action_group (midi_bindings, _("Draw Velocity"));
	RadioAction::Group draw_velocity_group;
	ActionManager::register_radio_action (velocity_actions, draw_velocity_group, X_("draw-velocity-auto"),  _("Auto"), []() { EditingContext::draw_velocity_action_method (DRAW_VEL_AUTO); });
	for (int i = 1; i <= 127; i++) {
		char buf[64];
		snprintf(buf, sizeof (buf), X_("draw-velocity-%d"), i);
		char vel[64];
		sprintf(vel, _("Velocity %d"), i);
		ActionManager::register_radio_action (velocity_actions, draw_velocity_group, buf, vel, [i]() { EditingContext::draw_velocity_action_method (i); });
	}

	Glib::RefPtr<ActionGroup> channel_actions = ActionManager::create_action_group (midi_bindings, _("Draw Channel"));
	RadioAction::Group draw_channel_group;
	ActionManager::register_radio_action (channel_actions, draw_channel_group, X_("draw-channel-auto"),  _("Auto"), []() { EditingContext::draw_channel_action_method (DRAW_CHAN_AUTO); });
	for (int i = 0; i <= 15; i++) {
		char buf[64];
		snprintf(buf, sizeof (buf), X_("draw-channel-%d"), i+1);
		char ch[64];
		sprintf(ch, X_("Channel %d"), i+1);
		ActionManager::register_radio_action (channel_actions, draw_channel_group, buf, ch, [i]() { EditingContext::draw_channel_action_method (i); });
	}

	ActionManager::set_sensitive (_midi_actions, false);
}

void
EditingContext::midi_action (void (MidiView::*method)())
{
	MidiRegionSelection ms = get_selection().midi_regions();

	if (ms.empty()) {
		return;
	}

	if (ms.size() > 1) {

		auto views = filter_to_unique_midi_region_views (ms);

		for (auto & mrv : views) {
			(mrv->*method) ();
		}

	} else {

		MidiRegionView* mrv = dynamic_cast<MidiRegionView*>(ms.front());

		if (mrv) {
			(mrv->*method)();
		}
	}
}

void
EditingContext::grid_type_selection_done (GridType gridtype)
{
	RefPtr<RadioAction> ract = grid_type_action (gridtype);
	if (ract && ract->get_active()) {  /*radio-action is already set*/
		set_grid_to(gridtype);         /*so we must set internal state here*/
	} else {
		ract->set_active ();
	}
}

void
EditingContext::snap_mode_selection_done (SnapMode mode)
{
	RefPtr<RadioAction> ract = snap_mode_action (mode);

	if (ract) {
		ract->set_active (true);
	}
}

RefPtr<RadioAction>
EditingContext::grid_type_action (GridType type)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (type) {
	case Editing::GridTypeBeatDiv32:
		action = "grid-type-thirtyseconds";
		break;
	case Editing::GridTypeBeatDiv28:
		action = "grid-type-twentyeighths";
		break;
	case Editing::GridTypeBeatDiv24:
		action = "grid-type-twentyfourths";
		break;
	case Editing::GridTypeBeatDiv20:
		action = "grid-type-twentieths";
		break;
	case Editing::GridTypeBeatDiv16:
		action = "grid-type-asixteenthbeat";
		break;
	case Editing::GridTypeBeatDiv14:
		action = "grid-type-fourteenths";
		break;
	case Editing::GridTypeBeatDiv12:
		action = "grid-type-twelfths";
		break;
	case Editing::GridTypeBeatDiv10:
		action = "grid-type-tenths";
		break;
	case Editing::GridTypeBeatDiv8:
		action = "grid-type-eighths";
		break;
	case Editing::GridTypeBeatDiv7:
		action = "grid-type-sevenths";
		break;
	case Editing::GridTypeBeatDiv6:
		action = "grid-type-sixths";
		break;
	case Editing::GridTypeBeatDiv5:
		action = "grid-type-fifths";
		break;
	case Editing::GridTypeBeatDiv4:
		action = "grid-type-quarters";
		break;
	case Editing::GridTypeBeatDiv3:
		action = "grid-type-thirds";
		break;
	case Editing::GridTypeBeatDiv2:
		action = "grid-type-halves";
		break;
	case Editing::GridTypeBeat:
		action = "grid-type-beat";
		break;
	case Editing::GridTypeBar:
		action = "grid-type-bar";
		break;
	case Editing::GridTypeNone:
		action = "grid-type-none";
		break;
	case Editing::GridTypeTimecode:
		action = "grid-type-timecode";
		break;
	case Editing::GridTypeCDFrame:
		action = "grid-type-cdframe";
		break;
	case Editing::GridTypeMinSec:
		action = "grid-type-minsec";
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible snap-to type", (int) type) << endmsg;
		abort(); /*NOTREACHED*/
	}

	std::string action_name = editor_name() + X_("Snap");
	act = ActionManager::get_action (action_name.c_str(), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "EditingContext::grid_type_chosen could not find action to match type.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

void
EditingContext::next_grid_choice ()
{
	switch (_grid_type) {
	case Editing::GridTypeBeatDiv32:
		set_grid_to (Editing::GridTypeNone);
		break;
	case Editing::GridTypeBeatDiv16:
		set_grid_to (Editing::GridTypeBeatDiv32);
		break;
	case Editing::GridTypeBeatDiv8:
		set_grid_to (Editing::GridTypeBeatDiv16);
		break;
	case Editing::GridTypeBeatDiv4:
		set_grid_to (Editing::GridTypeBeatDiv8);
		break;
	case Editing::GridTypeBeatDiv2:
		set_grid_to (Editing::GridTypeBeatDiv4);
		break;
	case Editing::GridTypeBeat:
		set_grid_to (Editing::GridTypeBeatDiv2);
		break;
	case Editing::GridTypeBar:
		set_grid_to (Editing::GridTypeBeat);
		break;
	case Editing::GridTypeNone:
		set_grid_to (Editing::GridTypeBar);
		break;
	case Editing::GridTypeBeatDiv3:
	case Editing::GridTypeBeatDiv6:
	case Editing::GridTypeBeatDiv12:
	case Editing::GridTypeBeatDiv24:
	case Editing::GridTypeBeatDiv5:
	case Editing::GridTypeBeatDiv10:
	case Editing::GridTypeBeatDiv20:
	case Editing::GridTypeBeatDiv7:
	case Editing::GridTypeBeatDiv14:
	case Editing::GridTypeBeatDiv28:
	case Editing::GridTypeTimecode:
	case Editing::GridTypeMinSec:
	case Editing::GridTypeCDFrame:
		break;  //do nothing
	}
}

void
EditingContext::prev_grid_choice ()
{
	switch (_grid_type) {
	case Editing::GridTypeBeatDiv32:
		set_grid_to (Editing::GridTypeBeatDiv16);
		break;
	case Editing::GridTypeBeatDiv16:
		set_grid_to (Editing::GridTypeBeatDiv8);
		break;
	case Editing::GridTypeBeatDiv8:
		set_grid_to (Editing::GridTypeBeatDiv4);
		break;
	case Editing::GridTypeBeatDiv4:
		set_grid_to (Editing::GridTypeBeatDiv2);
		break;
	case Editing::GridTypeBeatDiv2:
		set_grid_to (Editing::GridTypeBeat);
		break;
	case Editing::GridTypeBeat:
		set_grid_to (Editing::GridTypeBar);
		break;
	case Editing::GridTypeBar:
		set_grid_to (Editing::GridTypeNone);
		break;
	case Editing::GridTypeNone:
		set_grid_to (Editing::GridTypeBeatDiv32);
		break;
	case Editing::GridTypeBeatDiv3:
	case Editing::GridTypeBeatDiv6:
	case Editing::GridTypeBeatDiv12:
	case Editing::GridTypeBeatDiv24:
	case Editing::GridTypeBeatDiv5:
	case Editing::GridTypeBeatDiv10:
	case Editing::GridTypeBeatDiv20:
	case Editing::GridTypeBeatDiv7:
	case Editing::GridTypeBeatDiv14:
	case Editing::GridTypeBeatDiv28:
	case Editing::GridTypeTimecode:
	case Editing::GridTypeMinSec:
	case Editing::GridTypeCDFrame:
		break;  //do nothing
	}
}

void
EditingContext::grid_type_chosen (GridType type)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = grid_type_action (type);

	if (ract && ract->get_active()) {
		set_grid_to (type);
	}
}

void
EditingContext::draw_length_action_method (GridType type)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_length_action (type);

	if (ract && ract->get_active()) {
		/* It doesn't really matter which EditingContext executes this */
		current_editing_context()->set_draw_length_to (type);
	}
}

void
EditingContext::draw_length_chosen (GridType type)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_length_action (type);

	if (ract && ract->get_active()) {
		/* It doesn't really matter which EditingContext executes this */
		current_editing_context()->set_draw_length_to (type);
	} else {
		ract->set_active();
	}
}

void
EditingContext::draw_velocity_action_method (int v)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_velocity_action (v);

	if (ract && ract->get_active()) {
		/* It doesn't really matter which EditingContext executes this */
		current_editing_context()->set_draw_velocity_to (v);
	}
}

void
EditingContext::draw_velocity_chosen (int v)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_velocity_action (v);

	if (ract && ract->get_active()) {
		/* It doesn't really matter which EditingContext executes this */
		current_editing_context()->set_draw_velocity_to (v);
	} else {
		ract->set_active();
	}
}

void
EditingContext::draw_channel_action_method (int c)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_channel_action (c);

	if (ract && ract->get_active()) {
		/* It doesn't really matter which EditingContext executes this */
		current_editing_context()->set_draw_channel_to (c);
	}
}

void
EditingContext::draw_channel_chosen (int c)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_channel_action (c);

	if (ract && ract->get_active()) {
		/* It doesn't really matter which EditingContext executes this */
		current_editing_context()->set_draw_channel_to (c);
	} else {
		ract->set_active();
	}
}

RefPtr<RadioAction>
EditingContext::snap_mode_action (SnapMode mode)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (mode) {
	case Editing::SnapOff:
		action = X_("snap-off");
		break;
	case Editing::SnapNormal:
		action = X_("snap-normal");
		break;
	case Editing::SnapMagnetic:
		action = X_("snap-magnetic");
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible snap mode type", (int) mode) << endmsg;
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("Editor"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1: %2"), "EditingContext::snap_mode_chosen could not find action to match mode.", action) << endmsg;
		return RefPtr<RadioAction> ();
	}
}

void
EditingContext::cycle_snap_mode ()
{
	switch (_snap_mode) {
	case SnapOff:
	case SnapNormal:
		set_snap_mode (SnapMagnetic);
		break;
	case SnapMagnetic:
		set_snap_mode (SnapOff);
		break;
	}
}

void
EditingContext::snap_mode_chosen (SnapMode mode)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	if (mode == SnapNormal) {
		mode = SnapMagnetic;
	}

	RefPtr<RadioAction> ract = snap_mode_action (mode);

	if (ract && ract->get_active()) {
		set_snap_mode (mode);
	}
}

GridType
EditingContext::grid_type() const
{
	return _grid_type;
}

GridType
EditingContext::draw_length() const
{
	return _draw_length;
}

int
EditingContext::draw_velocity() const
{
	return _draw_velocity;
}

int
EditingContext::draw_channel() const
{
	return _draw_channel;
}

bool
EditingContext::grid_musical() const
{
	return grid_type_is_musical (_grid_type);
}

bool
EditingContext::grid_type_is_musical(GridType gt) const
{
	switch (gt) {
	case GridTypeBeatDiv32:
	case GridTypeBeatDiv28:
	case GridTypeBeatDiv24:
	case GridTypeBeatDiv20:
	case GridTypeBeatDiv16:
	case GridTypeBeatDiv14:
	case GridTypeBeatDiv12:
	case GridTypeBeatDiv10:
	case GridTypeBeatDiv8:
	case GridTypeBeatDiv7:
	case GridTypeBeatDiv6:
	case GridTypeBeatDiv5:
	case GridTypeBeatDiv4:
	case GridTypeBeatDiv3:
	case GridTypeBeatDiv2:
	case GridTypeBeat:
	case GridTypeBar:
		return true;
	case GridTypeNone:
	case GridTypeTimecode:
	case GridTypeMinSec:
	case GridTypeCDFrame:
		return false;
	}
	return false;
}

SnapMode
EditingContext::snap_mode() const
{
	return _snap_mode;
}

void
EditingContext::set_draw_length_to (GridType gt)
{
	if ( !grid_type_is_musical(gt) ) {  //range-check
		gt = DRAW_LEN_AUTO;
	}

	_draw_length = gt;
	DrawLengthChanged (); /* EMIT SIGNAL */
}

void
EditingContext::draw_length_changed ()
{
	if (DRAW_LEN_AUTO == _draw_length) {
		draw_length_selector.set_text (_("Auto"));
		return;
	}

	unsigned int grid_index = (unsigned int) _draw_length;
	std::string str = grid_type_strings[grid_index];
	draw_length_selector.set_text (str);
	instant_save ();
}

void
EditingContext::set_draw_velocity_to (int v)
{
	if ( v<0 || v>127 ) {  //range-check midi channel
		v = DRAW_VEL_AUTO;
	}

	_draw_velocity = v;
	DrawVelocityChanged (); /* EMIT SIGNAL */
}

void
EditingContext::draw_velocity_changed ()
{
	if (DRAW_VEL_AUTO == _draw_velocity) {
		draw_velocity_selector.set_text (_("Auto"));
		return;
	}

	char buf[64];
	snprintf (buf, sizeof (buf), "%d", _draw_velocity);
	draw_velocity_selector.set_text (buf);
	instant_save ();
}

void
EditingContext::set_draw_channel_to (int c)
{
	if (c < 0 || c > 15)  {  //range-check midi channel
		c = DRAW_CHAN_AUTO;
	}

	_draw_channel = c;
	DrawChannelChanged (); /* EMIT SIGNAL */
}

void
EditingContext::draw_channel_changed ()
{
	if (DRAW_CHAN_AUTO == _draw_channel) {
		draw_channel_selector.set_text (_("Auto"));
		return;
	}

	char buf[64];
	snprintf (buf, sizeof (buf), "%d", _draw_channel+1 );
	draw_channel_selector.set_text (buf);
	instant_save ();
}

void
EditingContext::set_grid_to (GridType gt)
{
	unsigned int grid_ind = (unsigned int)gt;

	if (internal_editing() && UIConfiguration::instance().get_grid_follows_internal()) {
		internal_grid_type = gt;
	} else {
		pre_internal_grid_type = gt;
	}

	bool grid_type_changed = true;
	if ( grid_type_is_musical(_grid_type) && grid_type_is_musical(gt))
		grid_type_changed = false;

	_grid_type = gt;

	if (grid_ind > grid_type_strings.size() - 1) {
		grid_ind = 0;
		_grid_type = (GridType)grid_ind;
	}

	std::string str = grid_type_strings[grid_ind];

	if (str != grid_type_selector.get_text()) {
		grid_type_selector.set_text (str);
	}

	if (grid_type_changed && UIConfiguration::instance().get_show_grids_ruler()) {
		show_rulers_for_grid ();
	}

	instant_save ();

	const bool grid_is_musical = grid_musical ();

	if (grid_is_musical) {
		compute_bbt_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());
		update_tempo_based_rulers ();
	} else if (current_mouse_mode () == Editing::MouseGrid) {
		Glib::RefPtr<RadioAction> ract = Glib::RefPtr<RadioAction>::cast_dynamic (get_mouse_mode_action (Editing::MouseObject));
		ract->set_active (true);
	}

	get_mouse_mode_action (Editing::MouseGrid)->set_sensitive (grid_is_musical);

	mark_region_boundary_cache_dirty ();

	redisplay_grid (false);

	SnapChanged (); /* EMIT SIGNAL */
}

void
EditingContext::set_snap_mode (SnapMode mode)
{
	if (internal_editing()) {
		internal_snap_mode = mode;
	} else {
		pre_internal_snap_mode = mode;
	}

	_snap_mode = mode;

	if (_snap_mode == SnapOff) {
		snap_mode_button.set_active_state (Gtkmm2ext::Off);
	} else {
		snap_mode_button.set_active_state (Gtkmm2ext::ExplicitActive);
	}

	instant_save ();
}


RefPtr<RadioAction>
EditingContext::draw_velocity_action (int v)
{
	char buf[64];
	const char* action = 0;
	RefPtr<Action> act;

	if (v==DRAW_VEL_AUTO) {
		action = "draw-velocity-auto";
	} else if (v>=1 && v<=127) {
		snprintf (buf, sizeof (buf), X_("draw-velocity-%d"), v);  //we don't allow drawing a velocity 0;  some synths use that as note-off
		action = buf;
	}

	act = ActionManager::get_action (_("Draw Velocity"), action);
	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;
	} else  {
		error << string_compose (_("programming error: %1"), "EditingContext::draw_velocity_action could not find action to match velocity.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

RefPtr<RadioAction>
EditingContext::draw_channel_action (int c)
{
	char buf[64];
	const char* action = 0;
	RefPtr<Action> act;

	if (c==DRAW_CHAN_AUTO) {
		action = "draw-channel-auto";
	} else if (c>=0 && c<=15) {
		snprintf (buf, sizeof (buf), X_("draw-channel-%d"), c+1);
		action = buf;
	}

	act = ActionManager::get_action (_("Draw Channel"), action);
	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;
	} else  {
		error << string_compose (_("programming error: %1"), "EditingContext::draw_channel_action could not find action to match channel.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

RefPtr<RadioAction>
EditingContext::draw_length_action (GridType type)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (type) {
	case Editing::GridTypeBeatDiv32:
		action = "draw-length-thirtyseconds";
		break;
	case Editing::GridTypeBeatDiv28:
		action = "draw-length-twentyeighths";
		break;
	case Editing::GridTypeBeatDiv24:
		action = "draw-length-twentyfourths";
		break;
	case Editing::GridTypeBeatDiv20:
		action = "draw-length-twentieths";
		break;
	case Editing::GridTypeBeatDiv16:
		action = "draw-length-asixteenthbeat";
		break;
	case Editing::GridTypeBeatDiv14:
		action = "draw-length-fourteenths";
		break;
	case Editing::GridTypeBeatDiv12:
		action = "draw-length-twelfths";
		break;
	case Editing::GridTypeBeatDiv10:
		action = "draw-length-tenths";
		break;
	case Editing::GridTypeBeatDiv8:
		action = "draw-length-eighths";
		break;
	case Editing::GridTypeBeatDiv7:
		action = "draw-length-sevenths";
		break;
	case Editing::GridTypeBeatDiv6:
		action = "draw-length-sixths";
		break;
	case Editing::GridTypeBeatDiv5:
		action = "draw-length-fifths";
		break;
	case Editing::GridTypeBeatDiv4:
		action = "draw-length-quarters";
		break;
	case Editing::GridTypeBeatDiv3:
		action = "draw-length-thirds";
		break;
	case Editing::GridTypeBeatDiv2:
		action = "draw-length-halves";
		break;
	case Editing::GridTypeBeat:
		action = "draw-length-beat";
		break;
	case Editing::GridTypeBar:
		action = "draw-length-bar";
		break;
	case Editing::GridTypeNone:
		action = "draw-length-auto";
		break;
	case Editing::GridTypeTimecode:
	case Editing::GridTypeCDFrame:
	case Editing::GridTypeMinSec:
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible grid length type", (int) type) << endmsg;
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("DrawLength"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "EditingContext::draw_length_chosen could not find action to match type.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

void
EditingContext::build_grid_type_menu ()
{
	using namespace Menu_Helpers;

	/* there's no Grid, but if Snap is engaged, the Snap preferences will be applied */
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeNone],      sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeNone)));
	grid_type_selector.AddMenuElem(SeparatorElem());

	/* musical grid: bars, quarter-notes, etc */
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBar],       sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBar)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeat],      sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeat)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv2],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv2)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv4],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv4)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv8],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv8)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv16], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv16)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv32], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv32)));

	/* triplet grid */
	grid_type_selector.AddMenuElem(SeparatorElem());
	Gtk::Menu *_triplet_menu = manage (new Menu);
	MenuList& triplet_items (_triplet_menu->items());
	{
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv3],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv3)));
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv6],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv6)));
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv12], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv12)));
		triplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv24], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv24)));
	}
	grid_type_selector.AddMenuElem (Menu_Helpers::MenuElem (_("Triplets"), *_triplet_menu));

	/* quintuplet grid */
	Gtk::Menu *_quintuplet_menu = manage (new Menu);
	MenuList& quintuplet_items (_quintuplet_menu->items());
	{
		quintuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv5],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv5)));
		quintuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv10], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv10)));
		quintuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv20], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv20)));
	}
	grid_type_selector.AddMenuElem (Menu_Helpers::MenuElem (_("Quintuplets"), *_quintuplet_menu));

	/* septuplet grid */
	Gtk::Menu *_septuplet_menu = manage (new Menu);
	MenuList& septuplet_items (_septuplet_menu->items());
	{
		septuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv7],  sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv7)));
		septuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv14], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv14)));
		septuplet_items.push_back (MenuElem (grid_type_strings[(int)GridTypeBeatDiv28], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeBeatDiv28)));
	}
	grid_type_selector.AddMenuElem (Menu_Helpers::MenuElem (_("Septuplets"), *_septuplet_menu));

	grid_type_selector.AddMenuElem(SeparatorElem());
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeTimecode], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeTimecode)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeMinSec], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeMinSec)));
	grid_type_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeCDFrame], sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_selection_done), (GridType) GridTypeCDFrame)));

	grid_type_selector.set_sizing_texts (grid_type_strings);
}

void
EditingContext::build_draw_midi_menus ()
{
	using namespace Menu_Helpers;

	/* Note-Length when drawing */
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeat],     []() { EditingContext::draw_length_chosen ((GridType) GridTypeBeat); }));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv2], []() { EditingContext::draw_length_chosen ((GridType) GridTypeBeatDiv2); }));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv4], []() { EditingContext::draw_length_chosen ((GridType) GridTypeBeatDiv4); }));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv8], []() { EditingContext::draw_length_chosen ((GridType) GridTypeBeatDiv8); }));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv16],[]() { EditingContext::draw_length_chosen ((GridType) GridTypeBeatDiv16); }));
	draw_length_selector.AddMenuElem (MenuElem (grid_type_strings[(int)GridTypeBeatDiv32],[]() { EditingContext::draw_length_chosen ((GridType) GridTypeBeatDiv32); }));
	draw_length_selector.AddMenuElem (MenuElem (_("Auto"),[]() { EditingContext::draw_length_chosen ((GridType) DRAW_LEN_AUTO); }));

	{
		std::vector<std::string> draw_grid_type_strings = {grid_type_strings.begin() + GridTypeBeat, grid_type_strings.begin() + GridTypeBeatDiv32 + 1};
		draw_grid_type_strings.push_back (_("Auto"));
		grid_type_selector.set_sizing_texts (draw_grid_type_strings);
	}

	/* Note-Velocity when drawing */

	draw_velocity_selector.AddMenuElem (MenuElem ("8",   []() { EditingContext::draw_velocity_chosen (8); }));
	draw_velocity_selector.AddMenuElem (MenuElem ("32",  []() { EditingContext::draw_velocity_chosen (32); }));
	draw_velocity_selector.AddMenuElem (MenuElem ("64",  []() { EditingContext::draw_velocity_chosen (64); }));
	draw_velocity_selector.AddMenuElem (MenuElem ("82",  []() { EditingContext::draw_velocity_chosen (82); }));
	draw_velocity_selector.AddMenuElem (MenuElem ("100", []() { EditingContext::draw_velocity_chosen (100); }));
	draw_velocity_selector.AddMenuElem (MenuElem ("127", []() { EditingContext::draw_velocity_chosen (127); }));
	draw_velocity_selector.AddMenuElem (MenuElem (_("Auto"),[]() { EditingContext::draw_velocity_chosen (DRAW_VEL_AUTO); }));

	/* Note-Channel when drawing */
	for (int i = 0; i<= 15; i++) {
		char buf[64];
		sprintf(buf, "%d", i+1);
		draw_channel_selector.AddMenuElem (MenuElem (buf, [i]() { EditingContext::draw_channel_chosen (i); }));
	}
	draw_channel_selector.AddMenuElem (MenuElem (_("Auto"),[]() { EditingContext::draw_channel_chosen (DRAW_CHAN_AUTO); }));
}

bool
EditingContext::drag_active () const
{
	return _drags->active();
}

bool
EditingContext::preview_video_drag_active () const
{
	return _drags->preview_video ();
}

Temporal::TimeDomain
EditingContext::time_domain () const
{
	if (_session) {
		return _session->config.get_default_time_domain();
	}

	/* Probably never reached */

	if (_snap_mode == SnapOff) {
		return Temporal::AudioTime;
	}

	switch (_grid_type) {
		case GridTypeNone:
			/* fallthrough */
		case GridTypeMinSec:
			/* fallthrough */
		case GridTypeCDFrame:
			/* fallthrough */
		case GridTypeTimecode:
			/* fallthrough */
			return Temporal::AudioTime;
		default:
			break;
	}

	return Temporal::BeatTime;
}

void
EditingContext::toggle_follow_playhead ()
{
	RefPtr<ToggleAction> tact = ActionManager::get_toggle_action (X_("Editor"), X_("toggle-follow-playhead"));
	set_follow_playhead (tact->get_active());
}

/** @param yn true to follow playhead, otherwise false.
 *  @param catch_up true to reset the editor view to show the playhead (if yn == true), otherwise false.
 */
void
EditingContext::set_follow_playhead (bool yn, bool catch_up)
{
	if (_follow_playhead != yn) {
		if ((_follow_playhead = yn) == true && catch_up) {
			/* catch up */
			reset_x_origin_to_follow_playhead ();
		}
		instant_save ();
	}
}

double
EditingContext::time_to_pixel (timepos_t const & pos) const
{
	return sample_to_pixel (pos.samples());
}

double
EditingContext::time_to_pixel_unrounded (timepos_t const & pos) const
{
	return sample_to_pixel_unrounded (pos.samples());
}

double
EditingContext::time_delta_to_pixel (timepos_t const& start, timepos_t const& end) const
{
	return sample_to_pixel (end.samples()) - sample_to_pixel (start.samples ());
}

double
EditingContext::duration_to_pixels (timecnt_t const & dur) const
{
	return sample_to_pixel (dur.samples());
}

double
EditingContext::duration_to_pixels_unrounded (timecnt_t const & dur) const
{
	return sample_to_pixel_unrounded (dur.samples());
}

/** Snap a position to the grid, if appropriate, taking into account current
 *  grid settings and also the state of any snap modifier keys that may be pressed.
 *  @param start Position to snap.
 *  @param event Event to get current key modifier information from, or 0.
 */
void
EditingContext::snap_to_with_modifier (timepos_t& start, GdkEvent const * event, Temporal::RoundMode direction, SnapPref pref, bool ensure_snap) const
{
	if (!_session || !event) {
		return;
	}

	if (ArdourKeyboard::indicates_snap (event->button.state)) {
		if (_snap_mode == SnapOff) {
			snap_to_internal (start, direction, pref, ensure_snap);
		}

	} else {
		if (_snap_mode != SnapOff) {
			snap_to_internal (start, direction, pref);
		} else if (ArdourKeyboard::indicates_snap_delta (event->button.state)) {
			/* SnapOff, but we pressed the snap_delta modifier */
			snap_to_internal (start, direction, pref, ensure_snap);
		}
	}
}

void
EditingContext::snap_to (timepos_t& start, Temporal::RoundMode direction, SnapPref pref, bool ensure_snap) const
{
	if (!_session || (_snap_mode == SnapOff && !ensure_snap)) {
		return;
	}

	snap_to_internal (start, direction, pref, ensure_snap);
}

timepos_t
EditingContext::snap_to_bbt (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref) const
{
	return snap_to_bbt_via_grid (presnap, direction, gpref, _grid_type);
}

timepos_t
EditingContext::snap_to_bbt_via_grid (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref, GridType grid_type) const
{
	timepos_t ret(presnap);
	TempoMap::SharedPtr tmap (TempoMap::use());

	/* Snap to bar always uses bars, and ignores visual grid, so it may
	 * sometimes snap to bars that are not visually distinguishable.
	 *
	 * XXX this should probably work totally different: we should get the
	 * nearby grid and walk towards the next bar point.
	 */

	if (grid_type == GridTypeBar) {
		return timepos_t (tmap->quarters_at (presnap).round_to_subdivision (get_grid_beat_divisions(grid_type), direction));
	}

	if (gpref != SnapToGrid_Unscaled) { // use the visual grid lines which are limited by the zoom scale that the user selected

		/* Determine the most obvious divisor of a beat to use
		 * for the snap, based on the grid setting.
		 */

		int divisor;
		switch (_grid_type) {
			case GridTypeBeatDiv3:
			case GridTypeBeatDiv6:
			case GridTypeBeatDiv12:
			case GridTypeBeatDiv24:
				divisor = 3;
				break;
			case GridTypeBeatDiv5:
			case GridTypeBeatDiv10:
			case GridTypeBeatDiv20:
				divisor = 5;
				break;
			case GridTypeBeatDiv7:
			case GridTypeBeatDiv14:
			case GridTypeBeatDiv28:
				divisor = 7;
				break;
			case GridTypeBeat:
				divisor = 1;
				break;
			case GridTypeNone:
				return ret;
			default:
				divisor = 2;
				break;
		};

		/* bbt_ruler_scale reflects the level of detail we will show
		 * for the visual grid. Adjust the "natural" divisor to reflect
		 * this level of detail, and snap to that.
		 *
		 * So, for example, if the grid is Div3, we use 3 divisions per
		 * beat, but if the visual grid is using bbt_show_sixteenths (a
		 * fairly high level of detail), we will snap to (2 * 3)
		 * divisions per beat. Etc.
		 */

		BBTRulerScale scale = bbt_ruler_scale;
		switch (scale) {
			case bbt_show_many:
			case bbt_show_64:
			case bbt_show_16:
			case bbt_show_4:
			case bbt_show_1:
				/* Round to Bar */
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (-1, direction));
				break;
			case bbt_show_quarters:
				/* Round to Beat */
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (1, direction));
				break;
			case bbt_show_eighths:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (1 * divisor, direction));
				break;
			case bbt_show_sixteenths:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (2 * divisor, direction));
				break;
			case bbt_show_thirtyseconds:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (4 * divisor, direction));
				break;
			case bbt_show_sixtyfourths:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (8 * divisor, direction));
				break;
			case bbt_show_onetwentyeighths:
				ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (16 * divisor, direction));
				break;
		}
	} else {
		/* Just use the grid as specified, without paying attention to
		 * zoom level
		 */

		ret = timepos_t (tmap->quarters_at (presnap).round_to_subdivision (get_grid_beat_divisions (grid_type), direction));
	}

	return ret;
}

void
EditingContext::check_best_snap (timepos_t const & presnap, timepos_t &test, timepos_t &dist, timepos_t &best) const
{
	timepos_t diff = timepos_t (presnap.distance (test).abs ());
	if (diff < dist) {
		dist = diff;
		best = test;
	}

	test = timepos_t::max (test.time_domain()); // reset this so it doesn't get accidentally reused
}

timepos_t
EditingContext::canvas_event_time (GdkEvent const * event, double* pcx, double* pcy) const
{
	timepos_t pos (canvas_event_sample (event, pcx, pcy));

	if (time_domain() == Temporal::AudioTime) {
		return pos;
	}

	return timepos_t (pos.beats());
}

samplepos_t
EditingContext::canvas_event_sample (GdkEvent const * event, double* pcx, double* pcy) const
{
	double x;
	double y;

	/* event coordinates are already in canvas units */

	if (!gdk_event_get_coords (event, &x, &y)) {
		std::cerr << "!NO c COORDS for event type " << event->type << std::endl;
		return 0;
	}

	if (pcx) {
		*pcx = x;
	}

	if (pcy) {
		*pcy = y;
	}

	/* note that pixel_to_sample_from_event() never returns less than zero, so even if the pixel
	   position is negative (as can be the case with motion events in particular),
	   the sample location is always positive.
	*/

	return pixel_to_sample_from_event (x);
}

uint32_t
EditingContext::count_bars (Beats const & start, Beats const & end) const
{
	TempoMapPoints bar_grid;
	TempoMap::SharedPtr tmap (TempoMap::use());
	bar_grid.reserve (4096);
	superclock_t s (tmap->superclock_at (start));
	superclock_t e (tmap->superclock_at (end));
	tmap->get_grid (bar_grid, s, e, 1);
	return bar_grid.size();
}

void
EditingContext::compute_bbt_ruler_scale (samplepos_t lower, samplepos_t upper)
{
	if (_session == 0) {
		return;
	}

	Temporal::BBT_Time lower_beat, upper_beat; // the beats at each end of the ruler
	Temporal::TempoMap::SharedPtr tmap (Temporal::TempoMap::use());
	Beats floor_lower_beat = std::max (Beats(), tmap->quarters_at_sample (lower)).round_down_to_beat ();

	if (floor_lower_beat < Temporal::Beats()) {
		floor_lower_beat = Temporal::Beats();
	}

	const samplepos_t beat_before_lower_pos = tmap->sample_at (floor_lower_beat);
	const samplepos_t beat_after_upper_pos = tmap->sample_at ((std::max (Beats(), tmap->quarters_at_sample  (upper)).round_down_to_beat()) + Beats (1, 0));

	lower_beat = Temporal::TempoMap::use()->bbt_at (timepos_t (beat_before_lower_pos));
	upper_beat = Temporal::TempoMap::use()->bbt_at (timepos_t (beat_after_upper_pos));
	uint32_t beats = 0;

	bbt_bar_helper_on = false;
	bbt_bars = 0;

	bbt_ruler_scale =  bbt_show_many;

	const Beats ceil_upper_beat = std::max (Beats(), tmap->quarters_at_sample (upper)).round_up_to_beat() + Beats (1, 0);

	if (ceil_upper_beat == floor_lower_beat) {
		return;
	}

	bbt_bars = count_bars (floor_lower_beat, ceil_upper_beat);

	double ruler_line_granularity = UIConfiguration::instance().get_ruler_granularity ();  //in pixels
	ruler_line_granularity = visible_canvas_width() / (ruler_line_granularity*5);  //fudge factor '5' probably related to (4+1 beats)/measure, I think

	beats = (ceil_upper_beat - floor_lower_beat).get_beats();
	double beat_density = ((beats + 1) * ((double) (upper - lower) / (double) (1 + beat_after_upper_pos - beat_before_lower_pos))) / (float)ruler_line_granularity;

	/* Only show the bar helper if there aren't many bars on the screen */
	if ((bbt_bars < 2) || (beats < 5)) {
		bbt_bar_helper_on = true;
	}

	if (beat_density > 2048) {
		bbt_ruler_scale = bbt_show_many;
	} else if (beat_density > 1024) {
		bbt_ruler_scale = bbt_show_64;
	} else if (beat_density > 256) {
		bbt_ruler_scale = bbt_show_16;
	} else if (beat_density > 64) {
		bbt_ruler_scale = bbt_show_4;
	} else if (beat_density > 16) {
		bbt_ruler_scale = bbt_show_1;
	} else if (beat_density > 4) {
		bbt_ruler_scale =  bbt_show_quarters;
	} else  if (beat_density > 2) {
		bbt_ruler_scale =  bbt_show_eighths;
	} else  if (beat_density > 1) {
		bbt_ruler_scale =  bbt_show_sixteenths;
	} else  if (beat_density > 0.5) {
		bbt_ruler_scale =  bbt_show_thirtyseconds;
	} else  if (beat_density > 0.25) {
		bbt_ruler_scale =  bbt_show_sixtyfourths;
	} else {
		bbt_ruler_scale =  bbt_show_onetwentyeighths;
	}

	/* Now that we know how fine a grid (Ruler) is allowable on this screen, limit it to the coarseness selected by the user */
	/* note: GridType and RulerScale are not the same enums, so it's not a simple mathematical operation */
	int suggested_scale = (int) bbt_ruler_scale;
	int divs = get_grid_music_divisions(_grid_type, 0);
	if (_grid_type == GridTypeBar) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_1);
	} else if (_grid_type == GridTypeBeat) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_quarters);
	}  else if ( divs < 4 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_eighths);
	}  else if ( divs < 8 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_sixteenths);
	} else if ( divs < 16 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_thirtyseconds);
	} else if ( divs < 32 ) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_sixtyfourths);
	} else {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_onetwentyeighths);
	}

	bbt_ruler_scale = (EditingContext::BBTRulerScale) suggested_scale;
}

Quantize*
EditingContext::get_quantize_op ()
{
	if (!quantize_dialog) {
		quantize_dialog = new QuantizeDialog (*this);
	}

	quantize_dialog->present ();
	int r = quantize_dialog->run ();
	quantize_dialog->hide ();


	if (r != Gtk::RESPONSE_OK) {
		return nullptr;
	}

	return new Quantize (quantize_dialog->snap_start(),
	                     quantize_dialog->snap_end(),
	                     quantize_dialog->start_grid_size(),
	                     quantize_dialog->end_grid_size(),
	                     quantize_dialog->strength(),
	                     quantize_dialog->swing(),
	                     quantize_dialog->threshold());
}

timecnt_t
EditingContext::relative_distance (timepos_t const & origin, timecnt_t const & duration, Temporal::TimeDomain domain)
{
	return Temporal::TempoMap::use()->convert_duration (duration, origin, domain);
}

/** Snap a time offset within our region using the current snap settings.
 *  @param x Time offset from this region's position.
 *  @param ensure_snap whether to ignore snap_mode (in the case of SnapOff) and magnetic snap.
 *  Used when inverting snap mode logic with key modifiers, or snap distance calculation.
 *  @return Snapped time offset from this region's position.
 */
timecnt_t
EditingContext::snap_relative_time_to_relative_time (timepos_t const & origin, timecnt_t const & x, bool ensure_snap) const
{
	/* x is relative to origin, convert it to global absolute time */
	timepos_t const session_pos = origin + x;

	/* try a snap in either direction */
	timepos_t snapped = session_pos;
	snap_to (snapped, Temporal::RoundNearest, SnapToAny_Visual, ensure_snap);

	/* if we went off the beginning of the region, snap forwards */
	if (snapped < origin) {
		snapped = session_pos;
		snap_to (snapped, Temporal::RoundUpAlways, SnapToAny_Visual, ensure_snap);
	}

	/* back to relative */
	return origin.distance (snapped);
}

std::shared_ptr<Temporal::TempoMap const>
EditingContext::start_local_tempo_map (std::shared_ptr<Temporal::TempoMap>)
{
	/* default is a no-op */
	return Temporal::TempoMap::use ();
}

bool
EditingContext::typed_event (ArdourCanvas::Item* item, GdkEvent *event, ItemType type)
{
	if (!session () || session()->loading () || session()->deletion_in_progress ()) {
		return false;
	}

	bool ret = false;

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

void
EditingContext::popup_note_context_menu (ArdourCanvas::Item* item, GdkEvent* event)
{
	using namespace Menu_Helpers;

	NoteBase* note = reinterpret_cast<NoteBase*>(item->get_data("notebase"));
	if (!note) {
		return;
	}

	/* We need to get the selection here and pass it to the operations, since
	   popping up the menu will cause a region leave event which clears
	   entered_regionview. */

	MidiView&       mrv = note->region_view();
	const RegionSelection rs  = region_selection ();
	const uint32_t sel_size = mrv.selection_size ();

	MenuList& items = _note_context_menu.items();
	items.clear();

	if (sel_size > 0) {
		items.push_back (MenuElem(_("Delete"), sigc::mem_fun(mrv, &MidiView::delete_selection)));
	}

	items.push_back(MenuElem(_("Edit..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::edit_notes), &mrv)));
	items.push_back(MenuElem(_("Transpose..."),  sigc::bind(sigc::mem_fun(*this, &EditingContext::transpose_regions), rs)));
	items.push_back(MenuElem(_("Legatize"), sigc::bind(sigc::mem_fun(*this, &EditingContext::legatize_regions), rs, false)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}
	items.push_back(MenuElem(_("Quantize..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::quantize_regions), rs)));
	items.push_back(MenuElem(_("Remove Overlap"), sigc::bind(sigc::mem_fun(*this, &EditingContext::legatize_regions), rs, true)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}
	items.push_back(MenuElem(_("Transform..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::transform_regions), rs)));

	_note_context_menu.popup (event->button.button, event->button.time);
}

XMLNode*
EditingContext::button_settings () const
{
	XMLNode* settings = ARDOUR_UI::instance()->editor_settings();
	XMLNode* node = find_named_node (*settings, X_("Buttons"));

	if (!node) {
		node = new XMLNode (X_("Buttons"));
	}

	return node;
}

std::vector<MidiView*>
EditingContext::filter_to_unique_midi_region_views (RegionSelection const & rs) const
{
	typedef std::pair<std::shared_ptr<MidiSource>,timepos_t> MapEntry;
	std::set<MapEntry> single_region_set;

	std::vector<MidiView*> views;

	/* build a list of regions that are unique with respect to their source
	 * and start position. Note: this is non-exhaustive... if someone has a
	 * non-forked copy of a MIDI region and then suitably modifies it, this
	 * will still put both regions into the list of things to be acted
	 * upon.
	 *
	 * Solution: user should not select both regions, or should fork one of them.
	 */

	for (auto const & rv : rs) {

		MidiView* mrv = dynamic_cast<MidiView*> (rv);

		if (!mrv) {
			continue;
		}

		MapEntry entry = make_pair (mrv->midi_region()->midi_source(), mrv->midi_region()->start());

		if (single_region_set.insert (entry).second) {
			views.push_back (mrv);
		}
	}

	return views;
}

void
EditingContext::quantize_region ()
{
	if (_session) {
		quantize_regions(region_selection ());
	}
}

void
EditingContext::quantize_regions (const RegionSelection& rs)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	Quantize* quant = get_quantize_op ();

	if (!quant) {
		return;
	}

	if (!quant->empty()) {
		apply_midi_note_edit_op (*quant, rs);
	}

	delete quant;
}

void
EditingContext::legatize_region (bool shrink_only)
{
	if (_session) {
		legatize_regions(region_selection (), shrink_only);
	}
}

void
EditingContext::legatize_regions (const RegionSelection& rs, bool shrink_only)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	Legatize legatize(shrink_only);
	apply_midi_note_edit_op (legatize, rs);
}

void
EditingContext::transform_region ()
{
	if (_session) {
		transform_regions(region_selection ());
	}
}

void
EditingContext::transform_regions (const RegionSelection& rs)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	TransformDialog td;

	td.present();
	const int r = td.run();
	td.hide();

	if (r == Gtk::RESPONSE_OK) {
		Transform transform(td.get());
		apply_midi_note_edit_op(transform, rs);
	}
}

void
EditingContext::transpose_region ()
{
	if (_session) {
		transpose_regions(region_selection ());
	}
}

void
EditingContext::transpose_regions (const RegionSelection& rs)
{
	if (rs.n_midi_regions() == 0) {
		return;
	}

	TransposeDialog d;
	int const r = d.run ();

	if (r == RESPONSE_ACCEPT) {
		Transpose transpose(d.semitones ());
		apply_midi_note_edit_op (transpose, rs);
	}
}

void
EditingContext::edit_notes (MidiView* mrv)
{
	MidiView::Selection const & s = mrv->selection();

	if (s.empty ()) {
		return;
	}

	EditNoteDialog* d = new EditNoteDialog (mrv, s);
	d->show_all ();

	d->signal_response().connect (sigc::bind (sigc::mem_fun (*this, &EditingContext::note_edit_done), d));
}

void
EditingContext::note_edit_done (int r, EditNoteDialog* d)
{
	d->done (r);
	delete d;
}

PBD::Command*
EditingContext::apply_midi_note_edit_op_to_region (MidiOperator& op, MidiView& mrv)
{
	Evoral::Sequence<Temporal::Beats>::Notes selected;
	mrv.selection_as_notelist (selected, true);

	if (selected.empty()) {
		return 0;
	}

	std::cerr << "Apply op to " << selected.size() << std::endl;

	std::vector<Evoral::Sequence<Temporal::Beats>::Notes> v;
	v.push_back (selected);

	timepos_t pos = mrv.midi_region()->source_position();

	return op (mrv.midi_region()->model(), pos.beats(), v);
}

void
EditingContext::apply_midi_note_edit_op (MidiOperator& op, const RegionSelection& rs)
{
	if (rs.empty()) {
		return;
	}

	bool in_command = false;

	std::vector<MidiView*> views = filter_to_unique_midi_region_views (rs);

	for (auto & mv : views) {

		Command* cmd = apply_midi_note_edit_op_to_region (op, *mv);
		if (cmd) {
			if (!in_command) {
				begin_reversible_command (op.name ());
				in_command = true;
			}
			(*cmd)();
			_session->add_command (cmd);
			}
	}

	if (in_command) {
		commit_reversible_command ();
		_session->set_dirty ();
	}
}

EditingContext*
EditingContext::current_editing_context()
{
	return _current_editing_context;
}

void
EditingContext::switch_editing_context (EditingContext* ec)
{
	assert (ec);
	_current_editing_context = ec;
}

double
EditingContext::horizontal_position () const
{
	return horizontal_adjustment.get_value();
}

void
EditingContext::set_horizontal_position (double p)
{
	p = std::max (0., p);

	horizontal_adjustment.set_value (p);

	_leftmost_sample = (samplepos_t) floor (p * samples_per_pixel);
}

Gdk::Cursor*
EditingContext::get_canvas_cursor () const
{
	Glib::RefPtr<Gdk::Window> win = get_canvas_viewport()->get_window();

	if (win) {
		return _cursors->from_gdk_cursor (gdk_window_get_cursor (win->gobj()));
	}

	return nullptr;
}

void
EditingContext::set_canvas_cursor (Gdk::Cursor* cursor)
{
	Glib::RefPtr<Gdk::Window> win = get_canvas()->get_window();

	if (win && !_cursors->is_invalid (cursor)) {
		/* glibmm 2.4 doesn't allow null cursor pointer because it uses
		   a Gdk::Cursor& as the argument to Gdk::Window::set_cursor().
		   But a null pointer just means "use parent window cursor",
		   and so should be allowed. Gtkmm 3.x has fixed this API.

		   For now, drop down and use C API
		*/
		gdk_window_set_cursor (win->gobj(), cursor ? cursor->gobj() : 0);
		gdk_flush ();
	}
}

void
EditingContext::pack_draw_box ()
{
	/* Draw  - these MIDI tools are only visible when in Draw mode */
	draw_box.set_spacing (2);
	draw_box.set_border_width (2);
	draw_box.pack_start (*manage (new Label (_("Len:"))), false, false);
	draw_box.pack_start (draw_length_selector, false, false, 4);
	draw_box.pack_start (*manage (new Label (_("Ch:"))), false, false);
	draw_box.pack_start (draw_channel_selector, false, false, 4);
	draw_box.pack_start (*manage (new Label (_("Vel:"))), false, false);
	draw_box.pack_start (draw_velocity_selector, false, false, 4);

	draw_length_selector.set_name ("mouse mode button");
	draw_velocity_selector.set_name ("mouse mode button");
	draw_channel_selector.set_name ("mouse mode button");

	draw_velocity_selector.set_sizing_text (_("Auto"));
	draw_channel_selector.set_sizing_text (_("Auto"));

	draw_velocity_selector.disable_scrolling ();
	draw_velocity_selector.signal_scroll_event().connect (sigc::mem_fun(*this, &EditingContext::on_velocity_scroll_event), false);
}

void
EditingContext::pack_snap_box ()
{
	snap_box.pack_start (snap_mode_button, false, false);
	snap_box.pack_start (grid_type_selector, false, false);
}

Glib::RefPtr<Action>
EditingContext::get_mouse_mode_action (MouseMode m) const
{
	switch (m) {
	case MouseRange:
		return ActionManager::get_action (X_("Editing"), X_("set-mouse-mode-range"));
	case MouseObject:
		return ActionManager::get_action (X_("Editing"), X_("set-mouse-mode-object"));
	case MouseCut:
		return ActionManager::get_action (X_("Editing"), X_("set-mouse-mode-cut"));
	case MouseDraw:
		return ActionManager::get_action (X_("Editing"), X_("set-mouse-mode-draw"));
	case MouseTimeFX:
		return ActionManager::get_action (X_("Editing"), X_("set-mouse-mode-timefx"));
	case MouseGrid:
		return ActionManager::get_action (X_("Editing"), X_("set-mouse-mode-grid"));
	case MouseContent:
		return ActionManager::get_action (X_("Editing"), X_("set-mouse-mode-content"));
	}
	return Glib::RefPtr<Action>();
}

void
EditingContext::bind_mouse_mode_buttons ()
{
	RefPtr<Action> act;

	act = ActionManager::get_action (X_("Editing"), X_("temporal-zoom-in"));
	zoom_in_button.set_related_action (act);
	act = ActionManager::get_action (X_("Editing"), X_("temporal-zoom-out"));
	zoom_out_button.set_related_action (act);

	mouse_move_button.set_related_action (get_mouse_mode_action (Editing::MouseObject));
	mouse_move_button.set_icon (ArdourWidgets::ArdourIcon::ToolGrab);
	mouse_move_button.set_name ("mouse mode button");

	mouse_select_button.set_related_action (get_mouse_mode_action (Editing::MouseRange));
	mouse_select_button.set_icon (ArdourWidgets::ArdourIcon::ToolRange);
	mouse_select_button.set_name ("mouse mode button");

	mouse_draw_button.set_related_action (get_mouse_mode_action (Editing::MouseDraw));
	mouse_draw_button.set_icon (ArdourWidgets::ArdourIcon::ToolDraw);
	mouse_draw_button.set_name ("mouse mode button");

	mouse_timefx_button.set_related_action (get_mouse_mode_action (Editing::MouseTimeFX));
	mouse_timefx_button.set_icon (ArdourWidgets::ArdourIcon::ToolStretch);
	mouse_timefx_button.set_name ("mouse mode button");

	mouse_grid_button.set_related_action (get_mouse_mode_action (Editing::MouseGrid));
	mouse_grid_button.set_icon (ArdourWidgets::ArdourIcon::ToolGrid);
	mouse_grid_button.set_name ("mouse mode button");

	mouse_content_button.set_related_action (get_mouse_mode_action (Editing::MouseContent));
	mouse_content_button.set_icon (ArdourWidgets::ArdourIcon::ToolContent);
	mouse_content_button.set_name ("mouse mode button");

	mouse_cut_button.set_related_action (get_mouse_mode_action (Editing::MouseCut));
	mouse_cut_button.set_icon (ArdourWidgets::ArdourIcon::ToolCut);
	mouse_cut_button.set_name ("mouse mode button");

	set_tooltip (mouse_move_button, _("Grab Mode (select/move objects)"));
	set_tooltip (mouse_cut_button, _("Cut Mode (split regions)"));
	set_tooltip (mouse_select_button, _("Range Mode (select time ranges)"));
	set_tooltip (mouse_grid_button, _("Grid Mode (edit tempo-map, drag/drop music-time grid)"));
	set_tooltip (mouse_draw_button, _("Draw Mode (draw and edit gain/notes/automation)"));
	set_tooltip (mouse_timefx_button, _("Stretch Mode (time-stretch audio and midi regions, preserving pitch)"));
	set_tooltip (mouse_content_button, _("Internal Edit Mode (edit notes and automation points)"));
}

void
EditingContext::set_mouse_mode (MouseMode m, bool force)
{
	if (_drags->active ()) {
		return;
	}

	if (!force && m == mouse_mode) {
		return;
	}

	Glib::RefPtr<Action>       act  = get_mouse_mode_action(m);
	Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);

	/* go there and back to ensure that the toggled handler is called to set up mouse_mode */
	tact->set_active (false);
	tact->set_active (true);

	/* NOTE: this will result in a call to mouse_mode_toggled which does the heavy lifting */
}


bool
EditingContext::on_velocity_scroll_event (GdkEventScroll* ev)
{
	int v = PBD::atoi (draw_velocity_selector.get_text ());
	switch (ev->direction) {
		case GDK_SCROLL_DOWN:
			v = std::min (127, v + 1);
			break;
		case GDK_SCROLL_UP:
			v = std::max (1, v - 1);
			break;
		default:
			return false;
	}
	set_draw_velocity_to(v);
	return true;
}

void
EditingContext::set_common_editing_state (XMLNode const & node)
{
	double z;
	if (node.get_property ("zoom", z)) {
		/* older versions of ardour used floating point samples_per_pixel */
		reset_zoom (llrintf (z));
	} else {
		reset_zoom (samples_per_pixel);
	}

	GridType grid_type;
	if (!node.get_property ("grid-type", grid_type)) {
		grid_type = _grid_type;
	}
	grid_type_selection_done (grid_type);

	GridType draw_length;
	if (!node.get_property ("draw-length", draw_length)) {
		draw_length = _draw_length;
	}
	draw_length_chosen (draw_length);

	int draw_vel;
	if (!node.get_property ("draw-velocity", draw_vel)) {
		draw_vel = _draw_velocity;
	}
	draw_velocity_chosen (draw_vel);

	int draw_chan;
	if (!node.get_property ("draw-channel", draw_chan)) {
		draw_chan = DRAW_CHAN_AUTO;
	}
	draw_channel_chosen (draw_chan);

	SnapMode sm;
	if (node.get_property ("snap-mode", sm)) {
		snap_mode_selection_done(sm);
		/* set text of Dropdown. in case _snap_mode == SnapOff (default)
		 * snap_mode_selection_done() will only mark an already active item as active
		 * which does not trigger set_text().
		 */
		set_snap_mode (sm);
	} else {
		set_snap_mode (_snap_mode);
	}

	node.get_property ("internal-grid-type", internal_grid_type);
	node.get_property ("internal-snap-mode", internal_snap_mode);
	node.get_property ("pre-internal-grid-type", pre_internal_grid_type);
	node.get_property ("pre-internal-snap-mode", pre_internal_snap_mode);

	std::string mm_str;
	if (node.get_property ("mouse-mode", mm_str)) {
		MouseMode m = str2mousemode(mm_str);
		set_mouse_mode (m, true);
	} else {
		set_mouse_mode (MouseObject, true);
	}

	samplepos_t lf_pos;
	if (node.get_property ("left-frame", lf_pos)) {
		if (lf_pos < 0) {
			lf_pos = 0;
		}
		reset_x_origin (lf_pos);
	}
}

void
EditingContext::get_common_editing_state (XMLNode& node) const
{
	node.set_property ("zoom", samples_per_pixel);
	node.set_property ("grid-type", _grid_type);
	node.set_property ("snap-mode", _snap_mode);
	node.set_property ("internal-grid-type", internal_grid_type);
	node.set_property ("internal-snap-mode", internal_snap_mode);
	node.set_property ("pre-internal-grid-type", pre_internal_grid_type);
	node.set_property ("pre-internal-snap-mode", pre_internal_snap_mode);
	node.set_property ("draw-length", _draw_length);
	node.set_property ("draw-velocity", _draw_velocity);
	node.set_property ("draw-channel", _draw_channel);
	node.set_property ("left-frame", _leftmost_sample);
}

bool
EditingContext::snap_mode_button_clicked (GdkEventButton* ev)
{
	if (ev->button != 3) {
		cycle_snap_mode();
		return true;
	}

	RCOptionEditor* rc_option_editor = ARDOUR_UI::instance()->get_rc_option_editor();
	if (rc_option_editor) {
		ARDOUR_UI::instance()->show_tabbable (rc_option_editor);
		rc_option_editor->set_current_page (_("Editor/Snap"));
	}

	return true;
}

void
EditingContext::register_grid_actions ()
{
	ActionManager::register_action (editor_actions, X_("GridChoice"), _("Snap & Grid"));

	RadioAction::Group snap_mode_group;
	/* deprecated */  ActionManager::register_radio_action (editor_actions, snap_mode_group, X_("snap-off"), _("No Grid"), (sigc::bind (sigc::mem_fun(*this, &EditingContext::snap_mode_chosen), Editing::SnapOff)));
	/* deprecated */  ActionManager::register_radio_action (editor_actions, snap_mode_group, X_("snap-normal"), _("Grid"), (sigc::bind (sigc::mem_fun(*this, &EditingContext::snap_mode_chosen), Editing::SnapNormal)));  //deprecated
	/* deprecated */  ActionManager::register_radio_action (editor_actions, snap_mode_group, X_("snap-magnetic"), _("Magnetic"), (sigc::bind (sigc::mem_fun(*this, &EditingContext::snap_mode_chosen), Editing::SnapMagnetic)));

	ActionManager::register_action (editor_actions, X_("cycle-snap-mode"), _("Toggle Snap"), sigc::mem_fun (*this, &EditingContext::cycle_snap_mode));
	ActionManager::register_action (editor_actions, X_("next-grid-choice"), _("Next Quantize Grid Choice"), sigc::mem_fun (*this, &EditingContext::next_grid_choice));
	ActionManager::register_action (editor_actions, X_("prev-grid-choice"), _("Previous Quantize Grid Choice"), sigc::mem_fun (*this, &EditingContext::prev_grid_choice));

	snap_actions = ActionManager::create_action_group (bindings, editor_name() + X_("Snap"));
	RadioAction::Group grid_choice_group;

	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-thirtyseconds"),  grid_type_strings[(int)GridTypeBeatDiv32].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv32)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twentyeighths"),  grid_type_strings[(int)GridTypeBeatDiv28].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv28)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twentyfourths"),  grid_type_strings[(int)GridTypeBeatDiv24].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv24)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twentieths"),     grid_type_strings[(int)GridTypeBeatDiv20].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv20)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-asixteenthbeat"), grid_type_strings[(int)GridTypeBeatDiv16].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv16)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-fourteenths"),    grid_type_strings[(int)GridTypeBeatDiv14].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv14)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twelfths"),       grid_type_strings[(int)GridTypeBeatDiv12].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv12)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-tenths"),         grid_type_strings[(int)GridTypeBeatDiv10].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv10)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-eighths"),        grid_type_strings[(int)GridTypeBeatDiv8].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv8)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-sevenths"),       grid_type_strings[(int)GridTypeBeatDiv7].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv7)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-sixths"),         grid_type_strings[(int)GridTypeBeatDiv6].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv6)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-fifths"),         grid_type_strings[(int)GridTypeBeatDiv5].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv5)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-quarters"),       grid_type_strings[(int)GridTypeBeatDiv4].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv4)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-thirds"),         grid_type_strings[(int)GridTypeBeatDiv3].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv3)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-halves"),         grid_type_strings[(int)GridTypeBeatDiv2].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv2)));

	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-timecode"),       grid_type_strings[(int)GridTypeTimecode].c_str(),      (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeTimecode)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-minsec"),         grid_type_strings[(int)GridTypeMinSec].c_str(),    (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeMinSec)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-cdframe"),        grid_type_strings[(int)GridTypeCDFrame].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeCDFrame)));

	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-beat"),           grid_type_strings[(int)GridTypeBeat].c_str(),      (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeat)));
	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-bar"),            grid_type_strings[(int)GridTypeBar].c_str(),       (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBar)));

	ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-none"),           grid_type_strings[(int)GridTypeNone].c_str(),      (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeNone)));
}

void
EditingContext::ensure_visual_change_idle_handler ()
{
	if (pending_visual_change.idle_handler_id < 0) {
		/* see comment in add_to_idle_resize above. */
		pending_visual_change.idle_handler_id = g_idle_add_full (G_PRIORITY_HIGH_IDLE + 10, _idle_visual_changer, this, NULL);
		pending_visual_change.being_handled = false;
	}
}

int
EditingContext::_idle_visual_changer (void* arg)
{
	return static_cast<EditingContext*>(arg)->idle_visual_changer ();
}

int
EditingContext::idle_visual_changer ()
{
	pending_visual_change.idle_handler_id = -1;

	if (pending_visual_change.pending == 0) {
		return 0;
	}

	/* set_horizontal_position() below (and maybe other calls) call
	   gtk_main_iteration(), so it's possible that a signal will be handled
	   half-way through this method.  If this signal wants an
	   idle_visual_changer we must schedule another one after this one, soa
	   mark the idle_handler_id as -1 here to allow that.  Also make a note
	   that we are doing the visual change, so that changes in response to
	   super-rapid-screen-update can be dropped if we are still processing
	   the last one.
	*/

	if (visual_change_queued) {
		return 0;
	}

	pending_visual_change.being_handled = true;

	VisualChange vc = pending_visual_change;

	pending_visual_change.pending = (VisualChange::Type) 0;

	visual_changer (vc);

	pending_visual_change.being_handled = false;

	visual_change_queued = true;

	return 0; /* this is always a one-shot call */
}


/** Queue up a change to the viewport x origin.
 *  @param sample New x origin.
 */
void
EditingContext::reset_x_origin (samplepos_t sample)
{
	pending_visual_change.add (VisualChange::TimeOrigin);
	pending_visual_change.time_origin = sample;
	ensure_visual_change_idle_handler ();
}

void
EditingContext::reset_y_origin (double y)
{
	pending_visual_change.add (VisualChange::YOrigin);
	pending_visual_change.y_origin = y;
	ensure_visual_change_idle_handler ();
}

void
EditingContext::reset_zoom (samplecnt_t spp)
{
	std::pair<timepos_t, timepos_t> ext = max_zoom_extent();
	samplecnt_t max_extents_pp = (ext.second.samples() - ext.first.samples())  / _track_canvas_width;

	if (spp > max_extents_pp) {
		spp = max_extents_pp;
	}

	if (spp == samples_per_pixel) {
		return;
	}

	pending_visual_change.add (VisualChange::ZoomLevel);
	pending_visual_change.samples_per_pixel = spp;
	if (spp == 0.0) {
		std::cerr << "spp set to zero\n";
		PBD::stacktrace (std::cerr, 12);
	}
	ensure_visual_change_idle_handler ();
}

void
EditingContext::pre_render ()
{
	visual_change_queued = false;

	if (pending_visual_change.pending != 0) {
		ensure_visual_change_idle_handler();
	}
}

/* Convenience functions to slightly reduce verbosity when registering actions */

RefPtr<Action>
EditingContext::reg_sens (RefPtr<ActionGroup> group, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<Action> act = ActionManager::register_action (group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
	return act;
}

void
EditingContext::toggle_reg_sens (RefPtr<ActionGroup> group, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<Action> act = ActionManager::register_toggle_action (group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
}

void
EditingContext::radio_reg_sens (RefPtr<ActionGroup> action_group, RadioAction::Group& radio_group, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<Action> act = ActionManager::register_radio_action (action_group, radio_group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
}

void
EditingContext::update_undo_redo_actions (PBD::UndoHistory const & history)
{
	string label;

	if (undo_action) {
		if (history.undo_depth() == 0) {
			label = S_("Command|Undo");
			undo_action->set_sensitive(false);
		} else {
			label = string_compose(S_("Command|Undo (%1)"), history.next_undo());
			undo_action->set_sensitive(true);
		}
		undo_action->property_label() = label;
	}

	if (redo_action) {
		if (history.redo_depth() == 0) {
			label = _("Redo");
			redo_action->set_sensitive (false);
		} else {
			label = string_compose(_("Redo (%1)"), history.next_redo());
			redo_action->set_sensitive (true);
		}
		redo_action->property_label() = label;
	}
}

int32_t
EditingContext::get_grid_beat_divisions (GridType gt) const
{
	switch (gt) {
	case GridTypeBeatDiv32:  return 32;
	case GridTypeBeatDiv28:  return 28;
	case GridTypeBeatDiv24:  return 24;
	case GridTypeBeatDiv20:  return 20;
	case GridTypeBeatDiv16:  return 16;
	case GridTypeBeatDiv14:  return 14;
	case GridTypeBeatDiv12:  return 12;
	case GridTypeBeatDiv10:  return 10;
	case GridTypeBeatDiv8:   return 8;
	case GridTypeBeatDiv7:   return 7;
	case GridTypeBeatDiv6:   return 6;
	case GridTypeBeatDiv5:   return 5;
	case GridTypeBeatDiv4:   return 4;
	case GridTypeBeatDiv3:   return 3;
	case GridTypeBeatDiv2:   return 2;
	case GridTypeBeat:       return 1;
	case GridTypeBar:        return -1;

	case GridTypeNone:       return 0;
	case GridTypeTimecode:   return 0;
	case GridTypeMinSec:     return 0;
	case GridTypeCDFrame:    return 0;
	default:                 return 0;
	}
	return 0;
}

/**
 * Return the musical grid divisions
 *
 * @param event_state the current keyboard modifier mask.
 * @return Music grid beat divisions
 */
int32_t
EditingContext::get_grid_music_divisions (Editing::GridType gt, uint32_t event_state) const
{
	return get_grid_beat_divisions (gt);
}

Temporal::Beats
EditingContext::get_grid_type_as_beats (bool& success, timepos_t const & position) const
{
	success = true;

	int32_t const divisions = get_grid_beat_divisions (_grid_type);
	/* Beat (+1), and Bar (-1) are handled below */
	if (divisions > 1) {
		/* grid divisions are divisions of a 1/4 note */
		return Temporal::Beats::ticks(Temporal::Beats::PPQN / divisions);
	}

	TempoMap::SharedPtr tmap (TempoMap::use());

	switch (_grid_type) {
	case GridTypeBar:
		if (_session) {
			const Meter& m = tmap->meter_at (position);
			return Temporal::Beats::from_double ((4.0 * m.divisions_per_bar()) / m.note_value());
		}
		break;

	case GridTypeBeat:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 4.0);

	case GridTypeBeatDiv2:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 8.0);

	case GridTypeBeatDiv4:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 16.0);

	case GridTypeBeatDiv8:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 32.0);

	case GridTypeBeatDiv16:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 64.0);

	case GridTypeBeatDiv32:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 128.0);

	case GridTypeBeatDiv3:  //Triplet eighth
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 12.0);


	case GridTypeBeatDiv6:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 24.0);

	case GridTypeBeatDiv12:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 48.0);

	case GridTypeBeatDiv24:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 96.0);

	case GridTypeBeatDiv5:  //Quintuplet //eighth
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 20.0);

	case GridTypeBeatDiv10:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 40.0);

	case GridTypeBeatDiv20:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 80.0);

	case GridTypeBeatDiv7:  //Septuplet eighth
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 28.0);

	case GridTypeBeatDiv14:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 56.0);

	case GridTypeBeatDiv28:
		return Temporal::Beats::from_double (tmap->meter_at (position).note_value() / 112.0);

	default:
		success = false;
		break;
	}

	return Temporal::Beats();
}

Temporal::Beats
EditingContext::get_draw_length_as_beats (bool& success, timepos_t const & position) const
{
	success = true;
	GridType grid_to_use = draw_length() == DRAW_LEN_AUTO ? grid_type() : draw_length();
	int32_t const divisions = get_grid_beat_divisions (grid_to_use);

	if (divisions != 0) {
		return Temporal::Beats::ticks (Temporal::Beats::PPQN / divisions);
	}

	success = false;
	return Temporal::Beats();
}

void
EditingContext::select_automation_line (GdkEventButton* event, ArdourCanvas::Item* item, ARDOUR::SelectionOperation op)
{
	AutomationLine* al = reinterpret_cast<AutomationLine*> (item->get_data ("line"));
	std::list<Selectable*> selectables;
	double mx = event->x;
	double my = event->y;
	bool press = (event->type == GDK_BUTTON_PRESS);

	al->grab_item().canvas_to_item (mx, my);

	uint32_t before, after;
	samplecnt_t const  where = (samplecnt_t) floor (canvas_to_timeline (mx) * samples_per_pixel);

	if (!al || !al->control_points_adjacent (where, before, after)) {
		return;
	}

	selectables.push_back (al->nth (before));
	selectables.push_back (al->nth (after));

	switch (op) {
	case SelectionSet:
		if (press) {
			selection->set (selectables);
			_mouse_changed_selection = true;
		}
		break;
	case SelectionAdd:
		if (press) {
			selection->add (selectables);
			_mouse_changed_selection = true;
		}
		break;
	case SelectionToggle:
		if (press) {
			selection->toggle (selectables);
			_mouse_changed_selection = true;
		}
		break;
	case SelectionExtend:
		/* XXX */
		break;
	case SelectionRemove:
		/* not relevant */
		break;
	}
}

/** Reset all selected points to the relevant default value */
void
EditingContext::reset_point_selection ()
{
	for (PointSelection::iterator i = selection->points.begin(); i != selection->points.end(); ++i) {
		ARDOUR::AutomationList::iterator j = (*i)->model ();
		(*j)->value = (*i)->line().the_list()->descriptor ().normal;
	}
}

void
EditingContext::choose_canvas_cursor_on_entry (ItemType type)
{
	if (_drags->active()) {
		return;
	}

	Gdk::Cursor* cursor = which_canvas_cursor (type);

	if (!_cursors->is_invalid (cursor)) {
		// Push a new enter context
		set_canvas_cursor (cursor);
	}
}

void
EditingContext::play_note_selection_clicked ()
{
	UIConfiguration::instance().set_sound_midi_notes (!UIConfiguration::instance().get_sound_midi_notes());
}

void
EditingContext::follow_playhead_clicked ()
{
}

void
EditingContext::cycle_zoom_focus ()
{
	switch (_zoom_focus) {
	case ZoomFocusLeft:
		set_zoom_focus (ZoomFocusRight);
		break;
	case ZoomFocusRight:
		set_zoom_focus (ZoomFocusCenter);
		break;
	case ZoomFocusCenter:
		set_zoom_focus (ZoomFocusPlayhead);
		break;
	case ZoomFocusPlayhead:
		set_zoom_focus (ZoomFocusMouse);
		break;
	case ZoomFocusMouse:
		set_zoom_focus (ZoomFocusEdit);
		break;
	case ZoomFocusEdit:
		set_zoom_focus (ZoomFocusLeft);
		break;
	}
}

void
EditingContext::temporal_zoom_step_mouse_focus_scale (bool zoom_out, double scale)
{
	PBD::Unwinder<Editing::ZoomFocus> zf (_zoom_focus, Editing::ZoomFocusMouse);
	temporal_zoom_step_scale (zoom_out, scale);
}

void
EditingContext::temporal_zoom_step_mouse_focus (bool zoom_out)
{
	temporal_zoom_step_mouse_focus_scale (zoom_out, 2.0);
}

void
EditingContext::temporal_zoom_step (bool zoom_out)
{
	temporal_zoom_step_scale (zoom_out, 2.0);
}

void
EditingContext::temporal_zoom_step_scale (bool zoom_out, double scale)
{
	ENSURE_GUI_THREAD (*this, &EditingContext::temporal_zoom_step, zoom_out, scale)

	samplecnt_t nspp = samples_per_pixel;

	if (zoom_out) {
		nspp *= scale;
		if (nspp == samples_per_pixel) {
			nspp *= 2.0;
		}
	} else {
		nspp /= scale;
		if (nspp == samples_per_pixel) {
			nspp /= 2.0;
		}
	}

	//zoom-behavior-tweaks
	//limit our maximum zoom to the session gui extents value
	std::pair<timepos_t, timepos_t> ext = max_zoom_extent();
	samplecnt_t session_extents_pp = (ext.second.samples() - ext.first.samples())  / _track_canvas_width;
	if (nspp > session_extents_pp) {
		nspp = session_extents_pp;
	}

	temporal_zoom (nspp);
}

void
EditingContext::temporal_zoom (samplecnt_t spp)
{
	if (!_session) {
		return;
	}

	samplepos_t current_page = current_page_samples();
	samplepos_t current_leftmost = _leftmost_sample;
	samplepos_t current_rightmost;
	samplepos_t current_center;
	samplepos_t new_page_size;
	samplepos_t half_page_size;
	samplepos_t leftmost_after_zoom = 0;
	samplepos_t where;
	bool in_track_canvas;
	bool use_mouse_sample = true;
	samplecnt_t nspp;
	double l;

	if (spp == samples_per_pixel) {
		return;
	}

	// Imposing an arbitrary limit to zoom out as too much zoom out produces
	// segfaults for lack of memory. If somebody decides this is not high enough I
	// believe it can be raisen to higher values but some limit must be in place.
	//
	// This constant represents 1 day @ 48kHz on a 1600 pixel wide display
	// all of which is used for the editor track displays. The whole day
	// would be 4147200000 samples, so 2592000 samples per pixel.

	nspp = std::min (spp, (samplecnt_t) 2592000);
	nspp = std::max ((samplecnt_t) 1, nspp);

	new_page_size = (samplepos_t) floor (_track_canvas_width * nspp);
	half_page_size = new_page_size / 2;

	Editing::ZoomFocus zf = effective_zoom_focus();

	switch (zf) {
	case ZoomFocusLeft:
		leftmost_after_zoom = current_leftmost;
		break;

	case ZoomFocusRight:
		current_rightmost = _leftmost_sample + current_page;
		if (current_rightmost < new_page_size) {
			leftmost_after_zoom = 0;
		} else {
			leftmost_after_zoom = current_rightmost - new_page_size;
		}
		break;

	case ZoomFocusCenter:
		current_center = current_leftmost + (current_page/2);
		if (current_center < half_page_size) {
			leftmost_after_zoom = 0;
		} else {
			leftmost_after_zoom = current_center - half_page_size;
		}
		break;

	case ZoomFocusPlayhead:
		/* centre playhead */
		l = _session->transport_sample() - (new_page_size * 0.5);

		if (l < 0) {
			leftmost_after_zoom = 0;
		} else if (l > max_samplepos) {
			leftmost_after_zoom = max_samplepos - new_page_size;
		} else {
			leftmost_after_zoom = (samplepos_t) l;
		}
		break;

	case ZoomFocusMouse:
		/* try to keep the mouse over the same point in the display */

		if (_drags->active()) {
			where = _drags->current_pointer_sample ();
		} else if (!mouse_sample (where, in_track_canvas)) {
			use_mouse_sample = false;
		}

		if (use_mouse_sample) {

			l = - ((new_page_size * ((where - current_leftmost)/(double)current_page)) - where);

			if (l < 0) {
				leftmost_after_zoom = 0;
			} else if (l > max_samplepos) {
				leftmost_after_zoom = max_samplepos - new_page_size;
			} else {
				leftmost_after_zoom = (samplepos_t) l;
			}
		} else {
			/* use playhead instead */
			where = _session->transport_sample();

			if (where < half_page_size) {
				leftmost_after_zoom = 0;
			} else {
				leftmost_after_zoom = where - half_page_size;
			}
		}
		break;

	case ZoomFocusEdit:
		/* try to keep the edit point in the same place */
		where = get_preferred_edit_position ().samples();
		{
			double l = - ((new_page_size * ((where - current_leftmost)/(double)current_page)) - where);

			if (l < 0) {
				leftmost_after_zoom = 0;
			} else if (l > max_samplepos) {
				leftmost_after_zoom = max_samplepos - new_page_size;
			} else {
				leftmost_after_zoom = (samplepos_t) l;
			}
		}
		break;

	}

	// leftmost_after_zoom = min (leftmost_after_zoom, _session->current_end_sample());

	reposition_and_zoom (leftmost_after_zoom, nspp);
}

void
EditingContext::calc_extra_zoom_edges (samplepos_t &start, samplepos_t &end)
{
	/* this func helps make sure we leave a little space
	   at each end of the editor so that the zoom doesn't fit the region
	   precisely to the screen.
	*/

	GdkScreen* screen = gdk_screen_get_default ();
	const gint pixwidth = gdk_screen_get_width (screen);
	const gint mmwidth = gdk_screen_get_width_mm (screen);
	const double pix_per_mm = (double) pixwidth/ (double) mmwidth;
	const double one_centimeter_in_pixels = pix_per_mm * 10.0;

	const samplepos_t range = end - start;
	const samplecnt_t new_fpp = (samplecnt_t) ceil ((double) range / (double) _track_canvas_width);
	const samplepos_t extra_samples = (samplepos_t) floor (one_centimeter_in_pixels * new_fpp);

	if (start > extra_samples) {
		start -= extra_samples;
	} else {
		start = 0;
	}

	if (max_samplepos - extra_samples > end) {
		end += extra_samples;
	} else {
		end = max_samplepos;
	}
}


void
EditingContext::temporal_zoom_by_sample (samplepos_t start, samplepos_t end)
{
	if (!_session) return;

	if ((start == 0 && end == 0) || end < start) {
		return;
	}

	samplepos_t range = end - start;

	const samplecnt_t new_fpp = (samplecnt_t) ceil ((double) range / (double) _track_canvas_width);

	samplepos_t new_page = range;
	samplepos_t middle = (samplepos_t) floor ((double) start + ((double) range / 2.0f));
	samplepos_t new_leftmost = (samplepos_t) floor ((double) middle - ((double) new_page / 2.0f));

	if (new_leftmost > middle) {
		new_leftmost = 0;
	}

	if (new_leftmost < 0) {
		new_leftmost = 0;
	}

	reposition_and_zoom (new_leftmost, new_fpp);
}

void
EditingContext::temporal_zoom_to_sample (bool coarser, samplepos_t sample)
{
	if (!_session) {
		return;
	}

	samplecnt_t range_before = sample - _leftmost_sample;
	samplecnt_t new_spp;

	if (coarser) {
		if (samples_per_pixel <= 1) {
			new_spp = 2;
		} else {
			new_spp = samples_per_pixel + (samples_per_pixel/2);
		}
		range_before += range_before/2;
	} else {
		if (samples_per_pixel >= 1) {
			new_spp = samples_per_pixel - (samples_per_pixel/2);
		} else {
			/* could bail out here since we cannot zoom any finer,
			   but leave that to the equality test below
			*/
			new_spp = samples_per_pixel;
		}

		range_before -= range_before/2;
	}

	if (new_spp == samples_per_pixel)  {
		return;
	}

	/* zoom focus is automatically taken as @p sample when this
	   method is used.
	*/

	samplepos_t new_leftmost = sample - (samplepos_t)range_before;

	if (new_leftmost > sample) {
		new_leftmost = 0;
	}

	if (new_leftmost < 0) {
		new_leftmost = 0;
	}

	reposition_and_zoom (new_leftmost, new_spp);
}

bool
EditingContext::mouse_sample (samplepos_t& where, bool& in_track_canvas) const
{
	/* gdk_window_get_pointer() has X11's XQueryPointer semantics in that it only
	 * pays attentions to subwindows. this means that menu windows are ignored, and
	 * if the pointer is in a menu, the return window from the call will be the
	 * the regular subwindow *under* the menu.
	 *
	 * this matters quite a lot if the pointer is moving around in a menu that overlaps
	 * the track canvas because we will believe that we are within the track canvas
	 * when we are not. therefore, we track enter/leave events for the track canvas
	 * and allow that to override the result of gdk_window_get_pointer().
	 */

	if (!within_track_canvas) {
		return false;
	}

	int x, y;
	Glib::RefPtr<Gdk::Window> canvas_window = const_cast<EditingContext*>(this)->get_canvas()->get_window();

	if (!canvas_window) {
		return false;
	}

	Glib::RefPtr<const Gdk::Window> pointer_window = Gdk::Display::get_default()->get_window_at_pointer (x, y);

	if (!pointer_window) {
		return false;
	}

	if (pointer_window != canvas_window) {
		in_track_canvas = false;
		return false;
	}

	in_track_canvas = true;

	GdkEvent event;
	event.type = GDK_BUTTON_RELEASE;
	event.button.x = x;
	event.button.y = y;

	where = window_event_sample (&event, 0, 0);

	return true;
}

samplepos_t
EditingContext::window_event_sample (GdkEvent const * event, double* pcx, double* pcy) const
{
	ArdourCanvas::Duple d;

	if (!gdk_event_get_coords (event, &d.x, &d.y)) {
		return 0;
	}

	/* event coordinates are in window units, so convert to canvas
	 */

	d = get_canvas()->window_to_canvas (d);

	if (pcx) {
		*pcx = d.x;
	}

	if (pcy) {
		*pcy = d.y;
	}

	return pixel_to_sample (canvas_to_timeline (d.x));
}

void
EditingContext::zoom_focus_selection_done (ZoomFocus f)
{
	RefPtr<RadioAction> ract = zoom_focus_action (f);
	if (ract) {
		ract->set_active ();
	}
}

RefPtr<RadioAction>
EditingContext::zoom_focus_action (ZoomFocus focus)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (focus) {
	case ZoomFocusLeft:
		action = X_("zoom-focus-left");
		break;
	case ZoomFocusRight:
		action = X_("zoom-focus-right");
		break;
	case ZoomFocusCenter:
		action = X_("zoom-focus-center");
		break;
	case ZoomFocusPlayhead:
		action = X_("zoom-focus-playhead");
		break;
	case ZoomFocusMouse:
		action = X_("zoom-focus-mouse");
		break;
	case ZoomFocusEdit:
		action = X_("zoom-focus-edit");
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible focus type", (int) focus) << endmsg;
		abort(); /*NOTREACHED*/
	}

	return ActionManager::get_radio_action (X_("Zoom"), action);
}

void
EditingContext::zoom_focus_chosen (ZoomFocus focus)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = zoom_focus_action (focus);

	if (ract && ract->get_active()) {
		set_zoom_focus (focus);
	}
}

void
EditingContext::alt_delete_ ()
{
	delete_ ();
}

/** Cut selected regions, automation points or a time range */
void
EditingContext::cut ()
{
	cut_copy (Cut);
}

/** Copy selected regions, automation points or a time range */
void
EditingContext::copy ()
{
	cut_copy (Copy);
}

void
EditingContext::load_shared_bindings ()
{
	/* This set of bindings may expand in the future to include things
	 * other than MIDI editing, but for now this is all we've got as far as
	 * bindings that need to be distinct from the Editors (because some of
	 * the keys may overlap.
	 */

	Bindings* midi_bindings = Bindings::get_bindings (X_("MIDI"));
	register_midi_actions (midi_bindings);

	Bindings* shared_bindings = Bindings::get_bindings (X_("Editing"));
	register_common_actions (shared_bindings);

	/* Give this editing context the chance to add more mode mode actions */
	add_mouse_mode_actions (_common_actions);

	/* Attach bindings to the canvas for this editing context */
	get_canvas()->set_data ("ardour-bindings", midi_bindings);
	get_canvas_viewport()->set_data ("ardour-bindings", shared_bindings);
}

void
EditingContext::drop_grid ()
{
	hide_grid_lines ();
	delete grid_lines;
	grid_lines = nullptr;
}

void
EditingContext::hide_grid_lines ()
{
	if (grid_lines) {
		grid_lines->hide();
	}
}

void
EditingContext::maybe_draw_grid_lines (ArdourCanvas::Container* group)
{
	if (!_session) {
		return;
	}

	if (!grid_lines) {
		grid_lines = new GridLines (*this, group, ArdourCanvas::LineSet::Vertical);
	}

	grid_marks.clear();
	samplepos_t rightmost_sample = _leftmost_sample + current_page_samples();

	if (grid_musical()) {
		 metric_get_bbt (grid_marks, _leftmost_sample, rightmost_sample, 12);
	} else if (_grid_type== GridTypeTimecode) {
		 metric_get_timecode (grid_marks, _leftmost_sample, rightmost_sample, 12);
	} else if (_grid_type == GridTypeCDFrame) {
		metric_get_minsec (grid_marks, _leftmost_sample, rightmost_sample, 12);
	} else if (_grid_type == GridTypeMinSec) {
		metric_get_minsec (grid_marks, _leftmost_sample, rightmost_sample, 12);
	}

	grid_lines->draw (grid_marks);
	grid_lines->show();
}


void
EditingContext::update_grid ()
{
	if (!_session) {
		return;
	}

	if (_grid_type == GridTypeNone) {
		hide_grid_lines ();
	} else if (grid_musical()) {
//		Temporal::TempoMapPoints grid;
//		grid.reserve (4096);
//		if (bbt_ruler_scale != bbt_show_many) {
//			compute_current_bbt_points (grid, _leftmost_sample, _leftmost_sample + current_page_samples());
//		}
		maybe_draw_grid_lines (time_line_group);
	} else {
		maybe_draw_grid_lines (time_line_group);
	}
}

