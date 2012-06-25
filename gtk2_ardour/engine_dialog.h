/*
    Copyright (C) 2010 Paul Davis

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

#ifndef __gtk2_ardour_engine_dialog_h__
#define __gtk2_ardour_engine_dialog_h__

#include <map>
#include <vector>
#include <string>

#include <gtkmm/checkbutton.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/notebook.h>
#include <gtkmm/comboboxtext.h>
#include <gtkmm/table.h>
#include <gtkmm/expander.h>
#include <gtkmm/box.h>
#include <gtkmm/buttonbox.h>
#include <gtkmm/button.h>

class EngineControl : public Gtk::VBox {
  public:
	EngineControl ();
	~EngineControl ();

	static bool engine_running ();
	int setup_engine ();

	bool was_used() const { return _used; }
	XMLNode& get_state ();
	void set_state (const XMLNode&);

  private:
	Gtk::Adjustment periods_adjustment;
	Gtk::SpinButton periods_spinner;
	Gtk::Adjustment ports_adjustment;
	Gtk::SpinButton ports_spinner;
	Gtk::Adjustment input_latency_adjustment;
	Gtk::SpinButton input_latency;
	Gtk::Adjustment output_latency_adjustment;
	Gtk::SpinButton output_latency;
	Gtk::Label latency_label;

	Gtk::CheckButton realtime_button;
	Gtk::CheckButton no_memory_lock_button;
	Gtk::CheckButton unlock_memory_button;
	Gtk::CheckButton soft_mode_button;
	Gtk::CheckButton monitor_button;
	Gtk::CheckButton force16bit_button;
	Gtk::CheckButton hw_monitor_button;
	Gtk::CheckButton hw_meter_button;
	Gtk::CheckButton verbose_output_button;

	Gtk::Button start_button;
	Gtk::Button stop_button;
	Gtk::HButtonBox button_box;

	Gtk::ComboBoxText sample_rate_combo;
	Gtk::ComboBoxText period_size_combo;

	Gtk::ComboBoxText preset_combo;
	Gtk::ComboBoxText serverpath_combo;
	Gtk::ComboBoxText driver_combo;
	Gtk::ComboBoxText interface_combo;
	Gtk::ComboBoxText timeout_combo;
	Gtk::ComboBoxText dither_mode_combo;
	Gtk::ComboBoxText audio_mode_combo;
	Gtk::ComboBoxText input_device_combo;
	Gtk::ComboBoxText output_device_combo;
	Gtk::ComboBoxText midi_driver_combo;

	Gtk::Table basic_packer;
	Gtk::Table options_packer;
	Gtk::Table device_packer;
	Gtk::HBox basic_hbox;
	Gtk::HBox options_hbox;
	Gtk::HBox device_hbox;
	Gtk::Notebook notebook;

	bool _used;

	void driver_changed ();
	void build_command_line (std::vector<std::string>&);

	std::map<std::string,std::vector<std::string> > devices;
	std::vector<std::string> backend_devs;
	void enumerate_devices (const std::string& driver);

#ifdef __APPLE__
	std::vector<std::string> enumerate_coreaudio_devices ();
#else
	std::vector<std::string> enumerate_alsa_devices ();
	std::vector<std::string> enumerate_oss_devices ();
	std::vector<std::string> enumerate_netjack_devices ();
	std::vector<std::string> enumerate_freebob_devices ();
	std::vector<std::string> enumerate_ffado_devices ();
	std::vector<std::string> enumerate_dummy_devices ();
#endif

	void redisplay_latency ();
	uint32_t get_rate();
	void audio_mode_changed ();
	std::vector<std::string> server_strings;
	void find_jack_servers (std::vector<std::string>&);
	std::string get_device_name (const std::string& driver, const std::string& human_readable_name);
};

#endif /* __gtk2_ardour_engine_dialog_h__ */
