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

  private:
// data types:
    struct State {
		std::string backend;
		std::string driver;
		std::string device;
		float sample_rate;
		uint32_t buffer_size;
		uint32_t input_latency;
		uint32_t output_latency;
		uint32_t input_channels;
		uint32_t output_channels;
		bool active;
		std::string midi_option;

		State() 
			: input_latency (0)
			, output_latency (0)
			, input_channels (0)
			, output_channels (0)
			, active (false) {}
    };

    typedef std::list<State> StateList;

// attributes
    uint32_t _desired_sample_rate;
    bool  _have_control;

	// this flag is set for emidiate return during combo-box change callbacks
	// when we don need to process current combo-box change
	uint32_t _ignore_changes;
    std::string _current_device;

	StateList states;

//	Sync stuff
	PBD::ScopedConnectionList update_connections;
    PBD::ScopedConnection running_connection;
    PBD::ScopedConnection stopped_connection;

// methods
	virtual void init();
	DeviceConnectionControl& add_device_capture_control(std::string device_capture_name, bool active, uint16_t capture_number, std::string track_name);
	DeviceConnectionControl& add_device_playback_control(std::string device_playback_name, bool active, uint16_t playback_number);
	DeviceConnectionControl& add_midi_capture_control(std::string device_capture_name, bool active);
	DeviceConnectionControl& add_midi_playback_control(bool active);

	void on_audio_settings (WavesButton*);
	void on_midi_settings (WavesButton*);
	void on_session_settings (WavesButton*);
	void on_control_panel (WavesButton*);
    void on_multi_out (WavesButton*);
    void on_stereo_out (WavesButton*);
	void on_ok(WavesButton*);
	void on_cancel(WavesButton*);
	void on_apply(WavesButton*);
	void on_capture_active_changed (DeviceConnectionControl* capture_control, bool active);
	void on_playback_active_changed (DeviceConnectionControl* playback_control, bool active);
	void on_midi_capture_active_changed (DeviceConnectionControl* capture_control, bool active);
	void on_midi_playback_active_changed (DeviceConnectionControl* playback_control, bool active);

	void engine_changed ();
	void device_changed (bool show_confirm_dial);
	void buffer_size_changed ();
	void sample_rate_changed ();
    void engine_running ();
    void engine_stopped ();

	void populate_engine_combo ();
	void populate_device_combo ();
	void populate_sample_rate_combo ();
	void populate_buffer_size_combo ();
	void populate_output_mode ();

	std::string bufsize_as_string (uint32_t sz);

    std::string         get_device_name() const { return _device_combo.get_active_text (); };
    ARDOUR::framecnt_t  get_sample_rate() const;
    ARDOUR::pframes_t   get_buffer_size () const;
	uint32_t            get_input_channels () const { return 0; };
    uint32_t            get_output_channels () const { return 0; };
    uint32_t            get_input_latency () const { return 0; };
    uint32_t            get_output_latency () const { return 0; };

	void show_buffer_duration ();
//};

