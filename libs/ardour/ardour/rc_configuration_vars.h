/*
 * Copyright (C) 2000-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2012-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2013-2015 Nick Mainsbridge <mainsbridge@gmail.com>
 * Copyright (C) 2013-2018 Colin Fletcher <colin.m.fletcher@googlemail.com>
 * Copyright (C) 2014-2019 Ben Loftis <ben@harrisonconsoles.com>
 * Copyright (C) 2018 Len Ovens <len@ovenwerks.net>
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

/*****************************************************
    DO NOT USE uint8_t or any other type that resolves
    to a single char, because the value will be
    stored incorrectly when serialized. Use int32_t
    instead and ensure that code correctly limits
    the value of the variable.
*****************************************************/

/* IO connection */

CONFIG_VARIABLE (bool, auto_connect_standard_busses, "auto-connect-standard-busses", true)
/* this variable is used to indicate output mode in Waves Tracks:
   "Multi Out" == AutoConnectPhysical and "Stereo Out" == AutoConnectMaster
*/
CONFIG_VARIABLE (AutoConnectOption, output_auto_connect, "output-auto-connect", AutoConnectMaster)
CONFIG_VARIABLE (AutoConnectOption, input_auto_connect, "input-auto-connect", AutoConnectPhysical)
CONFIG_VARIABLE (bool, strict_io, "strict-io", true)

/* Connect all physical inputs to a dummy port, this makes raw input data available.
 * `jack_port_get_buffer (jack_port_by_name (c, "system:capture_1") , n_samples);`
 * nees to work for input-monitoring (recorder page).
 */
CONFIG_VARIABLE (bool, work_around_jack_no_copy_optimization, "work-around-jack-no-copy-optimization", true)

/* Naming */
CONFIG_VARIABLE (TracksAutoNamingRule, tracks_auto_naming, "tracks-auto-naming", UseDefaultNames)

/* Transport Masters (all) */

CONFIG_VARIABLE (bool, transport_masters_just_roll_when_sync_lost, "transport-masters-just-roll-when-sync-lost", false)
CONFIG_VARIABLE (bool, midi_clock_sets_tempo, "midi-clock-sets-tempo", true)

/* MIDI and MIDI related */

CONFIG_VARIABLE (bool, trace_midi_input, "trace-midi-input", false)
CONFIG_VARIABLE (bool, trace_midi_output, "trace-midi-output", false)
CONFIG_VARIABLE (bool, send_mtc, "send-mtc", false)
CONFIG_VARIABLE (bool, send_mmc, "send-mmc", false)
CONFIG_VARIABLE (bool, send_midi_clock, "send-midi-clock", false)
CONFIG_VARIABLE (bool, mmc_control, "mmc-control", true)
CONFIG_VARIABLE (bool, midi_feedback, "midi-feedback", false)
CONFIG_VARIABLE (int32_t, mmc_receive_device_id, "mmc-receive-device-id", 0x7f)
CONFIG_VARIABLE (int32_t, mmc_send_device_id, "mmc-send-device-id", 0)
CONFIG_VARIABLE (int32_t, initial_program_change, "initial-program-change", -1)
CONFIG_VARIABLE (bool, first_midi_bank_is_zero, "display-first-midi-bank-as-zero", false)
CONFIG_VARIABLE (int32_t, inter_scene_gap_samples, "inter-scene-gap-samples", 1)
CONFIG_VARIABLE (bool, midi_input_follows_selection, "midi-input-follows-selection", 1)
CONFIG_VARIABLE (std::string, default_trigger_input_port, "default-trigger-input-port", "")

/* Timecode and related */

