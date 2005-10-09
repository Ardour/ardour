void
Editor::register_actions ()
{
	/* add named actions for the editor */

	Glib::RefPtr<ActionGroup> region_list_actions = ActionGroup::create ("Editor");

	add_action ("toggle-xfades-active", mem_fun(*this, &Editor::toggle_xfades_active));

	add_action ("playhead-to-next-region-start", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (Start)));
	add_action ("playhead-to-next-region-end", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (End)));
	add_action ("playhead-to-next-region-sync", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), playhead_cursor, RegionPoint (SyncPoint)));

	add_action ("playhead-to-previous-region-start", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (Start)));
	add_action ("playhead-to-previous-region-end", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (End)));
	add_action ("playhead-to-previous-region-sync", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), playhead_cursor, RegionPoint (SyncPoint)));

	add_action ("edit-cursor-to-next-region-start", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (Start)));
	add_action ("edit-cursor-to-next-region-end", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (End)));
	add_action ("edit-cursor-to-next-region-sync", bind (mem_fun(*this, &Editor::cursor_to_next_region_point), edit_cursor, RegionPoint (SyncPoint)));

	add_action ("edit-cursor-to-previous-region-start", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (Start)));
	add_action ("edit-cursor-to-previous-region-end", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (End)));
	add_action ("edit-cursor-to-previous-region-sync", bind (mem_fun(*this, &Editor::cursor_to_previous_region_point), edit_cursor, RegionPoint (SyncPoint)));

	add_action ("playhead-to-range-start", bind (mem_fun(*this, &Editor::cursor_to_selection_start), playhead_cursor));
	add_action ("playhead-to-range-end", bind (mem_fun(*this, &Editor::cursor_to_selection_end), playhead_cursor));

	add_action ("edit-cursor-to-range-start", bind (mem_fun(*this, &Editor::cursor_to_selection_start), edit_cursor));
	add_action ("edit-cursor-to-range-end", bind (mem_fun(*this, &Editor::cursor_to_selection_end), edit_cursor));

	add_action ("jump-forward-to-mark", mem_fun(*this, &Editor::jump_forward_to_mark));
	add_action ("jump-backward-to-mark", mem_fun(*this, &Editor::jump_backward_to_mark));
	add_action ("add-location-from-playhead", mem_fun(*this, &Editor::add_location_from_playhead_cursor));

	add_action ("nudge-forward", bind (mem_fun(*this, &Editor::nudge_forward), false));
	add_action ("nudge-next-forward", bind (mem_fun(*this, &Editor::nudge_forward), true));
	add_action ("nudge-backward", bind (mem_fun(*this, &Editor::nudge_backward), false));
	add_action ("nudge-next-backward", bind (mem_fun(*this, &Editor::nudge_backward), true));

	add_action ("toggle-playback", bind (mem_fun(*this, &Editor::toggle_playback), false));
	add_action ("toggle-playback-forget-capture", bind (mem_fun(*this, &Editor::toggle_playback), true));

	add_action ("toggle-loop-playback", mem_fun(*this, &Editor::toggle_loop_playback));
	
	add_action ("temporal-zoom-out", bind (mem_fun(*this, &Editor::temporal_zoom_step), true));
	add_action ("temporal-zoom-in", bind (mem_fun(*this, &Editor::temporal_zoom_step), false));
	add_action ("zoom-to-session", mem_fun(*this, &Editor::temporal_zoom_session));

	add_action ("scroll-tracks-up", mem_fun(*this, &Editor::scroll_tracks_up));
	add_action ("scroll-tracks-down", mem_fun(*this, &Editor::scroll_tracks_down));
	add_action ("step-tracks-up", mem_fun(*this, &Editor::scroll_tracks_up_line));
	add_action ("step-tracks-down", mem_fun(*this, &Editor::scroll_tracks_down_line));

	add_action ("scroll-backward", bind (mem_fun(*this, &Editor::scroll_backward), 0.8f));
	add_action ("scroll-forward", bind (mem_fun(*this, &Editor::scroll_forward), 0.8f));
	add_action ("goto", mem_fun(*this, &Editor::goto_frame));
	add_action ("center-playhead", mem_fun(*this, &Editor::center_playhead));
	add_action ("center-edit_cursor", mem_fun(*this, &Editor::center_edit_cursor));
	add_action ("playhead-forward", mem_fun(*this, &Editor::playhead_forward));
	add_action ("playhead-backward", mem_fun(*this, &Editor::playhead_backward));
	add_action ("playhead-to-edit", bind (mem_fun(*this, &Editor::cursor_align), true));
	add_action ("edit-to-playhead", bind (mem_fun(*this, &Editor::cursor_align), false));

	add_action ("align-regions-start", bind (mem_fun(*this, &Editor::align), ARDOUR::Start));
	add_action ("align-regions-start-relative", bind (mem_fun(*this, &Editor::align_relative), ARDOUR::Start));
	add_action ("align-regions-end", bind (mem_fun(*this, &Editor::align), ARDOUR::End));
	add_action ("align-regions-end-relative", bind (mem_fun(*this, &Editor::align_relative), ARDOUR::End));
	add_action ("align-regions-sync", bind (mem_fun(*this, &Editor::align), ARDOUR::SyncPoint));
	add_action ("align-regions-sync-relative", bind (mem_fun(*this, &Editor::align_relative), ARDOUR::SyncPoint));
	
	add_action ("set-playhead", mem_fun(*this, &Editor::kbd_set_playhead_cursor));
	add_action ("set-edit-cursor", mem_fun(*this, &Editor::kbd_set_edit_cursor));

	add_action ("set-mouse-mode-object", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseObject, false));
	add_action ("set-mouse-mode-range", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseRange, false));
	add_action ("set-mouse-mode-gain", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseGain, false));
	add_action ("set-mouse-mode-zoom", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseZoom, false));
	add_action ("set-mouse-mode-timefx", bind (mem_fun(*this, &Editor::set_mouse_mode), Editing::MouseTimeFX, false));

	add_action ("set-undo", bind (mem_fun(*this, &Editor::undo), 1U));
	add_action ("set-redo", bind (mem_fun(*this, &Editor::redo), 1U));

	add_action ("export-session", mem_fun(*this, &Editor::export_session));
	add_action ("export-range", mem_fun(*this, &Editor::export_selection));

	add_action ("editor-cut", mem_fun(*this, &Editor::cut));
	add_action ("editor-copy", mem_fun(*this, &Editor::copy));
	add_action ("editor-paste", mem_fun(*this, &Editor::keyboard_paste));
	add_action ("duplicate-region", mem_fun(*this, &Editor::keyboard_duplicate_region));
	add_action ("duplicate-range", mem_fun(*this, &Editor::keyboard_duplicate_selection));
	add_action ("insert-region", mem_fun(*this, &Editor::keyboard_insert_region_list_selection));
	add_action ("reverse-region", mem_fun(*this, &Editor::reverse_region));
	add_action ("normalize-region", mem_fun(*this, &Editor::normalize_region));
	add_action ("editor-crop", mem_fun(*this, &Editor::crop_region_to_selection));
	add_action ("insert-chunk", bind (mem_fun(*this, &Editor::paste_named_selection), 1.0f));

	add_action ("split-at-edit-cursor", mem_fun(*this, &Editor::split_region));
	add_action ("split-at-mouse", mem_fun(*this, &Editor::kbd_split));

	add_action ("brush-at-mouse", mem_fun(*this, &Editor::kbd_brush));
	add_action ("audition-at-mouse", mem_fun(*this, &Editor::kbd_audition));

	add_action ("start-range", mem_fun(*this, &Editor::keyboard_selection_begin));
	add_action ("finish-range", bind (mem_fun(*this, &Editor::keyboard_selection_finish), false));
	add_action ("finish-add-range", bind (mem_fun(*this, &Editor::keyboard_selection_finish), true));

	add_action ("extend-range-to-end-of-region", bind (mem_fun(*this, &Editor::extend_selection_to_end_of_region), false));
	add_action ("extend-range-to-start-of-region", bind (mem_fun(*this, &Editor::extend_selection_to_start_of_region), false));

	add_action ("toggle-follow-playhead", (mem_fun(*this, &Editor::toggle_follow_playhead)));
	add_action ("remove-last-capture", (mem_fun(*this, &Editor::remove_last_capture)));

	Glib::RefPtr<ActionGroup> region_list_actions = ActionGroup::create ("Zoom");
	RadioAction::Group snap_choice_group;

	add_action ("zoom-focus-left", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusLeft));
	add_action ("zoom-focus-right", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusRight));
	add_action ("zoom-focus-center", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusCenter));
	add_action ("zoom-focus-playhead", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusPlayhead));
	add_action ("zoom-focus-edit", bind (mem_fun(*this, &Editor::set_zoom_focus), Editing::ZoomFocusEdit));

	Glib::RefPtr<ActionGroup> snap_actions = ActionGroup::create ("Snap");
	RadioAction::Group snap_choice_group;

	add_action ("snap-to-frame", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToFrame)));
	add_action ("snap-to-cd-frame", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToCDFrame)));
	add_action ("snap-to-smpte-frame", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTEFrame)));
	add_action ("snap-to-smpte-seconds", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTESeconds)));
	add_action ("snap-to-smpte-minutes", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSMPTEMinutes)));
	add_action ("snap-to-seconds", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToSeconds)));
	add_action ("snap-to-minutes", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToMinutes)));
	add_action ("snap-to-thirtyseconds", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAThirtysecondBeat)));
	add_action ("snap-to-asixteenthbeat", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToASixteenthBeat)));
	add_action ("snap-to-eighths", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAEighthBeat)));
	add_action ("snap-to-quarters", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAQuarterBeat)));
	add_action ("snap-to-thirds", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToAThirdBeat)));
	add_action ("snap-to-beat", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToBeat)));
	add_action ("snap-to-bar", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToBar)));
	add_action ("snap-to-mark", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToMark)));
	add_action ("snap-to-edit-cursor", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToEditCursor)));
	add_action ("snap-to-region-start", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionStart)));
	add_action ("snap-to-region-end", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionEnd)));
	add_action ("snap-to-region-sync", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionSync)));
	add_action ("snap-to-region-boundary", (bind (mem_fun(*this, &Editor::set_snap_to), Editing::SnapToRegionBoundary)));

	/* REGION LIST */

	Glib::RefPtr<ActionGroup> region_list_actions = ActionGroup::create ("RegionList");
	RadioAction::Group sort_order_group;
	RadioAction::Group sort_order_group;

	region_list_actions->add (Action::create (X_("rlAudition"), _("Audition")), mem_fun(*this, &Editor::audition_region_from_region_list));
	region_list_actions->add (Action::create (X_("rlHide"), _("Hide")), mem_fun(*this, &Editor::hide_region_from_region_list));
	region_list_actions->add (Action::create (X_("rlRemove"), _("Remove")), mem_fun(*this, &Editor::remove_region_from_region_list));

	region_list_actions->add (ToggleAction::create (X_("rlShowAll"), _("Show all")), mem_fun(*this, &Editor::toggle_full_region_list));

	region_list_actions->add (RadioAction::create (sort_order_group, X_("SortAscending"),  _("Ascending")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_direction), true));
	region_list_actions->add (RadioAction::create (sort_order_group, X_("SortDescending"),   _("Descending"),),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_direction), false));

	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortByRegionName"),  _("By Region Name")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByName));
	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortByRegionLength"),  _("By Region Length")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByLength));
	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortByRegionPosition"),  _("By Region Position")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByPosition));
	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortByRegionTimestamp"),  _("By Region Timestamp")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByTimestamp));
	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortByRegionStartinFile"),  _("By Region Start in File")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByStartInFile));
	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortByRegionEndinFile"),  _("By Region End in File")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), ByEndInFile));
	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortBySourceFileName"),  _("By Source File Name")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileName));
	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortBySourceFileLength"),  _("By Source File Length")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileLength));
	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortBySourceFileCreationDate"),  _("By Source File Creation Date")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileCreationDate));
	region_list_actions->add (RadioAction::create (sort_type_group, X_("SortBySourceFilesystem"),  _("By Source Filesystem")),
				  bind (mem_fun(*this, &Editor::reset_region_list_sort_type), BySourceFileFS));

	region_list_actions->add (Action::create (X_("rlEmbedAudio"), _("Embed audio (link)")), mem_fun(*this, &Editor::embed_audio));
	region_list_actions->add (Action::create (X_("rlImportAudio"), _("Embed audio (link)")), bind (mem_fun(*this, &Editor::import_audio), false));

	/* now add them all */

	ui_manager->insert_action_group (region_list_actions);
	ui_manager->insert_action_group (snap_actions);
	ui_manager->insert_action_group (editor_actions);
}
