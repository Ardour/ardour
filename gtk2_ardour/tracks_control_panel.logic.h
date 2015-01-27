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

// class TracksControlPanel : public WavesDialog {
  public:
    void refresh_session_settings_info ();
    enum {
        AudioSystemSettingsTab,
        MIDISystemSettingsTab,
        SessionSettingsTab,
        PreferencesTab
    };
    void show_and_open_tab (int);

  private:

// attributes
    uint32_t _desired_sample_rate;
    bool  _have_control;

// this flag is set for immediate return during combo-box change callbacks
// when we do not need to process current combo-box change
	uint32_t _ignore_changes;
    std::string _current_device;

// Sync stuff
	PBD::ScopedConnectionList update_connections;
    PBD::ScopedConnection running_connection;
    PBD::ScopedConnection stopped_connection;

// Methods
	virtual void init();
	DeviceConnectionControl& add_device_capture_control(std::string port_name, bool active, uint16_t capture_number, std::string track_name);
	DeviceConnectionControl& add_device_playback_control(std::string port_name, bool active, uint16_t playback_number);
	MidiDeviceConnectionControl& add_midi_device_control(const std::string& midi_device_name,
                                                         const std::string& capture_name, bool capture_active,
                                                         const std::string& playback_name, bool playback_active);

	void on_a_settings_tab_button_clicked (WavesButton* clicked_button);
    void on_multi_out (WavesButton*);
    void on_stereo_out (WavesButton*);
    void on_browse_button (WavesButton*);
    void save_default_session_path();
    void save_auto_lock_time();
    void save_auto_save_time();
    void save_pre_record_buffer();
	void on_ok(WavesButton*);
	void on_cancel(WavesButton*);
    void update_configs();
    void update_session_config();
	void on_capture_active_changed (DeviceConnectionControl* capture_control, bool active);
	void on_playback_active_changed (DeviceConnectionControl* playback_control, bool active);
	void on_midi_capture_active_changed (MidiDeviceConnectionControl* control, bool active);
	void on_midi_playback_active_changed (MidiDeviceConnectionControl* control, bool active);
    void on_all_inputs_on_button(WavesButton*);
    void on_all_inputs_off_button(WavesButton*);
    void on_all_outputs_on_button(WavesButton*);
    void on_all_outputs_off_button(WavesButton*);
    void on_name_tracks_after_driver(WavesButton*);
    void on_reset_tracks_name_to_default(WavesButton*);
    void on_yes_button(WavesButton*);
    void on_no_button(WavesButton*);
    void on_control_panel_button(WavesButton*);
    ARDOUR::TracksAutoNamingRule _tracks_naming_rule;

	void on_engine_dropdown_item_clicked (WavesDropdown*, int);
	void on_device_dropdown_item_clicked (WavesDropdown*, int);
	void device_changed ();
	void buffer_size_changed ();
	void on_buffer_size_dropdown_item_clicked (WavesDropdown*, int);
	void on_sample_rate_dropdown_item_clicked (WavesDropdown*, int);
    void on_mtc_input_chosen (WavesDropdown*, int);
    void engine_running ();
    void engine_stopped ();
	void on_file_type_dropdown_item_clicked (WavesDropdown*, int);
    void on_bit_depth_dropdown_item_clicked (WavesDropdown*, int);
    void on_frame_rate_item_clicked (WavesDropdown*, int);

	void populate_engine_dropdown ();
	void populate_device_dropdown ();
	void populate_sample_rate_dropdown ();
	void populate_buffer_size_dropdown ();
    void populate_mtc_in_dropdown ();
	void populate_output_mode ();
    void populate_input_channels();
    void populate_output_channels();
    void populate_midi_ports();
    void populate_default_session_path();
    void display_waveform_color_fader();
    void color_adjustment_changed();

    // Session Settings
    void populate_file_type_dropdown();
    void populate_bit_depth_dropdown();
    void populate_frame_rate_dropdown();
    void populate_auto_lock_timer_dropdown();
    void populate_auto_save_timer_dropdown();
    void populate_pre_record_buffer_dropdown();

    // Engine State update callback handlers
    void on_port_registration_update();
    void on_buffer_size_update ();
    void on_device_list_update (bool current_device_disconnected);
    void on_parameter_changed (const std::string& parameter_name);
    void on_audio_input_configuration_changed ();
    void on_audio_output_configuration_changed ();
    void on_midi_input_configuration_changed ();
    void on_midi_output_configuration_changed ();
    void on_mtc_input_changed (const std::string&);
    void on_device_error ();

	bool on_key_press_event (GdkEventKey*);
    void accept ();
    void reject ();

	// Merged ARDOUR's preferences
	void display_waveform_shape ();
	void display_meter_hold ();
	void display_meter_falloff ();
	void display_hdd_buffering ();
	void display_mmc_control ();
	void display_send_mmc ();
	void display_mmc_send_device_id ();
	void display_mmc_receive_device_id ();
	void display_only_copy_imported_files ();
	void display_history_depth ();
	void display_saved_history_depth ();
	void display_denormal_protection ();
	void display_general_preferences ();
	void save_general_preferences ();

    void cleanup_input_channels_list();
    void cleanup_output_channels_list();
    void cleanup_midi_device_list();

	std::string bufsize_as_string (uint32_t sz);

    std::string         get_device_name() const { return _device_dropdown.get_text (); };
    ARDOUR::framecnt_t  get_sample_rate() const;
    ARDOUR::pframes_t   get_buffer_size () const;
	uint32_t            get_input_channels () const { return 0; };
    uint32_t            get_output_channels () const { return 0; };
    uint32_t            get_input_latency () const { return 0; };
    uint32_t            get_output_latency () const { return 0; };

    std::string _default_path_name;

	void show_buffer_duration ();
//};

