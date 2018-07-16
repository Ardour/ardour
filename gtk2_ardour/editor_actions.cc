/*
    Copyright (C) 2000-2007 Paul Davis

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

#include <gio/gio.h>
#include <gtk/gtkiconfactory.h>

#include "pbd/file_utils.h"

#include "gtkmm2ext/bindings.h"
#include "gtkmm2ext/utils.h"

#include "ardour/filesystem_paths.h"
#include "ardour/profile.h"
#include "ardour/session.h"
#include "ardour/types.h"

#include "canvas/canvas.h"
#include "canvas/pixbuf.h"

#include "LuaBridge/LuaBridge.h"

#include "actions.h"
#include "ardour_ui.h"
#include "editing.h"
#include "editor.h"
#include "gui_thread.h"
#include "luainstance.h"
#include "main_clock.h"
#include "time_axis_view.h"
#include "ui_config.h"
#include "utils.h"
#include "pbd/i18n.h"
#include "audio_time_axis.h"
#include "editor_group_tabs.h"
#include "editor_routes.h"
#include "editor_regions.h"

using namespace Gtk;
using namespace Glib;
using namespace std;
using namespace ARDOUR;
using namespace ARDOUR_UI_UTILS;
using namespace PBD;
using namespace Editing;

using Gtkmm2ext::Bindings;

/* Convenience functions to slightly reduce verbosity below */


