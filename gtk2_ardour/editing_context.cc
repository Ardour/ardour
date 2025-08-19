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
#include "editor_cursors.h"
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
std::vector<std::string> EditingContext::grid_type_strings;
std::vector<std::string> EditingContext::grid_type_short_labels;
MouseCursors* EditingContext::_cursors = nullptr;
bool EditingContext::need_shared_actions = true;

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

static const gchar *_grid_type_labels[] = {
	N_("No Grid"),
	N_("Bar"),
	N_("1/4"),
	N_("1/8"),
	N_("1/16"),
	N_("1/32"),
	N_("1/64"),
	N_("1/128"),
	N_("1/3"), // or "1/12" ?
	N_("1/6"),
	N_("1/12"),
	N_("1/24"),
	N_("1/5"),
	N_("1/10"),
	N_("1/20"),
	N_("1/7"),
	N_("1/14"),
	N_("1/28"),
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

EditingContext::EditingContext (std::string const & name)
	: rubberband_rect (0)
	, old_mouse_mode (Editing::MouseObject)
	, _name (name)
	, within_track_canvas (false)
	, pre_internal_grid_type (GridTypeBeat)
	, pre_internal_snap_mode (SnapOff)
	, internal_grid_type (GridTypeBeat)
	, internal_snap_mode (SnapOff)
	, _timeline_origin (0.)
	, play_note_selection_button (ArdourButton::default_elements)
	, follow_playhead_button (_("Follow Playhead"), ArdourButton::Element (ArdourButton::Edge | ArdourButton::Body | ArdourButton::VectorIcon), true)
	, follow_edits_button (_("Follow Range"), ArdourButton::Element (ArdourButton::Edge | ArdourButton::Body | ArdourButton::VectorIcon), true)
	, visible_channel_label (S_("MIDI|Ch:"))
	, _drags (new DragManager (this))
	, _leftmost_sample (0)
	, _playhead_cursor (nullptr)
	, _snapped_cursor (nullptr)
	, selection (new Selection (this, true))
	, cut_buffer (new Selection (this, false))
	, _selection_memento (new SelectionMemento())
	, samples_per_pixel (2048)
	, bbt_ruler_scale (bbt_show_many)
	, bbt_bars (0)
	, bbt_bar_helper_on (0)
	, _track_canvas_width (0)
	, _visible_canvas_width (0)
	, _visible_canvas_height (0)
	, quantize_dialog (nullptr)
	, vertical_adjustment (0.0, 0.0, 10.0, 400.0)
	, horizontal_adjustment (0.0, 0.0, 1e16)
	, own_bindings (nullptr)
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
	, temporary_zoom_focus_change (false)
 	, _dragging_playhead (false)

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

	grid_box.set_no_show_all ();

	if (grid_type_strings.empty()) {
		grid_type_strings =  I18N (_grid_type_strings);
	}

	if (grid_type_short_labels.empty()) {
		grid_type_short_labels =  I18N (_grid_type_labels);
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

	set_tooltip (draw_length_selector, _("Note Length to Draw (AUTO uses the current Grid setting)"));
	set_tooltip (draw_velocity_selector, _("Note Velocity to Draw (AUTO uses the nearest note's velocity)"));
	set_tooltip (draw_channel_selector, _("Note Channel to Draw (AUTO uses the nearest note's channel)"));
	set_tooltip (grid_type_selector, _("Grid Mode"));
	set_tooltip (snap_mode_button, _("Snap Mode\n\nRight-click to visit Snap preferences."));
	set_tooltip (zoom_focus_selector, _("Zoom Focus"));

	set_tooltip (play_note_selection_button, _("Play notes when selected"));
	set_tooltip (note_mode_button, _("Switch between sustained and percussive mode"));
	set_tooltip (follow_playhead_button, _("Scroll automatically to keep playhead visible"));
	set_tooltip (follow_edits_button, _("Playhead follows Range tool clicks, and Range selections"));
	/* Leave tip for full zoom button to derived class */
	set_tooltip (visible_channel_selector, _("Select visible MIDI channel"));

	play_note_selection_button.signal_clicked.connect (sigc::mem_fun (*this, &EditingContext::play_note_selection_clicked));
	note_mode_button.signal_clicked.connect (sigc::mem_fun (*this, &EditingContext::note_mode_clicked));
	full_zoom_button.signal_clicked.connect (sigc::mem_fun (*this, &EditingContext::full_zoom_clicked));

	follow_playhead_button.set_icon (ArdourIcon::EditorFollowPlayhead);
	follow_edits_button.set_icon (ArdourIcon::EditorFollowEdits);

	zoom_in_button.set_name ("zoom button");
	zoom_in_button.set_icon (ArdourIcon::ZoomIn);

	zoom_out_button.set_name ("zoom button");
	zoom_out_button.set_icon (ArdourIcon::ZoomOut);

	full_zoom_button.set_name ("zoom button");
	full_zoom_button.set_icon (ArdourIcon::ZoomFull);

	follow_playhead_button.set_name ("transport option button");
	follow_edits_button.set_name ("transport option button");

	note_mode_button.set_icon (ArdourIcon::Drum);
#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))
	note_mode_button.set_size_request (PX_SCALE(50), -1);
	note_mode_button.set_active_color (UIConfiguration::instance().color ("alert:yellow"));

	selection->PointsChanged.connect (sigc::mem_fun(*this, &EditingContext::point_selection_changed));

	for (int i = 0; i < 16; i++) {
		char buf[4];
		sprintf(buf, "%d", i+1);
		visible_channel_selector.add_menu_elem (MenuElem (buf, [this,i]() { set_visible_channel (i); }));
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
	ActionManager::drop_action_group (_midi_actions);
	ActionManager::drop_action_group (_common_actions);
	ActionManager::drop_action_group (editor_actions);
	ActionManager::drop_action_group (snap_actions);
	ActionManager::drop_action_group (length_actions);
	ActionManager::drop_action_group (channel_actions);
	ActionManager::drop_action_group (velocity_actions);
	ActionManager::drop_action_group (zoom_actions);
}

void
EditingContext::ui_parameter_changed (string parameter)
{
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

}

void
EditingContext::set_session (ARDOUR::Session* s)
{
	EC_LOCAL_TEMPO_SCOPE;

	SessionHandlePtr::set_session (s);
	disable_automation_bindings ();
}

void
EditingContext::set_selected_midi_region_view (MidiRegionView& mrv)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* clear note selection in all currently selected MidiRegionViews */

	if (get_selection().regions.contains (&mrv) && get_selection().regions.size() == 1) {
		/* Nothing to do */
		return;
	}

	midi_action (&MidiRegionView::clear_note_selection);
	get_selection().set (&mrv);
}

void
EditingContext::register_automation_actions (Bindings* automation_bindings, std::string const & prefix)
{
	EC_LOCAL_TEMPO_SCOPE;

	_automation_actions = ActionManager::create_action_group (automation_bindings, prefix + X_("Automation"));

	reg_sens (_automation_actions, "create-point", _("Create Automation Point"), sigc::mem_fun (*this, &EditingContext::automation_create_point_at_edit_point));
	reg_sens (_automation_actions, "move-points-later", _("Create Automation P (at Playhead)"), sigc::mem_fun (*this, &EditingContext::automation_move_points_later));
	reg_sens (_automation_actions, "move-points-earlier", _("Create Automation Point (at Playhead)"), sigc::mem_fun (*this, &EditingContext::automation_move_points_earlier));
	reg_sens (_automation_actions, "raise-points", _("Create Automation Point (at Playhead)"), sigc::mem_fun (*this, &EditingContext::automation_raise_points));
	reg_sens (_automation_actions, "lower-points", _("Create Automation Point (at Playhead)"), sigc::mem_fun (*this, &EditingContext::automation_lower_points));
	reg_sens (_automation_actions, "begin-edit", _("Open value entry window for automation editing"), sigc::mem_fun (*this, &EditingContext::automation_begin_edit));
	reg_sens (_automation_actions, "end-edit", _("Close value entry window for automation editing"), sigc::mem_fun (*this, &EditingContext::automation_end_edit));

	disable_automation_bindings ();
}

void
EditingContext::enable_automation_bindings ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_automation_actions) {
		ActionManager::set_sensitive (_automation_actions, true);
	}
}

void
EditingContext::disable_automation_bindings ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_automation_actions) {
		ActionManager::set_sensitive (_automation_actions, false);
	}
}

void
EditingContext::set_action_defaults ()
{
	EC_LOCAL_TEMPO_SCOPE;

#ifndef LIVETRAX
	follow_playhead_action->set_active (false);
	follow_playhead_action->set_active (true);
#else
	follow_playhead_action->set_active (true);
	follow_playhead_action->set_active (false);
#endif

	stationary_playhead_action->set_active (true);
	stationary_playhead_action->set_active (false);

	mouse_mode_actions[Editing::MouseObject]->set_active (false);
	mouse_mode_actions[Editing::MouseObject]->set_active (true);
	zoom_focus_actions[Editing::ZoomFocusLeft]->set_active (false);
	zoom_focus_actions[Editing::ZoomFocusLeft]->set_active (true);

	if (snap_mode_actions[Editing::SnapMagnetic]) {
		snap_mode_actions[Editing::SnapMagnetic]->set_active (false);
		snap_mode_actions[Editing::SnapMagnetic]->set_active (true);
	}
	if (grid_actions[Editing::GridTypeBeat]) {
		grid_actions[Editing::GridTypeBeat]->set_active (false);
		grid_actions[Editing::GridTypeBeat]->set_active (true);
	}
	if (draw_length_actions[DRAW_LEN_AUTO]) {
		draw_length_actions[DRAW_LEN_AUTO]->set_active (false);
		draw_length_actions[DRAW_LEN_AUTO]->set_active (true);
	}
	if (draw_velocity_actions[DRAW_VEL_AUTO]) {
		draw_velocity_actions[DRAW_VEL_AUTO]->set_active (false);
		draw_velocity_actions[DRAW_VEL_AUTO]->set_active (true);
	}
	if (draw_channel_actions[DRAW_CHAN_AUTO]) {
		draw_channel_actions[DRAW_CHAN_AUTO]->set_active (false);
		draw_channel_actions[DRAW_CHAN_AUTO]->set_active (true);
	}
}

