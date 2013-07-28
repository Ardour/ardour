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

/*****************************************************
    DO NOT USE uint8_t or any other type that resolves
    to a single char, because the value will be
    stored incorrectly when serialized. Use int32_t
    instead and ensure that code correctly limits
    the value of the variable.
*****************************************************/

/* IO connection */

CONFIG_VARIABLE (bool, auto_connect_standard_busses, "auto-connect-standard-busses", true)
CONFIG_VARIABLE (AutoConnectOption, output_auto_connect, "output-auto-connect", AutoConnectMaster)
CONFIG_VARIABLE (AutoConnectOption, input_auto_connect, "input-auto-connect", AutoConnectPhysical)

/* MIDI and MIDI related */

CONFIG_VARIABLE (bool, trace_midi_input, "trace-midi-input", false)
CONFIG_VARIABLE (bool, trace_midi_output, "trace-midi-output", false)
CONFIG_VARIABLE (bool, send_mtc, "send-mtc", false)
CONFIG_VARIABLE (bool, send_mmc, "send-mmc", true)
CONFIG_VARIABLE (bool, send_midi_clock, "send-midi-clock", false)
CONFIG_VARIABLE (bool, mmc_control, "mmc-control", true)
CONFIG_VARIABLE (bool, midi_feedback, "midi-feedback", false)
CONFIG_VARIABLE (int32_t, mmc_receive_device_id, "mmc-receive-device-id", 0x7f)
CONFIG_VARIABLE (int32_t, mmc_send_device_id, "mmc-send-device-id", 0)
CONFIG_VARIABLE (int32_t, initial_program_change, "initial-program-change", -1)
CONFIG_VARIABLE (bool, first_midi_bank_is_zero, "diplay-first-midi-bank-as-zero", false)

/* Timecode and related */

CONFIG_VARIABLE (int, mtc_qf_speed_tolerance, "mtc-qf-speed-tolerance", 5)
CONFIG_VARIABLE (bool, timecode_sync_frame_rate, "timecode-sync-frame-rate", true)
CONFIG_VARIABLE (bool, timecode_source_is_synced, "timecode-source-is-synced", true)
CONFIG_VARIABLE (bool, timecode_source_2997, "timecode-source-2997", false)
CONFIG_VARIABLE (SyncSource, sync_source, "sync-source", JACK)
CONFIG_VARIABLE (std::string, ltc_source_port, "ltc-source-port", "system:capture_1")
CONFIG_VARIABLE (bool, send_ltc, "send-ltc", false)
CONFIG_VARIABLE (bool, ltc_send_continuously, "ltc-send-continuously", true)
CONFIG_VARIABLE (std::string, ltc_output_port, "ltc-output-port", "")
CONFIG_VARIABLE (float, ltc_output_volume, "ltc-output-volume", 0.125893)

/* control surfaces */

CONFIG_VARIABLE (uint32_t, feedback_interval_ms,  "feedback-interval-ms", 100)
CONFIG_VARIABLE (bool, use_tranzport,  "use-tranzport", false)
CONFIG_VARIABLE (RemoteModel, remote_model, "remote-model", MixerOrdered)

/* disk operations */

CONFIG_VARIABLE (uint32_t, minimum_disk_io_bytes,  "minimum-disk-io-bytes", 1024 * 256)
CONFIG_VARIABLE (float, midi_readahead,  "midi-readahead", 1.0)
CONFIG_VARIABLE (float, audio_capture_buffer_seconds, "capture-buffer-seconds", 5.0)
CONFIG_VARIABLE (float, audio_playback_buffer_seconds, "playback-buffer-seconds", 5.0)
CONFIG_VARIABLE (float, midi_track_buffer_seconds, "midi-track-buffer-seconds", 1.0)
CONFIG_VARIABLE (uint32_t, disk_choice_space_threshold,  "disk-choice-space-threshold", 57600000)
CONFIG_VARIABLE (bool, auto_analyse_audio, "auto-analyse-audio", false)

/* OSC */

CONFIG_VARIABLE (uint32_t, osc_port, "osc-port", 3819)
CONFIG_VARIABLE (bool, use_osc, "use-osc", false)