RefPtr<Action>
Editor::register_region_action (RefPtr<ActionGroup> group, RegionActionTarget tgt, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<Action> act = myactions.register_action (group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
	region_action_map.insert (make_pair<string,RegionAction> (name, RegionAction (act,tgt)));
	return act;
}

void
Editor::register_toggle_region_action (RefPtr<ActionGroup> group, RegionActionTarget tgt, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<Action> act = myactions.register_toggle_action (group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
	region_action_map.insert (make_pair<string,RegionAction> (name, RegionAction (act,tgt)));
}

RefPtr<Action>
Editor::reg_sens (RefPtr<ActionGroup> group, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<Action> act = myactions.register_action (group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
	return act;
}

void
Editor::toggle_reg_sens (RefPtr<ActionGroup> group, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<Action> act = myactions.register_toggle_action (group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
}

void
Editor::radio_reg_sens (RefPtr<ActionGroup> action_group, RadioAction::Group& radio_group, char const * name, char const * label, sigc::slot<void> slot)
{
	RefPtr<Action> act = myactions.register_radio_action (action_group, radio_group, name, label, slot);
	ActionManager::session_sensitive_actions.push_back (act);
}

void
Editor::register_actions ()
{
	RefPtr<Action> act;

	editor_actions = myactions.create_action_group (X_("Editor"));
	editor_menu_actions = myactions.create_action_group (X_("EditorMenu"));

	/* non-operative menu items for menu bar */

	myactions.register_action (editor_menu_actions, X_("AlignMenu"), _("Align"));
	myactions.register_action (editor_menu_actions, X_("Autoconnect"), _("Autoconnect"));
	myactions.register_action (editor_menu_actions, X_("Crossfades"), _("Crossfades"));
	myactions.register_action (editor_menu_actions, X_("Edit"), _("Edit"));
	myactions.register_action (editor_menu_actions, X_("EditCursorMovementOptions"), _("Move Selected Marker"));
	myactions.register_action (editor_menu_actions, X_("EditSelectRangeOptions"), _("Select Range Operations"));
	myactions.register_action (editor_menu_actions, X_("EditSelectRegionOptions"), _("Select Regions"));
	myactions.register_action (editor_menu_actions, X_("EditPointMenu"), _("Edit Point"));
	myactions.register_action (editor_menu_actions, X_("FadeMenu"), _("Fade"));
	myactions.register_action (editor_menu_actions, X_("LatchMenu"), _("Latch"));
	myactions.register_action (editor_menu_actions, X_("RegionMenu"), _("Region"));
	myactions.register_action (editor_menu_actions, X_("RegionMenuLayering"), _("Layering"));
	myactions.register_action (editor_menu_actions, X_("RegionMenuPosition"), _("Position"));
	myactions.register_action (editor_menu_actions, X_("RegionMenuEdit"), _("Edit"));
	myactions.register_action (editor_menu_actions, X_("RegionMenuTrim"), _("Trim"));
	myactions.register_action (editor_menu_actions, X_("RegionMenuGain"), _("Gain"));
	myactions.register_action (editor_menu_actions, X_("RegionMenuRanges"), _("Ranges"));
	myactions.register_action (editor_menu_actions, X_("RegionMenuFades"), _("Fades"));
	myactions.register_action (editor_menu_actions, X_("RegionMenuMIDI"), _("MIDI"));
	myactions.register_action (editor_menu_actions, X_("RegionMenuDuplicate"), _("Duplicate"));
	myactions.register_action (editor_menu_actions, X_("Link"), _("Link"));
	myactions.register_action (editor_menu_actions, X_("ZoomFocusMenu"), _("Zoom Focus"));
	myactions.register_action (editor_menu_actions, X_("LocateToMarker"), _("Locate to Markers"));
	myactions.register_action (editor_menu_actions, X_("MarkerMenu"), _("Markers"));
	myactions.register_action (editor_menu_actions, X_("MeterFalloff"), _("Meter falloff"));
	myactions.register_action (editor_menu_actions, X_("MeterHold"), _("Meter hold"));
	myactions.register_action (editor_menu_actions, X_("MIDI"), _("MIDI Options"));
	myactions.register_action (editor_menu_actions, X_("MiscOptions"), _("Misc Options"));
	myactions.register_action (editor_menu_actions, X_("Monitoring"), _("Monitoring"));
	myactions.register_action (editor_menu_actions, X_("MoveActiveMarkMenu"), _("Active Mark"));
	myactions.register_action (editor_menu_actions, X_("MovePlayHeadMenu"), _("Playhead"));
	myactions.register_action (editor_menu_actions, X_("PlayMenu"), _("Play"));
	myactions.register_action (editor_menu_actions, X_("PrimaryClockMenu"), _("Primary Clock"));
	myactions.register_action (editor_menu_actions, X_("Pullup"), _("Pullup / Pulldown"));
	myactions.register_action (editor_menu_actions, X_("RegionEditOps"), _("Region operations"));
	myactions.register_action (editor_menu_actions, X_("RegionGainMenu"), _("Gain"));
	myactions.register_action (editor_menu_actions, X_("RulerMenu"), _("Rulers"));
	myactions.register_action (editor_menu_actions, X_("SavedViewMenu"), _("Views"));
	myactions.register_action (editor_menu_actions, X_("ScrollMenu"), _("Scroll"));
	myactions.register_action (editor_menu_actions, X_("SecondaryClockMenu"), _("Secondary Clock"));
	myactions.register_action (editor_menu_actions, X_("Select"), _("Select"));
	myactions.register_action (editor_menu_actions, X_("SelectMenu"), _("Select"));
	myactions.register_action (editor_menu_actions, X_("SeparateMenu"), _("Separate"));
	myactions.register_action (editor_menu_actions, X_("SetLoopMenu"), _("Loop"));
	myactions.register_action (editor_menu_actions, X_("SetPunchMenu"), _("Punch"));
	myactions.register_action (editor_menu_actions, X_("Solo"), _("Solo"));
	myactions.register_action (editor_menu_actions, X_("Subframes"), _("Subframes"));
	myactions.register_action (editor_menu_actions, X_("SyncMenu"), _("Sync"));
	myactions.register_action (editor_menu_actions, X_("TempoMenu"), _("Tempo"));
	myactions.register_action (editor_menu_actions, X_("Timecode"), _("Timecode fps"));

	act = myactions.register_action (editor_menu_actions, X_("TrackHeightMenu"), _("Height"));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);

	myactions.register_action (editor_menu_actions, X_("TrackMenu"), _("Track"));
	myactions.register_action (editor_menu_actions, X_("Tools"), _("Tools"));
	myactions.register_action (editor_menu_actions, X_("View"), _("View"));
	myactions.register_action (editor_menu_actions, X_("ZoomFocus"), _("Zoom Focus"));
	myactions.register_action (editor_menu_actions, X_("ZoomMenu"), _("Zoom"));
	myactions.register_action (editor_menu_actions, X_("LuaScripts"), _("Lua Scripts"));

	register_region_actions ();

	/* add named actions for the editor */

	/* We don't bother registering "unlock" because it would be insensitive
	   when required. Editor::unlock() must be invoked directly.
	*/
	myactions.register_action (editor_actions, "lock", S_("Session|Lock"), sigc::mem_fun (*this, &Editor::lock));

	toggle_reg_sens (editor_actions, "show-editor-mixer", _("Show Editor Mixer"), sigc::mem_fun (*this, &Editor::editor_mixer_button_toggled));
	toggle_reg_sens (editor_actions, "show-editor-list", _("Show Editor List"), sigc::mem_fun (*this, &Editor::editor_list_button_toggled));

	reg_sens (editor_actions, "playhead-to-next-region-boundary", _("Playhead to Next Region Boundary"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_next_region_boundary), true));
	reg_sens (editor_actions, "playhead-to-next-region-boundary-noselection", _("Playhead to Next Region Boundary (No Track Selection)"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_next_region_boundary), false));
	reg_sens (editor_actions, "playhead-to-previous-region-boundary", _("Playhead to Previous Region Boundary"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_previous_region_boundary), true));
	reg_sens (editor_actions, "playhead-to-previous-region-boundary-noselection", _("Playhead to Previous Region Boundary (No Track Selection)"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_previous_region_boundary), false));

	reg_sens (editor_actions, "playhead-to-next-region-start", _("Playhead to Next Region Start"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (Start)));
	reg_sens (editor_actions, "playhead-to-next-region-end", _("Playhead to Next Region End"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (End)));
	reg_sens (editor_actions, "playhead-to-next-region-sync", _("Playhead to Next Region Sync"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (SyncPoint)));

	reg_sens (editor_actions, "playhead-to-previous-region-start", _("Playhead to Previous Region Start"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (Start)));
	reg_sens (editor_actions, "playhead-to-previous-region-end", _("Playhead to Previous Region End"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (End)));
	reg_sens (editor_actions, "playhead-to-previous-region-sync", _("Playhead to Previous Region Sync"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (SyncPoint)));

	reg_sens (editor_actions, "selected-marker-to-next-region-boundary", _("To Next Region Boundary"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_next_region_boundary), true));
	reg_sens (editor_actions, "selected-marker-to-next-region-boundary-noselection", _("To Next Region Boundary (No Track Selection)"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_next_region_boundary), false));
	reg_sens (editor_actions, "selected-marker-to-previous-region-boundary", _("To Previous Region Boundary"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_previous_region_boundary), true));
	reg_sens (editor_actions, "selected-marker-to-previous-region-boundary-noselection", _("To Previous Region Boundary (No Track Selection)"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_previous_region_boundary), false));

	reg_sens (editor_actions, "edit-cursor-to-next-region-start", _("To Next Region Start"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_next_region_point), RegionPoint (Start)));
	reg_sens (editor_actions, "edit-cursor-to-next-region-end", _("To Next Region End"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_next_region_point), RegionPoint (End)));
	reg_sens (editor_actions, "edit-cursor-to-next-region-sync", _("To Next Region Sync"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_next_region_point), RegionPoint (SyncPoint)));

	reg_sens (editor_actions, "edit-cursor-to-previous-region-start", _("To Previous Region Start"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_previous_region_point), RegionPoint (Start)));
	reg_sens (editor_actions, "edit-cursor-to-previous-region-end", _("To Previous Region End"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_previous_region_point), RegionPoint (End)));
	reg_sens (editor_actions, "edit-cursor-to-previous-region-sync", _("To Previous Region Sync"), sigc::bind (sigc::mem_fun(*this, &Editor::selected_marker_to_previous_region_point), RegionPoint (SyncPoint)));

	reg_sens (editor_actions, "edit-cursor-to-range-start", _("To Range Start"), sigc::mem_fun(*this, &Editor::selected_marker_to_selection_start));
	reg_sens (editor_actions, "edit-cursor-to-range-end", _("To Range End"), sigc::mem_fun(*this, &Editor::selected_marker_to_selection_end));

	reg_sens (editor_actions, "playhead-to-range-start", _("Playhead to Range Start"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_selection_start), playhead_cursor));
	reg_sens (editor_actions, "playhead-to-range-end", _("Playhead to Range End"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_to_selection_end), playhead_cursor));

	reg_sens (editor_actions, "select-all-objects", _("Select All Objects"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_objects), Selection::Set));
	reg_sens (editor_actions, "select-all-tracks", _("Select All Tracks"), sigc::mem_fun(*this, &Editor::select_all_tracks));
	reg_sens (editor_actions, "deselect-all", _("Deselect All"), sigc::mem_fun(*this, &Editor::deselect_all));
	reg_sens (editor_actions, "invert-selection", _("Invert Selection"), sigc::mem_fun(*this, &Editor::invert_selection));

	reg_sens (editor_actions, "select-loop-range", _("Set Range to Loop Range"), sigc::mem_fun(*this, &Editor::set_selection_from_loop));
	reg_sens (editor_actions, "select-punch-range", _("Set Range to Punch Range"), sigc::mem_fun(*this, &Editor::set_selection_from_punch));
	reg_sens (editor_actions, "select-from-regions", _("Set Range to Selected Regions"), sigc::mem_fun(*this, &Editor::set_selection_from_region));

	reg_sens (editor_actions, "edit-current-tempo", _("Edit Current Tempo"), sigc::mem_fun(*this, &Editor::edit_current_tempo));
	reg_sens (editor_actions, "edit-current-meter", _("Edit Current Meter"), sigc::mem_fun(*this, &Editor::edit_current_meter));

	reg_sens (editor_actions, "select-all-after-edit-cursor", _("Select All After Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), true, false));
	reg_sens (editor_actions, "alternate-select-all-after-edit-cursor", _("Select All After Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), true, false));
	reg_sens (editor_actions, "select-all-before-edit-cursor", _("Select All Before Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), false, false));
	reg_sens (editor_actions, "alternate-select-all-before-edit-cursor", _("Select All Before Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_using_edit), false, false));

	reg_sens (editor_actions, "select-all-between-cursors", _("Select All Overlapping Edit Range"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_between), false));
	reg_sens (editor_actions, "select-all-within-cursors", _("Select All Inside Edit Range"), sigc::bind (sigc::mem_fun(*this, &Editor::select_all_selectables_between), true));

	reg_sens (editor_actions, "select-range-between-cursors", _("Select Edit Range"), sigc::mem_fun(*this, &Editor::select_range_between));

	reg_sens (editor_actions, "select-all-in-punch-range", _("Select All in Punch Range"), sigc::mem_fun(*this, &Editor::select_all_selectables_using_punch));
	reg_sens (editor_actions, "select-all-in-loop-range", _("Select All in Loop Range"), sigc::mem_fun(*this, &Editor::select_all_selectables_using_loop));

	reg_sens (editor_actions, "select-next-route", _("Select Next Track or Bus"), sigc::bind (sigc::mem_fun(*this, &Editor::select_next_stripable), true));
	reg_sens (editor_actions, "select-prev-route", _("Select Previous Track or Bus"), sigc::bind (sigc::mem_fun(*this, &Editor::select_prev_stripable), true));

	reg_sens (editor_actions, "select-next-stripable", _("Select Next Strip"), sigc::bind (sigc::mem_fun(*this, &Editor::select_next_stripable), false));
	reg_sens (editor_actions, "select-prev-stripable", _("Select Previous Strip"), sigc::bind (sigc::mem_fun(*this, &Editor::select_prev_stripable), false));

	act = reg_sens (editor_actions, "track-record-enable-toggle", _("Toggle Record Enable"), sigc::mem_fun(*this, &Editor::toggle_record_enable));
	ActionManager::track_selection_sensitive_actions.push_back (act);
	act = reg_sens (editor_actions, "track-solo-toggle", _("Toggle Solo"), sigc::mem_fun(*this, &Editor::toggle_solo));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);
	act = reg_sens (editor_actions, "track-mute-toggle", _("Toggle Mute"), sigc::mem_fun(*this, &Editor::toggle_mute));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);
	act = reg_sens (editor_actions, "track-solo-isolate-toggle", _("Toggle Solo Isolate"), sigc::mem_fun(*this, &Editor::toggle_solo_isolate));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);

	for (int i = 1; i <= 12; ++i) {
		string const a = string_compose (X_("save-visual-state-%1"), i);
		string const n = string_compose (_("Save View %1"), i);
		reg_sens (editor_actions, a.c_str(), n.c_str(), sigc::bind (sigc::mem_fun (*this, &Editor::start_visual_state_op), i - 1));
	}

	for (int i = 1; i <= 12; ++i) {
		string const a = string_compose (X_("goto-visual-state-%1"), i);
		string const n = string_compose (_("Go to View %1"), i);
		reg_sens (editor_actions, a.c_str(), n.c_str(), sigc::bind (sigc::mem_fun (*this, &Editor::cancel_visual_state_op), i - 1));
	}

	for (int i = 1; i <= 9; ++i) {
		string const a = string_compose (X_("goto-mark-%1"), i);
		string const n = string_compose (_("Locate to Mark %1"), i);
		reg_sens (editor_actions, a.c_str(), n.c_str(), sigc::bind (sigc::mem_fun (*this, &Editor::goto_nth_marker), i - 1));
	}

	reg_sens (editor_actions, "temporal-zoom-out", _("Zoom Out"), sigc::bind (sigc::mem_fun(*this, &Editor::temporal_zoom_step), true));
	reg_sens (editor_actions, "temporal-zoom-in", _("Zoom In"), sigc::bind (sigc::mem_fun(*this, &Editor::temporal_zoom_step), false));
	reg_sens (editor_actions, "zoom-to-session", _("Zoom to Session"), sigc::mem_fun(*this, &Editor::temporal_zoom_session));
	reg_sens (editor_actions, "zoom-to-extents", _("Zoom to Extents"), sigc::mem_fun(*this, &Editor::temporal_zoom_extents));
	reg_sens (editor_actions, "zoom-to-selection", _("Zoom to Selection"), sigc::bind (sigc::mem_fun(*this, &Editor::temporal_zoom_selection), Both));
	reg_sens (editor_actions, "zoom-to-selection-horiz", _("Zoom to Selection (Horizontal)"), sigc::bind (sigc::mem_fun(*this, &Editor::temporal_zoom_selection), Horizontal));
	reg_sens (editor_actions, "toggle-zoom", _("Toggle Zoom State"), sigc::mem_fun(*this, &Editor::swap_visual_state));

	reg_sens (editor_actions, "expand-tracks", _("Expand Track Height"), sigc::bind (sigc::mem_fun (*this, &Editor::tav_zoom_step), false));
	reg_sens (editor_actions, "shrink-tracks", _("Shrink Track Height"), sigc::bind (sigc::mem_fun (*this, &Editor::tav_zoom_step), true));

	reg_sens (editor_actions, "fit_1_track", _("Fit 1 Track"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 1));
	reg_sens (editor_actions, "fit_2_tracks", _("Fit 2 Tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 2));
	reg_sens (editor_actions, "fit_4_tracks", _("Fit 4 Tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 4));
	reg_sens (editor_actions, "fit_8_tracks", _("Fit 8 Tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 8));
	reg_sens (editor_actions, "fit_16_tracks", _("Fit 16 Tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 16));
	reg_sens (editor_actions, "fit_32_tracks", _("Fit 32 Tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 32));
	reg_sens (editor_actions, "fit_all_tracks", _("Fit All Tracks"), sigc::bind (sigc::mem_fun(*this, &Editor::set_visible_track_count), 0));

	reg_sens (editor_actions, "zoom_10_ms", _("Zoom to 10 ms"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 10));
	reg_sens (editor_actions, "zoom_100_ms", _("Zoom to 100 ms"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 100));
	reg_sens (editor_actions, "zoom_1_sec", _("Zoom to 1 sec"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 1000));
	reg_sens (editor_actions, "zoom_10_sec", _("Zoom to 10 sec"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 10 * 1000));
	reg_sens (editor_actions, "zoom_1_min", _("Zoom to 1 min"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 60 * 1000));
	reg_sens (editor_actions, "zoom_5_min", _("Zoom to 5 min"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 5 * 60 * 1000));
	reg_sens (editor_actions, "zoom_10_min", _("Zoom to 10 min"), sigc::bind (sigc::mem_fun(*this, &Editor::set_zoom_preset), 10 * 60 * 1000));

	act = reg_sens (editor_actions, "move-selected-tracks-up", _("Move Selected Tracks Up"), sigc::bind (sigc::mem_fun(*_routes, &EditorRoutes::move_selected_tracks), true));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);
	act = reg_sens (editor_actions, "move-selected-tracks-down", _("Move Selected Tracks Down"), sigc::bind (sigc::mem_fun(*_routes, &EditorRoutes::move_selected_tracks), false));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);

	act = reg_sens (editor_actions, "scroll-tracks-up", _("Scroll Tracks Up"), sigc::mem_fun(*this, &Editor::scroll_tracks_up));
	act = reg_sens (editor_actions, "scroll-tracks-down", _("Scroll Tracks Down"), sigc::mem_fun(*this, &Editor::scroll_tracks_down));
	act = reg_sens (editor_actions, "step-tracks-up", _("Step Tracks Up"), sigc::hide_return (sigc::bind (sigc::mem_fun(*this, &Editor::scroll_up_one_track), true)));
	act = reg_sens (editor_actions, "step-tracks-down", _("Step Tracks Down"), sigc::hide_return (sigc::bind (sigc::mem_fun(*this, &Editor::scroll_down_one_track), true)));
	act = reg_sens (editor_actions, "select-topmost", _("Select Topmost Track"), (sigc::mem_fun(*this, &Editor::select_topmost_track)));

	reg_sens (editor_actions, "scroll-backward", _("Scroll Backward"), sigc::bind (sigc::mem_fun(*this, &Editor::scroll_backward), 0.8f));
	reg_sens (editor_actions, "scroll-forward", _("Scroll Forward"), sigc::bind (sigc::mem_fun(*this, &Editor::scroll_forward), 0.8f));
	reg_sens (editor_actions, "center-playhead", _("Center Playhead"), sigc::mem_fun(*this, &Editor::center_playhead));
	reg_sens (editor_actions, "center-edit-cursor", _("Center Edit Point"), sigc::mem_fun(*this, &Editor::center_edit_point));

	reg_sens (editor_actions, "scroll-playhead-forward", _("Playhead Forward"), sigc::bind (sigc::mem_fun(*this, &Editor::scroll_playhead), true));;
	reg_sens (editor_actions, "scroll-playhead-backward", _("Playhead Backward"), sigc::bind (sigc::mem_fun(*this, &Editor::scroll_playhead), false));

	reg_sens (editor_actions, "playhead-to-edit", _("Playhead to Active Mark"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_align), true));
	reg_sens (editor_actions, "edit-to-playhead", _("Active Mark to Playhead"), sigc::bind (sigc::mem_fun(*this, &Editor::cursor_align), false));

	toggle_reg_sens (editor_actions, "toggle-skip-playback", _("Use Skip Ranges"), sigc::mem_fun(*this, &Editor::toggle_skip_playback));

	reg_sens (editor_actions, "set-loop-from-edit-range", _("Set Loop from Selection"), sigc::bind (sigc::mem_fun(*this, &Editor::set_loop_from_selection), false));
	reg_sens (editor_actions, "set-punch-from-edit-range", _("Set Punch from Selection"), sigc::mem_fun(*this, &Editor::set_punch_from_selection));
	reg_sens (editor_actions, "set-session-from-edit-range", _("Set Session Start/End from Selection"), sigc::mem_fun(*this, &Editor::set_session_extents_from_selection));

	/* this is a duplicated action so that the main menu can use a different label */
	reg_sens (editor_actions, "main-menu-play-selected-regions", _("Play Selected Regions"), sigc::mem_fun (*this, &Editor::play_selected_region));
	reg_sens (editor_actions, "play-from-edit-point", _("Play from Edit Point"), sigc::mem_fun(*this, &Editor::play_from_edit_point));
	reg_sens (editor_actions, "play-from-edit-point-and-return", _("Play from Edit Point and Return"), sigc::mem_fun(*this, &Editor::play_from_edit_point_and_return));

	reg_sens (editor_actions, "play-edit-range", _("Play Edit Range"), sigc::mem_fun(*this, &Editor::play_edit_range));

	reg_sens (editor_actions, "set-playhead", _("Playhead to Mouse"), sigc::mem_fun(*this, &Editor::set_playhead_cursor));
	reg_sens (editor_actions, "set-edit-point", _("Active Marker to Mouse"), sigc::mem_fun(*this, &Editor::set_edit_point));
	reg_sens (editor_actions, "set-auto-punch-range", _("Set Auto Punch In/Out from Playhead"), sigc::mem_fun(*this, &Editor::set_auto_punch_range));

	reg_sens (editor_actions, "duplicate", _("Duplicate"), sigc::bind (sigc::mem_fun(*this, &Editor::duplicate_range), false));

	/* Open the dialogue to duplicate selected regions multiple times */
	reg_sens (editor_actions, "multi-duplicate", _("Multi-Duplicate..."),
	          sigc::bind (sigc::mem_fun (*this, &Editor::duplicate_range), true));

	undo_action = reg_sens (editor_actions, "undo", S_("Command|Undo"), sigc::bind (sigc::mem_fun(*this, &Editor::undo), 1U));

	redo_action = reg_sens (editor_actions, "redo", _("Redo"), sigc::bind (sigc::mem_fun(*this, &Editor::redo), 1U));
	alternate_redo_action = reg_sens (editor_actions, "alternate-redo", _("Redo"), sigc::bind (sigc::mem_fun(*this, &Editor::redo), 1U));
	alternate_alternate_redo_action = reg_sens (editor_actions, "alternate-alternate-redo", _("Redo"), sigc::bind (sigc::mem_fun(*this, &Editor::redo), 1U));

	selection_undo_action = reg_sens (editor_actions, "undo-last-selection-op", _("Undo Selection Change"), sigc::mem_fun(*this, &Editor::undo_selection_op));
	selection_redo_action = reg_sens (editor_actions, "redo-last-selection-op", _("Redo Selection Change"), sigc::mem_fun(*this, &Editor::redo_selection_op));

	reg_sens (editor_actions, "export-audio", _("Export Audio"), sigc::mem_fun(*this, &Editor::export_audio));
	reg_sens (editor_actions, "export-range", _("Export Range"), sigc::mem_fun(*this, &Editor::export_range));

	act = reg_sens (editor_actions, "editor-separate", _("Separate"), sigc::mem_fun(*this, &Editor::separate_region_from_selection));
	ActionManager::mouse_edit_point_requires_canvas_actions.push_back (act);

	act = reg_sens (editor_actions, "separate-from-punch", _("Separate Using Punch Range"), sigc::mem_fun(*this, &Editor::separate_region_from_punch));
	act = reg_sens (editor_actions, "separate-from-loop", _("Separate Using Loop Range"), sigc::mem_fun(*this, &Editor::separate_region_from_loop));

	act = reg_sens (editor_actions, "editor-crop", _("Crop"), sigc::mem_fun(*this, &Editor::crop_region_to_selection));
	ActionManager::time_selection_sensitive_actions.push_back (act);

	reg_sens (editor_actions, "editor-cut", _("Cut"), sigc::mem_fun(*this, &Editor::cut));
	reg_sens (editor_actions, "editor-delete", _("Delete"), sigc::mem_fun(*this, &Editor::delete_));
	reg_sens (editor_actions, "alternate-editor-delete", _("Delete"), sigc::mem_fun(*this, &Editor::delete_));

	reg_sens (editor_actions, "split-region", _("Split/Separate"), sigc::mem_fun (*this, &Editor::split_region));

	reg_sens (editor_actions, "editor-copy", _("Copy"), sigc::mem_fun(*this, &Editor::copy));
	reg_sens (editor_actions, "editor-paste", _("Paste"), sigc::mem_fun(*this, &Editor::keyboard_paste));

	reg_sens (editor_actions, "editor-fade-range", _("Fade Range Selection"), sigc::mem_fun(*this, &Editor::fade_range));

	act = myactions.register_action (editor_actions, "set-tempo-from-edit-range", _("Set Tempo from Edit Range = Bar"), sigc::mem_fun(*this, &Editor::use_range_as_bar));
	ActionManager::time_selection_sensitive_actions.push_back (act);

	toggle_reg_sens (editor_actions, "toggle-log-window", _("Log"),
			sigc::mem_fun (ARDOUR_UI::instance(), &ARDOUR_UI::toggle_errors));

	reg_sens (editor_actions, "alternate-tab-to-transient-forwards", _("Move to Next Transient"), sigc::bind (sigc::mem_fun(*this, &Editor::tab_to_transient), true));
	reg_sens (editor_actions, "alternate-tab-to-transient-backwards", _("Move to Previous Transient"), sigc::bind (sigc::mem_fun(*this, &Editor::tab_to_transient), false));
	reg_sens (editor_actions, "tab-to-transient-forwards", _("Move to Next Transient"), sigc::bind (sigc::mem_fun(*this, &Editor::tab_to_transient), true));
	reg_sens (editor_actions, "tab-to-transient-backwards", _("Move to Previous Transient"), sigc::bind (sigc::mem_fun(*this, &Editor::tab_to_transient), false));

	reg_sens (editor_actions, "crop", _("Crop"), sigc::mem_fun(*this, &Editor::crop_region_to_selection));

//	reg_sens (editor_actions, "finish-add-range", _("Finish Add Range"), sigc::bind (sigc::mem_fun(*this, &Editor::keyboard_selection_finish), true));

	reg_sens (
		editor_actions,
		"move-range-start-to-previous-region-boundary",
		_("Move Range Start to Previous Region Boundary"),
		sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), false, false)
		);

	reg_sens (
		editor_actions,
		"move-range-start-to-next-region-boundary",
		_("Move Range Start to Next Region Boundary"),
		sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), false, true)
		);

	reg_sens (
		editor_actions,
		"move-range-end-to-previous-region-boundary",
		_("Move Range End to Previous Region Boundary"),
		sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), true, false)
		);

	reg_sens (
		editor_actions,
		"move-range-end-to-next-region-boundary",
		_("Move Range End to Next Region Boundary"),
		sigc::bind (sigc::mem_fun (*this, &Editor::move_range_selection_start_or_end_to_region_boundary), true, true)
		);

	toggle_reg_sens (editor_actions, "toggle-follow-playhead", _("Follow Playhead"), (sigc::mem_fun(*this, &Editor::toggle_follow_playhead)));
	act = reg_sens (editor_actions, "remove-last-capture", _("Remove Last Capture"), (sigc::mem_fun(*this, &Editor::remove_last_capture)));

	myactions.register_toggle_action (editor_actions, "toggle-stationary-playhead", _("Stationary Playhead"), (mem_fun(*this, &Editor::toggle_stationary_playhead)));

	act = reg_sens (editor_actions, "insert-time", _("Insert Time"), (sigc::mem_fun(*this, &Editor::do_insert_time)));
	ActionManager::track_selection_sensitive_actions.push_back (act);
	act = myactions.register_action (editor_actions, "remove-time", _("Remove Time"), (mem_fun(*this, &Editor::do_remove_time)));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::track_selection_sensitive_actions.push_back (act);


	act = reg_sens (editor_actions, "toggle-track-active", _("Toggle Active"), (sigc::mem_fun(*this, &Editor::toggle_tracks_active)));
	ActionManager::route_selection_sensitive_actions.push_back (act);
	act = reg_sens (editor_actions, "remove-track", _("Remove"), (sigc::mem_fun(*this, &Editor::remove_tracks)));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);

	act = reg_sens (editor_actions, "fit-selection", _("Fit Selection (Vertical)"), sigc::mem_fun(*this, &Editor::fit_selection));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);

	act = reg_sens (editor_actions, "track-height-largest", _("Largest"), sigc::bind (
				sigc::mem_fun(*this, &Editor::set_track_height), HeightLargest));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);
	act = reg_sens (editor_actions, "track-height-larger", _("Larger"), sigc::bind (
				sigc::mem_fun(*this, &Editor::set_track_height), HeightLarger));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);
	act = reg_sens (editor_actions, "track-height-large", _("Large"), sigc::bind (
				sigc::mem_fun(*this, &Editor::set_track_height), HeightLarge));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);
	act = reg_sens (editor_actions, "track-height-normal", _("Normal"), sigc::bind (
				sigc::mem_fun(*this, &Editor::set_track_height), HeightNormal));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);
	act = reg_sens (editor_actions, "track-height-small", _("Small"), sigc::bind (
				sigc::mem_fun(*this, &Editor::set_track_height), HeightSmall));
	ActionManager::stripable_selection_sensitive_actions.push_back (act);

	toggle_reg_sens (editor_actions, "sound-midi-notes", _("Sound Selected MIDI Notes"), sigc::mem_fun (*this, &Editor::toggle_sound_midi_notes));

	Glib::RefPtr<ActionGroup> zoom_actions = myactions.create_action_group (X_("Zoom"));
	RadioAction::Group zoom_group;

	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-left", _("Zoom Focus Left"), sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_chosen), Editing::ZoomFocusLeft));
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-right", _("Zoom Focus Right"), sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_chosen), Editing::ZoomFocusRight));
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-center", _("Zoom Focus Center"), sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_chosen), Editing::ZoomFocusCenter));
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-playhead", _("Zoom Focus Playhead"), sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_chosen), Editing::ZoomFocusPlayhead));
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-mouse", _("Zoom Focus Mouse"), sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_chosen), Editing::ZoomFocusMouse));
	radio_reg_sens (zoom_actions, zoom_group, "zoom-focus-edit", _("Zoom Focus Edit Point"), sigc::bind (sigc::mem_fun(*this, &Editor::zoom_focus_chosen), Editing::ZoomFocusEdit));

	myactions.register_action (editor_actions, X_("cycle-zoom-focus"), _("Next Zoom Focus"), sigc::mem_fun (*this, &Editor::cycle_zoom_focus));

	for (int i = 1; i <= 9; ++i) {
		string const a = string_compose (X_("script-action-%1"), i);
		string const n = string_compose (_("Unset #%1"), i);
		act = myactions.register_action (editor_actions, a.c_str(), n.c_str(), sigc::bind (sigc::mem_fun (*this, &Editor::trigger_script), i - 1));
		act->set_tooltip (_("no action bound"));
		act->set_sensitive (false);
	}

	Glib::RefPtr<ActionGroup> mouse_mode_actions = myactions.create_action_group (X_("MouseMode"));
	RadioAction::Group mouse_mode_group;

	act = myactions.register_toggle_action (mouse_mode_actions, "set-mouse-mode-object-range", _("Smart Object Mode"), sigc::mem_fun (*this, &Editor::mouse_mode_object_range_toggled));
	smart_mode_action = Glib::RefPtr<ToggleAction>::cast_static (act);
	smart_mode_button.set_related_action (smart_mode_action);
	smart_mode_button.set_text (_("Smart"));
	smart_mode_button.set_name ("mouse mode button");

	act = myactions.register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-object", _("Object Tool"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseObject));
	mouse_move_button.set_related_action (act);
	mouse_move_button.set_icon (ArdourWidgets::ArdourIcon::ToolGrab);
	mouse_move_button.set_name ("mouse mode button");

	act = myactions.register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-range", _("Range Tool"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseRange));
	mouse_select_button.set_related_action (act);
	mouse_select_button.set_icon (ArdourWidgets::ArdourIcon::ToolRange);
	mouse_select_button.set_name ("mouse mode button");

	act = myactions.register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-draw", _("Note Drawing Tool"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseDraw));
	mouse_draw_button.set_related_action (act);
	mouse_draw_button.set_icon (ArdourWidgets::ArdourIcon::ToolDraw);
	mouse_draw_button.set_name ("mouse mode button");

	act = myactions.register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-audition", _("Audition Tool"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseAudition));
	mouse_audition_button.set_related_action (act);
	mouse_audition_button.set_icon (ArdourWidgets::ArdourIcon::ToolAudition);
	mouse_audition_button.set_name ("mouse mode button");

	act = myactions.register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-timefx", _("Time FX Tool"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseTimeFX));
	mouse_timefx_button.set_related_action (act);
	mouse_timefx_button.set_icon (ArdourWidgets::ArdourIcon::ToolStretch);
	mouse_timefx_button.set_name ("mouse mode button");

	act = myactions.register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-content", _("Content Tool"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseContent));
	mouse_content_button.set_related_action (act);
	mouse_content_button.set_icon (ArdourWidgets::ArdourIcon::ToolContent);
	mouse_content_button.set_name ("mouse mode button");

	if(!Profile->get_mixbus()) {
		act = myactions.register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-cut", _("Cut Tool"), sigc::bind (sigc::mem_fun(*this, &Editor::mouse_mode_toggled), Editing::MouseCut));
		mouse_cut_button.set_related_action (act);
		mouse_cut_button.set_icon (ArdourWidgets::ArdourIcon::ToolCut);
		mouse_cut_button.set_name ("mouse mode button");
	}

	myactions.register_action (editor_actions, "step-mouse-mode", _("Step Mouse Mode"), sigc::bind (sigc::mem_fun(*this, &Editor::step_mouse_mode), true));

	RadioAction::Group edit_point_group;
	myactions.register_radio_action (editor_actions, edit_point_group, X_("edit-at-playhead"), _("Playhead"), (sigc::bind (sigc::mem_fun(*this, &Editor::edit_point_chosen), Editing::EditAtPlayhead)));
	myactions.register_radio_action (editor_actions, edit_point_group, X_("edit-at-mouse"), _("Mouse"), (sigc::bind (sigc::mem_fun(*this, &Editor::edit_point_chosen), Editing::EditAtMouse)));
	myactions.register_radio_action (editor_actions, edit_point_group, X_("edit-at-selected-marker"), _("Marker"), (sigc::bind (sigc::mem_fun(*this, &Editor::edit_point_chosen), Editing::EditAtSelectedMarker)));

	myactions.register_action (editor_actions, "cycle-edit-point", _("Change Edit Point"), sigc::bind (sigc::mem_fun (*this, &Editor::cycle_edit_point), false));
	myactions.register_action (editor_actions, "cycle-edit-point-with-marker", _("Change Edit Point Including Marker"), sigc::bind (sigc::mem_fun (*this, &Editor::cycle_edit_point), true));