void
EditingContext::register_common_actions (Bindings* common_bindings, std::string const & prefix)
{
	EC_LOCAL_TEMPO_SCOPE;

	_common_actions = ActionManager::create_action_group (common_bindings, prefix + X_("Editing"));

	reg_sens (_common_actions, "temporal-zoom-out", _("Zoom Out"), sigc::bind (sigc::mem_fun (*this, &EditingContext::temporal_zoom_step), true));
	reg_sens (_common_actions, "temporal-zoom-in", _("Zoom In"), sigc::bind (sigc::mem_fun (*this, &EditingContext::temporal_zoom_step), false));

	follow_playhead_action = toggle_reg_sens (_common_actions, "toggle-follow-playhead", _("Follow Playhead"), sigc::mem_fun (*this, &EditingContext::follow_playhead_chosen));
	stationary_playhead_action = toggle_reg_sens (_common_actions, "toggle-stationary-playhead", _("Stationary Playhead"), (mem_fun(*this, &EditingContext::stationary_playhead_chosen)));

	undo_action = reg_sens (_common_actions, "undo", S_("Command|Undo"), sigc::bind (sigc::mem_fun (*this, &EditingContext::undo), 1U));
	redo_action = reg_sens (_common_actions, "redo", _("Redo"), sigc::bind (sigc::mem_fun (*this, &EditingContext::redo), 1U));
	alternate_redo_action = reg_sens (_common_actions, "alternate-redo", _("Redo"), sigc::bind (sigc::mem_fun (*this, &EditingContext::redo), 1U));
	alternate_alternate_redo_action = reg_sens (_common_actions, "alternate-alternate-redo", _("Redo"), sigc::bind (sigc::mem_fun (*this, &EditingContext::redo), 1U));

	reg_sens (_common_actions, "editor-delete", _("Delete"), sigc::mem_fun (*this, &EditingContext::delete_));
	reg_sens (_common_actions, "alternate-editor-delete", _("Delete"), sigc::mem_fun (*this, &EditingContext::delete_));

	reg_sens (_common_actions, "editor-cut", _("Cut"), sigc::mem_fun (*this, &EditingContext::cut));
	reg_sens (_common_actions, "editor-copy", _("Copy"), sigc::mem_fun (*this, &EditingContext::copy));
	reg_sens (_common_actions, "editor-paste", _("Paste"), sigc::mem_fun (*this, &EditingContext::keyboard_paste));

	RadioAction::Group mouse_mode_group;

	mouse_mode_actions[Editing::MouseObject] = ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-object", _("Grab (Object Tool)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::mouse_mode_chosen), Editing::MouseObject));
	mouse_mode_actions[Editing::MouseRange] = ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-range", _("Range Tool"), sigc::bind (sigc::mem_fun (*this, &EditingContext::mouse_mode_chosen), Editing::MouseRange));
	mouse_mode_actions[Editing::MouseDraw] = ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-draw", _("Note Drawing Tool"), sigc::bind (sigc::mem_fun (*this, &EditingContext::mouse_mode_chosen), Editing::MouseDraw));
	mouse_mode_actions[Editing::MouseTimeFX] = ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-timefx", _("Time FX Tool"), sigc::bind (sigc::mem_fun (*this, &EditingContext::mouse_mode_chosen), Editing::MouseTimeFX));
	mouse_mode_actions[Editing::MouseGrid] = ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-grid", _("Grid Tool"), sigc::bind (sigc::mem_fun (*this, &EditingContext::mouse_mode_chosen), Editing::MouseGrid));
	mouse_mode_actions[Editing::MouseContent] = ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-content", _("Internal Edit (Content Tool)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::mouse_mode_chosen), Editing::MouseContent));
	mouse_mode_actions[Editing::MouseCut] = ActionManager::register_radio_action (_common_actions, mouse_mode_group, "set-mouse-mode-cut", _("Cut Tool"), sigc::bind (sigc::mem_fun (*this, &EditingContext::mouse_mode_chosen), Editing::MouseCut));

	zoom_actions = ActionManager::create_action_group (common_bindings, prefix + X_("Zoom"));
	RadioAction::Group zoom_group;

	zoom_focus_actions[Editing::ZoomFocusLeft] = radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-left", _("Zoom Focus Left"), sigc::bind (sigc::mem_fun (*this, &EditingContext::zoom_focus_chosen), Editing::ZoomFocusLeft));
	zoom_focus_actions[Editing::ZoomFocusRight] = radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-right", _("Zoom Focus Right"), sigc::bind (sigc::mem_fun (*this, &EditingContext::zoom_focus_chosen), Editing::ZoomFocusRight));
	zoom_focus_actions[Editing::ZoomFocusCenter] = radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-center", _("Zoom Focus Center"), sigc::bind (sigc::mem_fun (*this, &EditingContext::zoom_focus_chosen), Editing::ZoomFocusCenter));
	zoom_focus_actions[Editing::ZoomFocusPlayhead] = radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-playhead", _("Zoom Focus Playhead"), sigc::bind (sigc::mem_fun (*this, &EditingContext::zoom_focus_chosen), Editing::ZoomFocusPlayhead));
	zoom_focus_actions[Editing::ZoomFocusMouse] = radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-mouse", _("Zoom Focus Mouse"), sigc::bind (sigc::mem_fun (*this, &EditingContext::zoom_focus_chosen), Editing::ZoomFocusMouse));
	zoom_focus_actions[Editing::ZoomFocusEdit] = radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-edit", _("Zoom Focus Edit Point"), sigc::bind (sigc::mem_fun (*this, &EditingContext::zoom_focus_chosen), Editing::ZoomFocusEdit));

	ActionManager::register_action (zoom_actions, X_("cycle-zoom-focus"), _("Next Zoom Focus"), sigc::mem_fun (*this, &EditingContext::cycle_zoom_focus));

	/* Grid stuff */

	ActionManager::register_action (_common_actions, X_("GridChoice"), _("Snap & Grid"));

	RadioAction::Group snap_mode_group;
	snap_mode_actions[Editing::SnapOff] = ActionManager::register_radio_action (_common_actions, snap_mode_group, X_("snap-off"), _("No Grid"), (sigc::bind (sigc::mem_fun(*this, &EditingContext::snap_mode_chosen), Editing::SnapOff)));
	snap_mode_actions[Editing::SnapNormal] =  ActionManager::register_radio_action (_common_actions, snap_mode_group, X_("snap-normal"), _("Grid"), (sigc::bind (sigc::mem_fun(*this, &EditingContext::snap_mode_chosen), Editing::SnapNormal)));  //deprecated
	snap_mode_actions[Editing::SnapMagnetic] = ActionManager::register_radio_action (_common_actions, snap_mode_group, X_("snap-magnetic"), _("Magnetic"), (sigc::bind (sigc::mem_fun(*this, &EditingContext::snap_mode_chosen), Editing::SnapMagnetic)));

	ActionManager::register_action (_common_actions, X_("cycle-snap-mode"), _("Toggle Snap"), sigc::mem_fun (*this, &EditingContext::cycle_snap_mode));
	ActionManager::register_action (_common_actions, X_("next-grid-choice"), _("Next Quantize Grid Choice"), sigc::mem_fun (*this, &EditingContext::next_grid_choice));
	ActionManager::register_action (_common_actions, X_("prev-grid-choice"), _("Previous Quantize Grid Choice"), sigc::mem_fun (*this, &EditingContext::prev_grid_choice));

	snap_actions = ActionManager::create_action_group (common_bindings, prefix + X_("Snap"));
	RadioAction::Group grid_choice_group;

	grid_actions[Editing::GridTypeBeatDiv32] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-thirtyseconds"),  grid_type_strings[(int)GridTypeBeatDiv32].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv32)));
	grid_actions[Editing::GridTypeBeatDiv28] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twentyeighths"),  grid_type_strings[(int)GridTypeBeatDiv28].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv28)));
	grid_actions[Editing::GridTypeBeatDiv24] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twentyfourths"),  grid_type_strings[(int)GridTypeBeatDiv24].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv24)));
	grid_actions[Editing::GridTypeBeatDiv20] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twentieths"),     grid_type_strings[(int)GridTypeBeatDiv20].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv20)));
	grid_actions[Editing::GridTypeBeatDiv16] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-asixteenthbeat"), grid_type_strings[(int)GridTypeBeatDiv16].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv16)));
	grid_actions[Editing::GridTypeBeatDiv14] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-fourteenths"),    grid_type_strings[(int)GridTypeBeatDiv14].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv14)));
	grid_actions[Editing::GridTypeBeatDiv12] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twelfths"),       grid_type_strings[(int)GridTypeBeatDiv12].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv12)));
	grid_actions[Editing::GridTypeBeatDiv10] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-tenths"),         grid_type_strings[(int)GridTypeBeatDiv10].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv10)));
	grid_actions[Editing::GridTypeBeatDiv8] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-eighths"),        grid_type_strings[(int)GridTypeBeatDiv8].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv8)));
	grid_actions[Editing::GridTypeBeatDiv7] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-sevenths"),       grid_type_strings[(int)GridTypeBeatDiv7].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv7)));
	grid_actions[Editing::GridTypeBeatDiv6] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-sixths"),         grid_type_strings[(int)GridTypeBeatDiv6].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv6)));
	grid_actions[Editing::GridTypeBeatDiv5] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-fifths"),         grid_type_strings[(int)GridTypeBeatDiv5].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv5)));
	grid_actions[Editing::GridTypeBeatDiv4] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-quarters"),       grid_type_strings[(int)GridTypeBeatDiv4].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv4)));
	grid_actions[Editing::GridTypeBeatDiv3] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-thirds"),         grid_type_strings[(int)GridTypeBeatDiv3].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv3)));
	grid_actions[Editing::GridTypeBeatDiv2] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-halves"),         grid_type_strings[(int)GridTypeBeatDiv2].c_str(),  (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeatDiv2)));

	grid_actions[Editing::GridTypeTimecode] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-timecode"),       grid_type_strings[(int)GridTypeTimecode].c_str(),      (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeTimecode)));
	grid_actions[Editing::GridTypeMinSec] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-minsec"),         grid_type_strings[(int)GridTypeMinSec].c_str(),    (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeMinSec)));
	grid_actions[Editing::GridTypeCDFrame] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-cdframe"),        grid_type_strings[(int)GridTypeCDFrame].c_str(), (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeCDFrame)));

	grid_actions[Editing::GridTypeBeat] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-beat"),           grid_type_strings[(int)GridTypeBeat].c_str(),      (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBeat)));
	grid_actions[Editing::GridTypeBar] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-bar"),            grid_type_strings[(int)GridTypeBar].c_str(),       (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeBar)));

	grid_actions[Editing::GridTypeNone] = ActionManager::register_radio_action (snap_actions, grid_choice_group, X_("grid-type-none"),           grid_type_strings[(int)GridTypeNone].c_str(),      (sigc::bind (sigc::mem_fun(*this, &EditingContext::grid_type_chosen), Editing::GridTypeNone)));


	grid_actions[Editing::GridTypeBeatDiv32]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv32]);
	grid_actions[Editing::GridTypeBeatDiv28]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv28]);
	grid_actions[Editing::GridTypeBeatDiv24]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv24]);
	grid_actions[Editing::GridTypeBeatDiv20]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv20]);
	grid_actions[Editing::GridTypeBeatDiv16]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv16]);
	grid_actions[Editing::GridTypeBeatDiv14]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv14]);
	grid_actions[Editing::GridTypeBeatDiv12]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv12]);
	grid_actions[Editing::GridTypeBeatDiv10]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv10]);
	grid_actions[Editing::GridTypeBeatDiv8]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv8]);
	grid_actions[Editing::GridTypeBeatDiv7]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv7]);
	grid_actions[Editing::GridTypeBeatDiv6]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv6]);
	grid_actions[Editing::GridTypeBeatDiv5]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv5]);
	grid_actions[Editing::GridTypeBeatDiv4]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv4]);
	grid_actions[Editing::GridTypeBeatDiv3]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv3]);
	grid_actions[Editing::GridTypeBeatDiv2]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv2]);

	grid_actions[Editing::GridTypeTimecode]->set_short_label (grid_type_short_labels[Editing::GridTypeTimecode]);
	grid_actions[Editing::GridTypeMinSec]->set_short_label (grid_type_short_labels[Editing::GridTypeMinSec]);
	grid_actions[Editing::GridTypeCDFrame]->set_short_label (grid_type_short_labels[Editing::GridTypeCDFrame]);
	grid_actions[Editing::GridTypeBeat]->set_short_label (grid_type_short_labels[Editing::GridTypeBeat]);
	grid_actions[Editing::GridTypeBar]->set_short_label (grid_type_short_labels[Editing::GridTypeBar]);
	grid_actions[Editing::GridTypeNone]->set_short_label (grid_type_short_labels[Editing::GridTypeNone]);
}

