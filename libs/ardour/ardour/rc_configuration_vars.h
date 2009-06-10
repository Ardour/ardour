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

/* IO connection */

CONFIG_VARIABLE (bool, auto_connect_master, "auto-connect-master", true)
CONFIG_VARIABLE (AutoConnectOption, output_auto_connect, "output-auto-connect", AutoConnectOption (0))
CONFIG_VARIABLE (AutoConnectOption, input_auto_connect, "input-auto-connect", AutoConnectOption (0))

/* MIDI and MIDI related */

CONFIG_VARIABLE (std::string, mtc_port_name, "mtc-port-name", "default")
CONFIG_VARIABLE (std::string, mmc_port_name, "mmc-port-name", "default")
CONFIG_VARIABLE (std::string, midi_port_name, "midi-port-name", "default")
CONFIG_VARIABLE (std::string, midi_clock_port_name, "midi-clock-port-name", "default")
CONFIG_VARIABLE (bool, trace_midi_input, "trace-midi-input", false)
CONFIG_VARIABLE (bool, trace_midi_output, "trace-midi-output", false)
CONFIG_VARIABLE (bool, send_mtc, "send-mtc", false)
CONFIG_VARIABLE (bool, send_mmc, "send-mmc", true)
CONFIG_VARIABLE (bool, send_midi_clock, "send-midi-clock", false)
CONFIG_VARIABLE (bool, mmc_control, "mmc-control", true)
CONFIG_VARIABLE (bool, midi_feedback, "midi-feedback", false)
CONFIG_VARIABLE (uint8_t, mmc_receive_device_id, "mmc-receive-device-id", 0)
CONFIG_VARIABLE (uint8_t, mmc_send_device_id, "mmc-send-device-id", 0)
CONFIG_VARIABLE (int32_t, initial_program_change, "initial-program-change", -1)

/* control surfaces */

CONFIG_VARIABLE (uint32_t, feedback_interval_ms,  "feedback-interval-ms", 100)
CONFIG_VARIABLE (bool, use_tranzport,  "use-tranzport", false)
CONFIG_VARIABLE (std::string, mackie_emulation, "mackie-emulation", "mcu")
CONFIG_VARIABLE (RemoteModel, remote_model, "remote-model", MixerOrdered)

/* disk operations */

CONFIG_VARIABLE (uint32_t, minimum_disk_io_bytes,  "minimum-disk-io-bytes", 1024 * 256)
CONFIG_VARIABLE (float, midi_readahead,  "midi-readahead", 1.0)
CONFIG_VARIABLE (float, audio_track_buffer_seconds, "track-buffer-seconds", 5.0)
CONFIG_VARIABLE (float, midi_track_buffer_seconds, "midi-track-buffer-seconds", 1.0)
CONFIG_VARIABLE (uint32_t, disk_choice_space_threshold,  "disk-choice-space-threshold", 57600000)
CONFIG_VARIABLE (bool, auto_analyse_audio, "auto-analyse-audio", false)

/* OSC */

CONFIG_VARIABLE (uint32_t, osc_port, "osc-port", 3819)
CONFIG_VARIABLE (bool, use_osc, "use-osc", false)

/* editing related */

CONFIG_VARIABLE (EditMode, edit_mode, "edit-mode", Slide)
CONFIG_VARIABLE (bool, link_region_and_track_selection, "link-region-and-track-selection", false)
CONFIG_VARIABLE (std::string, keyboard_layout_name, "keyboard-layout-name", "ansi")
CONFIG_VARIABLE (bool, automation_follows_regions, "automation-follows-regions", false)
CONFIG_VARIABLE (bool, region_boundaries_from_selected_tracks, "region-boundaries-from-selected-tracks", true)
CONFIG_VARIABLE (bool, region_boundaries_from_onscreen_tracks, "region-boundaries-from-onscreen_tracks", true)

/* monitoring, mute, solo etc */

CONFIG_VARIABLE (bool, mute_affects_pre_fader, "mute-affects-pre-fader", true)
CONFIG_VARIABLE (bool, mute_affects_post_fader, "mute-affects-post-fader", true)
CONFIG_VARIABLE (bool, mute_affects_control_outs, "mute-affects-control-outs", true)
CONFIG_VARIABLE (bool, mute_affects_main_outs, "mute-affects-main-outs", true)
CONFIG_VARIABLE (MonitorModel, monitoring_model, "monitoring-model", ExternalMonitoring)
CONFIG_VARIABLE (SoloModel, solo_model, "solo-model", InverseMute)
CONFIG_VARIABLE (bool, solo_latched, "solo-latched", true)
CONFIG_VARIABLE (bool, latched_record_enable, "latched-record-enable", false)
CONFIG_VARIABLE (bool, all_safe, "all-safe", false)
CONFIG_VARIABLE (bool, show_solo_mutes, "show-solo-mutes", false)
CONFIG_VARIABLE (bool, solo_mute_override, "solo-mute-override", false)
CONFIG_VARIABLE (bool, tape_machine_mode, "tape-machine-mode", false)
CONFIG_VARIABLE (gain_t, solo_mute_gain, "solo_mute-gain", 0.0)