//	myactions.register_action (editor_actions, "set-edit-splice", _("Splice"), sigc::bind (sigc::mem_fun (*this, &Editor::set_edit_mode), Splice));
	myactions.register_action (editor_actions, "set-edit-ripple", _("Ripple"), bind (mem_fun (*this, &Editor::set_edit_mode), Ripple));
	myactions.register_action (editor_actions, "set-edit-slide", _("Slide"), sigc::bind (sigc::mem_fun (*this, &Editor::set_edit_mode), Slide));
	myactions.register_action (editor_actions, "set-edit-lock", S_("EditMode|Lock"), sigc::bind (sigc::mem_fun (*this, &Editor::set_edit_mode), Lock));
	myactions.register_action (editor_actions, "cycle-edit-mode", _("Cycle Edit Mode"), sigc::mem_fun (*this, &Editor::cycle_edit_mode));

	myactions.register_action (editor_actions, X_("GridChoice"), _("Snap & Grid"));

	RadioAction::Group snap_mode_group;
	/* deprecated */  myactions.register_radio_action (editor_actions, snap_mode_group, X_("snap-off"), _("No Grid"), (sigc::bind (sigc::mem_fun(*this, &Editor::snap_mode_chosen), Editing::SnapOff)));
	/* deprecated */  myactions.register_radio_action (editor_actions, snap_mode_group, X_("snap-normal"), _("Grid"), (sigc::bind (sigc::mem_fun(*this, &Editor::snap_mode_chosen), Editing::SnapNormal)));  //deprecated
	/* deprecated */  myactions.register_radio_action (editor_actions, snap_mode_group, X_("snap-magnetic"), _("Magnetic"), (sigc::bind (sigc::mem_fun(*this, &Editor::snap_mode_chosen), Editing::SnapMagnetic)));

	snap_mode_button.set_text (_("Snap"));
	snap_mode_button.set_name ("mouse mode button");
	snap_mode_button.signal_button_press_event().connect (sigc::mem_fun (*this, &Editor::snap_mode_button_clicked), false);

	myactions.register_action (editor_actions, X_("cycle-snap-mode"), _("Toggle Snap"), sigc::mem_fun (*this, &Editor::cycle_snap_mode));
	myactions.register_action (editor_actions, X_("next-grid-choice"), _("Next Quantize Grid Choice"), sigc::mem_fun (*this, &Editor::next_grid_choice));
	myactions.register_action (editor_actions, X_("prev-grid-choice"), _("Previous Quantize Grid Choice"), sigc::mem_fun (*this, &Editor::prev_grid_choice));

	Glib::RefPtr<ActionGroup> snap_actions = myactions.create_action_group (X_("Snap"));
	RadioAction::Group grid_choice_group;

	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-thirtyseconds"),  grid_type_strings[(int)GridTypeBeatDiv32].c_str(), (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv32)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twentyeighths"),  grid_type_strings[(int)GridTypeBeatDiv28].c_str(), (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv28)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twentyfourths"),  grid_type_strings[(int)GridTypeBeatDiv24].c_str(), (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv24)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twentieths"),     grid_type_strings[(int)GridTypeBeatDiv20].c_str(), (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv20)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-asixteenthbeat"), grid_type_strings[(int)GridTypeBeatDiv16].c_str(), (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv16)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-fourteenths"),    grid_type_strings[(int)GridTypeBeatDiv14].c_str(), (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv14)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-twelfths"),       grid_type_strings[(int)GridTypeBeatDiv12].c_str(), (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv12)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-tenths"),         grid_type_strings[(int)GridTypeBeatDiv10].c_str(), (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv10)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-eighths"),        grid_type_strings[(int)GridTypeBeatDiv8].c_str(),  (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv8)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-sevenths"),       grid_type_strings[(int)GridTypeBeatDiv7].c_str(),  (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv7)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-sixths"),         grid_type_strings[(int)GridTypeBeatDiv6].c_str(),  (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv6)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-fifths"),         grid_type_strings[(int)GridTypeBeatDiv5].c_str(),  (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv5)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-quarters"),       grid_type_strings[(int)GridTypeBeatDiv4].c_str(),  (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv4)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-thirds"),         grid_type_strings[(int)GridTypeBeatDiv3].c_str(),  (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv3)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-halves"),         grid_type_strings[(int)GridTypeBeatDiv2].c_str(),  (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeatDiv2)));

	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-timecode"),       grid_type_strings[(int)GridTypeTimecode].c_str(),      (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeTimecode)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-minsec"),         grid_type_strings[(int)GridTypeMinSec].c_str(),    (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeMinSec)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-cdframe"),        grid_type_strings[(int)GridTypeCDFrame].c_str(), (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeCDFrame)));

	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-beat"),           grid_type_strings[(int)GridTypeBeat].c_str(),      (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBeat)));
	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-bar"),            grid_type_strings[(int)GridTypeBar].c_str(),       (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeBar)));

	myactions.register_radio_action (snap_actions, grid_choice_group, X_("grid-type-none"),           grid_type_strings[(int)GridTypeNone].c_str(),      (sigc::bind (sigc::mem_fun(*this, &Editor::grid_type_chosen), Editing::GridTypeNone)));

	myactions.register_toggle_action (editor_actions, X_("show-marker-lines"), _("Show Marker Lines"), sigc::mem_fun (*this, &Editor::toggle_marker_lines));

	/* RULERS */

	Glib::RefPtr<ActionGroup> ruler_actions = myactions.create_action_group (X_("Rulers"));
	ruler_tempo_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-tempo-ruler"), _("Tempo"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_time_tempo)));
	ruler_meter_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-meter-ruler"), _("Meter"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_time_meter)));
	ruler_range_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-range-ruler"), _("Ranges"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_time_range_marker)));
	ruler_marker_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-marker-ruler"), _("Markers"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_time_marker)));
	ruler_cd_marker_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-cd-marker-ruler"), _("CD Markers"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_time_cd_marker)));
	ruler_loop_punch_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-loop-punch-ruler"), _("Loop/Punch"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_time_transport_marker)));
	ruler_bbt_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-bbt-ruler"), _("Bars & Beats"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_metric_bbt)));
	ruler_samples_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-samples-ruler"), _("Samples"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_metric_samples)));
	ruler_timecode_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-timecode-ruler"), _("Timecode"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_metric_timecode)));
	ruler_minsec_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-minsec-ruler"), _("Min:Sec"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_metric_minsec)));

	myactions.register_action (editor_menu_actions, X_("VideoMonitorMenu"), _("Video Monitor"));

	ruler_video_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (ruler_actions, X_("toggle-video-ruler"), _("Video"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_ruler_visibility), ruler_video_timeline)));
	xjadeo_proc_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (editor_actions, X_("ToggleJadeo"), _("Video Monitor"), sigc::mem_fun (*this, &Editor::set_xjadeo_proc)));

	xjadeo_ontop_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (editor_actions, X_("toggle-vmon-ontop"), _("Always on Top"), sigc::bind (sigc::mem_fun (*this, &Editor::set_xjadeo_viewoption), (int) 1)));
	xjadeo_timecode_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (editor_actions, X_("toggle-vmon-timecode"), _("Timecode"), sigc::bind (sigc::mem_fun (*this, &Editor::set_xjadeo_viewoption), (int) 2)));
	xjadeo_sample_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (editor_actions, X_("toggle-vmon-frame"), _("Frame number"), sigc::bind (sigc::mem_fun (*this, &Editor::set_xjadeo_viewoption), (int) 3)));
	xjadeo_osdbg_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (editor_actions, X_("toggle-vmon-osdbg"), _("Timecode Background"), sigc::bind (sigc::mem_fun (*this, &Editor::set_xjadeo_viewoption), (int) 4)));
	xjadeo_fullscreen_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (editor_actions, X_("toggle-vmon-fullscreen"), _("Fullscreen"), sigc::bind (sigc::mem_fun (*this, &Editor::set_xjadeo_viewoption), (int) 5)));
	xjadeo_letterbox_action = Glib::RefPtr<ToggleAction>::cast_static (myactions.register_toggle_action (editor_actions, X_("toggle-vmon-letterbox"), _("Letterbox"), sigc::bind (sigc::mem_fun (*this, &Editor::set_xjadeo_viewoption), (int) 6)));
	xjadeo_zoom_100 = reg_sens (editor_actions, "zoom-vmon-100", _("Original Size"), sigc::bind (sigc::mem_fun (*this, &Editor::set_xjadeo_viewoption), (int) 7));

	/* set defaults here */

	no_ruler_shown_update = true;

	if (Profile->get_trx()) {
		ruler_marker_action->set_active (true);
		ruler_meter_action->set_active (false);
		ruler_tempo_action->set_active (false);
		ruler_range_action->set_active (false);
		ruler_loop_punch_action->set_active (false);
		ruler_loop_punch_action->set_active (false);
		ruler_bbt_action->set_active (true);
		ruler_cd_marker_action->set_active (false);
		ruler_timecode_action->set_active (false);
		ruler_minsec_action->set_active (true);
	} else {
		ruler_marker_action->set_active (true);
		ruler_meter_action->set_active (true);
		ruler_tempo_action->set_active (true);
		ruler_range_action->set_active (true);
		ruler_loop_punch_action->set_active (true);
		ruler_loop_punch_action->set_active (true);
		ruler_bbt_action->set_active (true);
		ruler_cd_marker_action->set_active (true);
		ruler_timecode_action->set_active (true);
		ruler_minsec_action->set_active (false);
	}

	ruler_video_action->set_active (false);
	xjadeo_proc_action->set_active (false);
	xjadeo_proc_action->set_sensitive (false);
	xjadeo_ontop_action->set_active (false);
	xjadeo_ontop_action->set_sensitive (false);
	xjadeo_timecode_action->set_active (false);
	xjadeo_timecode_action->set_sensitive (false);
	xjadeo_sample_action->set_active (false);
	xjadeo_sample_action->set_sensitive (false);
	xjadeo_osdbg_action->set_active (false);
	xjadeo_osdbg_action->set_sensitive (false);
	xjadeo_fullscreen_action->set_active (false);
	xjadeo_fullscreen_action->set_sensitive (false);
	xjadeo_letterbox_action->set_active (false);
	xjadeo_letterbox_action->set_sensitive (false);
	xjadeo_zoom_100->set_sensitive (false);

	ruler_samples_action->set_active (false);
	no_ruler_shown_update = false;

	/* REGION LIST */

	Glib::RefPtr<ActionGroup> rl_actions = myactions.create_action_group (X_("RegionList"));
	RadioAction::Group sort_type_group;
	RadioAction::Group sort_order_group;

	/* the region list popup menu */
	myactions.register_action (rl_actions, X_("RegionListSort"), _("Sort"));

	act = myactions.register_action (rl_actions, X_("rlAudition"), _("Audition"), sigc::mem_fun(*this, &Editor::audition_region_from_region_list));
	ActionManager::region_list_selection_sensitive_actions.push_back (act);

	act = myactions.register_action (rl_actions, X_("rlHide"), _("Hide"), sigc::mem_fun(*this, &Editor::hide_region_from_region_list));
	ActionManager::region_list_selection_sensitive_actions.push_back (act);

	act = myactions.register_action (rl_actions, X_("rlShow"), _("Show"), sigc::mem_fun(*this, &Editor::show_region_in_region_list));
	ActionManager::region_list_selection_sensitive_actions.push_back (act);

	myactions.register_toggle_action (rl_actions, X_("rlShowAll"), _("Show All"), sigc::mem_fun(*_regions, &EditorRegions::toggle_full));
	myactions.register_toggle_action (rl_actions, X_("rlShowAuto"), _("Show Automatic Regions"), sigc::mem_fun (*_regions, &EditorRegions::toggle_show_auto_regions));

	myactions.register_radio_action (rl_actions, sort_order_group, X_("SortAscending"),  _("Ascending"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_direction), true));
	myactions.register_radio_action (rl_actions, sort_order_group, X_("SortDescending"),   _("Descending"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_direction), false));

	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortByRegionName"),  _("By Region Name"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), ByName, false));
	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortByRegionLength"),  _("By Region Length"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), ByLength, false));
	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortByRegionPosition"),  _("By Region Position"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), ByPosition, false));
	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortByRegionTimestamp"),  _("By Region Timestamp"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), ByTimestamp, false));
	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortByRegionStartinFile"),  _("By Region Start in File"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), ByStartInFile, false));
	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortByRegionEndinFile"),  _("By Region End in File"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), ByEndInFile, false));
	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFileName"),  _("By Source File Name"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), BySourceFileName, false));
	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFileLength"),  _("By Source File Length"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), BySourceFileLength, false));
	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFileCreationDate"),  _("By Source File Creation Date"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), BySourceFileCreationDate, false));
	myactions.register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFilesystem"),  _("By Source Filesystem"),
			sigc::bind (sigc::mem_fun (*_regions, &EditorRegions::reset_sort_type), BySourceFileFS, false));

	myactions.register_action (rl_actions, X_("removeUnusedRegions"), _("Remove Unused"), sigc::mem_fun (*_regions, &EditorRegions::remove_unused_regions));

	act = reg_sens (editor_actions, X_("addExistingPTFiles"), _("Import PT session"), sigc::mem_fun (*this, &Editor::external_pt_dialog));
	ActionManager::write_sensitive_actions.push_back (act);

	/* the next two are duplicate items with different names for use in two different contexts */

	act = reg_sens (editor_actions, X_("addExternalAudioToRegionList"), _("Import to Region List..."), sigc::bind (sigc::mem_fun(*this, &Editor::add_external_audio_action), ImportAsRegion));
	ActionManager::write_sensitive_actions.push_back (act);

	act = myactions.register_action (editor_actions, X_("importFromSession"), _("Import from Session"), sigc::mem_fun(*this, &Editor::session_import_dialog));
	ActionManager::write_sensitive_actions.push_back (act);


	act = myactions.register_action (editor_actions, X_("bring-into-session"), _("Bring all media into session folder"), sigc::mem_fun(*this, &Editor::bring_all_sources_into_session));
	ActionManager::write_sensitive_actions.push_back (act);

	myactions.register_toggle_action (editor_actions, X_("ToggleSummary"), _("Show Summary"), sigc::mem_fun (*this, &Editor::set_summary));

	myactions.register_toggle_action (editor_actions, X_("ToggleGroupTabs"), _("Show Group Tabs"), sigc::mem_fun (*this, &Editor::set_group_tabs));

	myactions.register_action (editor_actions, X_("toggle-midi-input-active"), _("Toggle MIDI Input Active for Editor-Selected Tracks/Busses"),
	                           sigc::bind (sigc::mem_fun (*this, &Editor::toggle_midi_input_active), false));


	/* MIDI stuff */
	reg_sens (editor_actions, "quantize", _("Quantize"), sigc::mem_fun (*this, &Editor::quantize_region));

}

