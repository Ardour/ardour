/*
 * Copyright (C) 2016-2017 Len Ovens <len@ovenwerks.net>
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
#ifndef osc_gui_h
#define osc_gui_h


#include "osc.h"

#include "pbd/i18n.h"

// preset stuff
static const char* const preset_dir_name = "osc";
static const char* const preset_suffix = ".preset";
static const char * const preset_env_variable_name = "ARDOUR_OSC_PATH";

namespace ArdourSurface {

class OSC_GUI : public Gtk::Notebook
{
public:
	OSC_GUI (OSC&);
	~OSC_GUI ();


private:
	// settings page
	Gtk::ComboBoxText debug_combo;
	Gtk::ComboBoxText portmode_combo;
	Gtk::SpinButton port_entry;
	Gtk::SpinButton bank_entry;
	Gtk::SpinButton send_page_entry;
	Gtk::SpinButton plugin_page_entry;
	Gtk::ComboBoxText gainmode_combo;
	Gtk::ComboBoxText preset_combo;
	std::vector<std::string> preset_options;
	std::map<std::string,std::string> preset_files;
	bool preset_busy;
	void get_session ();
	void restore_sesn_values ();
	uint32_t sesn_portmode;
	std::string sesn_port;
	uint32_t sesn_bank;
	uint32_t sesn_send;
	uint32_t sesn_plugin;
	uint32_t sesn_strips;
	uint32_t sesn_feedback;
	uint32_t sesn_gainmode;
	void save_user ();
	void scan_preset_files ();
	void load_preset (std::string preset);

	void debug_changed ();
	void portmode_changed ();
	void gainmode_changed ();
	void clear_device ();
	void factory_reset ();
	void reshow_values ();
	void port_changed ();
	bool port_focus_out (GdkEventFocus*);
	void bank_changed ();
	void send_page_changed ();
	void plugin_page_changed ();
	void strips_changed ();
	void feedback_changed ();
	void preset_changed ();
	// Strip types calculator
	uint32_t def_strip;
	void calculate_strip_types ();
	Gtk::Label current_strip_types;
	Gtk::CheckButton audio_tracks;
	Gtk::CheckButton midi_tracks;
	Gtk::CheckButton audio_buses;
	Gtk::CheckButton foldback_busses;
	Gtk::CheckButton midi_buses;
	Gtk::CheckButton control_masters;
	Gtk::CheckButton master_type;
	Gtk::CheckButton monitor_type;
	Gtk::CheckButton selected_tracks;
	Gtk::CheckButton hidden_tracks;
	Gtk::CheckButton usegroups;
	int stvalue;
	// feedback calculator
	uint32_t def_feedback;
	void calculate_feedback ();
	Gtk::Label current_feedback;
	Gtk::CheckButton strip_buttons_button;
	Gtk::CheckButton strip_control_button;
	Gtk::CheckButton ssid_as_path;
	Gtk::CheckButton heart_beat;
	Gtk::CheckButton master_fb;
	Gtk::CheckButton bar_and_beat;
	Gtk::CheckButton smpte;
	Gtk::CheckButton meter_float;
	Gtk::CheckButton meter_led;
	Gtk::CheckButton signal_present;
	Gtk::CheckButton hp_samples;
	Gtk::CheckButton hp_min_sec;
	Gtk::CheckButton hp_gui;
	Gtk::CheckButton select_fb;
	Gtk::CheckButton use_osc10;
	int fbvalue;
	void set_bitsets ();



	OSC& cp;
};


void*
OSC::get_gui () const
{
	if (!gui) {
		const_cast<OSC*>(this)->build_gui ();
	}
	static_cast<Gtk::Notebook*>(gui)->show_all();
	return gui;
}

void
OSC::tear_down_gui ()
{
	if (gui) {
		Gtk::Widget *w = static_cast<Gtk::Notebook*>(gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete (OSC_GUI*) gui;
	gui = 0;
}

void
OSC::build_gui ()
{
	gui = (void*) new OSC_GUI (*this);
}

} // end namespace

#endif // osc_gui_h