void
EditingContext::register_midi_actions (Bindings* midi_bindings, std::string const & prefix)
{
	EC_LOCAL_TEMPO_SCOPE;

	_midi_actions = ActionManager::create_action_group (midi_bindings, prefix + X_("Notes"));

	/* two versions to allow same action for Delete and Backspace */

	ActionManager::register_action (_midi_actions, X_("clear-selection"), _("Clear Note Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::clear_note_selection));
	ActionManager::register_action (_midi_actions, X_("invert-selection"), _("Invert Note Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::invert_selection));
	ActionManager::register_action (_midi_actions, X_("extend-selection"), _("Extend Note Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::extend_selection));
	ActionManager::register_action (_midi_actions, X_("duplicate-selection"), _("Duplicate Note Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::duplicate_selection));

	/* Lengthen */

	ActionManager::register_action (_midi_actions, X_("move-starts-earlier-fine"), _("Move Note Start Earlier (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_starts_earlier_fine));
	ActionManager::register_action (_midi_actions, X_("move-starts-earlier"), _("Move Note Start Earlier"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_starts_earlier));
	ActionManager::register_action (_midi_actions, X_("move-ends-later-fine"), _("Move Note Ends Later (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_ends_later_fine));
	ActionManager::register_action (_midi_actions, X_("move-ends-later"), _("Move Note Ends Later"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_ends_later));

	/* Shorten */

	ActionManager::register_action (_midi_actions, X_("move-starts-later-fine"), _("Move Note Start Later (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_starts_later_fine));
	ActionManager::register_action (_midi_actions, X_("move-starts-later"), _("Move Note Start Later"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_starts_later));
	ActionManager::register_action (_midi_actions, X_("move-ends-earlier-fine"), _("Move Note Ends Earlier (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_ends_earlier_fine));
	ActionManager::register_action (_midi_actions, X_("move-ends-earlier"), _("Move Note Ends Earlier"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiRegionView::move_note_ends_earlier));


	/* Alt versions allow bindings for both Tab and ISO_Left_Tab, if desired */

	ActionManager::register_action (_midi_actions, X_("select-next"), _("Select Next"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::select_next_note));
	ActionManager::register_action (_midi_actions, X_("alt-select-next"), _("Select Next (alternate)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::select_next_note));
	ActionManager::register_action (_midi_actions, X_("select-previous"), _("Select Previous"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::select_previous_note));
	ActionManager::register_action (_midi_actions, X_("alt-select-previous"), _("Select Previous (alternate)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::select_previous_note));
	ActionManager::register_action (_midi_actions, X_("add-select-next"), _("Add Next to Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::add_select_next_note));
	ActionManager::register_action (_midi_actions, X_("alt-add-select-next"), _("Add Next to Selection (alternate)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::add_select_next_note));
	ActionManager::register_action (_midi_actions, X_("add-select-previous"), _("Add Previous to Selection"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::add_select_previous_note));
	ActionManager::register_action (_midi_actions, X_("alt-add-select-previous"), _("Add Previous to Selection (alternate)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::add_select_previous_note));

	ActionManager::register_action (_midi_actions, X_("increase-velocity"), _("Increase Velocity"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::increase_note_velocity));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine"), _("Increase Velocity (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::increase_note_velocity_fine));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-smush"), _("Increase Velocity (allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::increase_note_velocity_smush));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-together"), _("Increase Velocity (non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::increase_note_velocity_together));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-smush"), _("Increase Velocity (fine, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::increase_note_velocity_fine_smush));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-together"), _("Increase Velocity (fine, non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::increase_note_velocity_fine_together));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-smush-together"), _("Increase Velocity (maintain ratios, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::increase_note_velocity_smush_together));
	ActionManager::register_action (_midi_actions, X_("increase-velocity-fine-smush-together"), _("Increase Velocity (fine, allow mush, non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::increase_note_velocity_fine_smush_together));

	ActionManager::register_action (_midi_actions, X_("decrease-velocity"), _("Decrease Velocity"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::decrease_note_velocity));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine"), _("Decrease Velocity (fine)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::decrease_note_velocity_fine));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-smush"), _("Decrease Velocity (allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::decrease_note_velocity_smush));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-together"), _("Decrease Velocity (non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::decrease_note_velocity_together));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-smush"), _("Decrease Velocity (fine, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::decrease_note_velocity_fine_smush));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-together"), _("Decrease Velocity (fine, non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::decrease_note_velocity_fine_together));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-smush-together"), _("Decrease Velocity (maintain ratios, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::decrease_note_velocity_smush_together));
	ActionManager::register_action (_midi_actions, X_("decrease-velocity-fine-smush-together"), _("Decrease Velocity (fine, allow mush, non-relative)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::decrease_note_velocity_fine_smush_together));

	ActionManager::register_action (_midi_actions, X_("transpose-up-octave"), _("Transpose Up (octave)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::transpose_up_octave));
	ActionManager::register_action (_midi_actions, X_("transpose-up-octave-smush"), _("Transpose Up (octave, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::transpose_up_octave_smush));
	ActionManager::register_action (_midi_actions, X_("transpose-up-semitone"), _("Transpose Up (semitone)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::transpose_up_tone));
	ActionManager::register_action (_midi_actions, X_("transpose-up-semitone-smush"), _("Transpose Up (semitone, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::transpose_up_octave_smush));

	ActionManager::register_action (_midi_actions, X_("transpose-down-octave"), _("Transpose Down (octave)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::transpose_down_octave));
	ActionManager::register_action (_midi_actions, X_("transpose-down-octave-smush"), _("Transpose Down (octave, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::transpose_down_octave_smush));
	ActionManager::register_action (_midi_actions, X_("transpose-down-semitone"), _("Transpose Down (semitone)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::transpose_down_tone));
	ActionManager::register_action (_midi_actions, X_("transpose-down-semitone-smush"), _("Transpose Down (semitone, allow mush)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::transpose_down_octave_smush));

	ActionManager::register_action (_midi_actions, X_("nudge-later"), _("Nudge Notes Later (grid)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::nudge_notes_later));
	ActionManager::register_action (_midi_actions, X_("nudge-later-fine"), _("Nudge Notes Later (1/4 grid)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::nudge_notes_later_fine));
	ActionManager::register_action (_midi_actions, X_("nudge-earlier"), _("Nudge Notes Earlier (grid)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::nudge_notes_earlier));
	ActionManager::register_action (_midi_actions, X_("nudge-earlier-fine"), _("Nudge Notes Earlier (1/4 grid)"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::nudge_notes_earlier_fine));

	ActionManager::register_action (_midi_actions, X_("split-notes-grid"), _("Split Selected Notes on grid boundaries"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::split_notes_grid));
	ActionManager::register_action (_midi_actions, X_("split-notes-more"), _("Split Selected Notes into more pieces"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::split_notes_more));
	ActionManager::register_action (_midi_actions, X_("split-notes-less"), _("Split Selected Notes into less pieces"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::split_notes_less));
	ActionManager::register_action (_midi_actions, X_("join-notes"), _("Join Selected Notes"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::join_notes));

	ActionManager::register_action (_midi_actions, X_("edit-channels"), _("Edit Note Channels"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::channel_edit));
	ActionManager::register_action (_midi_actions, X_("edit-velocities"), _("Edit Note Velocities"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action), &MidiView::velocity_edit));

	ActionManager::register_action (_midi_actions, X_("quantize-selected-notes"), _("Quantize Selected Notes"), sigc::bind (sigc::mem_fun (*this, &EditingContext::midi_action),  &MidiView::quantize_selected_notes));

	length_actions = ActionManager::create_action_group (midi_bindings, prefix + X_("DrawLength"));
	RadioAction::Group draw_length_group;

	draw_length_actions[Editing::GridTypeBeatDiv32] = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-thirtyseconds"),  grid_type_strings[(int)GridTypeBeatDiv32].c_str(), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv32));
	draw_length_actions[Editing::GridTypeBeatDiv28] = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentyeighths"),  grid_type_strings[(int)GridTypeBeatDiv28].c_str(), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv28));
	draw_length_actions[Editing::GridTypeBeatDiv24] = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentyfourths"),  grid_type_strings[(int)GridTypeBeatDiv24].c_str(), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv24));
	draw_length_actions[Editing::GridTypeBeatDiv20] = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twentieths"),     grid_type_strings[(int)GridTypeBeatDiv20].c_str(), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv20));
	draw_length_actions[Editing::GridTypeBeatDiv16] = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-asixteenthbeat"), grid_type_strings[(int)GridTypeBeatDiv16].c_str(), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv16));
	draw_length_actions[Editing::GridTypeBeatDiv14] = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-fourteenths"),    grid_type_strings[(int)GridTypeBeatDiv14].c_str(), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv14));
	draw_length_actions[Editing::GridTypeBeatDiv12] = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-twelfths"),       grid_type_strings[(int)GridTypeBeatDiv12].c_str(), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv12));
	draw_length_actions[Editing::GridTypeBeatDiv10] = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-tenths"),         grid_type_strings[(int)GridTypeBeatDiv10].c_str(), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv10));
	draw_length_actions[Editing::GridTypeBeatDiv8]  = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-eighths"),        grid_type_strings[(int)GridTypeBeatDiv8].c_str(),  sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv8));
	draw_length_actions[Editing::GridTypeBeatDiv7]  = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-sevenths"),       grid_type_strings[(int)GridTypeBeatDiv7].c_str(),  sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv7));
	draw_length_actions[Editing::GridTypeBeatDiv6]  = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-sixths"),         grid_type_strings[(int)GridTypeBeatDiv6].c_str(),  sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv6));
	draw_length_actions[Editing::GridTypeBeatDiv5]  = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-fifths"),         grid_type_strings[(int)GridTypeBeatDiv5].c_str(),  sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv5));
	draw_length_actions[Editing::GridTypeBeatDiv4]  = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-quarters"),       grid_type_strings[(int)GridTypeBeatDiv4].c_str(),  sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv4));
	draw_length_actions[Editing::GridTypeBeatDiv3]  = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-thirds"),         grid_type_strings[(int)GridTypeBeatDiv3].c_str(),  sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv3));
	draw_length_actions[Editing::GridTypeBeatDiv2]  = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-halves"),         grid_type_strings[(int)GridTypeBeatDiv2].c_str(),  sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeatDiv2));
	draw_length_actions[Editing::GridTypeBeat]      = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-beat"),           grid_type_strings[(int)GridTypeBeat].c_str(),      sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBeat));
	draw_length_actions[Editing::GridTypeBar]       = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-bar"),            grid_type_strings[(int)GridTypeBar].c_str(),       sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), Editing::GridTypeBar));
	draw_length_actions[DRAW_LEN_AUTO]              = ActionManager::register_radio_action (length_actions, draw_length_group, X_("draw-length-auto"),           _("Auto"),                                         sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_length_chosen), DRAW_LEN_AUTO));

	draw_length_actions[Editing::GridTypeBeatDiv32]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv32]);
	draw_length_actions[Editing::GridTypeBeatDiv28]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv28]);
	draw_length_actions[Editing::GridTypeBeatDiv24]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv24]);
	draw_length_actions[Editing::GridTypeBeatDiv20]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv20]);
	draw_length_actions[Editing::GridTypeBeatDiv16]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv16]);
	draw_length_actions[Editing::GridTypeBeatDiv14]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv14]);
	draw_length_actions[Editing::GridTypeBeatDiv12]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv12]);
	draw_length_actions[Editing::GridTypeBeatDiv10]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv10]);
	draw_length_actions[Editing::GridTypeBeatDiv8]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv8]);
	draw_length_actions[Editing::GridTypeBeatDiv7]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv7]);
	draw_length_actions[Editing::GridTypeBeatDiv6]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv6]);
	draw_length_actions[Editing::GridTypeBeatDiv5]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv5]);
	draw_length_actions[Editing::GridTypeBeatDiv4]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv4]);
	draw_length_actions[Editing::GridTypeBeatDiv3]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv3]);
	draw_length_actions[Editing::GridTypeBeatDiv2]->set_short_label (grid_type_short_labels[Editing::GridTypeBeatDiv2]);
	draw_length_actions[Editing::DRAW_LEN_AUTO]->set_short_label (_("Auto"));

	velocity_actions = ActionManager::create_action_group (midi_bindings, prefix + X_("DrawVelocity"));
	RadioAction::Group draw_velocity_group;
	draw_velocity_actions[DRAW_VEL_AUTO] = ActionManager::register_radio_action (velocity_actions, draw_velocity_group, X_("draw-velocity-auto"),  _("Auto"), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_velocity_chosen), DRAW_VEL_AUTO));
	for (int i = 1; i <= 127; i++) {
		char buf[64];
		snprintf(buf, sizeof (buf), X_("draw-velocity-%d"), i);
		char vel[64];
		sprintf(vel, _("Velocity %d"), i);
		draw_velocity_actions[i] = ActionManager::register_radio_action (velocity_actions, draw_velocity_group, buf, vel, sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_velocity_chosen), i));
		snprintf (buf,sizeof (buf), "%d", i);
		draw_velocity_actions[i]->set_short_label (buf);
	}

	channel_actions = ActionManager::create_action_group (midi_bindings, prefix + X_("DrawChannel"));
	RadioAction::Group draw_channel_group;
	draw_channel_actions[DRAW_CHAN_AUTO] = ActionManager::register_radio_action (channel_actions, draw_channel_group, X_("draw-channel-auto"),  _("Auto"), sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_channel_chosen), DRAW_CHAN_AUTO));
	for (int i = 0; i <= 15; i++) {
		char buf[64];
		snprintf(buf, sizeof (buf), X_("draw-channel-%d"), i+1);
		char ch[64];
		sprintf(ch, X_("Channel %d"), i+1);
		draw_channel_actions[i] = ActionManager::register_radio_action (channel_actions, draw_channel_group, buf, ch, sigc::bind (sigc::mem_fun (*this, &EditingContext::draw_channel_chosen), i));
	}

	ActionManager::set_sensitive (_midi_actions, false);
}

void
EditingContext::midi_action (void (MidiView::*method)())
{
	EC_LOCAL_TEMPO_SCOPE;

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
EditingContext::next_grid_choice ()
{
	EC_LOCAL_TEMPO_SCOPE;

	switch (grid_type()) {
	case Editing::GridTypeBeatDiv32:
		set_grid_type (Editing::GridTypeNone);
		break;
	case Editing::GridTypeBeatDiv16:
		set_grid_type (Editing::GridTypeBeatDiv32);
		break;
	case Editing::GridTypeBeatDiv8:
		set_grid_type (Editing::GridTypeBeatDiv16);
		break;
	case Editing::GridTypeBeatDiv4:
		set_grid_type (Editing::GridTypeBeatDiv8);
		break;
	case Editing::GridTypeBeatDiv2:
		set_grid_type (Editing::GridTypeBeatDiv4);
		break;
	case Editing::GridTypeBeat:
		set_grid_type (Editing::GridTypeBeatDiv2);
		break;
	case Editing::GridTypeBar:
		set_grid_type (Editing::GridTypeBeat);
		break;
	case Editing::GridTypeNone:
		set_grid_type (Editing::GridTypeBar);
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
	EC_LOCAL_TEMPO_SCOPE;

	switch (grid_type()) {
	case Editing::GridTypeBeatDiv32:
		set_grid_type (Editing::GridTypeBeatDiv16);
		break;
	case Editing::GridTypeBeatDiv16:
		set_grid_type (Editing::GridTypeBeatDiv8);
		break;
	case Editing::GridTypeBeatDiv8:
		set_grid_type (Editing::GridTypeBeatDiv4);
		break;
	case Editing::GridTypeBeatDiv4:
		set_grid_type (Editing::GridTypeBeatDiv2);
		break;
	case Editing::GridTypeBeatDiv2:
		set_grid_type (Editing::GridTypeBeat);
		break;
	case Editing::GridTypeBeat:
		set_grid_type (Editing::GridTypeBar);
		break;
	case Editing::GridTypeBar:
		set_grid_type (Editing::GridTypeNone);
		break;
	case Editing::GridTypeNone:
		set_grid_type (Editing::GridTypeBeatDiv32);
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
EditingContext::grid_type_chosen (GridType gt)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	auto ti = grid_actions.find (gt);
	assert (ti != grid_actions.end());

	if (!ti->second->get_active()) {
		return;
	}

	unsigned int grid_ind = (unsigned int) gt;

	if (internal_editing() && UIConfiguration::instance().get_grid_follows_internal()) {
		internal_grid_type = gt;
	} else {
		pre_internal_grid_type = gt;
	}

	grid_type_selector.set_active (grid_type_short_labels[grid_ind]);

	if (UIConfiguration::instance().get_show_grids_ruler()) {
		show_rulers_for_grid ();
	}

	instant_save ();

	const bool grid_is_musical = grid_musical ();

	if (grid_is_musical) {
		compute_bbt_ruler_scale (_leftmost_sample, _leftmost_sample + current_page_samples());
		update_tempo_based_rulers ();
	} else if (current_mouse_mode () == Editing::MouseGrid) {
		mouse_mode_actions[Editing::MouseObject]->set_active (true);
	}

	mouse_mode_actions[Editing::MouseGrid]->set_sensitive (grid_is_musical);

	mark_region_boundary_cache_dirty ();

	redisplay_grid (false);

	SnapChanged (); /* EMIT SIGNAL */
}

void
EditingContext::draw_length_chosen (GridType type)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = draw_length_actions[type];

	if (!ract->get_active()) {
		return;
	}

	if ((DRAW_LEN_AUTO != type) && !grid_type_is_musical (type) ) {  // is this is sensible sanity check ?
		set_draw_length (DRAW_LEN_AUTO);
		return;
	}

	if (DRAW_LEN_AUTO == (unsigned int) type) {
		draw_length_selector.set_active (_("Auto"));
	} else {
		draw_length_selector.set_active (grid_type_short_labels[(unsigned int) type]);
	}

	instant_save ();
}

void
EditingContext::draw_velocity_chosen (int v)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract;

	if (v == DRAW_VEL_AUTO) {
		ract = draw_velocity_actions[DRAW_VEL_AUTO];
	} else {
		ract = draw_velocity_actions[std::max (std::min (v, 127), 0)];
	}

	if (!ract->get_active()) {
		return;
	}

	if (DRAW_VEL_AUTO == v) {
		draw_velocity_selector.set_active (_("Auto"));
	} else {
		char buf[64];
		snprintf (buf, sizeof (buf), "%d", v);
		draw_velocity_selector.set_text (buf);
	}

	instant_save ();
}

void
EditingContext::draw_channel_chosen (int c)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract;

	if (c == DRAW_CHAN_AUTO) {
		ract = draw_channel_actions[DRAW_CHAN_AUTO];
	} else {
		ract = draw_channel_actions[std::max (std::min (c, 5), 0)];
	}

	if (!ract->get_active()) {
		return;
	}

	if (DRAW_CHAN_AUTO == c) {
		draw_channel_selector.set_active (_("Auto"));
	} else {
		char buf[64];
		snprintf (buf, sizeof (buf), "%d", c+1 );
		draw_channel_selector.set_active (buf);
	}

	instant_save ();
}

void
EditingContext::cycle_snap_mode ()
{
	EC_LOCAL_TEMPO_SCOPE;

	switch (snap_mode()) {
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
	EC_LOCAL_TEMPO_SCOPE;

	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	if (mode == SnapNormal) {
		mode = SnapMagnetic;
	}

	if (!snap_mode_actions[mode]->get_active()) {
		return;
	}

	if (internal_editing()) {
		internal_snap_mode = mode;
	} else {
		pre_internal_snap_mode = mode;
	}

	if (mode == SnapOff) {
		snap_mode_button.set_active_state (Gtkmm2ext::Off);
	} else {
		snap_mode_button.set_active_state (Gtkmm2ext::ExplicitActive);
	}

	instant_save ();
}

GridType
EditingContext::grid_type() const
{
	EC_LOCAL_TEMPO_SCOPE;

	for (auto const & [grid_type,action] : grid_actions) {
		if (action->get_active()) {
			return grid_type;
		}
	}

	return Editing::GridTypeNone;
}

GridType
EditingContext::draw_length() const
{
	EC_LOCAL_TEMPO_SCOPE;

	for (auto const & [len,action] : draw_length_actions) {
		if (action->get_active()) {
			return len;
		}
	}

	return GridTypeBeat;
}

int
EditingContext::draw_velocity() const
{
	EC_LOCAL_TEMPO_SCOPE;

	for (auto const & [vel,action] : draw_velocity_actions) {
		if (action->get_active()) {
			return vel;
		}
	}

	return 64;
}

int
EditingContext::draw_channel() const
{
	EC_LOCAL_TEMPO_SCOPE;

	for (auto const & [chn,action] : draw_channel_actions) {
		if (action->get_active()) {
			return chn;
		}
	}

	return 0;
}

bool
EditingContext::grid_musical() const
{
	EC_LOCAL_TEMPO_SCOPE;

	return grid_type_is_musical (grid_type());
}

bool
EditingContext::grid_type_is_musical(GridType gt) const
{
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	for (auto const & [mode,action] : snap_mode_actions) {
		if (action->get_active()) {
			return mode;
		}
	}

	return Editing::SnapOff;
}

void
EditingContext::set_draw_length (GridType gt)
{
	EC_LOCAL_TEMPO_SCOPE;

	draw_length_actions[gt]->set_active (true);
}

void
EditingContext::set_draw_velocity (int v)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (v == DRAW_VEL_AUTO) {
		draw_velocity_actions[v]->set_active (true);
	} else {
		draw_velocity_actions[std::max (std::min (v, 127), 0)]->set_active (true);
	}
}

void
EditingContext::set_draw_channel (int c)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (c == DRAW_CHAN_AUTO) {
		draw_channel_actions[c]->set_active (true);
	} else {
		draw_channel_actions[std::max (std::min (c, 15), 0)]->set_active (true);
	}
}

void
EditingContext::set_grid_type (GridType gt)
{
	EC_LOCAL_TEMPO_SCOPE;

	grid_actions[gt]->set_active (true);
}

void
EditingContext::set_snap_mode (SnapMode mode)
{
	EC_LOCAL_TEMPO_SCOPE;

	snap_mode_actions[mode]->set_active (true);;
}

void
EditingContext::build_grid_type_menu ()
{
	EC_LOCAL_TEMPO_SCOPE;

	using namespace Menu_Helpers;

	/* there's no Grid, but if Snap is engaged, the Snap preferences will be applied */
	grid_type_selector.append (grid_actions[GridTypeNone]);
	grid_type_selector.add_separator ();

	/* musical grid: bars, quarter-notes, etc */
	grid_type_selector.append (grid_actions[GridTypeBar]);
	grid_type_selector.append (grid_actions[GridTypeBeat]);
	grid_type_selector.append (grid_actions[GridTypeBeatDiv2]);
	grid_type_selector.append (grid_actions[GridTypeBeatDiv4]);
	grid_type_selector.append (grid_actions[GridTypeBeatDiv8]);
	grid_type_selector.append (grid_actions[GridTypeBeatDiv16]);
	grid_type_selector.append (grid_actions[GridTypeBeatDiv32]);

	/* triplet grid */
	grid_type_selector.add_separator ();;
	Gtk::Menu *triplet_menu = manage (new Menu);
	{
		grid_type_selector.append (*triplet_menu, grid_actions[GridTypeBeatDiv3]);
		grid_type_selector.append (*triplet_menu, grid_actions[GridTypeBeatDiv6]);
		grid_type_selector.append (*triplet_menu, grid_actions[GridTypeBeatDiv12]);
		grid_type_selector.append (*triplet_menu, grid_actions[GridTypeBeatDiv24]);
	}
	grid_type_selector.add_menu_elem (Menu_Helpers::MenuElem (_("Triplets"), *triplet_menu));

	/* quintuplet grid */
	Gtk::Menu *quintuplet_menu = manage (new Menu);
	{
		grid_type_selector.append (*quintuplet_menu, grid_actions[GridTypeBeatDiv5]);
		grid_type_selector.append (*quintuplet_menu, grid_actions[GridTypeBeatDiv10]);
		grid_type_selector.append (*quintuplet_menu, grid_actions[GridTypeBeatDiv20]);
	}

	grid_type_selector.add_menu_elem (Menu_Helpers::MenuElem (_("Quintuplets"), *quintuplet_menu));

	/* septuplet grid */
	Gtk::Menu *septuplet_menu = manage (new Menu);
	{
		grid_type_selector.append (*septuplet_menu, grid_actions[GridTypeBeatDiv7]);
		grid_type_selector.append (*septuplet_menu, grid_actions[GridTypeBeatDiv14]);
		grid_type_selector.append (*septuplet_menu, grid_actions[GridTypeBeatDiv28]);
	}
	grid_type_selector.add_menu_elem (Menu_Helpers::MenuElem (_("Septuplets"), *septuplet_menu));

	grid_type_selector.add_separator ();
	grid_type_selector.append (grid_actions[GridTypeTimecode]);
	grid_type_selector.append (grid_actions[GridTypeMinSec]);
	grid_type_selector.append (grid_actions[GridTypeCDFrame]);

	grid_type_selector.set_sizing_texts (grid_type_short_labels);
}

void
EditingContext::build_draw_midi_menus ()
{
	EC_LOCAL_TEMPO_SCOPE;

	using namespace Menu_Helpers;

	/* Note-Length when drawing */

	std::vector<GridType> grids ({
			GridTypeBeat,
			GridTypeBeatDiv2,
			GridTypeBeatDiv4,
			GridTypeBeatDiv8,
			GridTypeBeatDiv16,
			GridTypeBeatDiv32,
			GridTypeNone});
	std::vector<std::string> draw_grid_type_strings;

	for (auto & g : grids) {
		Glib::RefPtr<RadioAction> ract = draw_length_actions[g];
		draw_length_selector.append (ract);
		draw_grid_type_strings.push_back (ract->get_short_label());

	}

	draw_length_selector.set_sizing_texts (draw_grid_type_strings);

	/* Note-Velocity when drawing */

	std::vector<int> preselected_velocities ({8,32,64,82,100,127, DRAW_VEL_AUTO});
	std::vector<std::string> draw_velocity_strings;

	for (auto & v : preselected_velocities) {
		Glib::RefPtr<RadioAction> ract = draw_velocity_actions[v];
		assert (ract);
		draw_velocity_selector.append (ract);
		draw_velocity_strings.push_back (ract->get_short_label());
	}

	draw_velocity_selector.set_sizing_texts (draw_velocity_strings);

	std::vector<int> possible_channels ({0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15, DRAW_CHAN_AUTO});
	std::vector<std::string> draw_channel_strings;

	for (auto & c : possible_channels) {
		Glib::RefPtr<RadioAction> ract = draw_channel_actions[c];
		assert (ract);
		draw_channel_selector.append (ract);
		draw_channel_strings.push_back (ract->get_short_label());
	}

	draw_channel_selector.set_sizing_texts (draw_channel_strings);

	draw_channel_chosen (draw_channel());
	draw_length_chosen (draw_length());
	draw_velocity_chosen (draw_velocity());
}

bool
EditingContext::drag_active () const
{
	EC_LOCAL_TEMPO_SCOPE;

	return _drags->active();
}

bool
EditingContext::preview_video_drag_active () const
{
	EC_LOCAL_TEMPO_SCOPE;

	return _drags->preview_video ();
}

Temporal::TimeDomain
EditingContext::time_domain () const
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_session) {
		return _session->config.get_default_time_domain();
	}

	/* Probably never reached */

	if (snap_mode() == SnapOff) {
		return Temporal::AudioTime;
	}

	switch (grid_type()) {
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
EditingContext::toggle_stationary_playhead ()
{
	EC_LOCAL_TEMPO_SCOPE;

	stationary_playhead_action->set_active (!stationary_playhead_action->get_active ());
}

void
EditingContext::stationary_playhead_chosen ()
{
	EC_LOCAL_TEMPO_SCOPE;

	instant_save ();
}

void
EditingContext::set_stationary_playhead (bool yn)
{
	EC_LOCAL_TEMPO_SCOPE;

	stationary_playhead_action->set_active (yn);
}

bool
EditingContext::stationary_playhead () const
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!stationary_playhead_action) {
		return false;
	}

	return stationary_playhead_action->get_active ();
}