static void _lua_print (std::string s) {
#ifndef NDEBUG
	std::cout << "LuaInstance: " << s << "\n";
#endif
	PBD::info << "LuaInstance: " << s << endmsg;
}

void
Editor::trigger_script_by_name (const std::string script_name)
{
	string script_path;
	ARDOUR::LuaScriptList scr = LuaScripting::instance ().scripts(LuaScriptInfo::EditorAction);
	for (ARDOUR::LuaScriptList::const_iterator s = scr.begin(); s != scr.end(); ++s) {

		if ((*s)->name == script_name) {
			script_path = (*s)->path;

			if (!Glib::file_test (script_path, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_REGULAR)) {
				cerr << "Lua Script action: path to " << script_path << " does not appear to be valid" << endl;
				return;
			}

			LuaState lua;
			lua.Print.connect (&_lua_print);  //ToDo
			lua.sandbox (false);
			lua_State* L = lua.getState();
			LuaInstance::register_classes (L);
			LuaBindings::set_session (L, _session);
			luabridge::push <PublicEditor *> (L, &PublicEditor::instance());
			lua_setglobal (L, "Editor");
			lua.do_command ("function ardour () end");
			lua.do_file (script_path);
			luabridge::LuaRef args (luabridge::newTable (L));

			//ToDo:  args?
			//	args["how_many"]   = count;

			try {
				luabridge::LuaRef fn = luabridge::getGlobal (L, "factory");
				if (fn.isFunction()) {
					fn (args)();
				}
			} catch (luabridge::LuaException const& e) {
				cerr << "LuaException:" << e.what () << endl;
			} catch (...) {
				cerr << "Lua script failed: " << script_path << endl;
			}
				
			continue;  //script found; we're done
		}
	}

	cerr << "Lua script was not found: " << script_name << endl;
}

