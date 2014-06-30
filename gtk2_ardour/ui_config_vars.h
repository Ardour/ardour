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

UI_CONFIG_VARIABLE(std::string, icon_set, "icon-set", "default")
UI_CONFIG_VARIABLE(std::string, ui_rc_file, "ui-rc-file", "ardour3_ui_dark.rc")
UI_CONFIG_VARIABLE(bool, flat_buttons, "flat-buttons", false)
UI_CONFIG_VARIABLE(float, waveform_gradient_depth, "waveform-gradient-depth", 0)
UI_CONFIG_VARIABLE(float, timeline_item_gradient_depth, "timeline-item-gradient-depth", 0.5)
UI_CONFIG_VARIABLE(bool, all_floating_windows_are_dialogs, "all-floating-windows-are-dialogs", false)
UI_CONFIG_VARIABLE (bool, color_regions_using_track_color, "color-regions-using-track-color", false)
UI_CONFIG_VARIABLE (bool, show_waveform_clipping, "show-waveform-clipping", true)
UI_CONFIG_VARIABLE (uint32_t, lock_gui_after_seconds, "lock-gui-after-seconds", 0)
UI_CONFIG_VARIABLE (bool, draggable_playhead, "draggable-playhead", true)

