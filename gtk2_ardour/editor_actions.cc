#include <ardour/ardour.h>

#include "utils.h"
#include "editor.h"
#include "editing.h"
#include "actions.h"

#include "i18n.h"

using namespace Gtk;
using namespace std;
using namespace sigc;
using namespace ARDOUR;
using namespace Editing;
using namespace ActionManager;

void
Editor::register_actions ()
{

	Glib::RefPtr<ActionGroup> editor_actions = ActionGroup::create (X_("Editor"));

	/* start with actions shared between all top level (major) windows */

	merge_actions (editor_actions, ARDOUR_UI::instance()::get_common_actions());

	/* add named actions for the editor */

	register_action (editor_actions, "toggle-xfades-active", _("toggle xfades active"), mem_fun(*this, &Editor::toggle_xfades_active));

	register_action (editor_actions, "playhead-to-next-region-start", _("playhead to next region start"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (Start)));
	register_action (editor_actions, "playhead-to-next-region-end", _("playhead to next region end"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (End)));
	register_action (editor_actions, "playhead-to-next-region-sync", _("playhead to next region sync"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (SyncPoint)));

	register_action (editor_actions, "playhead-to-previous-region-start", _("playhead to previous region start"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (Start)));
	register_action (editor_actions, "playhead-to-previous-region-end", _("playhead to previous region end"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (End)));
	register_action (editor_actions, "playhead-to-previous-region-sync", _("playhead to previous region sync"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (SyncPoint)));

	register_action (editor_actions, "edit-cursor-to-next-region-start", _("edit cursor to next region start"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (Start)));
	register_action (editor_actions, "edit-cursor-to-next-region-end", _("edit cursor to next region end"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (End)));
	register_action (editor_actions, "edit-cursor-to-next-region-sync", _("edit cursor to next region sync"), bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (SyncPoint)));

	register_action (editor_actions, "edit-cursor-to-previous-region-start", _("edit cursor to previous region start"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (Start)));
	register_action (editor_actions, "edit-cursor-to-previous-region-end", _("edit cursor to previous region end"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (End)));
	register_action (editor_actions, "edit-cursor-to-previous-region-sync", _("edit cursor to previous region sync"), bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (SyncPoint)));

	register_action (editor_actions, "playhead-to-range-start", _("playhead to range start"), bind (mem_fun(*this, &Editor::cursor_to_selection_start), playhead_cursor));
	register_action (editor_actions, "playhead-to-range-end", _("playhead to range end"), bind (mem_fun(*this, &Editor::cursor_to_selection_end), playhead_cursor));

	register_action (editor_actions, "edit-cursor-to-range-start", _("edit cursor to range start"), bind (mem_fun(*this, &Editor::cursor_to_selection_start), edit_cursor));
	register_action (editor_actions, "edit-cursor-to-range-end", _("edit cursor to range end"), bind (mem_fun(*this, &Editor::cursor_to_selection_end), edit_cursor));

	register_action (editor_actions, "jump-forward-to-mark", _("jump forward to mark"), mem_fun(*this, &Editor::jump_forward_to_mark));
	register_action (editor_actions, "jump-backward-to-mark", _("jump backward to mark"), mem_fun(*this, &Editor::jump_backward_to_mark));
	register_action (editor_actions, "add-location-from-playhead", _("add location from playhead"), mem_fun(*this, &Editor::add_location_from_playhead_cursor));

	register_action (editor_actions, "nudge-forward", _("nudge forward"), bind (mem_fun(*this, &Editor::nudge_forward), false));
	register_action (editor_actions, "nudge-next-forward", _("nudge next forward"), bind (mem_fun(*this, &Editor::nudge_forward), true));
	register_action (editor_actions, "nudge-backward", _("nudge backward"), bind (mem_fun(*this, &Editor::nudge_backward), false));
	register_action (editor_actions, "nudge-next-backward", _("nudge next backward"), bind (mem_fun(*this, &Editor::nudge_backward), true));

	register_action (editor_actions, "toggle-playback", _("toggle playback"), bind (mem_fun(*this, &Editor::toggle_playback), false));
	register_action (editor_actions, "toggle-playback-forget-capture", _("toggle playback forget capture"), bind (mem_fun(*this, &Editor::toggle_playback), true));

	register_action (editor_actions, "toggle-loop-playback", _("toggle loop playback"), mem_fun(*this, &Editor::toggle_loop_playback));
	
	register_action (editor_actions, "temporal-zoom-out", _("temporal zoom out"), bind (mem_fun(*this, &Editor::temporal_zoom_step), true));
	register_action (editor_actions, "temporal-zoom-in", _("temporal zoom in"), bind (mem_fun(*this, &Editor::temporal_zoom_step), false));
	register_action (editor_actions, "zoom-to-session", _("zoom to session"), mem_fun(*this, &Editor::temporal_zoom_session));

	register_action (editor_actions, "scroll-tracks-up", _("scroll tracks up"), mem_fun(*this, &Editor::scroll_tracks_up));
	register_action (editor_actions, "scroll-tracks-down", _("scroll tracks down"), mem_fun(*this, &Editor::scroll_tracks_down));
	register_action (editor_actions, "step-tracks-up", _("step tracks up"), mem_fun(*this, &Editor::scroll_tracks_up_line));
	register_action (editor_actions, "step-tracks-down", _("step tracks down"), mem_fun(*this, &Editor::scroll_tracks_down_line));

	register_action (editor_actions, "scroll-backward", _("scroll backward"), bind (mem_fun(*this, &Editor::scroll_backward), 0.8f));
	register_action (editor_actions, "scroll-forward", _("scroll forward"), bind (mem_fun(*this, &Editor::scroll_forward), 0.8f));
	register_action (editor_actions, "goto", _("goto"), mem_fun(*this, &Editor::goto_frame));
	register_action (editor_actions, "center-playhead", _("center playhead"), mem_fun(*this, &Editor::center_playhead));
	register_action (editor_actions, "center-edit_cursor", _("center edit_cursor"), mem_fun(*this, &Editor::center_edit_cursor));
	register_action (editor_actions, "playhead-forward", _("playhead forward"), mem_fun(*this, &Editor::playhead_forward));
	register_action (editor_actions, "playhead-backward", _("playhead backward"), mem_fun(*this, &Editor::playhead_backward));
	register_action (editor_actions, "playhead-to-edit", _("playhead to edit"), bind (mem_fun(*this, &Editor::cursor_align), true));
	register_action (editor_actions, "edit-to-playhead", _("edit to playhead"), bind (mem_fun(*this, &Editor::cursor_align), false));

	register_action (editor_actions, "align-regions-start", _("align regions start"), bind (mem_fun(*this, &Editor::align), ARDOUR::Start));
	register_action (editor_actions, "align-regions-start-relative", _("align regions start relative"), bind (mem_fun(*this, &Editor::align_relative), ARDOUR::Start));
	register_action (editor_actions, "align-regions-end", _("align regions end"), bind (mem_fun(*this, &Editor::align), ARDOUR::End));
	register_action (editor_actions, "align-regions-end-relative", _("align regions end relative"), bind (mem_fun(*this, &Editor::align_relative), ARDOUR::End));
	register_action (editor_actions, "align-regions-sync", _("align regions sync"), bind (mem_fun(*this, &Editor::align), ARDOUR::SyncPoint));
	register_action (editor_actions, "align-regions-sync-relative", _("align regions sync relative"), bind (mem_fun(*this, &Editor::align_relative), ARDOUR::SyncPoint));
	
	register_action (editor_actions, "set-playhead", _("set playhead"), mem_fun(*this, &Editor::kbd_set_playhead_cursor));
	register_action (editor_actions, "set-edit-cursor", _("set edit cursor"), mem_fun(*this, &Editor::kbd_set_edit_cursor));

	register_action (editor_actions, "set-undo", _("set undo"), bind (mem_fun(*this, &Editor::undo), 1U));
	register_action (editor_actions, "set-redo", _("set redo"), bind (mem_fun(*this, &Editor::redo), 1U));

	register_action (editor_actions, "export-session", _("export session"), mem_fun(*this, &Editor::export_session));
	register_action (editor_actions, "export-range", _("export range"), mem_fun(*this, &Editor::export_selection));

	register_action (editor_actions, "editor-cut", _("editor cut"), mem_fun(*this, &Editor::cut));
	register_action (editor_actions, "editor-copy", _("editor copy"), mem_fun(*this, &Editor::copy));
	register_action (editor_actions, "editor-paste", _("editor paste"), mem_fun(*this, &Editor::keyboard_paste));
	register_action (editor_actions, "duplicate-region", _("duplicate region"), mem_fun(*this, &Editor::keyboard_duplicate_region));
	register_action (editor_actions, "duplicate-range", _("duplicate range"), mem_fun(*this, &Editor::keyboard_duplicate_selection));
	register_action (editor_actions, "insert-region", _("insert region"), mem_fun(*this, &Editor::keyboard_insert_region_list_selection));
	register_action (editor_actions, "reverse-region", _("reverse region"), mem_fun(*this, &Editor::reverse_region));
	register_action (editor_actions, "normalize-region", _("normalize region"), mem_fun(*this, &Editor::normalize_region));
	register_action (editor_actions, "editor-crop", _("editor crop"), mem_fun(*this, &Editor::crop_region_to_selection));
	register_action (editor_actions, "insert-chunk", _("insert chunk"), bind (mem_fun(*this, &Editor::paste_named_selection), 1.0f));

	register_action (editor_actions, "split-at-edit-cursor", _("split at edit cursor"), mem_fun(*this, &Editor::split_region));
	register_action (editor_actions, "split-at-mouse", _("split at mouse"), mem_fun(*this, &Editor::kbd_split));

	register_action (editor_actions, "brush-at-mouse", _("brush at mouse"), mem_fun(*this, &Editor::kbd_brush));
	register_action (editor_actions, "audition-at-mouse", _("audition at mouse"), mem_fun(*this, &Editor::kbd_audition));

	register_action (editor_actions, "start-range", _("start range"), mem_fun(*this, &Editor::keyboard_selection_begin));
	register_action (editor_actions, "finish-range", _("finish range"), bind (mem_fun(*this, &Editor::keyboard_selection_finish), false));
	register_action (editor_actions, "finish-add-range", _("finish add range"), bind (mem_fun(*this, &Editor::keyboard_selection_finish), true));

	register_action (editor_actions, "extend-range-to-end-of-region", _("extend range to end of region"), bind (mem_fun(*this, &Editor::extend_selection_to_end_of_region), false));
	register_action (editor_actions, "extend-range-to-start-of-region", _("extend range to start of region"), bind (mem_fun(*this, &Editor::extend_selection_to_start_of_region), false));

	register_action (editor_actions, "toggle-follow-playhead", _("toggle follow playhead"), (mem_fun(*this, &Editor::toggle_follow_playhead)));
	register_action (editor_actions, "remove-last-capture", _("remove last capture"), (mem_fun(*this, &Editor::remove_last_capture)));

	Glib::RefPtr<ActionGroup> zoom_actions = ActionGroup::create (X_("Zoom"));
	RadioAction::Group zoom_group;

	register_radio_action (zoom_actions, zoom_group, "zoom-focus-left", _("zoom focus left"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusLeft));
	register_radio_action (zoom_actions, zoom_group, "zoom-focus-right", _("zoom focus right"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusRight));
	register_radio_action (zoom_actions, zoom_group, "zoom-focus-center", _("zoom focus center"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusCenter));
	register_radio_action (zoom_actions, zoom_group, "zoom-focus-playhead", _("zoom focus playhead"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusPlayhead));
	register_radio_action (zoom_actions, zoom_group, "zoom-focus-edit", _("zoom focus edit"), bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusEdit));

	Glib::RefPtr<ActionGroup> mouse_mode_actions = ActionGroup::create (X_("MouseMode"));
	RadioAction::Group mouse_mode_group;

	register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-object", _("set mouse mode object"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseObject, false));
	register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-range", _("set mouse mode range"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseRange, false));
	register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-gain", _("set mouse mode gain"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseGain, false));
	register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-zoom", _("set mouse mode zoom"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseZoom, false));
	register_radio_action (mouse_mode_actions, mouse_mode_group, "set-mouse-mode-timefx", _("set mouse mode timefx"), bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseTimeFX, false));

	Glib::RefPtr<ActionGroup> snap_actions = ActionGroup::create (X_("Snap"));
	RadioAction::Group snap_choice_group;

	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-frame"), _("snap to frame"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToFrame)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-cd-frame"), _("snap to cd frame"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToCDFrame)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-smpte-frame"), _("snap to smpte frame"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTEFrame)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-smpte-seconds"), _("snap to smpte seconds"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTESeconds)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-smpte-minutes"), _("snap to smpte minutes"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTEMinutes)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-seconds"), _("snap to seconds"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSeconds)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-minutes"), _("snap to minutes"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToMinutes)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-thirtyseconds"), _("snap to thirtyseconds"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAThirtysecondBeat)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-asixteenthbeat"), _("snap to asixteenthbeat"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToASixteenthBeat)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-eighths"), _("snap to eighths"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAEighthBeat)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-quarters"), _("snap to quarters"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAQuarterBeat)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-thirds"), _("snap to thirds"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAThirdBeat)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-beat"), _("snap to beat"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToBeat)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-bar"), _("snap to bar"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToBar)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-mark"), _("snap to mark"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToMark)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-edit-cursor"), _("snap to edit cursor"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToEditCursor)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-region-start"), _("snap to region start"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionStart)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-region-end"), _("snap to region end"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionEnd)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-region-sync"), _("snap to region sync"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionSync)));
	register_radio_action (snap_actions, snap_choice_group, X_("snap-to-region-boundary"), _("snap to region boundary"), (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionBoundary)));

	/* REGION LIST */

	Glib::RefPtr<ActionGroup> rl_actions = ActionGroup::create (X_("RegionList"));
	RadioAction::Group sort_type_group;
	RadioAction::Group sort_order_group;

	register_action (rl_actions, X_("rlAudition"), _("Audition"), mem_fun(*this, &Editor::audition_region_from_region_list));
	register_action (rl_actions, X_("rlHide"), _("Hide"), mem_fun(*this, &Editor::hide_region_from_region_list));
	register_action (rl_actions, X_("rlRemove"), _("Remove"), mem_fun (*this, &Editor::remove_region_from_region_list));
	register_action (rl_actions, X_("rlShowAll"), _("Show all"), mem_fun(*this, &Editor::toggle_full_region_list));

	register_radio_action (rl_actions, sort_order_group, X_("SortAscending"),  _("Ascending"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_direction), true));
	register_radio_action (rl_actions, sort_order_group, X_("SortDescending"),   _("Descending"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_direction), false));
	
	register_radio_action (rl_actions, sort_type_group, X_("SortByRegionName"),  _("By Region Name"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByName));
	register_radio_action (rl_actions, sort_type_group, X_("SortByRegionLength"),  _("By Region Length"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByLength));
	register_radio_action (rl_actions, sort_type_group, X_("SortByRegionPosition"),  _("By Region Position"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByPosition));
	register_radio_action (rl_actions, sort_type_group, X_("SortByRegionTimestamp"),  _("By Region Timestamp"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByTimestamp));
	register_radio_action (rl_actions, sort_type_group, X_("SortByRegionStartinFile"),  _("By Region Start in File"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByStartInFile));
	register_radio_action (rl_actions, sort_type_group, X_("SortByRegionEndinFile"),  _("By Region End in File"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByEndInFile));
	register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFileName"),  _("By Source File Name"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileName));
	register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFileLength"),  _("By Source File Length"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileLength));
	register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFileCreationDate"),  _("By Source File Creation Date"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileCreationDate));
	register_radio_action (rl_actions, sort_type_group, X_("SortBySourceFilesystem"),  _("By Source Filesystem"),
			       bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileFS));
	
	register_action (rl_actions, X_("rlEmbedAudio"), _("Embed audio (link)"), mem_fun(*this, &Editor::embed_audio));
	register_action (rl_actions, X_("rlImportAudio"), _("Embed audio (link)"), bind (mem_fun(*this, &Editor::import_audio), false));

	/* now add them all */

	ui_manager->insert_action_group (rl_actions);
	ui_manager->insert_action_group (mouse_mode_actions);
	ui_manager->insert_action_group (snap_actions);
	ui_manager->insert_action_group (editor_actions);

	/* and tell the world about us */

	register_ui_manager (ui_manager);
}