void
Editor::load_bindings ()
{
	bindings = Bindings::get_bindings (X_("Editor"), myactions);
	global_hpacker.set_data ("ardour-bindings", bindings);
}

void
Editor::toggle_skip_playback ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), "toggle-skip-playback");

	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		bool s = Config->get_skip_playback ();
		if (tact->get_active() != s) {
			Config->set_skip_playback (tact->get_active());
		}
	}
}

void
Editor::toggle_ruler_visibility (RulerType rt)
{
	const char* action = 0;

	if (no_ruler_shown_update) {
		return;
	}

	switch (rt) {
	case ruler_metric_timecode:
		action = "toggle-timecode-ruler";
		break;
	case ruler_metric_bbt:
		action = "toggle-bbt-ruler";
		break;
	case ruler_metric_samples:
		action = "toggle-samples-ruler";
		break;
	case ruler_metric_minsec:
		action = "toggle-minsec-ruler";
		break;
	case ruler_time_tempo:
		action = "toggle-tempo-ruler";
		break;
	case ruler_time_meter:
		action = "toggle-meter-ruler";
		break;
	case ruler_time_marker:
		action = "toggle-marker-ruler";
		break;
	case ruler_time_range_marker:
		action = "toggle-range-ruler";
		break;
	case ruler_time_transport_marker:
		action = "toggle-loop-punch-ruler";
		break;
	case ruler_time_cd_marker:
		action = "toggle-cd-marker-ruler";
		break;
	case ruler_video_timeline:
		action = "toggle-video-ruler";
		break;
	}

	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Rulers"), action);

	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		update_ruler_visibility ();
		store_ruler_visibility ();
	}
}