/* editing related */

CONFIG_VARIABLE (EditMode, edit_mode, "edit-mode", Slide)
CONFIG_VARIABLE (bool, link_region_and_track_selection, "link-region-and-track-selection", false)
CONFIG_VARIABLE (bool, link_editor_and_mixer_selection, "link-editor-and-mixer-selection", false)
CONFIG_VARIABLE (std::string, keyboard_layout_name, "keyboard-layout-name", "ansi")
CONFIG_VARIABLE (bool, automation_follows_regions, "automation-follows-regions", true)
CONFIG_VARIABLE (bool, region_boundaries_from_selected_tracks, "region-boundaries-from-selected-tracks", true)
CONFIG_VARIABLE (bool, region_boundaries_from_onscreen_tracks, "region-boundaries-from-onscreen_tracks", true)
CONFIG_VARIABLE (bool, autoscroll_editor, "autoscroll-editor", true)

/* monitoring, mute, solo etc */

CONFIG_VARIABLE (bool, mute_affects_pre_fader, "mute-affects-pre-fader", true)
CONFIG_VARIABLE (bool, mute_affects_post_fader, "mute-affects-post-fader", true)
CONFIG_VARIABLE (bool, mute_affects_control_outs, "mute-affects-control-outs", true)
CONFIG_VARIABLE (bool, mute_affects_main_outs, "mute-affects-main-outs", true)
CONFIG_VARIABLE (MonitorModel, monitoring_model, "monitoring-model", ExternalMonitoring)
CONFIG_VARIABLE (ListenPosition, listen_position, "listen-position", AfterFaderListen)
CONFIG_VARIABLE (PFLPosition, pfl_position, "pfl-position", PFLFromAfterProcessors)
CONFIG_VARIABLE (AFLPosition, afl_position, "afl-position", AFLFromAfterProcessors)
CONFIG_VARIABLE (bool, use_monitor_bus, "use-monitor-bus", false)

CONFIG_VARIABLE (bool, solo_control_is_listen_control, "solo-control-is-listen-control", false)
CONFIG_VARIABLE (bool, exclusive_solo, "exclusive-solo", false)
CONFIG_VARIABLE (bool, latched_record_enable, "latched-record-enable", false)
CONFIG_VARIABLE (bool, all_safe, "all-safe", false)
CONFIG_VARIABLE (bool, show_solo_mutes, "show-solo-mutes", true)
CONFIG_VARIABLE (bool, solo_mute_override, "solo-mute-override", false)
CONFIG_VARIABLE (bool, tape_machine_mode, "tape-machine-mode", false)
CONFIG_VARIABLE (gain_t, solo_mute_gain, "solo-mute-gain", 0.0)
CONFIG_VARIABLE (std::string, monitor_bus_preferred_bundle, "monitor-bus-preferred-bundle", "")
CONFIG_VARIABLE (bool, quieten_at_speed, "quieten-at-speed", true)

/* click */

CONFIG_VARIABLE (bool, clicking, "clicking", false)
CONFIG_VARIABLE (std::string, click_sound, "click-sound", "")
CONFIG_VARIABLE (std::string, click_emphasis_sound, "click-emphasis-sound", "")
CONFIG_VARIABLE (gain_t, click_gain, "click-gain", 1.0)

/* transport control and related */

/** if true, we call Processor::flush() on all processors when the transport is stopped.
 *  Note that processors are still run when the transport is not moving.
 */
