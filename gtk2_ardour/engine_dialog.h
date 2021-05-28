/*
 * Copyright (C) 2007-2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2007-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015-2016 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef __gtk2_ardour_engine_dialog_h__
#define __gtk2_ardour_engine_dialog_h__

#include <map>
#include <vector>
#include <string>

#include <gtkmm/box.h>
#include <gtkmm/button.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/checkbutton.h>
#include <gtkmm/expander.h>
#include <gtkmm/notebook.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>

#include "pbd/signals.h"

#include "widgets/ardour_button.h"

#include "ardour_dialog.h"

class EngineControl : public ArdourDialog, public PBD::ScopedConnectionList
{
public:
	EngineControl ();
	~EngineControl ();

	static bool need_setup ();

	XMLNode& get_state ();
	bool set_state (const XMLNode&);

	void set_desired_sample_rate (uint32_t);

private:
	Gtk::Notebook notebook;

	Gtk::Label engine_status;

	/* core fields used by all backends */

	Gtk::Table basic_packer;
	Gtk::HBox basic_hbox;
	Gtk::VBox basic_vbox;

	Gtk::ComboBoxText backend_combo;
	Gtk::ComboBoxText driver_combo;
	Gtk::ComboBoxText device_combo;
	Gtk::ComboBoxText input_device_combo;
	Gtk::ComboBoxText output_device_combo;
	Gtk::ComboBoxText sample_rate_combo;
	Gtk::ComboBoxText midi_option_combo;
	Gtk::ComboBoxText buffer_size_combo;
	Gtk::Label        buffer_size_duration_label;
	Gtk::ComboBoxText nperiods_combo;
	Gtk::Adjustment input_latency_adjustment;
	Gtk::SpinButton input_latency;
	Gtk::Adjustment output_latency_adjustment;
	Gtk::SpinButton output_latency;
	Gtk::Adjustment input_channels_adjustment;
	Gtk::SpinButton input_channels;
	Gtk::Adjustment output_channels_adjustment;
	Gtk::SpinButton output_channels;
	Gtk::Adjustment ports_adjustment;
	Gtk::SpinButton ports_spinner;

	Gtk::Label                  have_control_text;
	ArdourWidgets::ArdourButton control_app_button;
	ArdourWidgets::ArdourButton midi_devices_button;
	ArdourWidgets::ArdourButton start_stop_button;
	ArdourWidgets::ArdourButton update_devices_button;
	ArdourWidgets::ArdourButton use_buffered_io_button;
	ArdourWidgets::ArdourButton try_autostart_button;

	Gtk::Button     connect_disconnect_button;

	/* latency measurement */

	class ChannelNameCols : public Gtk::TreeModelColumnRecord
	{
		public:
			ChannelNameCols () {
				add (pretty_name);
				add (port_name);
			}
			Gtk::TreeModelColumn<std::string> pretty_name;
			Gtk::TreeModelColumn<std::string> port_name;
	};

	ChannelNameCols              lm_output_channel_cols;
	Glib::RefPtr<Gtk::ListStore> lm_output_channel_list;
	Gtk::ComboBox                lm_output_channel_combo;

	ChannelNameCols              lm_input_channel_cols;
	Glib::RefPtr<Gtk::ListStore> lm_input_channel_list;
	Gtk::ComboBox                lm_input_channel_combo;

	Gtk::Label                  lm_measure_label;
	Gtk::Button                 lm_measure_button;
	Gtk::Button                 lm_use_button;
	Gtk::Button                 lm_back_button;
	ArdourWidgets::ArdourButton lm_button_audio;
	Gtk::Label                  lm_title;
	Gtk::Label                  lm_preamble;
	Gtk::Label                  lm_results;
	Gtk::Table                  lm_table;
	Gtk::VBox                   lm_vbox;
	bool                        have_lm_results;
	bool                        lm_running;
	bool                        was_running_before_lm;

	/* MIDI Tab */

	Gtk::VBox midi_vbox;
	Gtk::Button midi_back_button;
	Gtk::Table midi_device_table;

	/* MIDI ... JACK */

	Gtk::CheckButton aj_button;

	uint32_t ignore_changes; // state save/load
	uint32_t ignore_device_changes; // AudioEngine::DeviceListChanged
	uint32_t _desired_sample_rate;
	bool     started_at_least_once;
	bool     queue_device_changed;

	void driver_changed ();
	void backend_changed ();
	void sample_rate_changed ();
	void buffer_size_changed ();
	void nperiods_changed ();
	void channels_changed ();
	void latency_changed ();
	void midi_option_changed ();

	void setup_midi_tab_for_backend ();
	void setup_midi_tab_for_jack ();
	void refresh_midi_display (std::string focus = "");

	void update_midi_options ();

	std::string bufsize_as_string (uint32_t);
	std::string nperiods_as_string (uint32_t);

	std::vector<float> get_default_sample_rates ();
	std::vector<uint32_t> get_default_buffer_sizes ();

	std::vector<float> get_sample_rates_for_all_devices ();
	std::vector<uint32_t> get_buffer_sizes_for_all_devices ();

	float get_rate() const;
	uint32_t get_buffer_size() const;
	uint32_t get_nperiods() const;
	uint32_t get_input_channels() const;
	uint32_t get_output_channels() const;
	uint32_t get_input_latency() const;
	uint32_t get_output_latency() const;
	std::string get_device_name() const;
	std::string get_input_device_name() const;
	std::string get_output_device_name() const;
	std::string get_driver() const;
	std::string get_backend() const;
	std::string get_midi_option () const;
	bool get_use_buffered_io () const;

	std::string get_default_device (const std::string&,
	                                const std::vector<std::string>&);

	void device_changed ();
	void input_device_changed ();
	void output_device_changed ();
	bool set_driver_popdown_strings ();
	bool set_device_popdown_strings ();
	bool set_input_device_popdown_strings ();
	bool set_output_device_popdown_strings ();
	void set_samplerate_popdown_strings ();
	void set_buffersize_popdown_strings ();
	void set_nperiods_popdown_strings ();
	void list_devices ();
	void show_buffer_duration ();

	void configure_midi_devices ();

	struct MidiDeviceSetting {
		std::string name;
		bool enabled;
		uint32_t input_latency;
		uint32_t output_latency;

		MidiDeviceSetting (std::string n, bool en = true, uint32_t inl = 0, uint32_t oul = 0)
			: name (n)
			, enabled (en)
			, input_latency (inl)
			, output_latency (oul)
		{}
	};

	typedef boost::shared_ptr<MidiDeviceSetting> MidiDeviceSettings;
	bool _can_set_midi_latencies;
	std::vector<MidiDeviceSettings> _midi_devices;

	MidiDeviceSettings find_midi_device(std::string devicename) const {
		for (std::vector<MidiDeviceSettings>::const_iterator p = _midi_devices.begin(); p != _midi_devices.end(); ++p) {
			if ((*p)->name == devicename) {
				return *p;
			}
		}
		return MidiDeviceSettings();
	}

	struct StateStruct {
		std::string backend;
		std::string driver;
		std::string device;
		std::string input_device;
		std::string output_device;
		float sample_rate;
		uint32_t buffer_size;
		uint32_t n_periods;
		uint32_t input_latency;
		uint32_t output_latency;
		uint32_t input_channels;
		uint32_t output_channels;
		bool active;
		bool use_buffered_io;
		std::string midi_option;
		std::vector<MidiDeviceSettings> midi_devices;
		std::string lm_input;
		std::string lm_output;
		time_t lru;

		StateStruct()
			: sample_rate (48000)
			, buffer_size (1024)
			, input_latency (0)
			, output_latency (0)
			, input_channels (0)
			, output_channels (0)
			, active (false)
			, use_buffered_io (false)
			, lru (0)
		{}
	};

	typedef boost::shared_ptr<StateStruct> State;
	typedef std::list<State> StateList;
	static bool state_sort_cmp (const State &a, const State &b);

	StateList states;

	bool set_state_for_backend (const std::string& backend);

	State get_matching_state (const std::string& backend,
	                          const std::string& driver,
	                          const std::string& device);
	State get_matching_state (const std::string& backend,
	                          const std::string& driver,
	                          const std::string& input_device,
	                          const std::string& output_device);
	State get_saved_state_for_currently_displayed_backend_and_device ();
	void maybe_display_saved_state ();
	State save_state ();
	void store_state (State);
	bool equivalent_states (const State&, const State&);

	bool set_current_state (const State& state);
	void set_default_state ();

	bool  _have_control;

	static bool print_channel_count (Gtk::SpinButton*);

	void build_notebook ();
	void build_full_control_notebook ();
	void build_no_control_notebook ();

	void connect_changed_signals ();
	void block_changed_signals ();
	void unblock_changed_signals ();

	class SignalBlocker
	{
	public:
		SignalBlocker (EngineControl& engine_control, const std::string& reason);

		~SignalBlocker ();

	private:
		EngineControl& ec;
		std::string m_reason;
	};

	uint32_t block_signals;

	sigc::connection backend_combo_connection;
	sigc::connection driver_combo_connection;
	sigc::connection sample_rate_combo_connection;
	sigc::connection buffer_size_combo_connection;
	sigc::connection nperiods_combo_connection;
	sigc::connection device_combo_connection;
	sigc::connection input_device_combo_connection;
	sigc::connection output_device_combo_connection;
	sigc::connection midi_option_combo_connection;
	sigc::connection input_latency_connection;
	sigc::connection output_latency_connection;
	sigc::connection input_channels_connection;
	sigc::connection output_channels_connection;

	void on_show ();
	void on_map ();
	void config_parameter_changed (std::string const&);
	void control_app_button_clicked ();
	void start_stop_button_clicked ();
	void update_devices_button_clicked ();
	void use_buffered_io_button_clicked ();
	void try_autostart_button_clicked ();
	void use_latency_button_clicked ();
	void manage_control_app_sensitivity ();
	int push_state_to_backend (bool start);
	void post_push ();
	void update_sensitivity ();
	bool start_engine ();
	bool stop_engine (bool for_latency = false);

	/* latency measurement */
	void latency_button_clicked ();
	void latency_back_button_clicked ();
	bool check_audio_latency_measurement ();
	bool check_midi_latency_measurement ();
	sigc::connection latency_timeout;
	void enable_latency_tab ();
	void disable_latency_tab ();
	void start_latency_detection ();
	void end_latency_detection ();

	void on_switch_page (GtkNotebookPage*, guint page_num);
	bool on_delete_event (GdkEventAny*);
	void on_response (int);

	void engine_running ();
	void engine_stopped ();
	void device_list_changed ();

	PBD::ScopedConnection running_connection;
	PBD::ScopedConnectionList stopped_connection;
	PBD::ScopedConnection devicelist_connection;

	void connect_disconnect_click ();
	void calibrate_audio_latency ();
	void calibrate_midi_latency (MidiDeviceSettings);

	MidiDeviceSettings _measure_midi;
	void midi_latency_adjustment_changed(Gtk::Adjustment *, MidiDeviceSettings, bool);
	void midi_device_enabled_toggled(ArdourWidgets::ArdourButton *, MidiDeviceSettings);
	sigc::connection lm_back_button_signal;
};

#endif /* __gtk2_ardour_engine_dialog_h__ */