CONFIG_VARIABLE (bool, run_all_transport_masters_always, "run-all-transport-masters-always", true)
CONFIG_VARIABLE (int, mtc_qf_speed_tolerance, "mtc-qf-speed-tolerance", 5)
CONFIG_VARIABLE (bool, timecode_sync_frame_rate, "timecode-sync-frame-rate", true)
CONFIG_VARIABLE (bool, send_ltc, "send-ltc", false)
CONFIG_VARIABLE (bool, ltc_send_continuously, "ltc-send-continuously", true)
CONFIG_VARIABLE (std::string, ltc_output_port, "ltc-output-port", "")
CONFIG_VARIABLE (float, ltc_output_volume, "ltc-output-volume", 0.125893)

/* control surfaces */

CONFIG_VARIABLE (uint32_t, feedback_interval_ms,  "feedback-interval-ms", 100)
CONFIG_VARIABLE (bool, use_tranzport,  "use-tranzport", false)

/* disk operations */

CONFIG_VARIABLE (uint32_t, minimum_disk_read_bytes, "minimum-disk-read-bytes", ARDOUR::DiskReader::default_chunk_samples() * sizeof (ARDOUR::Sample))
CONFIG_VARIABLE (uint32_t, minimum_disk_write_bytes, "minimum-disk-write-bytes", ARDOUR::DiskWriter::default_chunk_samples() * sizeof (ARDOUR::Sample))
CONFIG_VARIABLE (BufferingPreset, buffering_preset, "buffering-preset", Medium)
CONFIG_VARIABLE (float, audio_capture_buffer_seconds, "capture-buffer-seconds", 5.0)
CONFIG_VARIABLE (float, audio_playback_buffer_seconds, "playback-buffer-seconds", 5.0)
CONFIG_VARIABLE (float, midi_track_buffer_seconds, "midi-track-buffer-seconds", 1.0)
CONFIG_VARIABLE (uint32_t, disk_choice_space_threshold,  "disk-choice-space-threshold", 57600000)
CONFIG_VARIABLE (bool, auto_analyse_audio, "auto-analyse-audio", false)
CONFIG_VARIABLE (float, transient_sensitivity, "transient-sensitivity", 50)
CONFIG_VARIABLE (float, max_transport_speed, "max-transport-speed", 2.0)

/* OSC */

CONFIG_VARIABLE (uint32_t, osc_port, "osc-port", 3819)
CONFIG_VARIABLE (bool, use_osc, "use-osc", false)

/* editing related */

CONFIG_VARIABLE (LayerModel, layer_model, "layer-model", Manual)
CONFIG_VARIABLE (bool, automation_follows_regions, "automation-follows-regions", true)
CONFIG_VARIABLE (bool, region_boundaries_from_selected_tracks, "region-boundaries-from-selected-tracks", true)
CONFIG_VARIABLE (bool, region_boundaries_from_onscreen_tracks, "region-boundaries-from-onscreen_tracks", true)
CONFIG_VARIABLE (FadeShape, default_fade_shape, "default-fade-shape", FadeConstantPower)
CONFIG_VARIABLE (RangeSelectionAfterSplit, range_selection_after_split, "range-selection-after-split", PreserveSel)
CONFIG_VARIABLE (RegionSelectionAfterSplit, region_selection_after_split, "region-selection-after-split", None)
CONFIG_VARIABLE (bool, interview_editing, "interview-editing", false)

/* monitoring, mute, solo etc */

CONFIG_VARIABLE (bool, mute_affects_pre_fader, "mute-affects-pre-fader", false)
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
CONFIG_VARIABLE (bool, auto_input_does_talkback, "auto-input-does-talkback", false)
CONFIG_VARIABLE (bool, use_master_volume, "use-master-volume", false)
CONFIG_VARIABLE (gain_t, solo_mute_gain, "solo-mute-gain", 0.0)
CONFIG_VARIABLE (std::string, monitor_bus_preferred_bundle, "monitor-bus-preferred-bundle", "")
CONFIG_VARIABLE (bool, quieten_at_speed, "quieten-at-speed", true)