void
Editor::set_summary ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleSummary"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
		_session->config.set_show_summary (tact->get_active ());
	}
}

void
Editor::set_group_tabs ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleGroupTabs"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
		_session->config.set_show_group_tabs (tact->get_active ());
	}
}

void
Editor::set_close_video_sensitive (bool onoff)
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Main"), X_("CloseVideo"));
	if (act) {
		act->set_sensitive (onoff);
	}
}

void
Editor::set_xjadeo_sensitive (bool onoff)
{
	xjadeo_proc_action->set_sensitive(onoff);
}

void
Editor::toggle_xjadeo_proc (int state)
{
	switch(state) {
		case 1:
			xjadeo_proc_action->set_active(true);
			break;
		case 0:
			xjadeo_proc_action->set_active(false);
			break;
		default:
			xjadeo_proc_action->set_active(!xjadeo_proc_action->get_active());
			break;
	}
	bool onoff = xjadeo_proc_action->get_active();
	xjadeo_ontop_action->set_sensitive(onoff);
	xjadeo_timecode_action->set_sensitive(onoff);
	xjadeo_sample_action->set_sensitive(onoff);
	xjadeo_osdbg_action->set_sensitive(onoff);
	xjadeo_fullscreen_action->set_sensitive(onoff);
	xjadeo_letterbox_action->set_sensitive(onoff);
	xjadeo_zoom_100->set_sensitive(onoff);
}

void
Editor::set_xjadeo_proc ()
{
	if (xjadeo_proc_action->get_active()) {
		ARDOUR_UI::instance()->video_timeline->open_video_monitor();
	} else {
		ARDOUR_UI::instance()->video_timeline->close_video_monitor();
	}
}

void
Editor::toggle_xjadeo_viewoption (int what, int state)
{
	Glib::RefPtr<Gtk::ToggleAction> action;
	switch (what) {
		case 1:
			action = xjadeo_ontop_action;
			break;
		case 2:
			action = xjadeo_timecode_action;
			break;
		case 3:
			action = xjadeo_sample_action;
			break;
		case 4:
			action = xjadeo_osdbg_action;
			break;
		case 5:
			action = xjadeo_fullscreen_action;
			break;
		case 6:
			action = xjadeo_letterbox_action;
			break;
		case 7:
			return;
		default:
			return;
	}

	switch(state) {
		case 1:
			action->set_active(true);
			break;
		case 0:
			action->set_active(false);
			break;
		default:
			action->set_active(!action->get_active());
			break;
	}
}