void
EditingContext::toggle_follow_playhead ()
{
	EC_LOCAL_TEMPO_SCOPE;

	set_follow_playhead (!follow_playhead_action->get_active(), true);
}

void
EditingContext::follow_playhead_chosen ()
{
	EC_LOCAL_TEMPO_SCOPE;

	instant_save ();
}

/** @param yn true to follow playhead, otherwise false.
 *  @param catch_up true to reset the editor view to show the playhead (if yn == true), otherwise false.
 */
void
EditingContext::set_follow_playhead (bool yn, bool catch_up)
{
	EC_LOCAL_TEMPO_SCOPE;

	assert (follow_playhead_action);
	follow_playhead_action->set_active (yn);
	if (yn && catch_up) {
		/* catch up */
		reset_x_origin_to_follow_playhead ();
	}
}

bool
EditingContext::follow_playhead() const
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!follow_playhead_action) {
		return false;
	}

	return follow_playhead_action->get_active ();
}

double
EditingContext::time_to_pixel (timepos_t const & pos) const
{
	EC_LOCAL_TEMPO_SCOPE;

	return sample_to_pixel (pos.samples());
}

double
EditingContext::time_to_pixel_unrounded (timepos_t const & pos) const
{
	EC_LOCAL_TEMPO_SCOPE;

	return sample_to_pixel_unrounded (pos.samples());
}