CONFIG_VARIABLE (bool, link_send_and_route_panner, "link-send-and-route-panner", true)
CONFIG_VARIABLE (std::string, midi_audition_synth_uri, "midi-audition-synth-uri", "@default@") /*deprecated*/

/* click */

CONFIG_VARIABLE (bool, clicking, "clicking", false)
CONFIG_VARIABLE (bool, click_record_only, "click-record-only", false)
CONFIG_VARIABLE (std::string, click_sound, "click-sound", "")
CONFIG_VARIABLE (std::string, click_emphasis_sound, "click-emphasis-sound", "")
CONFIG_VARIABLE (gain_t, click_gain, "click-gain", 1.0)
CONFIG_VARIABLE (bool, use_click_emphasis, "use-click-emphasis", true)

/* transport control and related */

/** if true, we call Processor::flush() on all processors when the transport is stopped.
 *  Note that processors are still run when the transport is not moving.
 */
CONFIG_VARIABLE (bool, skip_playback, "skip-playback", true)
CONFIG_VARIABLE (bool, plugins_stop_with_transport, "plugins-stop-with-transport", false)
CONFIG_VARIABLE (bool, recording_resets_xrun_count, "recording-resets-xrun-count,", false)
CONFIG_VARIABLE (bool, stop_recording_on_xrun, "stop-recording-on-xrun", false)
CONFIG_VARIABLE (bool, create_xrun_marker, "create-xrun-marker", false)
CONFIG_VARIABLE (bool, stop_at_session_end, "stop-at-session-end", false)
CONFIG_VARIABLE (float, preroll_seconds, "preroll-seconds", -2.0f)
CONFIG_VARIABLE (bool, loop_is_mode, "loop-is-mode", false)
CONFIG_VARIABLE (LoopFadeChoice, loop_fade_choice, "loop-fade-choice", XFadeLoop)
CONFIG_VARIABLE (samplecnt_t, preroll, "preroll", 0)
CONFIG_VARIABLE (samplecnt_t, postroll, "postroll", 0)
CONFIG_VARIABLE (float, shuttle_speed_factor, "shuttle-speed-factor", 1.0f) // used for MMC shuttle
CONFIG_VARIABLE (float, shuttle_speed_threshold, "shuttle-speed-threshold", 5.0f) // used for MMC shuttle
CONFIG_VARIABLE (ShuttleUnits, shuttle_units, "shuttle-units", Percentage)
CONFIG_VARIABLE (float, shuttle_max_speed, "shuttle-max-speed", 8.0f)
CONFIG_VARIABLE (bool, locate_while_waiting_for_sync, "locate-while-waiting-for-sync", false)
CONFIG_VARIABLE (bool, disable_disarm_during_roll, "disable-disarm-during-roll", false)
CONFIG_VARIABLE (AutoReturnTarget, auto_return_target_list, "auto-return-target-list", AutoReturnTarget(LastLocate|RangeSelectionStart|Loop|RegionSelectionStart))
CONFIG_VARIABLE (bool, reset_default_speed_on_stop, "reset-default-speed-on-stop", false)
CONFIG_VARIABLE (bool, rewind_ffwd_like_tape_decks, "rewind-ffwd-like-tape-decks", true)
CONFIG_VARIABLE (bool, auto_return_after_rewind_ffwd, "auto-return-after-rewind-ffwd", false)
CONFIG_VARIABLE (CueBehavior, cue_behavior, "cue-behavior", FollowCues)

/* metering */

CONFIG_VARIABLE (float, meter_falloff, "meter-falloff", 13.3f)
CONFIG_VARIABLE (MeterType, meter_type_master, "meter-type-master", MeterK14)
CONFIG_VARIABLE (MeterType, meter_type_track, "meter-type-track", MeterPeak)
CONFIG_VARIABLE (MeterType, meter_type_bus, "meter-type-bus", MeterPeak)

/* miscellany */