void
Editor::set_xjadeo_viewoption (int what)
{
	Glib::RefPtr<Gtk::ToggleAction> action;
	switch (what) {
		case 1:
			action = xjadeo_ontop_action;
			break;
		case 2:
			action = xjadeo_timecode_action;
			break;
		case 3:
			action = xjadeo_sample_action;
			break;
		case 4:
			action = xjadeo_osdbg_action;
			break;
		case 5:
			action = xjadeo_fullscreen_action;
			break;
		case 6:
			action = xjadeo_letterbox_action;
			break;
		case 7:
			ARDOUR_UI::instance()->video_timeline->control_video_monitor(what, 0);
			return;
		default:
			return;
	}
	if (action->get_active()) {
		ARDOUR_UI::instance()->video_timeline->control_video_monitor(what, 1);
	} else {
		ARDOUR_UI::instance()->video_timeline->control_video_monitor(what, 0);
	}
}

void
Editor::edit_current_meter ()
{
	ARDOUR::MeterSection* ms = const_cast<ARDOUR::MeterSection*>(&_session->tempo_map().meter_section_at_sample (ARDOUR_UI::instance()->primary_clock->absolute_time()));
	edit_meter_section (ms);
}

void
Editor::edit_current_tempo ()
{
	ARDOUR::TempoSection* ts = const_cast<ARDOUR::TempoSection*>(&_session->tempo_map().tempo_section_at_sample (ARDOUR_UI::instance()->primary_clock->absolute_time()));
	edit_tempo_section (ts);
}

RefPtr<RadioAction>
Editor::grid_type_action (GridType type)
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

	act = ActionManager::get_action (X_("Snap"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1"), "Editor::grid_type_chosen could not find action to match type.") << endmsg;
		return RefPtr<RadioAction>();
	}
}

void
Editor::next_grid_choice ()
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
Editor::prev_grid_choice ()
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
Editor::grid_type_chosen (GridType type)
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

RefPtr<RadioAction>
Editor::snap_mode_action (SnapMode mode)
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
		error << string_compose (_("programming error: %1: %2"), "Editor::snap_mode_chosen could not find action to match mode.", action) << endmsg;
		return RefPtr<RadioAction> ();
	}
}

void
Editor::cycle_snap_mode ()
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
Editor::snap_mode_chosen (SnapMode mode)
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

RefPtr<RadioAction>
Editor::edit_point_action (EditPoint ep)
{
	const char* action = 0;
	RefPtr<Action> act;

	switch (ep) {
	case Editing::EditAtPlayhead:
		action = X_("edit-at-playhead");
		break;
	case Editing::EditAtSelectedMarker:
		action = X_("edit-at-selected-marker");
		break;
	case Editing::EditAtMouse:
		action = X_("edit-at-mouse");
		break;
	default:
		fatal << string_compose (_("programming error: %1: %2"), "Editor: impossible edit point type", (int) ep) << endmsg;
		abort(); /*NOTREACHED*/
	}

	act = ActionManager::get_action (X_("Editor"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;

	} else  {
		error << string_compose (_("programming error: %1: %2"), "Editor::edit_point_action could not find action to match edit point.", action) << endmsg;
		return RefPtr<RadioAction> ();
	}
}

void
Editor::edit_point_chosen (EditPoint ep)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<RadioAction> ract = edit_point_action (ep);

	if (ract && ract->get_active()) {
		set_edit_point_preference (ep);
	}
}


RefPtr<RadioAction>
Editor::zoom_focus_action (ZoomFocus focus)
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

	act = ActionManager::get_action (X_("Zoom"), action);

	if (act) {
		RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
		return ract;
	} else {
		error << string_compose (_("programming error: %1: %2"), "Editor::zoom_focus_action could not find action to match focus.", action) << endmsg;
	}

	return RefPtr<RadioAction> ();
}

void
Editor::toggle_sound_midi_notes ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("sound-midi-notes"));

	if (act) {
		bool s = UIConfiguration::instance().get_sound_midi_notes();
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
		if (tact->get_active () != s) {
			UIConfiguration::instance().set_sound_midi_notes (tact->get_active());
		}
	}
}

void
Editor::zoom_focus_chosen (ZoomFocus focus)
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

/** A Configuration parameter has changed.
 * @param parameter_name Name of the changed parameter.
 */
void
Editor::parameter_changed (std::string p)
{
	ENSURE_GUI_THREAD (*this, &Editor::parameter_changed, p)

	if (p == "auto-loop") {
		update_loop_range_view ();
	} else if (p == "punch-in") {
		update_punch_range_view ();
	} else if (p == "punch-out") {
		update_punch_range_view ();
	} else if (p == "timecode-format") {
		update_just_timecode ();
	} else if (p == "show-region-fades") {
		update_region_fade_visibility ();
	} else if (p == "edit-mode") {
		edit_mode_selector.set_text (edit_mode_to_string (Config->get_edit_mode()));
	} else if (p == "show-track-meters") {
		toggle_meter_updating();
	} else if (p == "show-summary") {

		bool const s = _session->config.get_show_summary ();
 		if (s) {
 			_summary_hbox.show ();
 		} else {
 			_summary_hbox.hide ();
 		}

		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleSummary"));
		if (act) {
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
			if (tact->get_active () != s) {
				tact->set_active (s);
			}
		}
	} else if (p == "show-group-tabs") {

		bool const s = _session->config.get_show_group_tabs ();
		if (s) {
			_group_tabs->show ();
		} else {
			_group_tabs->hide ();
		}

		reset_controls_layout_width ();

		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleGroupTabs"));
		if (act) {
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
			if (tact->get_active () != s) {
				tact->set_active (s);
			}
		}
	} else if (p == "timecode-offset" || p == "timecode-offset-negative") {
		update_just_timecode ();
	} else if (p == "sound-midi-notes") {
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("sound-midi-notes"));

		if (act) {
			bool s = UIConfiguration::instance().get_sound_midi_notes();
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
			if (tact->get_active () != s) {
				tact->set_active (s);
			}
		}
	} else if (p == "show-region-gain") {
		set_gain_envelope_visibility ();
	} else if (p == "skip-playback") {
		Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-skip-playback"));

		if (act) {
			bool s = Config->get_skip_playback ();
			Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic (act);
			if (tact->get_active () != s) {
				tact->set_active (s);
			}
		}
	}
}

void
Editor::reset_canvas_action_sensitivity (bool onoff)
{
	if (_edit_point != EditAtMouse) {
		onoff = true;
	}

	for (vector<Glib::RefPtr<Action> >::iterator x = ActionManager::mouse_edit_point_requires_canvas_actions.begin();
	     x != ActionManager::mouse_edit_point_requires_canvas_actions.end(); ++x) {
		(*x)->set_sensitive (onoff);
	}
}

