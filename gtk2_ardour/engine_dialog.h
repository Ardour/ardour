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
#include <gtkmm/treeview.h>
#include <gtkmm/treestore.h>

class EngineControl : public Gtk::VBox {
  public:
	EngineControl ();
	~EngineControl ();

	static bool need_setup ();
	int setup_engine ();
        int prepare ();

	bool was_used() const { return _used; }
	XMLNode& get_state ();
	void set_state (const XMLNode&);

	std::string soundgrid_lan_port () const;

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

	Gtk::Label device_label;

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

	struct SGInventoryColumns : public Gtk::TreeModel::ColumnRecord {
		SGInventoryColumns () {
			add (assign);
			add (device);
			add (channels);
			add (name);
			add (mac);
			add (status);
			add (id);
		}

		Gtk::TreeModelColumn<std::string> device;
		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> mac;
		Gtk::TreeModelColumn<std::string> status;
		Gtk::TreeModelColumn<std::string> assign; 
		Gtk::TreeModelColumn<std::string> channels;
		Gtk::TreeModelColumn<std::string> id;
	};

	struct SGServerInventoryColumns : public Gtk::TreeModel::ColumnRecord {
		SGServerInventoryColumns () {
			add (assign);
			add (name);
			add (channels);
			add (mac);
		}

		Gtk::TreeModelColumn<std::string> name;
		Gtk::TreeModelColumn<std::string> mac;
		Gtk::TreeModelColumn<std::string> assign; 
		Gtk::TreeModelColumn<std::string> channels;
	};

	struct SGNumericalColumns : public Gtk::TreeModel::ColumnRecord {
		SGNumericalColumns () {
			add (number);
		}
		Gtk::TreeModelColumn<uint32_t> number;
	};

	SGInventoryColumns sg_iobox_columns;
	SGServerInventoryColumns sg_server_columns;
	SGNumericalColumns sg_assignment_columns;
	SGNumericalColumns sg_channel_columns;

	Glib::RefPtr<Gtk::TreeStore> soundgrid_iobox_model;
	Glib::RefPtr<Gtk::TreeStore> soundgrid_server_model;
	Glib::RefPtr<Gtk::TreeStore> sg_assignment_model;
	Glib::RefPtr<Gtk::TreeStore> sg_channel_model;

	Gtk::TreeView soundgrid_iobox_display;
	Gtk::TreeView soundgrid_server_display;
	Gtk::VBox soundgrid_vbox;

        void set_soundgrid_parameters ();
	void soundgrid_configure ();

	Gtk::Notebook notebook;

	bool _used;

        static bool engine_running ();
        static bool soundgrid_requested ();

	void driver_changed ();
        void interface_changed ();
	void build_command_line (std::vector<std::string>&);

	std::map<std::string,std::vector<std::string> > devices;
	std::vector<std::string> backend_devs;
	void enumerate_devices (const std::string& driver);

#ifdef __APPLE__
	std::vector<std::string> enumerate_coreaudio_devices ();
#else
	std::vector<std::string> enumerate_alsa_devices ();
	std::vector<std::string> enumerate_oss_devices ();
	std::vector<std::string> enumerate_freebob_devices ();
	std::vector<std::string> enumerate_ffado_devices ();
#endif
	std::vector<std::string> enumerate_netjack_devices ();
	std::vector<std::string> enumerate_dummy_devices ();

	void redisplay_latency ();
	uint32_t get_rate();
	void audio_mode_changed ();
	std::vector<std::string> server_strings;
	void find_jack_servers (std::vector<std::string>&);
	std::string get_device_name (const std::string& driver, const std::string& human_readable_name);

#ifdef HAVE_SOUNDGRID
        Gtk::Label*     inputs_label;
        Gtk::Adjustment inputs_adjustment;
        Gtk::SpinButton inputs_spinner;

        Gtk::Label*     outputs_label;
        Gtk::Adjustment outputs_adjustment;
        Gtk::SpinButton outputs_spinner;

        Gtk::Label*     tracks_label;
        Gtk::Adjustment tracks_adjustment;
        Gtk::SpinButton tracks_spinner;

        Gtk::Label*     busses_label;
        Gtk::Adjustment busses_adjustment;
        Gtk::SpinButton busses_spinner;
#endif
};

#endif /* __gtk2_ardour_engine_dialog_h__ */