CONFIG_VARIABLE (bool, try_autostart_engine, "try-autostart-engine", true)
CONFIG_VARIABLE (bool, hide_dummy_backend, "hide-dummy-backend", true)
CONFIG_VARIABLE (bool, copy_demo_sessions, "copy-demo-sessions", true)
CONFIG_VARIABLE (std::string, auditioner_output_left, "auditioner-output-left", "default")
CONFIG_VARIABLE (std::string, auditioner_output_right, "auditioner-output-right", "default")
CONFIG_VARIABLE (bool, replicate_missing_region_channels, "replicate-missing-region-channels", true)
CONFIG_VARIABLE (bool, hiding_groups_deactivates_groups, "hiding-groups-deactivates-groups", true)
CONFIG_VARIABLE (bool, verify_remove_last_capture, "verify-remove-last-capture", true)
CONFIG_VARIABLE (bool, save_history, "save-history", true)
CONFIG_VARIABLE (int32_t, saved_history_depth, "save-history-depth", 20)
CONFIG_VARIABLE (int32_t, history_depth, "history-depth", 20)
CONFIG_VARIABLE (RegionEquivalence, region_equivalence, "region-equivalency", LayerTime)
CONFIG_VARIABLE (bool, periodic_safety_backups, "periodic-safety-backups", true)
CONFIG_VARIABLE (uint32_t, periodic_safety_backup_interval, "periodic-safety-backup-interval", 120)
CONFIG_VARIABLE (float, automation_interval_msecs, "automation-interval-msecs", 30)
#ifdef __APPLE__
CONFIG_VARIABLE_SPECIAL (std::string, default_session_parent_dir, "default-session-parent-dir", "~/Music", poor_mans_glob)
#elif defined (PLATFORM_WINDOWS)
CONFIG_VARIABLE_SPECIAL (std::string, default_session_parent_dir, "default-session-parent-dir", "~\\Documents", poor_mans_glob)
#else
CONFIG_VARIABLE_SPECIAL (std::string, default_session_parent_dir, "default-session-parent-dir", "~", poor_mans_glob)
#endif
CONFIG_VARIABLE (std::string, clip_library_dir, "clip-library-dir", "@default@") /* writable folder */
CONFIG_VARIABLE (std::string, sample_lib_path, "sample-lib-path", "") /* custom paths */
CONFIG_VARIABLE (bool, allow_special_bus_removal, "allow-special-bus-removal", false)
CONFIG_VARIABLE (int32_t, processor_usage, "processor-usage", -1)
CONFIG_VARIABLE (int32_t, cpu_dma_latency, "cpu-dma-latency", -1) /* >=0 to enable */
CONFIG_VARIABLE (gain_t, max_gain, "max-gain", 2.0) /* +6.0dB */
CONFIG_VARIABLE (uint32_t, max_recent_sessions, "max-recent-sessions", 10)
CONFIG_VARIABLE (uint32_t, max_recent_templates, "max-recent-templates", 10)
CONFIG_VARIABLE (double, automation_thinning_factor, "automation-thinning-factor", 20.0)
CONFIG_VARIABLE (std::string, freesound_download_dir, "freesound-download-dir", Glib::get_home_dir() + "/Freesound/snd")
CONFIG_VARIABLE (samplecnt_t, range_location_minimum, "range-location-minimum", 128) /* samples */
CONFIG_VARIABLE (EditMode, edit_mode, "edit-mode", Slide)
CONFIG_VARIABLE (Temporal::TimeDomain, default_automation_time_domain, "default-automation-time-domain", Temporal::BeatTime)

/* plugin related */

