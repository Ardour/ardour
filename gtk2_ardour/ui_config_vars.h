/*
 * Copyright (C) 2012-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2018 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2015-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2016-2017 Tim Mayberry <mojofunk@gmail.com>
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

UI_CONFIG_VARIABLE (std::string, icon_set, "icon-set", "default")
UI_CONFIG_VARIABLE (std::string, ui_rc_file, "ui-rc-file", "clearlooks.rc")
UI_CONFIG_VARIABLE (std::string, ui_font_family, "ui-font-family", "Sans")
UI_CONFIG_VARIABLE (std::string, color_file, "color-file", "dark")
UI_CONFIG_VARIABLE (bool, flat_buttons, "flat-buttons", false)
UI_CONFIG_VARIABLE (bool, boxy_buttons, "boxy-buttons", false)
UI_CONFIG_VARIABLE (bool, blink_rec_arm, "blink-rec-arm", false)
UI_CONFIG_VARIABLE (bool, blink_alert_indicators, "blink-alert-indicators", true)
UI_CONFIG_VARIABLE (float, waveform_gradient_depth, "waveform-gradient-depth", 0)
UI_CONFIG_VARIABLE (float, timeline_item_gradient_depth, "timeline-item-gradient-depth", 0.4)
UI_CONFIG_VARIABLE (bool, all_floating_windows_are_dialogs, "all-floating-windows-are-dialogs", false)
UI_CONFIG_VARIABLE (bool, floating_monitor_section, "floating-monitor-section", false)
UI_CONFIG_VARIABLE (bool, transients_follow_front, "transients-follow-front", false)
UI_CONFIG_VARIABLE (bool, color_regions_using_track_color, "color-regions-using-track-color", false)
UI_CONFIG_VARIABLE (uint32_t, vertical_region_gap, "vertical-region-gap", 0)
UI_CONFIG_VARIABLE (bool, editor_stereo_only_meters, "editor-stereo-only-meters", true)
UI_CONFIG_VARIABLE (bool, show_waveform_clipping, "show-waveform-clipping", true)
UI_CONFIG_VARIABLE (uint32_t, lock_gui_after_seconds, "lock-gui-after-seconds", 0)
UI_CONFIG_VARIABLE (ARDOUR::ScreenSaverMode, screen_saver_mode, "screen-saver-mode", InhibitWhileRecording)
UI_CONFIG_VARIABLE (bool, draggable_playhead, "draggable-playhead", true)
UI_CONFIG_VARIABLE (float, draggable_playhead_speed, "draggable-playhead-speed", 0.5)
UI_CONFIG_VARIABLE (float, extra_ui_extents_time, "extra-ui-extents-time", 1.0)
UI_CONFIG_VARIABLE (bool, new_automation_points_on_lane, "new-automation-points-on-lane", false)
UI_CONFIG_VARIABLE (bool, automation_edit_cancels_auto_hide, "automation-edit-cancels-auto-hide", false)
UI_CONFIG_VARIABLE (std::string, keyboard_layout, "keyboard-layout", "ansi")
UI_CONFIG_VARIABLE (std::string, keyboard_layout_name, "keyboard-layout-name", "ansi")
UI_CONFIG_VARIABLE (std::string, default_bindings, "default-bindings", "ardour")
UI_CONFIG_VARIABLE (std::string, vkeybd_layout, "vkeybd-layout", "QWERTY Single")
UI_CONFIG_VARIABLE (bool, only_copy_imported_files, "only-copy-imported-files", true)
UI_CONFIG_VARIABLE (bool, autoplay_files, "autoplay-files", false)
UI_CONFIG_VARIABLE (bool, autoplay_clips, "autoplay-clips", true)
UI_CONFIG_VARIABLE (bool, default_narrow_ms, "default-narrow_ms", false)
UI_CONFIG_VARIABLE (bool, name_new_markers, "name-new-markers", false)
UI_CONFIG_VARIABLE (bool, rubberbanding_snaps_to_grid, "rubberbanding-snaps-to-grid", false)
UI_CONFIG_VARIABLE (int32_t, font_scale, "font-scale", 102400)
UI_CONFIG_VARIABLE (bool, show_waveforms, "show-waveforms", true)
UI_CONFIG_VARIABLE (bool, show_waveforms_while_recording, "show-waveforms-while-recording", true)
UI_CONFIG_VARIABLE (ARDOUR::WaveformScale, waveform_scale, "waveform-scale", Logarithmic)
UI_CONFIG_VARIABLE (ARDOUR::WaveformShape, waveform_shape, "waveform-shape", Traditional)
UI_CONFIG_VARIABLE (bool, update_editor_during_summary_drag, "update-editor-during-summary-drag", true)
UI_CONFIG_VARIABLE (bool, never_display_periodic_midi, "never-display-periodic-midi", true)
UI_CONFIG_VARIABLE (bool, sound_midi_notes, "sound-midi-notes", false)
UI_CONFIG_VARIABLE (bool, show_plugin_scan_window, "show-plugin-scan-window", false)
UI_CONFIG_VARIABLE (bool, show_zoom_tools, "show-zoom-tools", true)
UI_CONFIG_VARIABLE (bool, use_mouse_position_as_zoom_focus_on_scroll, "use-mouse-position-as-zoom-focus-on-scroll", true)
UI_CONFIG_VARIABLE (bool, use_time_rulers_to_zoom_with_vertical_drag, "use-time-rulers-to-zoom-with-vertical-drag", false)
UI_CONFIG_VARIABLE (bool, preview_video_frame_on_drag, "preview-video-frame-on-drag", true)
UI_CONFIG_VARIABLE (bool, use_double_click_to_zoom_to_selection, "use-double-click-to-zoom-to-selection", false)
UI_CONFIG_VARIABLE (bool, widget_prelight, "widget-prelight", true)
UI_CONFIG_VARIABLE (bool, use_tooltips, "use-tooltips", true)
UI_CONFIG_VARIABLE (std::string, mixer_strip_visibility, "mixer-element-visibility", "Input,PhaseInvert,RecMon,SoloIsoLock,Output,Comments")
UI_CONFIG_VARIABLE (bool, allow_non_quarter_pulse, "allow-non-quarter-pulse", false)
UI_CONFIG_VARIABLE (bool, show_region_gain, "show-region-gain", false)
UI_CONFIG_VARIABLE (bool, show_region_xrun_markers, "show-region-xrun-markers", true)
UI_CONFIG_VARIABLE (bool, show_region_cue_markers, "show-region-cue-markers", true)
UI_CONFIG_VARIABLE (bool, show_name_highlight, "show-name-highlight", false)
UI_CONFIG_VARIABLE (ARDOUR::ClockDeltaMode, primary_clock_delta_mode, "primary-clock-delta-mode", NoDelta)
UI_CONFIG_VARIABLE (ARDOUR::ClockDeltaMode, secondary_clock_delta_mode, "secondary-clock-delta-mode", NoDelta)
UI_CONFIG_VARIABLE (ARDOUR::samplecnt_t, clock_display_limit, "clock-display-limit", 8553600) /* seconds; default 99h, 0 = unlimited */
UI_CONFIG_VARIABLE (bool, show_track_meters, "show-track-meters", true)
UI_CONFIG_VARIABLE (bool, follow_edits, "follow-edits", false)
UI_CONFIG_VARIABLE (bool, super_rapid_clock_update, "super-rapid-clock-update", false)
UI_CONFIG_VARIABLE (bool, autoscroll_editor, "autoscroll-editor", true)
UI_CONFIG_VARIABLE (bool, link_region_and_track_selection, "link-region-and-track-selection", false)  // DEPRECATED
UI_CONFIG_VARIABLE (float, meter_hold, "meter-hold", 100.0f)
UI_CONFIG_VARIABLE (ARDOUR::VUMeterStandard, meter_vu_standard, "meter-vu-standard", ARDOUR::MeteringVUstandard)
UI_CONFIG_VARIABLE (ARDOUR::MeterLineUp, meter_line_up_level, "meter-line-up-level", ARDOUR::MeteringLineUp18)
UI_CONFIG_VARIABLE (ARDOUR::MeterLineUp, meter_line_up_din, "meter-line-up-din", ARDOUR::MeteringLineUp15)
UI_CONFIG_VARIABLE (ARDOUR::InputMeterLayout, input_meter_layout, "input-meter-layout", ARDOUR::LayoutAutomatic)
UI_CONFIG_VARIABLE (bool, input_meter_scopes, "input-meter-scopes", true)
UI_CONFIG_VARIABLE (float, meter_peak, "meter-peak", 0.0f)
UI_CONFIG_VARIABLE (bool, meter_style_led, "meter-style-led", false)
UI_CONFIG_VARIABLE (bool, show_editor_meter, "show-editor-meter", true)
UI_CONFIG_VARIABLE (bool, show_toolbar_recpunch, "show-toolbar-recpunch", true)
UI_CONFIG_VARIABLE (bool, show_toolbar_monitoring, "show-toolbar-monitoring", false) /* deprecated */
UI_CONFIG_VARIABLE (bool, show_toolbar_selclock, "show-toolbar-selclock", false)
UI_CONFIG_VARIABLE (bool, show_toolbar_latency, "show-toolbar-latency", false)
UI_CONFIG_VARIABLE (bool, show_toolbar_monitor_info, "show-toolbar-monitor-info", false)
UI_CONFIG_VARIABLE (bool, show_mini_timeline, "show-mini-timeline", true)
UI_CONFIG_VARIABLE (bool, show_secondary_clock, "show-secondary-clock", true)
UI_CONFIG_VARIABLE (double, waveform_clip_level, "waveform-clip-level", -0.0933967) /* units of dB */
UI_CONFIG_VARIABLE (bool, no_new_session_dialog, "no-new-session-dialog", false)
UI_CONFIG_VARIABLE (bool, buggy_gradients, "buggy-gradients", false)
UI_CONFIG_VARIABLE (bool, cairo_image_surface, "cairo-image-surface", false)
UI_CONFIG_VARIABLE (uint64_t, waveform_cache_size, "waveform-cache-size", 100) /* units of megagbytes */
UI_CONFIG_VARIABLE (int32_t, recent_session_sort, "recent-session-sort", 0)
UI_CONFIG_VARIABLE (bool, save_export_analysis_image, "save-export-analysis-image", false)
UI_CONFIG_VARIABLE (bool, save_export_mixer_screenshot, "save-export-mixer-screenshot", false)
UI_CONFIG_VARIABLE (bool, open_gui_after_adding_plugin, "open-gui-after-adding-plugin", true)
UI_CONFIG_VARIABLE (bool, show_inline_display_by_default, "show-inline-display-by-default", true)
UI_CONFIG_VARIABLE (int32_t, max_plugin_chart, "max-plugin-chart", 10)
UI_CONFIG_VARIABLE (int32_t, max_plugin_recent, "max-plugin-recent", 10)
UI_CONFIG_VARIABLE (bool, prefer_inline_over_gui, "prefer-inline-over-gui", true)
UI_CONFIG_VARIABLE (uint32_t, max_inline_controls, "max-inline-controls", 32) /* per processor */
UI_CONFIG_VARIABLE (uint32_t, action_table_columns, "action-table-columns", 3)
UI_CONFIG_VARIABLE (bool, hide_splash_screen, "hide-splash-screen", false)
UI_CONFIG_VARIABLE (bool, check_announcements, "check-announcements,", true)
UI_CONFIG_VARIABLE (bool, use_wm_visibility, "use-wm-visibility", true)
UI_CONFIG_VARIABLE (std::string, stripable_color_palette, "stripable-color-palette", "#AA3939:#FFAAAA:#D46A6A:#801515:#550000:#AA8E39:#FFEAAA:#D4BA6A:#806515:#554000:#343477:#8080B3:#565695:#1A1A59:#09093B:#2D882D:#88CC88:#55AA55:#116611:#004400")  /* Gtk::ColorSelection::palette_to_string */
UI_CONFIG_VARIABLE (bool, use_note_bars_for_velocity, "use-note-bars-for-velocity", true)
UI_CONFIG_VARIABLE (bool, use_note_color_for_velocity, "use-note-color-for-velocity", true)
UI_CONFIG_VARIABLE (bool, show_snapped_cursor, "show-snapped-cursor", true)
UI_CONFIG_VARIABLE (uint32_t, snap_threshold, "snap-threshold", 25)
UI_CONFIG_VARIABLE (uint32_t, ruler_granularity, "ruler-granularity", 25)
UI_CONFIG_VARIABLE (bool, snap_to_marks, "snap-to-marks", true)
UI_CONFIG_VARIABLE (bool, snap_to_region_sync, "snap-to-region-sync", true)
UI_CONFIG_VARIABLE (bool, snap_to_region_start, "snap-to-region-start", true)
UI_CONFIG_VARIABLE (bool, snap_to_region_end, "snap-to-region-end", true)
UI_CONFIG_VARIABLE (bool, snap_to_grid, "snap-to-grid", true)
UI_CONFIG_VARIABLE (bool, show_grids_ruler, "show-grids-ruler", true)
UI_CONFIG_VARIABLE (bool, rulers_follow_grid, "rulers-follow-grid", false)
UI_CONFIG_VARIABLE (bool, grid_follows_internal, "grid-follows-internal", false)  //this feature is deprecated, default it FALSE for now; remove it in v6
UI_CONFIG_VARIABLE (bool, show_region_name, "show-region-name", true)
UI_CONFIG_VARIABLE (int, time_axis_name_ellipsize_mode, "time-axis-name-ellipsize-mode", 0)
UI_CONFIG_VARIABLE (bool, show_triggers_inline, "show-triggers-inline", false)
UI_CONFIG_VARIABLE (bool, one_plugin_window_only, "one-plugin-window-only", false)