/* click */

CONFIG_VARIABLE (bool, clicking, "clicking", false)
CONFIG_VARIABLE (std::string, click_sound, "click-sound", "")
CONFIG_VARIABLE (std::string, click_emphasis_sound, "click-emphasis-sound", "")

/* transport control and related */

CONFIG_VARIABLE (bool, plugins_stop_with_transport, "plugins-stop-with-transport", false)
CONFIG_VARIABLE (bool, do_not_record_plugins, "do-not-record-plugins", false)
CONFIG_VARIABLE (bool, stop_recording_on_xrun, "stop-recording-on-xrun", false)
CONFIG_VARIABLE (bool, create_xrun_marker, "create-xrun-marker", true)
CONFIG_VARIABLE (bool, stop_at_session_end, "stop-at-session-end", true)
CONFIG_VARIABLE (bool, seamless_loop, "seamless-loop", false)
CONFIG_VARIABLE (nframes_t, preroll, "preroll", 0)
CONFIG_VARIABLE (nframes_t, postroll, "postroll", 0)
CONFIG_VARIABLE (float, rf_speed, "rf-speed", 2.0f)
CONFIG_VARIABLE (float, shuttle_speed_factor, "shuttle-speed-factor", 1.0f)
CONFIG_VARIABLE (float, shuttle_speed_threshold, "shuttle-speed-threshold", 5.0f)
CONFIG_VARIABLE (SlaveSource, slave_source, "slave-source", None)
CONFIG_VARIABLE (ShuttleBehaviour, shuttle_behaviour, "shuttle-behaviour", Sprung)
CONFIG_VARIABLE (ShuttleUnits, shuttle_units, "shuttle-units", Percentage)
CONFIG_VARIABLE (bool, primary_clock_delta_edit_cursor, "primary-clock-delta-edit-cursor", false)
CONFIG_VARIABLE (bool, secondary_clock_delta_edit_cursor, "secondary-clock-delta-edit-cursor", false)
CONFIG_VARIABLE (bool, show_track_meters, "show-track-meters", true)
CONFIG_VARIABLE (bool, locate_while_waiting_for_sync, "locate-while-waiting-for-sync", false)

/* metering */

CONFIG_VARIABLE (float, meter_hold, "meter-hold", 100.0f)
CONFIG_VARIABLE (float, meter_falloff, "meter-falloff", 27.0f)

/* miscellany */

CONFIG_VARIABLE (bool, hiding_groups_deactivates_groups, "hiding-groups-deactivates-groups", true)
CONFIG_VARIABLE (bool, verify_remove_last_capture, "verify-remove-last-capture", true)
CONFIG_VARIABLE (bool, no_new_session_dialog, "no-new-session-dialog", false)
CONFIG_VARIABLE (bool, use_vst, "use-vst", true)
CONFIG_VARIABLE (bool, save_history, "save-history", true)
CONFIG_VARIABLE (int32_t, saved_history_depth, "save-history-depth", 20)
CONFIG_VARIABLE (int32_t, history_depth, "history-depth", 20)
CONFIG_VARIABLE (bool, use_overlap_equivalency, "use-overlap-equivalency", false)
CONFIG_VARIABLE (bool, periodic_safety_backups, "periodic-safety-backups", true)
CONFIG_VARIABLE (uint32_t, periodic_safety_backup_interval, "periodic-safety-backup-interval", 120)
CONFIG_VARIABLE (float, automation_interval, "automation-interval", 50)
CONFIG_VARIABLE (bool, sync_all_route_ordering, "sync-all-route-ordering", true)
CONFIG_VARIABLE (bool, only_copy_imported_files, "only-copy-imported-files", true)
CONFIG_VARIABLE (bool, new_plugins_active, "new-plugins-active", true)
CONFIG_VARIABLE (std::string, keyboard_layout, "keyboard-layout", "ansi")
CONFIG_VARIABLE (std::string, default_bindings, "default-bindings", "ardour")
CONFIG_VARIABLE (bool, default_narrow_ms, "default-narrow_ms", false)
CONFIG_VARIABLE (bool, name_new_markers, "name-new-markers", false)
CONFIG_VARIABLE (bool, rubberbanding_snaps_to_grid, "rubberbanding-snaps-to-grid", false)
CONFIG_VARIABLE (long, font_scale, "font-scale", 102400)
CONFIG_VARIABLE (std::string, default_session_parent_dir, "default-session-parent-dir", "~")
CONFIG_VARIABLE (bool, show_waveforms, "show-waveforms", true)
CONFIG_VARIABLE (WaveformScale, waveform_scale, "waveform-scale", Linear)
CONFIG_VARIABLE (WaveformShape, waveform_shape, "waveform-shape", Traditional)

/* denormal management */

CONFIG_VARIABLE (bool, denormal_protection, "denormal-protection", false)
CONFIG_VARIABLE (DenormalModel, denormal_model, "denormal-model", DenormalNone)