CONFIG_VARIABLE (bool, plugins_stop_with_transport, "plugins-stop-with-transport", false)
CONFIG_VARIABLE (bool, stop_recording_on_xrun, "stop-recording-on-xrun", false)
CONFIG_VARIABLE (bool, create_xrun_marker, "create-xrun-marker", true)
CONFIG_VARIABLE (bool, stop_at_session_end, "stop-at-session-end", false)
CONFIG_VARIABLE (bool, seamless_loop, "seamless-loop", false)
CONFIG_VARIABLE (framecnt_t, preroll, "preroll", 0)
CONFIG_VARIABLE (framecnt_t, postroll, "postroll", 0)
CONFIG_VARIABLE (float, rf_speed, "rf-speed", 2.0f)
CONFIG_VARIABLE (float, shuttle_speed_factor, "shuttle-speed-factor", 1.0f)
CONFIG_VARIABLE (float, shuttle_speed_threshold, "shuttle-speed-threshold", 5.0f)
CONFIG_VARIABLE (ShuttleBehaviour, shuttle_behaviour, "shuttle-behaviour", Sprung)
CONFIG_VARIABLE (ShuttleUnits, shuttle_units, "shuttle-units", Percentage)
CONFIG_VARIABLE (bool, primary_clock_delta_edit_cursor, "primary-clock-delta-edit-cursor", false)
CONFIG_VARIABLE (bool, secondary_clock_delta_edit_cursor, "secondary-clock-delta-edit-cursor", false)
CONFIG_VARIABLE (bool, show_track_meters, "show-track-meters", true)
CONFIG_VARIABLE (bool, locate_while_waiting_for_sync, "locate-while-waiting-for-sync", false)
CONFIG_VARIABLE (bool, disable_disarm_during_roll, "disable-disarm-during-roll", false)
CONFIG_VARIABLE (bool, always_play_range, "always-play-range", false)
CONFIG_VARIABLE (bool, super_rapid_clock_update, "super-rapid-clock-update", false)

/* metering */

CONFIG_VARIABLE (float, meter_hold, "meter-hold", 100.0f)
CONFIG_VARIABLE (float, meter_falloff, "meter-falloff", 32.0f)
CONFIG_VARIABLE (VUMeterStandard, meter_vu_standard, "meter-vu-standard", MeteringVUstandard)
CONFIG_VARIABLE (MeterLineUp, meter_line_up_level, "meter-line-up-level", MeteringLineUp18)
CONFIG_VARIABLE (MeterLineUp, meter_line_up_din, "meter-line-up-din", MeteringLineUp15)
CONFIG_VARIABLE (float, meter_peak, "meter-peak", 0.0f)
CONFIG_VARIABLE (bool, meter_style_led, "meter-style-led", true)

/* miscellany */

CONFIG_VARIABLE (std::string, auditioner_output_left, "auditioner-output-left", "default")
CONFIG_VARIABLE (std::string, auditioner_output_right, "auditioner-output-right", "default")
CONFIG_VARIABLE (bool, replicate_missing_region_channels, "replicate-missing-region-channels", false)
CONFIG_VARIABLE (bool, hiding_groups_deactivates_groups, "hiding-groups-deactivates-groups", true)
CONFIG_VARIABLE (bool, verify_remove_last_capture, "verify-remove-last-capture", true)
CONFIG_VARIABLE (bool, no_new_session_dialog, "no-new-session-dialog", false)
CONFIG_VARIABLE (bool, use_windows_vst, "use-windows-vst", true)
CONFIG_VARIABLE (bool, use_lxvst, "use-lxvst", true)
CONFIG_VARIABLE (bool, save_history, "save-history", true)
CONFIG_VARIABLE (int32_t, saved_history_depth, "save-history-depth", 20)
CONFIG_VARIABLE (int32_t, history_depth, "history-depth", 20)
CONFIG_VARIABLE (bool, use_overlap_equivalency, "use-overlap-equivalency", false)
CONFIG_VARIABLE (bool, periodic_safety_backups, "periodic-safety-backups", true)
CONFIG_VARIABLE (uint32_t, periodic_safety_backup_interval, "periodic-safety-backup-interval", 120)
CONFIG_VARIABLE (float, automation_interval_msecs, "automation-interval-msecs", 30)
CONFIG_VARIABLE (bool, sync_all_route_ordering, "sync-all-route-ordering", true)
CONFIG_VARIABLE (bool, only_copy_imported_files, "only-copy-imported-files", false)
CONFIG_VARIABLE (bool, keep_tearoffs, "keep-tearoffs", false)
CONFIG_VARIABLE (bool, new_plugins_active, "new-plugins-active", true)
CONFIG_VARIABLE (std::string, keyboard_layout, "keyboard-layout", "ansi")
CONFIG_VARIABLE (std::string, default_bindings, "default-bindings", "ardour")
CONFIG_VARIABLE (bool, default_narrow_ms, "default-narrow_ms", false)
CONFIG_VARIABLE (bool, name_new_markers, "name-new-markers", false)
CONFIG_VARIABLE (bool, rubberbanding_snaps_to_grid, "rubberbanding-snaps-to-grid", false)
CONFIG_VARIABLE (long, font_scale, "font-scale", 81920)
CONFIG_VARIABLE (std::string, default_session_parent_dir, "default-session-parent-dir", "~")
CONFIG_VARIABLE (bool, show_waveforms, "show-waveforms", true)
CONFIG_VARIABLE (bool, show_waveforms_while_recording, "show-waveforms-while-recording", true)
CONFIG_VARIABLE (WaveformScale, waveform_scale, "waveform-scale", Linear)
CONFIG_VARIABLE (WaveformShape, waveform_shape, "waveform-shape", Traditional)
CONFIG_VARIABLE (bool, allow_special_bus_removal, "allow-special-bus-removal", false)
CONFIG_VARIABLE (int32_t, processor_usage, "processor-usage", -1)
CONFIG_VARIABLE (gain_t, max_gain, "max-gain", 2.0) /* +6.0dB */
CONFIG_VARIABLE (bool, update_editor_during_summary_drag, "update-editor-during-summary-drag", true)
CONFIG_VARIABLE (bool, never_display_periodic_midi, "never-display-periodic-midi", true)
CONFIG_VARIABLE (bool, sound_midi_notes, "sound-midi-notes", false)
CONFIG_VARIABLE (bool, use_plugin_own_gui, "use-plugin-own-gui", true)
CONFIG_VARIABLE (uint32_t, max_recent_sessions, "max-recent-sessions", 10)
CONFIG_VARIABLE (double, automation_thinning_factor, "automation-thinning-factor", 20.0)
CONFIG_VARIABLE (std::string, freesound_download_dir, "freesound-download-dir", Glib::get_home_dir() + "/Freesound/snd")

