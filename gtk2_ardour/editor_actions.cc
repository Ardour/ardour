#include <ardour/ardour.h>

#include "utils.h"
#include "editor.h"
#include "editing.h"
#include "actions.h"
#include "ardour_ui.h"
#include "i18n.h"

using namespace Gtk;
using namespace Glib;
using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace PBD;
using namespace Editing;

void
Editor::register_actions ()
{
	RefPtr<Action> act;

	editor_actions = ActionGroup::create (X_("Editor"));
	
	/* non-operative menu items for menu bar */

	ActionManager::register_action (editor_actions, X_("Edit"), _("Edit"));
	ActionManager::register_action (editor_actions, X_("EditSelectRegionOptions"), _("Select regions"));
	ActionManager::register_action (editor_actions, X_("EditSelectRangeOptions"), _("Select range operations"));
	ActionManager::register_action (editor_actions, X_("EditCursorMovementOptions"), _("Move edit cursor"));
	ActionManager::register_action (editor_actions, X_("RegionEditOps"), _("Region operations"));
	ActionManager::register_action (editor_actions, X_("Tools"), _("Tools"));
	ActionManager::register_action (editor_actions, X_("View"), _("View"));
	ActionManager::register_action (editor_actions, X_("ZoomFocus"), _("ZoomFocus"));
	ActionManager::register_action (editor_actions, X_("MeterHold"), _("Meter hold"));
	ActionManager::register_action (editor_actions, X_("MeterFalloff"), _("Meter falloff"));
	ActionManager::register_action (editor_actions, X_("Solo"), _("Solo"));
	ActionManager::register_action (editor_actions, X_("Crossfades"), _("Crossfades"));
	ActionManager::register_action (editor_actions, X_("Monitoring"), _("Monitoring"));
	ActionManager::register_action (editor_actions, X_("Autoconnect"), _("Autoconnect"));
	ActionManager::register_action (editor_actions, X_("Layering"), _("Layering"));
	ActionManager::register_action (editor_actions, X_("Metering"), _("Metering"));
	ActionManager::register_action (editor_actions, X_("MeteringFallOffRate"), _("Fall off rate"));
	ActionManager::register_action (editor_actions, X_("MeteringHoldTime"), _("Hold Time"));
	ActionManager::register_action (editor_actions, X_("addExistingAudioFiles"), _("Add Existing Audio"));

	/* add named actions for the editor */


	act = ActionManager::register_toggle_action (editor_actions, "show-editor-mixer", _("Show Editor Mixer"), mem_fun (*this, &Editor::editor_mixer_button_toggled));
	ActionManager::session_sensitive_actions.push_back (act);

	RadioAction::Group crossfade_model_group;

	act = ActionManager::register_radio_action (editor_actions, crossfade_model_group, "CrossfadesFull", _("Span Entire Overlap"), bind (mem_fun(*this, &Editor::set_crossfade_model), FullCrossfade));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_radio_action (editor_actions, crossfade_model_group, "CrossfadesShort", _("Short"), bind (mem_fun(*this, &Editor::set_crossfade_model), ShortCrossfade));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (editor_actions, "toggle-xfades-active", _("Active"), mem_fun(*this, &Editor::toggle_xfades_active));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (editor_actions, "toggle-xfades-visible", _("Visible"), mem_fun(*this, &Editor::toggle_xfade_visibility));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_toggle_action (editor_actions, "toggle-auto-xfades", _("Created Automatically"), mem_fun(*this, &Editor::toggle_auto_xfade));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "playhead-to-next-region-start", _("Playhead to Next Region Start"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (Start)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "playhead-to-next-region-end", _("Playhead to Next Region End"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (End)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "playhead-to-next-region-sync", _("Playhead to Next Region Sync"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (SyncPoint)));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "playhead-to-previous-region-start", _("Playhead to Previous Region Start"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (Start)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "playhead-to-previous-region-end", _("Playhead to Previous Region End"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (End)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "playhead-to-previous-region-sync", _("Playhead to Previous Region Sync"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (SyncPoint)));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "edit-cursor-to-next-region-start", _("Edit Cursor to Next Region Start"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (Start)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "edit-cursor-to-next-region-end", _("Edit Cursor to Next Region End"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (End)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "edit-cursor-to-next-region-sync", _("Edit Cursor to Next Region Sync"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (SyncPoint)));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "edit-cursor-to-previous-region-start", _("Edit Cursor to Previous Region Start"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (Start)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "edit-cursor-to-previous-region-end", _("Edit Cursor to Previous Region End"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (End)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "edit-cursor-to-previous-region-sync", _("Edit Cursor to Previous Region Sync"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (SyncPoint)));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "playhead-to-range-start", _("Playhead to Range Start"), bind (mem_fun(*this, &Editor::cursor_to_selection_start), playhead_cursor));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "playhead-to-range-end", _("Playhead to Range End"), bind (mem_fun(*this, &Editor::cursor_to_selection_end), playhead_cursor));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "edit-cursor-to-range-start", _("Edit Cursor to Range Start"), bind (mem_fun(*this, &Editor::cursor_to_selection_start), edit_cursor));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "edit-cursor-to-range-end", _("Edit Cursor to Range End"), bind (mem_fun(*this, &Editor::cursor_to_selection_end), edit_cursor));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "select-all", _("select all"), bind (mem_fun(*this, &Editor::select_all), Selection::Set));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "select-all-after-edit-cursor", _("Select All After Edit Cursor"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), edit_cursor, true));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "select-all-before-edit-cursor", _("Select All Before Edit Cursor"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), edit_cursor, false));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "select-all-after-playhead", _("Select All After Playhead"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, true));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "select-all-before-playhead", _("Select All Before Playhead"), bind (mem_fun(*this, &Editor::select_all_selectables_using_cursor), playhead_cursor, false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "select-all-between-cursors", _("Select All Between Cursors"), bind (mem_fun(*this, &Editor::select_all_selectables_between_cursors), playhead_cursor, edit_cursor));
	ActionManager::session_sensitive_actions.push_back (act);

       	act = ActionManager::register_action (editor_actions, "select-all-in-punch-range", _("Select All in Punch Range"), mem_fun(*this, &Editor::select_all_selectables_using_punch));
	ActionManager::session_sensitive_actions.push_back (act);
       	act = ActionManager::register_action (editor_actions, "select-all-in-loop-range", _("Select All in Loop Range"), mem_fun(*this, &Editor::select_all_selectables_using_loop));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "jump-forward-to-mark", _("Jump Forward to Mark"), mem_fun(*this, &Editor::jump_forward_to_mark));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "jump-backward-to-mark", _("Jump Backward to Mark"), mem_fun(*this, &Editor::jump_backward_to_mark));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "add-location-from-playhead", _("Add Location from Playhead"), mem_fun(*this, &Editor::add_location_from_playhead_cursor));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "nudge-forward", _("Nudge Forward"), bind (mem_fun(*this, &Editor::nudge_forward), false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "nudge-next-forward", _("Nudge Next Forward"), bind (mem_fun(*this, &Editor::nudge_forward), true));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "nudge-backward", _("Nudge Backward"), bind (mem_fun(*this, &Editor::nudge_backward), false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "nudge-next-backward", _("Nudge Next Backward"), bind (mem_fun(*this, &Editor::nudge_backward), true));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "temporal-zoom-out", _("Zoom Out"), bind (mem_fun(*this, &Editor::temporal_zoom_step), true));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "temporal-zoom-in", _("Zoom In"), bind (mem_fun(*this, &Editor::temporal_zoom_step), false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "zoom-to-session", _("Zoom to Session"), mem_fun(*this, &Editor::temporal_zoom_session));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "scroll-tracks-up", _("Scroll Tracks Up"), mem_fun(*this, &Editor::scroll_tracks_up));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "scroll-tracks-down", _("Scroll Tracks Down"), mem_fun(*this, &Editor::scroll_tracks_down));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "step-tracks-up", _("Step Tracks Up"), mem_fun(*this, &Editor::scroll_tracks_up_line));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "step-tracks-down", _("Step Tracks Down"), mem_fun(*this, &Editor::scroll_tracks_down_line));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "scroll-backward", _("Scroll Backward"), bind (mem_fun(*this, &Editor::scroll_backward), 0.8f));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "scroll-forward", _("Scroll Forward"), bind (mem_fun(*this, &Editor::scroll_forward), 0.8f));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "goto", _("goto"), mem_fun(*this, &Editor::goto_frame));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "center-playhead", _("Center Playhead"), mem_fun(*this, &Editor::center_playhead));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "center-edit-cursor", _("Center Edit Cursor"), mem_fun(*this, &Editor::center_edit_cursor));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "playhead-forward", _("Playhead Forward"), mem_fun(*this, &Editor::playhead_forward));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "playhead-backward", _("Playhead Backward"), mem_fun(*this, &Editor::playhead_backward));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "playhead-to-edit", _("Playhead to Edit"), bind (mem_fun(*this, &Editor::cursor_align), true));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "edit-to-playhead", _("Edit to Playhead"), bind (mem_fun(*this, &Editor::cursor_align), false));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "align-regions-start", _("Align Regions Start"), bind (mem_fun(*this, &Editor::align), ARDOUR::Start));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "align-regions-start-relative", _("Align Regions Start Relative"), bind (mem_fun(*this, &Editor::align_relative), ARDOUR::Start));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "align-regions-end", _("Align Regions End"), bind (mem_fun(*this, &Editor::align), ARDOUR::End));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "align-regions-end-relative", _("Align Regions End Relative"), bind (mem_fun(*this, &Editor::align_relative), ARDOUR::End));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "align-regions-sync", _("Align Regions Sync"), bind (mem_fun(*this, &Editor::align), ARDOUR::SyncPoint));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "align-regions-sync-relative", _("Align Regions Sync Relative"), bind (mem_fun(*this, &Editor::align_relative), ARDOUR::SyncPoint));
	ActionManager::session_sensitive_actions.push_back (act);
	
        act = ActionManager::register_action (editor_actions, "audition-at-mouse", _("Audition at Mouse"), mem_fun(*this, &Editor::kbd_audition));
        ActionManager::session_sensitive_actions.push_back (act);
        act = ActionManager::register_action (editor_actions, "brush-at-mouse", _("Brush at Mouse"), mem_fun(*this, &Editor::kbd_brush));
        ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "set-edit-cursor", _("Set Edit Cursor"), mem_fun(*this, &Editor::kbd_set_edit_cursor));
        ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "mute-unmute-region", _("Mute/Unmute Region"), mem_fun(*this, &Editor::kbd_mute_unmute_region));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "set-playhead", _("Set Playhead"), mem_fun(*this, &Editor::kbd_set_playhead_cursor));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "split-region", _("Split Region"), mem_fun(*this, &Editor::kbd_split));
        ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "set-region-sync-position", _("Set Region Sync Position"), mem_fun(*this, &Editor::kbd_set_sync_position));
        ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "undo", _("Undo"), bind (mem_fun(*this, &Editor::undo), 1U));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "redo", _("Redo"), bind (mem_fun(*this, &Editor::redo), 1U));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "export-session", _("Export Session"), mem_fun(*this, &Editor::export_session));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "export-range", _("Export Range"), mem_fun(*this, &Editor::export_selection));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "editor-cut", _("Cut"), mem_fun(*this, &Editor::cut));
	ActionManager::session_sensitive_actions.push_back (act);
	/* Note: for now, editor-delete does the exact same thing as editor-cut */
	act = ActionManager::register_action (editor_actions, "editor-delete", _("Delete"), mem_fun(*this, &Editor::cut));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "editor-copy", _("Copy"), mem_fun(*this, &Editor::copy));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "editor-paste", _("Paste"), mem_fun(*this, &Editor::keyboard_paste));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "duplicate-region", _("Duplicate Region"), mem_fun(*this, &Editor::keyboard_duplicate_region));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "duplicate-range", _("Duplicate Range"), mem_fun(*this, &Editor::keyboard_duplicate_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "insert-region", _("Insert Region"), mem_fun(*this, &Editor::keyboard_insert_region_list_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "reverse-region", _("Reverse Region"), mem_fun(*this, &Editor::reverse_region));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "normalize-region", _("Normalize Region"), mem_fun(*this, &Editor::normalize_region));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "crop", _("crop"), mem_fun(*this, &Editor::crop_region_to_selection));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "insert-chunk", _("Insert Chunk"), bind (mem_fun(*this, &Editor::paste_named_selection), 1.0f));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "split-at-edit-cursor", _("Split at edit cursor"), mem_fun(*this, &Editor::split_region));
	ActionManager::edit_cursor_in_region_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "start-range", _("Start Range"), mem_fun(*this, &Editor::keyboard_selection_begin));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "finish-range", _("Finish Range"), bind (mem_fun(*this, &Editor::keyboard_selection_finish), false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "finish-add-range", _("Finish add Range"), bind (mem_fun(*this, &Editor::keyboard_selection_finish), true));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_action (editor_actions, "extend-range-to-end-of-region", _("Extend Range to End of Region"), bind (mem_fun(*this, &Editor::extend_selection_to_end_of_region), false));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "extend-range-to-start-of-region", _("Extend Range to Start of Region"), bind (mem_fun(*this, &Editor::extend_selection_to_start_of_region), false));
	ActionManager::session_sensitive_actions.push_back (act);

	act = ActionManager::register_toggle_action (editor_actions, "toggle-follow-playhead", _("Follow Playhead"), (mem_fun(*this, &Editor::toggle_follow_playhead)));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, "remove-last-capture", _("Remove Last Capture"), (mem_fun(*this, &Editor::remove_last_capture)));
	ActionManager::session_sensitive_actions.push_back (act);

	Glib::RefPtr<ActionGroup> zoom_actions = ActionGroup::create (X_("Zoom"));
	RadioAction::Group zoom_group;

	ActionManager::register_radio_action (zoom_actions, zoom_group, "zoom-focus-left", _("Zoom Focus Left"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusLeft));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_radio_action (zoom_actions, zoom_group, "zoom-focus-right", _("Zoom Focus Right"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusRight));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_radio_action (zoom_actions, zoom_group, "zoom-focus-center", _("Zoom Focus Center"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusCenter));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_radio_action (zoom_actions, zoom_group, "zoom-focus-playhead", _("Zoom Focus Playhead"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusPlayhead));
	ActionManager::session_sensitive_actions.push_back (act);
	ActionManager::register_radio_action (zoom_actions, zoom_group, "zoom-focus-edit", _("Zoom Focus Edit"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusEdit));
	ActionManager::session_sensitive_actions.push_back (act);

	Glib::RefPtr<ActionGroup> mouse_mode_actions = ActionGroup::create (X_("MouseMode"));
	RadioAction::Group mouse_mode_group;

	ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-object", _("Object Tool"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseObject, false));
	ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-range", _("Range Tool"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseRange, false));
	ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-gain", _("Gain Tool"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseGain, false));
	ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-zoom", _("Zoom Tool"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseZoom, false));
	ActionManager::register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-timefx", _("Timefx Tool"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseTimeFX, false));

	Glib::RefPtr<ActionGroup> snap_actions = ActionGroup::create (X_("Snap"));
	RadioAction::Group snap_choice_group;

	ActionManager::register_action (editor_actions, X_("SnapTo"), _("Snap To"));

	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-frame"), _("Snap to frame"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToFrame)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-cd-frame"), _("Snap to cd frame"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToCDFrame)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-smpte-frame"), _("Snap to SMPTE frame"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTEFrame)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-smpte-seconds"), _("Snap to SMPTE seconds"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTESeconds)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-smpte-minutes"), _("Snap to SMPTE minutes"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTEMinutes)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-seconds"), _("Snap to seconds"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSeconds)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-minutes"), _("Snap to minutes"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToMinutes)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-thirtyseconds"), _("Snap to thirtyseconds"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAThirtysecondBeat)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-asixteenthbeat"), _("Snap to asixteenthbeat"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToASixteenthBeat)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-eighths"), _("Snap to eighths"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAEighthBeat)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-quarters"), _("Snap to quarters"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAQuarterBeat)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-thirds"), _("Snap to thirds"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAThirdBeat)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-beat"), _("Snap to beat"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToBeat)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-bar"), _("Snap to bar"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToBar)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-mark"), _("Snap to mark"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToMark)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-edit-cursor"), _("Snap to edit cursor"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToEditCursor)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-region-start"), _("Snap to region start"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionStart)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-region-end"), _("Snap to region end"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionEnd)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-region-sync"), _("Snap to region sync"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionSync)));
	ActionManager::register_radio_action (snap_actions, snap_choice_group, X_("snap-to-region-boundary"), _("Snap to region boundary"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionBoundary)));

	/* REGION LIST */

	Glib::RefPtr<ActionGroup> rl_actions = ActionGroup::create (X_("RegionList"));
	RadioAction::Group sort_type_group;
	RadioAction::Group sort_order_group;

	/* the region list popup menu */
	ActionManager::register_action (rl_actions, X_("RegionListSort"), _("Sort"));

	act = ActionManager::register_action (rl_actions, X_("rlAudition"), _("Audition"), mem_fun(*this, &Editor::audition_region_from_region_list));
	ActionManager::region_list_selection_sensitive_actions.push_back (act);
	act = ActionManager::register_action (rl_actions, X_("rlHide"), _("Hide"), mem_fun(*this, &Editor::hide_region_from_region_list));
	ActionManager::region_list_selection_sensitive_actions.push_back (act);
	act = ActionManager::register_action (rl_actions, X_("rlRemove"), _("Remove"), mem_fun (*this, &Editor::remove_region_from_region_list));
	ActionManager::region_list_selection_sensitive_actions.push_back (act);
	ActionManager::register_toggle_action (rl_actions, X_("rlShowAll"), _("Show all"), mem_fun(*this, &Editor::toggle_full_region_list));
	ActionManager::register_toggle_action (rl_actions, X_("rlShowAuto"), _("Show automatic regions"), mem_fun(*this, &Editor::toggle_show_auto_regions));

	ActionManager::register_radio_action (rl_actions, sort_order_group, X_("SortAscending"),  _("Ascending"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_direction), true));
	ActionManager::register_radio_action (rl_actions, sort_order_group, X_("SortDescending"),   _("Descending"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_direction), false));
	
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortByRegionName"),  _("By Region Name"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByName));
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortByRegionLength"),  _("By Region Length"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByLength));
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortByRegionPosition"),  _("By Region Position"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByPosition));
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortByRegionTimestamp"),  _("By Region Timestamp"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByTimestamp));
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortByRegionStartinFile"),  _("By Region Start in File"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByStartInFile));
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortByRegionEndinFile"),  _("By Region End in File"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByEndInFile));
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFileName"),  _("By Source File Name"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileName));
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFileLength"),  _("By Source File Length"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileLength));
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFileCreationDate"),  _("By Source File Creation Date"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileCreationDate));
	ActionManager::register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFilesystem"),  _("By Source Filesystem"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileFS));


	/* the next two are duplicate items with different names for use in two different contexts */

	act = ActionManager::register_action (editor_actions, X_("addExternalAudioToRegionList"), _("Add External Audio"), bind (mem_fun(*this, &Editor::add_external_audio_action), ImportAsRegion));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, X_("addExternalAudioAsRegion"), _("as Region(s)"), bind (mem_fun(*this, &Editor::add_external_audio_action), ImportAsRegion));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, X_("addExternalAudioAsTrack"), _("as Tracks"), bind (mem_fun(*this, &Editor::add_external_audio_action), ImportAsTrack));
	ActionManager::session_sensitive_actions.push_back (act);
	act = ActionManager::register_action (editor_actions, X_("addExternalAudioToTrack"), _("to Tracks"), bind (mem_fun(*this, &Editor::add_external_audio_action), ImportToTrack));
	ActionManager::session_sensitive_actions.push_back (act);

	ActionManager::register_toggle_action (editor_actions, X_("ToggleWaveformVisibility"), _("Show Waveforms"), mem_fun (*this, &Editor::toggle_waveform_visibility));
	ActionManager::register_toggle_action (editor_actions, X_("ToggleWaveformsWhileRecording"), _("Show Waveforms While Recording"), mem_fun (*this, &Editor::toggle_waveforms_while_recording));
	act = ActionManager::register_toggle_action (editor_actions, X_("ToggleMeasureVisibility"), _("Show Measures"), mem_fun (*this, &Editor::toggle_measure_visibility));
	
	RadioAction::Group meter_falloff_group;
	RadioAction::Group meter_hold_group;

	/*
	    Slowest = 6.6dB/sec falloff at update rate of 40ms
	    Slow    = 6.8dB/sec falloff at update rate of 40ms
	*/

	ActionManager::register_radio_action (editor_actions, meter_falloff_group, X_("MeterFalloffOff"), _("Off"), bind (mem_fun (*this, &Editor::set_meter_falloff), 0));
	ActionManager::register_radio_action (editor_actions, meter_falloff_group, X_("MeterFalloffSlowest"), _("Slowest"), bind (mem_fun (*this, &Editor::set_meter_falloff), 1)); 
	ActionManager::register_radio_action (editor_actions, meter_falloff_group, X_("MeterFalloffSlow"), _("Slow"), bind (mem_fun (*this, &Editor::set_meter_falloff), 2));
	ActionManager::register_radio_action (editor_actions, meter_falloff_group, X_("MeterFalloffMedium"), _("Medium"), bind (mem_fun (*this, &Editor::set_meter_falloff), 3));
	ActionManager::register_radio_action (editor_actions, meter_falloff_group, X_("MeterFalloffFast"), _("Fast"), bind (mem_fun (*this, &Editor::set_meter_falloff), 4));
	ActionManager::register_radio_action (editor_actions, meter_falloff_group, X_("MeterFalloffFaster"), _("Faster"), bind (mem_fun (*this, &Editor::set_meter_falloff), 5));
	ActionManager::register_radio_action (editor_actions, meter_falloff_group, X_("MeterFalloffFastest"), _("Fastest"), bind (mem_fun (*this, &Editor::set_meter_falloff), 6));

	ActionManager::register_radio_action (editor_actions, meter_hold_group,  X_("MeterHoldOff"), _("Off"), bind (mem_fun (*this, &Editor::set_meter_hold), 0));
	ActionManager::register_radio_action (editor_actions, meter_hold_group,  X_("MeterHoldShort"), _("Short"), bind (mem_fun (*this, &Editor::set_meter_hold), 40));
	ActionManager::register_radio_action (editor_actions, meter_hold_group,  X_("MeterHoldMedium"), _("Medium"), bind (mem_fun (*this, &Editor::set_meter_hold), 100));
	ActionManager::register_radio_action (editor_actions, meter_hold_group,  X_("MeterHoldLong"), _("Long"), bind (mem_fun (*this, &Editor::set_meter_hold), 200));

	RadioAction::Group layer_model_group;

	ActionManager::register_radio_action (editor_actions, layer_model_group,  X_("LayerLaterHigher"), _("Later is Higher"), bind (mem_fun (*this, &Editor::set_layer_model), Session::LaterHigher));
	ActionManager::register_radio_action (editor_actions, layer_model_group,  X_("LayerMoveAddHigher"), _("Most Recently Moved/Added is Higher"), bind (mem_fun (*this, &Editor::set_layer_model), Session::MoveAddHigher));
	ActionManager::register_radio_action (editor_actions, layer_model_group,  X_("LayerAddHigher"), _("Most Recently Added is Higher"), bind (mem_fun (*this, &Editor::set_layer_model), Session::AddHigher));

	ActionManager::add_action_group (rl_actions);
	ActionManager::add_action_group (zoom_actions);
	ActionManager::add_action_group (mouse_mode_actions);
	ActionManager::add_action_group (snap_actions);
	ActionManager::add_action_group (editor_actions);
}

