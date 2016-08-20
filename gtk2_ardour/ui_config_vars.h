/*
    Copyright (C) 2012 Paul Davis

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

UI_CONFIG_VARIABLE (std::string, icon_set, "icon-set", "default")
UI_CONFIG_VARIABLE (std::string, ui_rc_file, "ui-rc-file", "clearlooks.rc")
UI_CONFIG_VARIABLE (std::string, color_file, "color-file", "dark")
UI_CONFIG_VARIABLE (bool, flat_buttons, "flat-buttons", false)
UI_CONFIG_VARIABLE (bool, blink_rec_arm, "blink-rec-arm", false)
UI_CONFIG_VARIABLE (bool, blink_alert_indicators, "blink-alert-indicators", true)
UI_CONFIG_VARIABLE (float, waveform_gradient_depth, "waveform-gradient-depth", 0)
UI_CONFIG_VARIABLE (float, timeline_item_gradient_depth, "timeline-item-gradient-depth", 0.5)
UI_CONFIG_VARIABLE (bool, all_floating_windows_are_dialogs, "all-floating-windows-are-dialogs", false)
UI_CONFIG_VARIABLE (bool, floating_monitor_section, "floating-monitor-section", false)
UI_CONFIG_VARIABLE (bool, transients_follow_front, "transients-follow-front", false)
UI_CONFIG_VARIABLE (bool, color_regions_using_track_color, "color-regions-using-track-color", false)
UI_CONFIG_VARIABLE (bool, show_waveform_clipping, "show-waveform-clipping", true)
UI_CONFIG_VARIABLE (uint32_t, lock_gui_after_seconds, "lock-gui-after-seconds", 0)
UI_CONFIG_VARIABLE (bool, draggable_playhead, "draggable-playhead", true)
UI_CONFIG_VARIABLE (std::string, keyboard_layout, "keyboard-layout", "ansi")
UI_CONFIG_VARIABLE (std::string, keyboard_layout_name, "keyboard-layout-name", "ansi")
UI_CONFIG_VARIABLE (std::string, default_bindings, "default-bindings", "ardour")
UI_CONFIG_VARIABLE (bool, only_copy_imported_files, "only-copy-imported-files", false)
UI_CONFIG_VARIABLE (bool, default_narrow_ms, "default-narrow_ms", false)
UI_CONFIG_VARIABLE (bool, name_new_markers, "name-new-markers", false)
UI_CONFIG_VARIABLE (bool, rubberbanding_snaps_to_grid, "rubberbanding-snaps-to-grid", false)
UI_CONFIG_VARIABLE (int32_t, font_scale, "font-scale", 102400)
UI_CONFIG_VARIABLE (bool, show_waveforms, "show-waveforms", true)
UI_CONFIG_VARIABLE (bool, show_waveforms_while_recording, "show-waveforms-while-recording", true)
UI_CONFIG_VARIABLE (ARDOUR::WaveformScale, waveform_scale, "waveform-scale", Linear)
UI_CONFIG_VARIABLE (ARDOUR::WaveformShape, waveform_shape, "waveform-shape", Traditional)
UI_CONFIG_VARIABLE (bool, update_editor_during_summary_drag, "update-editor-during-summary-drag", true)
UI_CONFIG_VARIABLE (bool, never_display_periodic_midi, "never-display-periodic-midi", true)
UI_CONFIG_VARIABLE (bool, sound_midi_notes, "sound-midi-notes", false)
UI_CONFIG_VARIABLE (bool, show_plugin_scan_window, "show-plugin-scan-window", false)
UI_CONFIG_VARIABLE (bool, show_zoom_tools, "show-zoom-tools", true)
UI_CONFIG_VARIABLE (bool, use_mouse_position_as_zoom_focus_on_scroll, "use-mouse-position-as-zoom-focus-on-scroll", true)
UI_CONFIG_VARIABLE (bool, use_time_rulers_to_zoom_with_vertical_drag, "use-time-rulers-to-zoom-with-vertical-drag", false)
UI_CONFIG_VARIABLE (bool, use_double_click_to_zoom_to_selection, "use-double-click-to-zoom-to-selection", false)
UI_CONFIG_VARIABLE (bool, widget_prelight, "widget-prelight", true)
UI_CONFIG_VARIABLE (bool, use_tooltips, "use-tooltips", true)
UI_CONFIG_VARIABLE (std::string, mixer_strip_visibility, "mixer-element-visibility", "Input,PhaseInvert,RecMon,SoloIsoLock,Output,Comments")
UI_CONFIG_VARIABLE (bool, allow_non_quarter_pulse, "allow-non-quarter-pulse", false)
UI_CONFIG_VARIABLE (bool, show_region_gain, "show-region-gain", false)
UI_CONFIG_VARIABLE (bool, show_name_highlight, "show-name-highlight", false)
UI_CONFIG_VARIABLE (bool, primary_clock_delta_edit_cursor, "primary-clock-delta-edit-cursor", false)
UI_CONFIG_VARIABLE (bool, secondary_clock_delta_edit_cursor, "secondary-clock-delta-edit-cursor", false)
UI_CONFIG_VARIABLE (bool, show_track_meters, "show-track-meters", true)
UI_CONFIG_VARIABLE (bool, editor_stereo_only_meters, "editor-stereo-only-meters", false)
UI_CONFIG_VARIABLE (bool, follow_edits, "follow-edits", false)
UI_CONFIG_VARIABLE (bool, super_rapid_clock_update, "super-rapid-clock-update", false)
UI_CONFIG_VARIABLE (bool, autoscroll_editor, "autoscroll-editor", true)
UI_CONFIG_VARIABLE (bool, link_region_and_track_selection, "link-region-and-track-selection", false)  // DEPRECATED
UI_CONFIG_VARIABLE (float, meter_hold, "meter-hold", 100.0f)
UI_CONFIG_VARIABLE (ARDOUR::VUMeterStandard, meter_vu_standard, "meter-vu-standard", ARDOUR::MeteringVUstandard)
UI_CONFIG_VARIABLE (ARDOUR::MeterLineUp, meter_line_up_level, "meter-line-up-level", ARDOUR::MeteringLineUp18)
UI_CONFIG_VARIABLE (ARDOUR::MeterLineUp, meter_line_up_din, "meter-line-up-din", ARDOUR::MeteringLineUp15)
UI_CONFIG_VARIABLE (ARDOUR::LocaleMode, locale_mode, "locale-mode", ARDOUR::SET_LC_ALL)
UI_CONFIG_VARIABLE (float, meter_peak, "meter-peak", 0.0f)
UI_CONFIG_VARIABLE (bool, meter_style_led, "meter-style-led", false)
UI_CONFIG_VARIABLE (bool, show_editor_meter, "show-editor-meter", true)
UI_CONFIG_VARIABLE (bool, show_toolbar_recpunch, "show-toolbar-recpunch", true)
UI_CONFIG_VARIABLE (bool, show_toolbar_monitoring, "show-toolbar-monitoring", false)
UI_CONFIG_VARIABLE (bool, show_toolbar_selclock, "show-toolbar-selclock", false)
UI_CONFIG_VARIABLE (bool, show_mini_timeline, "show-mini-timeline", true)
UI_CONFIG_VARIABLE (bool, show_secondary_clock, "show-secondary-clock", true)
UI_CONFIG_VARIABLE (double, waveform_clip_level, "waveform-clip-level", -0.0933967) /* units of dB */
UI_CONFIG_VARIABLE (bool, hiding_groups_deactivates_groups, "hiding-groups-deactivates-groups", true)
UI_CONFIG_VARIABLE (bool, no_new_session_dialog, "no-new-session-dialog", false)
UI_CONFIG_VARIABLE (bool, buggy_gradients, "buggy-gradients", false)
UI_CONFIG_VARIABLE (bool, cairo_image_surface, "cairo-image-surface", false)
UI_CONFIG_VARIABLE (uint64_t, waveform_cache_size, "waveform-cache-size", 100) /* units of megagbytes */
UI_CONFIG_VARIABLE (int32_t, recent_session_sort, "recent-session-sort", 0)
UI_CONFIG_VARIABLE (bool, save_export_analysis_image, "save-export-analysis-image", false)
UI_CONFIG_VARIABLE (std::string, xjadeo_binary, "xjadeo-binary", "")
UI_CONFIG_VARIABLE (bool, open_gui_after_adding_plugin, "open-gui-after-adding-plugin", true)
UI_CONFIG_VARIABLE (bool, show_inline_display_by_default, "show-inline-display-by-default", true)
UI_CONFIG_VARIABLE (bool, prefer_inline_over_gui, "prefer-inline-over-gui", true)
UI_CONFIG_VARIABLE (uint32_t, action_table_columns, "action-table-columns", 0)
UI_CONFIG_VARIABLE (bool, use_wm_visibility, "use-wm-visibility", true)
UI_CONFIG_VARIABLE (std::string, stripable_color_palette, "stripable-color-palette", "#AA3939:#FFAAAA:#D46A6A:#801515:#550000:#AA8E39:#FFEAAA:#D4BA6A:#806515:#554000:#343477:#8080B3:#565695:#1A1A59:#09093B:#2D882D:#88CC88:#55AA55:#116611:#004400")  /* Gtk::ColorSelection::palette_to_string */