/* denormal management */

CONFIG_VARIABLE (bool, denormal_protection, "denormal-protection", false)
CONFIG_VARIABLE (DenormalModel, denormal_model, "denormal-model", DenormalFTZDAZ)

/* visibility of various things */

CONFIG_VARIABLE (bool, show_zoom_tools, "show-zoom-tools", true)
CONFIG_VARIABLE (bool, widget_prelight, "widget-prelight", true)
CONFIG_VARIABLE (bool, use_tooltips, "use-tooltips", true)
CONFIG_VARIABLE (std::string, mixer_strip_visibility, "mixer-strip-visibility", "PhaseInvert,SoloSafe,SoloIsolated,Group,MeterPoint")
CONFIG_VARIABLE (bool, allow_non_quarter_pulse, "allow-non-quarter-pulse", false)
CONFIG_VARIABLE (bool, show_region_gain, "show-region-gain", false)

/* web addresses used in the program */

CONFIG_VARIABLE (std::string, osx_pingback_url, "osx-pingback-url", "http://community.ardour.org/pingback/osx/")
CONFIG_VARIABLE (std::string, linux_pingback_url, "linux-pingback-url", "http://community.ardour.org/pingback/linux/")
CONFIG_VARIABLE (std::string, tutorial_manual_url, "tutorial-manual-url", "http://ardour.org/flossmanual")
CONFIG_VARIABLE (std::string, reference_manual_url, "reference-manual-url", "http://manual.ardour.org/")
CONFIG_VARIABLE (std::string, updates_url, "updates-url", "http://ardour.org/whatsnew.html")
CONFIG_VARIABLE (std::string, donate_url, "donate-url", "http://ardour.org/donate")

/* video timeline configuration */
CONFIG_VARIABLE (bool, video_advanced_setup, "video-advanced-setup", false)
CONFIG_VARIABLE (std::string, video_server_url, "video-server-url", "http://localhost:1554")
CONFIG_VARIABLE (std::string, video_server_docroot, "video-server-docroot", "/")
CONFIG_VARIABLE (bool, show_video_export_info, "show-video-export-info", true)
CONFIG_VARIABLE (bool, show_video_server_dialog, "show-video-server-dialog", false)