void
Editor::toggle_waveform_visibility ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleWaveformVisibility"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		set_show_waveforms (tact->get_active());
	}
}

void
Editor::toggle_waveforms_while_recording ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleWaveformVisibility"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		set_show_waveforms_recording (tact->get_active());
	}
}

void
Editor::toggle_measure_visibility ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("ToggleMeasureVisibility"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		set_show_measures (tact->get_active());
	}
}

void
Editor::toggle_auto_xfade ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-auto-xfades"));
	if (act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		Config->set_auto_xfade (tact->get_active());
	}
}

void
Editor::toggle_xfades_active ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-xfades-active"));
	if (session && act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		session->set_crossfades_active (tact->get_active());
	}
}

void
Editor::toggle_xfade_visibility ()
{
	Glib::RefPtr<Action> act = ActionManager::get_action (X_("Editor"), X_("toggle-xfades-visible"));
	if (session && act) {
		Glib::RefPtr<ToggleAction> tact = Glib::RefPtr<ToggleAction>::cast_dynamic(act);
		// set_xfade_visibility (tact->get_active());
	}
}

void
Editor::set_layer_model (Session::LayerModel model)
{
	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	RefPtr<Action> act;

	if (session) {
		switch (model) {
		case Session::LaterHigher:
			act = ActionManager::get_action (X_("Editor"), X_("LayerLaterHigher"));
			break;
		case Session::MoveAddHigher:
			act = ActionManager::get_action (X_("Editor"), X_("LayerMoveAddHigher"));
			break;
		case Session::AddHigher:
			act = ActionManager::get_action (X_("Editor"), X_("LayerAddHigher"));
			break;
		}
		
		if (act) {
			RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
			if (ract && ract->get_active()) {
				session->set_layer_model (model);
			}
		}
	}
}

void
Editor::set_crossfade_model (CrossfadeModel model)
{
	RefPtr<Action> act;

	/* this is driven by a toggle on a radio group, and so is invoked twice,
	   once for the item that became inactive and once for the one that became
	   active.
	*/

	if (session) {
		switch (model) {
		case FullCrossfade:
			act = ActionManager::get_action (X_("Editor"), X_("CrossfadesFull"));
			break;
		case ShortCrossfade:
			act = ActionManager::get_action (X_("Editor"), X_("CrossfadesShort"));
			break;
		}
		
		if (act) {
			RefPtr<RadioAction> ract = RefPtr<RadioAction>::cast_dynamic(act);
			if (ract && ract->get_active()) {
				session->set_xfade_model (model);
			}
		}
	}
}