CONFIG_VARIABLE (bool, new_plugins_active, "new-plugins-active", true)
CONFIG_VARIABLE (bool, use_plugin_own_gui, "use-plugin-own-gui", true)
CONFIG_VARIABLE (bool, use_windows_vst, "use-windows-vst", true)
CONFIG_VARIABLE (bool, use_lxvst, "use-lxvst", true)
CONFIG_VARIABLE (bool, use_macvst, "use-macvst", true)
CONFIG_VARIABLE (bool, use_audio_units, "use-audio-units", true)
CONFIG_VARIABLE (bool, use_vst3, "use-vst3", true)
CONFIG_VARIABLE (bool, discover_plugins_on_start, "discover-plugins-on-start", false)
CONFIG_VARIABLE (bool, verbose_plugin_scan, "verbose-plugin-scan", false)
CONFIG_VARIABLE (bool, conceal_lv1_if_lv2_exists, "conceal-lv1-if-lv2-exists", true)
CONFIG_VARIABLE (bool, conceal_vst2_if_vst3_exists, "conceal-vst2-if-vst3-exists", true)
CONFIG_VARIABLE (bool, show_vst3_micro_edit_inline, "show-vst3-micro-edit-inline", true)
CONFIG_VARIABLE (bool, ask_replace_instrument, "ask-replace-instrument", true)
CONFIG_VARIABLE (bool, ask_setup_instrument, "ask-setup-instrument", true)
CONFIG_VARIABLE (uint32_t, plugin_scan_timeout, "plugin-scan-timeout", 150) /* deci-seconds */
CONFIG_VARIABLE (uint32_t, limit_n_automatables, "limit-n-automatables", 512)
CONFIG_VARIABLE (uint32_t, plugin_cache_version, "plugin-cache-version", 0)

/* custom user plugin paths */
CONFIG_VARIABLE (std::string, plugin_path_vst, "plugin-path-vst", "@default@")
CONFIG_VARIABLE (std::string, plugin_path_lxvst, "plugin-path-lxvst", "@default@")
CONFIG_VARIABLE (std::string, plugin_path_vst3, "plugin-path-vst3", "@default@")

/* denormal management */

CONFIG_VARIABLE (bool, denormal_protection, "denormal-protection", false)
CONFIG_VARIABLE (DenormalModel, denormal_model, "denormal-model", DenormalFTZDAZ)


/* web addresses used in the program */

CONFIG_VARIABLE (std::string, osx_pingback_url, "osx-pingback-url", "http://community.ardour.org/pingback/osx/")
CONFIG_VARIABLE (std::string, linux_pingback_url, "linux-pingback-url", "http://community.ardour.org/pingback/linux/")
CONFIG_VARIABLE (std::string, windows_pingback_url, "windows-pingback-url", "http://community.ardour.org/pingback/windows/")
CONFIG_VARIABLE (std::string, tutorial_manual_url, "tutorial-manual-url", "http://ardour.org/tutorial")
CONFIG_VARIABLE (std::string, reference_manual_url, "reference-manual-url", "http://manual.ardour.org/")
CONFIG_VARIABLE (std::string, updates_url, "updates-url", "http://ardour.org/whatsnew.html")
CONFIG_VARIABLE (std::string, donate_url, "donate-url", "http://ardour.org/donate")

/* video timeline configuration */
CONFIG_VARIABLE (std::string, xjadeo_binary, "xjadeo-binary", "")
CONFIG_VARIABLE (bool, video_advanced_setup, "video-advanced-setup", false)
CONFIG_VARIABLE (std::string, video_server_url, "video-server-url", "http://127.0.0.1:1554")
#ifndef PLATFORM_WINDOWS
CONFIG_VARIABLE (std::string, video_server_docroot, "video-server-docroot", "/")
#else
CONFIG_VARIABLE (std::string, video_server_docroot, "video-server-docroot", "C:\\")
#endif
CONFIG_VARIABLE (bool, show_video_export_info, "show-video-export-info", true)
CONFIG_VARIABLE (bool, show_video_server_dialog, "show-video-server-dialog", false)

/* export */
CONFIG_VARIABLE (float, export_preroll, "export-preroll", 2.0) // seconds
CONFIG_VARIABLE (float, export_silence_threshold, "export-silence-threshold", -90) // dB
