/*
    Copyright (C) 2014 Waves Audio Ltd.

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

#ifndef __gtk2_ardour_tracks_control_panel_h__
#define __gtk2_ardour_tracks_control_panel_h__

#include <map>
#include <vector>
#include <string>

#include "ardour/types.h"

#include <gtkmm/layout.h>

#include "pbd/signals.h"
#include "pbd/xml++.h"

#include "waves_dialog.h"
#include "waves_dropdown.h"
#include "device_connection_control.h"
#include "midi_device_connection_control.h"
#include "option_editor.h"

class WavesButton;

class TracksControlPanel : public WavesDialog, public PBD::ScopedConnectionList {
  public:
	TracksControlPanel ();
    ~TracksControlPanel ();

  private:
	Gtk::VBox& _device_capture_list;
	Gtk::VBox& _device_playback_list;
	Gtk::VBox& _midi_device_list;
    Gtk::VBox& _enable_ltc_generator_vbox;
    Gtk::VBox& _ltc_output_port_vbox;
    Gtk::VBox& _ltc_generator_level_vbox;
    Gtk::HBox& _ltc_send_continuously_hbox;
    WavesButton& _all_inputs_on_button;
    WavesButton& _all_inputs_off_button;
    WavesButton& _all_outputs_on_button;
    WavesButton& _all_outputs_off_button;
	Gtk::Container& _audio_settings_tab;
	Gtk::Container& _midi_settings_tab;
	Gtk::Container& _session_settings_tab;
	Gtk::Container& _general_settings_tab;
    Gtk::Container& _sync_settings_tab;
	WavesButton& _audio_settings_tab_button;
	WavesButton& _midi_settings_tab_button;
	WavesButton& _session_settings_tab_button;
	WavesButton& _general_settings_tab_button;
    WavesButton& _sync_settings_tab_button;
	WavesButton& _multi_out_button;
	WavesButton& _stereo_out_button;
	WavesButton& _ok_button;
	WavesButton& _cancel_button;
	WavesButton& _control_panel_button;
	WavesButton& _no_button;
	WavesButton& _yes_button;
    WavesButton& _browse_button;
    WavesButton& _name_tracks_after_driver;
    WavesButton& _reset_tracks_name_to_default;
    Gtk::Adjustment& _color_adjustment;
    Gtk::Adjustment& _ltc_generator_level_adjustment;
    Gtk::Container& _color_box;
	WavesButton& _obey_mmc_commands_button;
	WavesButton& _send_mmc_commands_button;
	WavesButton& _dc_bias_against_denormals_button;
	WavesButton& _copy_imported_files_button;
    WavesButton& _enable_ltc_generator_button;
    WavesButton& _ltc_send_continuously_button;
	Gtk::SpinButton& _inbound_mmc_device_spinbutton;
	Gtk::SpinButton& _outbound_mmc_device_spinbutton;
	Gtk::SpinButton& _limit_undo_history_spinbutton;
	Gtk::SpinButton& _save_undo_history_spinbutton;
	WavesDropdown& _engine_dropdown;
	WavesDropdown& _device_dropdown;
	WavesDropdown& _sample_rate_dropdown;
	WavesDropdown& _buffer_size_dropdown;
	WavesDropdown& _file_type_dropdown;
	WavesDropdown& _bit_depth_dropdown;
	WavesDropdown& _frame_rate_dropdown;
	WavesDropdown& _auto_lock_timer_dropdown;
	WavesDropdown& _auto_save_timer_dropdown;
	WavesDropdown& _pre_record_buffer_dropdown;
	WavesDropdown& _waveform_shape_dropdown;
	WavesDropdown& _peak_hold_time_dropdown;
	WavesDropdown& _dpm_fall_off_dropdown;
	WavesDropdown& _hard_disk_buffering_dropdown;
    WavesDropdown& _sync_tool_dropdown;
    WavesDropdown& _mtc_in_dropdown;
    WavesDropdown& _ltc_in_dropdown;
    WavesDropdown& _ltc_out_dropdown;
    
    Gtk::Label& _latency_label;
    Gtk::Label& _default_open_path;
    Gtk::Label& _ltc_generator_level_label;
    
    Gtk::Layout& _sync_input_port_layout;
    
#include "tracks_control_panel.logic.h"
};

#endif /* __gtk2_ardour_tracks_control_panel_h__ */