void
Editor::register_region_actions ()
{
	_region_actions = myactions.create_action_group (X_("Region"));

	/* PART 1: actions that operate on the selection, and for which the edit point type and location is irrelevant */

	/* Remove selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "remove-region", _("Remove"), sigc::mem_fun (*this, &Editor::remove_selected_regions));

	/* Offer dialogue box to rename the first selected region */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "rename-region", _("Rename..."), sigc::mem_fun (*this, &Editor::rename_region));

	/* Raise all selected regions by 1 layer */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "raise-region", _("Raise"), sigc::mem_fun (*this, &Editor::raise_region));

	/* Raise all selected regions to the top */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "raise-region-to-top", _("Raise to Top"), sigc::mem_fun (*this, &Editor::raise_region_to_top));

	/* Lower all selected regions by 1 layer */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "lower-region", _("Lower"), sigc::mem_fun (*this, &Editor::lower_region));

	/* Lower all selected regions to the bottom */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "lower-region-to-bottom", _("Lower to Bottom"), sigc::mem_fun (*this, &Editor::lower_region_to_bottom));

	/* Move selected regions to their original (`natural') position */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "naturalize-region", _("Move to Original Position"), sigc::mem_fun (*this, &Editor::naturalize_region));

	/* Toggle `locked' status of selected regions */
	register_toggle_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "toggle-region-lock", _("Lock"), sigc::mem_fun(*this, &Editor::toggle_region_lock));
	register_toggle_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "toggle-region-video-lock", _("Lock to Video"), sigc::mem_fun(*this, &Editor::toggle_region_video_lock));
	register_toggle_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "toggle-region-lock-style", _("Glue to Bars and Beats"), sigc::mem_fun (*this, &Editor::toggle_region_lock_style));

	/* Remove sync points from selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "remove-region-sync", _("Remove Sync"), sigc::mem_fun(*this, &Editor::remove_region_sync));

	/* Mute or unmute selected regions */
	register_toggle_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "toggle-region-mute", _("Mute"), sigc::mem_fun(*this, &Editor::toggle_region_mute));

	/* Open the normalize dialogue to operate on the selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "normalize-region", _("Normalize..."), sigc::mem_fun(*this, &Editor::normalize_region));

	/* Reverse selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "reverse-region", _("Reverse"), sigc::mem_fun (*this, &Editor::reverse_region));

	/* Split selected multi-channel regions into mono regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "split-multichannel-region", _("Make Mono Regions"), sigc::mem_fun (*this, &Editor::split_multichannel_region));

	/* Boost selected region gain */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "boost-region-gain", _("Boost Gain"), sigc::bind (sigc::mem_fun(*this, &Editor::adjust_region_gain), true));

	/* Cut selected region gain */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "cut-region-gain", _("Cut Gain"), sigc::bind (sigc::mem_fun(*this, &Editor::adjust_region_gain), false));

	/* Reset selected region gain */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "reset-region-gain", _("Reset Gain"), sigc::mem_fun(*this, &Editor::reset_region_gain));

	/* Open the pitch shift dialogue for any selected audio regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "pitch-shift-region", _("Pitch Shift..."), sigc::mem_fun (*this, &Editor::pitch_shift_region));

	/* Open the transpose dialogue for any selected MIDI regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "transpose-region", _("Transpose..."), sigc::mem_fun (*this, &Editor::transpose_region));

	/* Toggle selected region opacity */
	register_toggle_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "toggle-opaque-region", _("Opaque"), sigc::mem_fun (*this, &Editor::toggle_opaque_region));

	/* Toggle active status of selected regions' fade in */
	register_toggle_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "toggle-region-fade-in", _("Fade In"), sigc::bind (sigc::mem_fun (*this, &Editor::toggle_region_fades), 1));

	/* Toggle active status of selected regions' fade out */
	register_toggle_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "toggle-region-fade-out", _("Fade Out"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_region_fades), -1));

	/* Toggle active status of selected regions' fade in and out */
	register_toggle_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "toggle-region-fades", _("Fades"), sigc::bind (sigc::mem_fun(*this, &Editor::toggle_region_fades), 0));

	/* Duplicate selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "duplicate-region", _("Duplicate"), sigc::bind (sigc::mem_fun (*this, &Editor::duplicate_regions), 1));

	/* Open the dialogue to duplicate selected regions multiple times */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "multi-duplicate-region", _("Multi-Duplicate..."), sigc::bind (sigc::mem_fun(*this, &Editor::duplicate_range), true));

	/* Fill tracks with selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "region-fill-track", _("Fill Track"), sigc::mem_fun (*this, &Editor::region_fill_track));

	/* Set up the loop range from the selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "set-loop-from-region", _("Set Loop Range"), sigc::bind (sigc::mem_fun (*this, &Editor::set_loop_from_region), false));

	/* Set up the loop range from the selected regions, and start playback of it */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "loop-region", _("Loop"), sigc::bind  (sigc::mem_fun(*this, &Editor::set_loop_from_region), true));

	/* Set the punch range from the selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "set-punch-from-region", _("Set Punch"), sigc::mem_fun (*this, &Editor::set_punch_from_region));

	/* Add a single range marker around all selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "add-range-marker-from-region", _("Add Single Range Marker"), sigc::mem_fun (*this, &Editor::add_location_from_region));

	/* Add a range marker around each selected region */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "add-range-markers-from-region", _("Add Range Marker Per Region"), sigc::mem_fun (*this, &Editor::add_locations_from_region));

	/* Snap selected regions to the grid */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "snap-regions-to-grid", _("Snap Position to Grid"), sigc::mem_fun (*this, &Editor::snap_regions_to_grid));

	/* Close gaps in selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "close-region-gaps", _("Close Gaps"), sigc::mem_fun (*this, &Editor::close_region_gaps));

	/* Open the Rhythm Ferret dialogue for the selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "show-rhythm-ferret", _("Rhythm Ferret..."), sigc::mem_fun (*this, &Editor::show_rhythm_ferret));

	/* Export the first selected region */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "export-region", _("Export..."), sigc::mem_fun (*this, &Editor::export_region));

	/* Separate under selected regions: XXX not sure what this does */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "separate-under-region", _("Separate Under"), sigc::mem_fun (*this, &Editor::separate_under_selected_regions));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "set-fade-in-length", _("Set Fade In Length"), sigc::bind (sigc::mem_fun (*this, &Editor::set_fade_length), true));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "alternate-set-fade-in-length", _("Set Fade In Length"), sigc::bind (sigc::mem_fun (*this, &Editor::set_fade_length), true));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "set-fade-out-length", _("Set Fade Out Length"), sigc::bind (sigc::mem_fun (*this, &Editor::set_fade_length), false));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "alternate-set-fade-out-length", _("Set Fade Out Length"), sigc::bind (sigc::mem_fun (*this, &Editor::set_fade_length), false));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "set-tempo-from-region", _("Set Tempo from Region = Bar"), sigc::mem_fun (*this, &Editor::set_tempo_from_region));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "split-region-at-transients", _("Split at Percussion Onsets"), sigc::mem_fun(*this, &Editor::split_region_at_transients));

	/* Open the list editor dialogue for the selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "show-region-list-editor", _("List Editor..."), sigc::mem_fun (*this, &Editor::show_midi_list_editor));

	/* Open the region properties dialogue for the selected regions */
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "show-region-properties", _("Properties..."), sigc::mem_fun (*this, &Editor::show_region_properties));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "play-selected-regions", _("Play selected Regions"), sigc::mem_fun(*this, &Editor::play_selected_region));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "bounce-regions-processed", _("Bounce (with processing)"), (sigc::bind (sigc::mem_fun (*this, &Editor::bounce_region_selection), true)));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "bounce-regions-unprocessed", _("Bounce (without processing)"), (sigc::bind (sigc::mem_fun (*this, &Editor::bounce_region_selection), false)));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "combine-regions", _("Combine"), sigc::mem_fun (*this, &Editor::combine_regions));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "uncombine-regions", _("Uncombine"), sigc::mem_fun (*this, &Editor::uncombine_regions));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "loudness-analyze-region", _("Loudness Analysis..."), sigc::mem_fun (*this, &Editor::loudness_analyze_region_selection));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "spectral-analyze-region", _("Spectral Analysis..."), sigc::mem_fun (*this, &Editor::spectral_analyze_region_selection));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "reset-region-gain-envelopes", _("Reset Envelope"), sigc::mem_fun (*this, &Editor::reset_region_gain_envelopes));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "reset-region-scale-amplitude", _("Reset Gain"), sigc::mem_fun (*this, &Editor::reset_region_scale_amplitude));

	register_toggle_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "toggle-region-gain-envelope-active", _("Envelope Active"), sigc::mem_fun (*this, &Editor::toggle_gain_envelope_active));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "quantize-region", _("Quantize..."), sigc::mem_fun (*this, &Editor::quantize_region));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "legatize-region", _("Legatize"), sigc::bind(sigc::mem_fun (*this, &Editor::legatize_region), false));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "transform-region", _("Transform..."), sigc::mem_fun (*this, &Editor::transform_region));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "remove-overlap", _("Remove Overlap"), sigc::bind(sigc::mem_fun (*this, &Editor::legatize_region), true));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "insert-patch-change", _("Insert Patch Change..."), sigc::bind (sigc::mem_fun (*this, &Editor::insert_patch_change), false));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "insert-patch-change-context", _("Insert Patch Change..."), sigc::bind (sigc::mem_fun (*this, &Editor::insert_patch_change), true));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "fork-region", _("Unlink from other copies"), sigc::mem_fun (*this, &Editor::fork_region));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "strip-region-silence", _("Strip Silence..."), sigc::mem_fun (*this, &Editor::strip_region_silence));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions), "set-selection-from-region", _("Set Range Selection"), sigc::mem_fun (*this, &Editor::set_selection_from_region));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "nudge-forward", _("Nudge Later"), sigc::bind (sigc::mem_fun (*this, &Editor::nudge_forward), false, false));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "alternate-nudge-forward", _("Nudge Later"), sigc::bind (sigc::mem_fun (*this, &Editor::nudge_forward), false, false));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "nudge-backward", _("Nudge Earlier"), sigc::bind (sigc::mem_fun (*this, &Editor::nudge_backward), false, false));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "alternate-nudge-backward", _("Nudge Earlier"), sigc::bind (sigc::mem_fun (*this, &Editor::nudge_backward), false, false));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "sequence-regions", _("Sequence Regions"), sigc::mem_fun (*this, &Editor::sequence_regions));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "nudge-forward-by-capture-offset", _("Nudge Later by Capture Offset"), sigc::mem_fun (*this, &Editor::nudge_forward_capture_offset));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "nudge-backward-by-capture-offset", _("Nudge Earlier by Capture Offset"), sigc::mem_fun (*this, &Editor::nudge_backward_capture_offset));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "trim-region-to-loop", _("Trim to Loop"), sigc::mem_fun (*this, &Editor::trim_region_to_loop));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "trim-region-to-punch", _("Trim to Punch"), sigc::mem_fun (*this, &Editor::trim_region_to_punch));

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "trim-to-previous-region", _("Trim to Previous"), sigc::mem_fun(*this, &Editor::trim_region_to_previous_region_end));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EnteredRegions), "trim-to-next-region", _("Trim to Next"), sigc::mem_fun(*this, &Editor::trim_region_to_next_region_start));

	/* PART 2: actions that are not related to the selection, but for which the edit point type and location is important */

	register_region_action (_region_actions, RegionActionTarget (ListSelection), "insert-region-from-region-list", _("Insert Region from Region List"), sigc::bind (sigc::mem_fun (*this, &Editor::insert_region_list_selection), 1));

	/* PART 3: actions that operate on the selection and also require the edit point location */

	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "set-region-sync-position", _("Set Sync Position"), sigc::mem_fun (*this, &Editor::set_region_sync_position));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "place-transient", _("Place Transient"), sigc::mem_fun (*this, &Editor::place_transient));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "trim-front", _("Trim Start at Edit Point"), sigc::mem_fun (*this, &Editor::trim_region_front));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "trim-back", _("Trim End at Edit Point"), sigc::mem_fun (*this, &Editor::trim_region_back));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "align-regions-start", _("Align Start"), sigc::bind (sigc::mem_fun(*this, &Editor::align_regions), ARDOUR::Start));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "align-regions-start-relative", _("Align Start Relative"), sigc::bind (sigc::mem_fun (*this, &Editor::align_regions_relative), ARDOUR::Start));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "align-regions-end", _("Align End"), sigc::bind (sigc::mem_fun (*this, &Editor::align_regions), ARDOUR::End));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "align-regions-end-relative", _("Align End Relative"), sigc::bind (sigc::mem_fun(*this, &Editor::align_regions_relative), ARDOUR::End));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "align-regions-sync", _("Align Sync"), sigc::bind (sigc::mem_fun(*this, &Editor::align_regions), ARDOUR::SyncPoint));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "align-regions-sync-relative", _("Align Sync Relative"), sigc::bind (sigc::mem_fun (*this, &Editor::align_regions_relative), ARDOUR::SyncPoint));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "choose-top-region", _("Choose Top..."), sigc::bind (sigc::mem_fun (*this, &Editor::change_region_layering_order), false));
	register_region_action (_region_actions, RegionActionTarget (SelectedRegions|EditPointRegions), "choose-top-region-context-menu", _("Choose Top..."), sigc::bind (sigc::mem_fun (*this, &Editor::change_region_layering_order), true));

	/* desensitize them all by default. region selection will change this */
	sensitize_all_region_actions (false);
}