double
EditingContext::time_delta_to_pixel (timepos_t const& start, timepos_t const& end) const
{
	EC_LOCAL_TEMPO_SCOPE;

	return sample_to_pixel (end.samples()) - sample_to_pixel (start.samples ());
}

double
EditingContext::duration_to_pixels (timecnt_t const & dur) const
{
	EC_LOCAL_TEMPO_SCOPE;

	return sample_to_pixel (dur.samples());
}

double
EditingContext::duration_to_pixels_unrounded (timecnt_t const & dur) const
{
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	if (!_session || !event) {
		return;
	}

	if (ArdourKeyboard::indicates_snap (event->button.state)) {
		if (snap_mode() == SnapOff) {
			snap_to_internal (start, direction, pref, ensure_snap);
		}

	} else {
		if (snap_mode() != SnapOff) {
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
	EC_LOCAL_TEMPO_SCOPE;

	if (!_session || (snap_mode() == SnapOff && !ensure_snap)) {
		return;
	}

	snap_to_internal (start, direction, pref, ensure_snap);
}

timepos_t
EditingContext::snap_to_bbt (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref) const
{
	EC_LOCAL_TEMPO_SCOPE;

	return snap_to_bbt_via_grid (presnap, direction, gpref, grid_type());
}

timepos_t
EditingContext::snap_to_bbt_via_grid (timepos_t const & presnap, Temporal::RoundMode direction, SnapPref gpref, GridType grid_type) const
{
	EC_LOCAL_TEMPO_SCOPE;

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
		switch (grid_type) {
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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	timepos_t pos (canvas_event_sample (event, pcx, pcy));

	if (time_domain() == Temporal::AudioTime) {
		return pos;
	}

	return timepos_t (pos.beats());
}

samplepos_t
EditingContext::canvas_event_sample (GdkEvent const * event, double* pcx, double* pcy) const
{
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	GridType gt (grid_type());
	int divs = get_grid_music_divisions(gt, 0);
	if (gt == GridTypeBar) {
		suggested_scale = std::min(suggested_scale, (int) bbt_show_1);
	} else if (gt == GridTypeBeat) {
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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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

bool
EditingContext::typed_event (ArdourCanvas::Item* item, GdkEvent *event, ItemType type)
{
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	using namespace Menu_Helpers;

	NoteBase* note = reinterpret_cast<NoteBase*>(item->get_data("notebase"));
	if (!note) {
		return;
	}

	/* We need to get the selection here and pass it to the operations, since
	   popping up the menu will cause a region leave event which clears
	   entered_regionview. */

	MidiView&       mrv = note->midi_view();
	const uint32_t sel_size = mrv.selection_size ();
	MidiViews mvs (midiviews_from_region_selection (region_selection ()));

	if (std::find (mvs.begin(), mvs.end(), &mrv) == mvs.end()) {
		mvs.push_back (&mrv);
	}

	MenuList& items = _note_context_menu.items();
	items.clear();

	if (sel_size > 0) {
		items.push_back (MenuElem(_("Delete"), sigc::mem_fun(mrv, &MidiView::delete_selection)));
	}

	items.push_back(MenuElem(_("Edit..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::edit_notes), &mrv)));
	items.push_back(MenuElem(_("Transpose..."),  sigc::bind(sigc::mem_fun(*this, &EditingContext::transpose_regions), mvs)));
	items.push_back(MenuElem(_("Legatize"), sigc::bind(sigc::mem_fun(*this, &EditingContext::legatize_regions), mvs, false)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}
	items.push_back(MenuElem(_("Quantize..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::quantize_regions), mvs)));
	items.push_back(MenuElem(_("Remove Overlap"), sigc::bind(sigc::mem_fun(*this, &EditingContext::legatize_regions), mvs, true)));
	if (sel_size < 2) {
		items.back().set_sensitive (false);
	}
	items.push_back(MenuElem(_("Transform..."), sigc::bind(sigc::mem_fun(*this, &EditingContext::transform_regions), mvs)));

	_note_context_menu.popup (event->button.button, event->button.time);
}

XMLNode*
EditingContext::button_settings () const
{
	EC_LOCAL_TEMPO_SCOPE;

	XMLNode* settings = ARDOUR_UI::instance()->editor_settings();
	XMLNode* node = find_named_node (*settings, X_("Buttons"));

	if (!node) {
		node = new XMLNode (X_("Buttons"));
	}

	return node;
}

EditingContext::MidiViews
EditingContext::filter_to_unique_midi_region_views (RegionSelection const & rs) const
{
	EC_LOCAL_TEMPO_SCOPE;

	return filter_to_unique_midi_region_views (midiviews_from_region_selection (rs));
}

EditingContext::MidiViews
EditingContext::filter_to_unique_midi_region_views (MidiViews const & mvs) const
{
	EC_LOCAL_TEMPO_SCOPE;

	typedef std::pair<std::shared_ptr<MidiSource>,timepos_t> MapEntry;
	std::set<MapEntry> single_region_set;

	MidiViews views;

	/* build a list of regions that are unique with respect to their source
	 * and start position. Note: this is non-exhaustive... if someone has a
	 * non-forked copy of a MIDI region and then suitably modifies it, this
	 * will still put both regions into the list of things to be acted
	 * upon.
	 *
	 * Solution: user should not select both regions, or should fork one of them.
	 */

	for (auto const & mv : mvs) {

		MapEntry entry = make_pair (mv->midi_region()->midi_source(), mv->midi_region()->start());

		if (single_region_set.insert (entry).second) {
			views.push_back (mv);
		}
	}

	return views;
}

EditingContext::MidiViews
EditingContext::midiviews_from_region_selection (RegionSelection const & rs) const
{
	EC_LOCAL_TEMPO_SCOPE;

	MidiViews views;

	for (auto & rv : rs) {
		MidiView* mrv = dynamic_cast<MidiView*> (rv);
		if (mrv) {
			views.push_back (mrv);
		}
	}

	return views;
}

void
EditingContext::quantize_region ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_session) {
		quantize_regions (midiviews_from_region_selection (region_selection()));
	}
}

void
EditingContext::quantize_regions (const MidiViews& rs)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (rs.empty()) {
		std::cerr << "no regions\n";
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
	EC_LOCAL_TEMPO_SCOPE;

	if (_session) {
		legatize_regions (midiviews_from_region_selection (region_selection ()), shrink_only);
	}
}

void
EditingContext::legatize_regions (const MidiViews& rs, bool shrink_only)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (rs.empty()) {
		return;
	}

	Legatize legatize (shrink_only);
	apply_midi_note_edit_op (legatize, rs);
}

void
EditingContext::transform_region ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_session) {
		transform_regions (midiviews_from_region_selection (region_selection ()));
	}
}

void
EditingContext::transform_regions (const MidiViews& rs)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (rs.empty()) {
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
	EC_LOCAL_TEMPO_SCOPE;

	if (_session) {
		transpose_regions (midiviews_from_region_selection (region_selection ()));
	}
}

void
EditingContext::transpose_regions (const MidiViews& rs)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (rs.empty()) {
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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	d->done (r);
	delete d;
}

PBD::Command*
EditingContext::apply_midi_note_edit_op_to_region (MidiOperator& op, MidiView& mrv)
{
	EC_LOCAL_TEMPO_SCOPE;

	Evoral::Sequence<Temporal::Beats>::Notes selected;
	mrv.selection_as_notelist (selected, true);

	if (selected.empty()) {
		return 0;
	}

	std::vector<Evoral::Sequence<Temporal::Beats>::Notes> v;
	v.push_back (selected);

	timepos_t pos = mrv.midi_region()->source_position();

	return op (mrv.midi_region()->model(), pos.beats(), v);
}

void
EditingContext::apply_midi_note_edit_op (MidiOperator& op, const RegionSelection& rs)
{
	EC_LOCAL_TEMPO_SCOPE;

	apply_midi_note_edit_op (op, midiviews_from_region_selection (rs));
}

void
EditingContext::apply_midi_note_edit_op (MidiOperator& op, const MidiViews& rs)
{
	EC_LOCAL_TEMPO_SCOPE;

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
			add_command (cmd);
			}
	}

	if (in_command) {
		commit_reversible_command ();
		_session->set_dirty ();
	}
}

double
EditingContext::horizontal_position () const
{
	EC_LOCAL_TEMPO_SCOPE;

	return horizontal_adjustment.get_value();
}

void
EditingContext::set_horizontal_position (double pixel)
{
	EC_LOCAL_TEMPO_SCOPE;

	pixel = std::max (0., pixel);

	_leftmost_sample = (samplepos_t) floor (pixel * samples_per_pixel);
	horizontal_adjustment.set_value (pixel);
}

Gdk::Cursor*
EditingContext::get_canvas_cursor () const
{
	EC_LOCAL_TEMPO_SCOPE;

	Glib::RefPtr<Gdk::Window> win = get_canvas_viewport()->get_window();

	if (win) {
		return _cursors->from_gdk_cursor (gdk_window_get_cursor (win->gobj()));
	}

	return nullptr;
}

void
EditingContext::set_canvas_cursor (Gdk::Cursor* cursor)
{
	EC_LOCAL_TEMPO_SCOPE;

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
EditingContext::pack_draw_box (bool with_channel)
{
	EC_LOCAL_TEMPO_SCOPE;

	/* Draw  - these MIDI tools are only visible when in Draw mode */
	draw_box.set_spacing (2);
	draw_box.set_border_width (2);
	draw_box.pack_start (*manage (new Label (_("Len:"))), false, false);
	draw_box.pack_start (draw_length_selector, false, false, 4);
	if (with_channel) {
		draw_box.pack_start (*manage (new Label (S_("MIDI|Ch:"))), false, false);
		draw_box.pack_start (draw_channel_selector, false, false, 4);
	}
	draw_box.pack_start (*manage (new Label (_("Vel:"))), false, false);
	draw_box.pack_start (draw_velocity_selector, false, false, 4);

	draw_length_selector.set_name ("mouse mode button");
	draw_velocity_selector.set_name ("mouse mode button");
	draw_channel_selector.set_name ("mouse mode button");

	draw_velocity_selector.set_sizing_text (_("Auto"));
	draw_channel_selector.set_sizing_text (_("Auto"));

	draw_velocity_selector.disable_scrolling ();
	draw_velocity_selector.signal_scroll_event().connect (sigc::mem_fun(*this, &EditingContext::on_velocity_scroll_event), false);

	draw_box.show_all_children ();
	draw_box.set_no_show_all ();
}

void
EditingContext::pack_snap_box ()
{
	EC_LOCAL_TEMPO_SCOPE;

	snap_box.pack_start (snap_mode_button, false, false);
	snap_box.pack_start (grid_type_selector, false, false);
}

void
EditingContext::bind_mouse_mode_buttons ()
{
	EC_LOCAL_TEMPO_SCOPE;

	RefPtr<Action> act;

	act = ActionManager::get_action ((_name + X_("Editing")).c_str(), X_("temporal-zoom-in"));
	zoom_in_button.set_related_action (act);
	act = ActionManager::get_action ((_name + X_("Editing")).c_str(), X_("temporal-zoom-out"));
	zoom_out_button.set_related_action (act);

	follow_playhead_button.set_related_action (follow_playhead_action);

	act = ActionManager::get_action (X_("Transport"), X_("ToggleFollowEdits"));
	follow_edits_button.set_related_action (act);

	mouse_move_button.set_related_action (mouse_mode_actions[Editing::MouseObject]);
	mouse_move_button.set_icon (ArdourWidgets::ArdourIcon::ToolGrab);
	mouse_move_button.set_name ("mouse mode button");

	mouse_select_button.set_related_action (mouse_mode_actions[Editing::MouseRange]);
	mouse_select_button.set_icon (ArdourWidgets::ArdourIcon::ToolRange);
	mouse_select_button.set_name ("mouse mode button");

	mouse_draw_button.set_related_action (mouse_mode_actions[Editing::MouseDraw]);
	mouse_draw_button.set_icon (ArdourWidgets::ArdourIcon::ToolDraw);
	mouse_draw_button.set_name ("mouse mode button");

	mouse_timefx_button.set_related_action (mouse_mode_actions[Editing::MouseTimeFX]);
	mouse_timefx_button.set_icon (ArdourWidgets::ArdourIcon::ToolStretch);
	mouse_timefx_button.set_name ("mouse mode button");

	mouse_grid_button.set_related_action (mouse_mode_actions[Editing::MouseGrid]);
	mouse_grid_button.set_icon (ArdourWidgets::ArdourIcon::ToolGrid);
	mouse_grid_button.set_name ("mouse mode button");

	mouse_content_button.set_related_action (mouse_mode_actions[Editing::MouseContent]);
	mouse_content_button.set_icon (ArdourWidgets::ArdourIcon::ToolContent);
	mouse_content_button.set_name ("mouse mode button");

	mouse_cut_button.set_related_action (mouse_mode_actions[Editing::MouseCut]);
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

Editing::MouseMode
EditingContext::current_mouse_mode() const
{
	EC_LOCAL_TEMPO_SCOPE;

	for (auto & [mode,action] : mouse_mode_actions) {
		if (action->get_active()) {
			return mode;
		}
	}

	return MouseObject;
}

void
EditingContext::set_mouse_mode (MouseMode m, bool force)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_drags->active ()) {
		return;
	}

	if (force && mouse_mode_actions[m]->get_active()) {
		mouse_mode_actions[m]->set_active (false);
	}

	mouse_mode_actions[m]->set_active (true);
}

bool
EditingContext::on_velocity_scroll_event (GdkEventScroll* ev)
{
	EC_LOCAL_TEMPO_SCOPE;

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
	set_draw_velocity (v);
	return true;
}

void
EditingContext::set_common_editing_state (XMLNode const & node)
{
	EC_LOCAL_TEMPO_SCOPE;

	double z;
	if (node.get_property ("zoom", z)) {
		/* older versions of ardour used floating point samples_per_pixel */
		reset_zoom (llrintf (z));
	} else {
		reset_zoom (samples_per_pixel);
	}

	GridType grid_type;
	if (!node.get_property ("grid-type", grid_type)) {
		grid_type = GridTypeNone;
	}
	set_grid_type (grid_type);

	SnapMode sm;
	if (!node.get_property ("snap-mode", sm)) {
		sm = SnapOff;
	}
	set_snap_mode (sm);

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
	EC_LOCAL_TEMPO_SCOPE;

	node.set_property ("zoom", samples_per_pixel);
	node.set_property ("grid-type", grid_type());
	node.set_property ("snap-mode", snap_mode());
	node.set_property ("internal-grid-type", internal_grid_type);
	node.set_property ("internal-snap-mode", internal_snap_mode);
	node.set_property ("pre-internal-grid-type", pre_internal_grid_type);
	node.set_property ("pre-internal-snap-mode", pre_internal_snap_mode);
	node.set_property ("left-frame", _leftmost_sample);
}

bool
EditingContext::snap_mode_button_clicked (GdkEventButton* ev)
{
	EC_LOCAL_TEMPO_SCOPE;

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
EditingContext::ensure_visual_change_idle_handler ()
{
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	pending_visual_change.idle_handler_id = -1;

	if (pending_visual_change.pending == 0) {
		return G_SOURCE_REMOVE;
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
		return G_SOURCE_REMOVE;
	}

	pending_visual_change.being_handled = true;

	VisualChange vc = pending_visual_change;

	pending_visual_change.pending = (VisualChange::Type) 0;

	visual_changer (vc);

	pending_visual_change.being_handled = false;

	visual_change_queued = true;

	return G_SOURCE_REMOVE; /* this is always a one-shot call */
}


/** Queue up a change to the viewport x origin.
 *  @param sample New x origin.
 */
void
EditingContext::reset_x_origin (samplepos_t sample)
{
	EC_LOCAL_TEMPO_SCOPE;

	pending_visual_change.add (VisualChange::TimeOrigin);
	pending_visual_change.time_origin = sample;
	ensure_visual_change_idle_handler ();
}

void
EditingContext::reset_y_origin (double y)
{
	EC_LOCAL_TEMPO_SCOPE;

	pending_visual_change.add (VisualChange::YOrigin);
	pending_visual_change.y_origin = y;
	ensure_visual_change_idle_handler ();
}

void
EditingContext::reset_zoom (samplecnt_t spp)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_track_canvas_width <= 0) {
		return;
	}

	std::pair<timepos_t, timepos_t> ext = max_zoom_extent();
	samplecnt_t max_extents_pp = max_extents_scale() * ((ext.second.samples() - ext.first.samples())  / _track_canvas_width);

	if (spp > max_extents_pp) {
		spp = max_extents_pp;
	}

	if (spp == samples_per_pixel) {
		return;
	}

	pending_visual_change.add (VisualChange::ZoomLevel);
	pending_visual_change.samples_per_pixel = spp;
	ensure_visual_change_idle_handler ();
}

void
EditingContext::pre_render ()
{
	EC_LOCAL_TEMPO_SCOPE;

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

Glib::RefPtr<ToggleAction>
EditingContext::toggle_reg_sens (RefPtr<ActionGroup> group, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<ToggleAction> act = ActionManager::register_toggle_action (group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
	return act;
}

Glib::RefPtr<Gtk::RadioAction>
EditingContext::radio_reg_sens (RefPtr<ActionGroup> action_group, RadioAction::Group& radio_group, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<RadioAction> act = ActionManager::register_radio_action (action_group, radio_group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
	return act;
}

void
EditingContext::update_undo_redo_actions (PBD::UndoHistory const & history)
{
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	return get_grid_beat_divisions (gt);
}

Temporal::Beats
EditingContext::get_grid_type_as_beats (bool& success, timepos_t const & position) const
{
	EC_LOCAL_TEMPO_SCOPE;

	success = true;

	int32_t const divisions = get_grid_beat_divisions (grid_type());
	/* Beat (+1), and Bar (-1) are handled below */
	if (divisions > 1) {
		/* grid divisions are divisions of a 1/4 note */
		return Temporal::Beats::ticks(Temporal::Beats::PPQN / divisions);
	}

	TempoMap::SharedPtr tmap (TempoMap::use());

	switch (grid_type()) {
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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	for (PointSelection::iterator i = selection->points.begin(); i != selection->points.end(); ++i) {
		ARDOUR::AutomationList::iterator j = (*i)->model ();
		(*j)->value = (*i)->line().the_list()->descriptor ().normal;
	}
}

void
EditingContext::choose_canvas_cursor_on_entry (ItemType type)
{
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

	UIConfiguration::instance().set_sound_midi_notes (!UIConfiguration::instance().get_sound_midi_notes());
}

void
EditingContext::cycle_zoom_focus ()
{
	EC_LOCAL_TEMPO_SCOPE;

	switch (zoom_focus()) {
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
	EC_LOCAL_TEMPO_SCOPE;

	ZoomFocus old_zf (zoom_focus());
	PBD::Unwinder<bool> uw (temporary_zoom_focus_change, true);
	set_zoom_focus (Editing::ZoomFocusMouse);
	temporal_zoom_step_scale (zoom_out, scale);
	set_zoom_focus (old_zf);
}

void
EditingContext::temporal_zoom_step_mouse_focus (bool zoom_out)
{
	EC_LOCAL_TEMPO_SCOPE;

	temporal_zoom_step_mouse_focus_scale (zoom_out, 2.0);
}

void
EditingContext::temporal_zoom_step (bool zoom_out)
{
	EC_LOCAL_TEMPO_SCOPE;

	temporal_zoom_step_scale (zoom_out, 2.0);
}

void
EditingContext::temporal_zoom_step_scale (bool zoom_out, double scale)
{
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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
	EC_LOCAL_TEMPO_SCOPE;

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

Editing::ZoomFocus
EditingContext::zoom_focus () const
{
	EC_LOCAL_TEMPO_SCOPE;

	for (auto & [mode,action] : zoom_focus_actions) {
		if (action->get_active()) {
			return mode;
		}
	}

	return ZoomFocusLeft;
}

void
EditingContext::zoom_focus_chosen (ZoomFocus focus)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (temporary_zoom_focus_change) {
		/* we are just changing settings momentarily, no need to do anything */
		return;
	}

	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	if (!zoom_focus_actions[focus]->get_active()) {
		return;
	}

	zoom_focus_selector.set_active (zoom_focus_strings[(int)focus]);
	instant_save ();
}

void
EditingContext::alt_delete_ ()
{
	EC_LOCAL_TEMPO_SCOPE;

	delete_ ();
}

/** Cut selected regions, automation points or a time range */
void
EditingContext::cut ()
{
	EC_LOCAL_TEMPO_SCOPE;

	cut_copy (Cut);
}

/** Copy selected regions, automation points or a time range */
void
EditingContext::copy ()
{
	EC_LOCAL_TEMPO_SCOPE;

	cut_copy (Copy);
}

void
EditingContext::load_shared_bindings ()
{
	EC_LOCAL_TEMPO_SCOPE;

	Bindings* m = Bindings::get_bindings (X_("MIDI"));
	Bindings* b = Bindings::get_bindings (X_("Editing"));
	Bindings* a = Bindings::get_bindings (X_("Automation"));

	if (need_shared_actions) {
		register_midi_actions (m, string());
		register_common_actions (b, string());
		register_automation_actions (a, string());
		need_shared_actions = false;
	}

	/* Copy each set of shared bindings but give them a new name, which will make them refer to actions
	 * named after this EditingContext (ie. unique to this EC)
	 */

	Bindings* midi_bindings = new Bindings (_name, *m);
	register_midi_actions (midi_bindings, _name);
	midi_bindings->associate ();

	Bindings* shared_bindings = new Bindings (_name, *b);
	register_common_actions (shared_bindings, _name);
	shared_bindings->associate ();

	Bindings* automation_bindings = new Bindings (_name, *a);
	register_automation_actions (automation_bindings, _name);
	automation_bindings->associate ();

	/* Attach bindings to the canvas for this editing context */

	bindings.push_back (automation_bindings);
	bindings.push_back (midi_bindings);
	bindings.push_back (shared_bindings);
}

void
EditingContext::drop_grid ()
{
	EC_LOCAL_TEMPO_SCOPE;

	hide_grid_lines ();
	grid_lines.reset ();
}

void
EditingContext::hide_grid_lines ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (grid_lines) {
		grid_lines->hide();
	}
}

void
EditingContext::maybe_draw_grid_lines (ArdourCanvas::Container* group)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_session) {
		return;
	}

	if (!grid_lines) {
		grid_lines.reset (new GridLines (*this, group, ArdourCanvas::LineSet::Vertical));

	}

	grid_marks.clear();
	samplepos_t rightmost_sample = _leftmost_sample + current_page_samples();
	GridType gt (grid_type());

	if (grid_musical()) {
		metric_get_bbt (grid_marks, _leftmost_sample, rightmost_sample, 12);
	} else if (gt == GridTypeTimecode) {
		metric_get_timecode (grid_marks, _leftmost_sample, rightmost_sample, 12);
	} else if (gt == GridTypeCDFrame) {
		metric_get_minsec (grid_marks, _leftmost_sample, rightmost_sample, 12);
	} else if (gt == GridTypeMinSec) {
		metric_get_minsec (grid_marks, _leftmost_sample, rightmost_sample, 12);
	}

	grid_lines->draw (grid_marks);
	grid_lines->show();
}


void
EditingContext::update_grid ()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_session) {
		return;
	}

	if (grid_type() == GridTypeNone) {
		hide_grid_lines ();
	} else {
		maybe_draw_grid_lines (time_line_group);
	}
}

Location*
EditingContext::transport_loop_location()
{
	EC_LOCAL_TEMPO_SCOPE;

	if (_session) {
		return _session->locations()->auto_loop_location();
	} else {
		return 0;
	}
}

void
EditingContext::set_loop_range (timepos_t const & start, timepos_t const & end, string cmd)
{
	EC_LOCAL_TEMPO_SCOPE;

	if (!_session) {
		return;
	}
	if (_session->get_play_loop () && _session->actively_recording ()) {
		return;
	}

	begin_reversible_command (cmd);

	Location* tll;

	if ((tll = transport_loop_location()) == 0) {
		Location* loc = new Location (*_session, start, end, _("Loop"),  Location::IsAutoLoop);
		XMLNode &before = _session->locations()->get_state();
		_session->locations()->add (loc, true);
		_session->set_auto_loop_location (loc);
		XMLNode &after = _session->locations()->get_state();
		add_command (new MementoCommand<Locations>(*(_session->locations()), &before, &after));
	} else {
		XMLNode &before = tll->get_state();
		tll->set_hidden (false, this);
		tll->set (start, end);
		XMLNode &after = tll->get_state();
		add_command (new MementoCommand<Location>(*tll, &before, &after));
	}

	commit_reversible_command ();
}

bool
EditingContext::allow_trim_cursors () const
{
	EC_LOCAL_TEMPO_SCOPE;

	auto mouse_mode = current_mouse_mode();
	return mouse_mode == MouseContent || mouse_mode == MouseTimeFX || mouse_mode == MouseDraw;
}

/** Queue a change for the Editor viewport x origin to follow the playhead */
void
EditingContext::reset_x_origin_to_follow_playhead ()
{
	EC_LOCAL_TEMPO_SCOPE;

	assert (_session);

	samplepos_t const sample = _playhead_cursor->current_sample ();

	if (sample < _leftmost_sample || sample > _leftmost_sample + current_page_samples()) {

		if (_session->transport_speed() < 0) {

			if (sample > (current_page_samples() / 2)) {
				center_screen (sample-(current_page_samples()/2));
			} else {
				center_screen (current_page_samples()/2);
			}

		} else {

			samplepos_t l = 0;

			if (sample < _leftmost_sample) {
				/* moving left */
				if (_session->transport_rolling()) {
					/* rolling; end up with the playhead at the right of the page */
					l = sample - current_page_samples ();
				} else {
					/* not rolling: end up with the playhead 1/4 of the way along the page */
					l = sample - current_page_samples() / 4;
				}
			} else {
				/* moving right */
				if (_session->transport_rolling()) {
					/* rolling: end up with the playhead on the left of the page */
					l = sample;
				} else {
					/* not rolling: end up with the playhead 3/4 of the way along the page */
					l = sample - 3 * current_page_samples() / 4;
				}
			}

			if (l < 0) {
				l = 0;
			}

			center_screen_internal (l + (current_page_samples() / 2), current_page_samples ());
		}
	}
}


void
EditingContext::center_screen (samplepos_t sample)
{
	EC_LOCAL_TEMPO_SCOPE;

	samplecnt_t const page = _visible_canvas_width * samples_per_pixel;

	/* if we're off the page, then scroll.
	 */

	if (sample < _leftmost_sample || sample >= _leftmost_sample + page) {
		center_screen_internal (sample, page);
	}
}

void
EditingContext::center_screen_internal (samplepos_t sample, float page)
{
	EC_LOCAL_TEMPO_SCOPE;

	page /= 2;

	if (sample > page) {
		sample -= (samplepos_t) page;
	} else {
		sample = 0;
	}

	reset_x_origin (sample);
}
