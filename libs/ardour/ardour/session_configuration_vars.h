/*
    Copyright (C) 2009 Paul Davis

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

CONFIG_VARIABLE (CrossfadeModel, xfade_model, "xfade-model", FullCrossfade)
CONFIG_VARIABLE (bool, auto_xfade, "auto-xfade", true)
CONFIG_VARIABLE (float, short_xfade_seconds, "short-xfade-seconds", 0.015)
CONFIG_VARIABLE (bool, xfades_active, "xfades-active", true)
CONFIG_VARIABLE (bool, xfades_visible, "xfades-visible", true)
CONFIG_VARIABLE (uint32_t, destructive_xfade_msecs,  "destructive-xfade-msecs", 2)
CONFIG_VARIABLE (bool, use_region_fades, "use-region-fades", true)
CONFIG_VARIABLE (bool, show_region_fades, "show-region-fades", true)
CONFIG_VARIABLE (SampleFormat, native_file_data_format,  "native-file-data-format", ARDOUR::FormatFloat)
CONFIG_VARIABLE (HeaderFormat, native_file_header_format,  "native-file-header-format", ARDOUR::WAVE)
CONFIG_VARIABLE (bool, auto_play, "auto-play", false)
CONFIG_VARIABLE (bool, auto_return, "auto-return", false)
CONFIG_VARIABLE (bool, auto_input, "auto-input", true)
CONFIG_VARIABLE (bool, punch_in, "punch-in", false)
CONFIG_VARIABLE (bool, punch_out, "punch-out", false)
CONFIG_VARIABLE (uint32_t, subframes_per_frame, "subframes-per-frame", 100)
CONFIG_VARIABLE (SmpteFormat, smpte_format, "smpte-format", smpte_30)
CONFIG_VARIABLE_SPECIAL(Glib::ustring, raid_path, "raid-path", "", path_expand)
CONFIG_VARIABLE (std::string, bwf_country_code, "bwf-country-code", "US")
CONFIG_VARIABLE (std::string, bwf_organization_code, "bwf-organization-code", "US")
CONFIG_VARIABLE (bool, end_marker_is_free, "end-marker-is-free", true)
CONFIG_VARIABLE (LayerModel, layer_model, "layer-model", MoveAddHigher)
CONFIG_VARIABLE (std::string, auditioner_output_left, "auditioner-output-left", "default")
CONFIG_VARIABLE (std::string, auditioner_output_right, "auditioner-output-right", "default")
CONFIG_VARIABLE (bool, timecode_source_is_synced, "timecode-source-is-synced", false)
CONFIG_VARIABLE (bool, jack_time_master, "jack-time-master", true)
CONFIG_VARIABLE (bool, use_video_sync, "use-video-sync", false)
CONFIG_VARIABLE (float, video_pullup, "video-pullup", 0.0f)
CONFIG_VARIABLE (bool, show_summary, "show-summary", true)
CONFIG_VARIABLE (bool, show_group_tabs, "show-group-tabs", true)
