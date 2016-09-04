/*
    Copyright (C) 2016 Robin Gareus <robin@gareus.org
    Copyright (C) 2009-2012 Paul Davis

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

#include <iostream>
#include <list>
#include <string>
#include <vector>
//#include <glibmm/miscutils.h>

#include <errno.h>

#include "pbd/file_utils.h"

#include <gtkmm/box.h>
#include <gtkmm/notebook.h>
#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/button.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/comboboxtext.h>

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "ardour/filesystem_paths.h"

#include "osc.h"
#include "osc_gui.h"

#include "pbd/i18n.h"

using namespace PBD;
using namespace ARDOUR;
using namespace Gtk;
using namespace Gtkmm2ext;
using namespace ArdourSurface;

OSC_GUI::OSC_GUI (OSC& p)
	: cp (p)
{
	int n = 0; // table row
	Table* table = manage (new Table);
	Label* label;
	Button* button;
	table->set_row_spacings (10);
	table->set_col_spacings (6);
	table->set_border_width (12);
	get_session ();
	preset_busy = true;

	// show our url
	label = manage (new Gtk::Label(_("Connection:")));
	label->set_alignment(1, .5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	label = manage (new Gtk::Label(cp.get_server_url()));
	table->attach (*label, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	++n;

	// show and set port to auto (default) or manual (one surface only)
	label = manage (new Gtk::Label(_("Port Mode:")));
	label->set_alignment(1, .5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (portmode_combo, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	std::vector<std::string> portmode_options;
	portmode_options.push_back (_("Auto"));
	portmode_options.push_back (_("Manual"));

	set_popdown_strings (portmode_combo, portmode_options);
	portmode_combo.set_active ((int)cp.get_portmode());
	++n;

	// port entry box
	label = manage (new Gtk::Label(_("Reply Manual Port:")));
	label->set_alignment(1, .5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (port_entry, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	port_entry.set_range(1024, 0xffff);
	port_entry.set_increments (1, 100);
	port_entry.set_text(cp.get_remote_port().c_str());
	if (!cp.get_portmode()) {
		port_entry.set_sensitive (false);
	}
	++n;

	// default banksize setting
	label = manage (new Gtk::Label(_("Bank Size:")));
	label->set_alignment(1, .5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (bank_entry, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	bank_entry.set_range (0, 0xffff);
	bank_entry.set_increments (1, 8);
	bank_entry.set_value (cp.get_banksize());

	++n;

	// Gain Mode
	label = manage (new Gtk::Label(_("Gain Mode:")));
	label->set_alignment(1, .5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (gainmode_combo, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	std::vector<std::string> gainmode_options;
	gainmode_options.push_back (_("dB"));
	gainmode_options.push_back (_("Position"));

	set_popdown_strings (gainmode_combo, gainmode_options);
	gainmode_combo.set_active ((int)cp.get_gainmode());
	++n;

	// debug setting
	label = manage (new Gtk::Label(_("Debug:")));
	label->set_alignment(1, .5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (debug_combo, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);

	std::vector<std::string> debug_options;
	debug_options.push_back (_("Off"));
	debug_options.push_back (_("Log invalid messages"));
	debug_options.push_back (_("Log all messages"));

	set_popdown_strings (debug_combo, debug_options);
	debug_combo.set_active ((int)cp.get_debug_mode());
	++n;

	// Preset loader combo
	label = manage (new Gtk::Label(_("Preset:")));
	label->set_alignment(1, .5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (preset_combo, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);

	preset_files.clear();
	// no files for these two
	preset_options.push_back (_("Last Loaded Session"));
	preset_options.push_back (_("Ardour Factory Setting"));
	// user is special it appears in menu even if no file is present
	preset_options.push_back ("User");
	preset_files["User"] = "";
	// scan for OSC .preset files
	scan_preset_files ();

	set_popdown_strings (preset_combo, preset_options);
	preset_combo.set_active (0);
	preset_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::preset_changed));
	++n;

	// refresh button
	button = manage (new Gtk::Button(_("Clear OSC Devices")));
	table->attach (*button, 0, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 10);

	table->show_all ();
	append_page (*table, _("OSC Setup"));

	debug_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::debug_changed));
	portmode_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::portmode_changed));
	gainmode_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::gainmode_changed));
	button->signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::clear_device));
	port_entry.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::port_changed));
	bank_entry.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::bank_changed));

	// Strip Types Calculate Page
	int stn = 0; // table row
	Table* sttable = manage (new Table);
	sttable->set_row_spacings (8);
	sttable->set_col_spacings (6);
	sttable->set_border_width (25);

	// show our url
	label = manage (new Gtk::Label(_("Select Desired Types of Tracks")));
	sttable->attach (*label, 0, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	++stn;

	label = manage (new Gtk::Label(_("Strip Types Value:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 15);
	calculate_strip_types ();
	current_strip_types.set_width_chars(10);
	sttable->attach (current_strip_types, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 15);
	++stn;

	label = manage (new Gtk::Label(_("Audio Tracks:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (audio_tracks, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Midi Tracks:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (midi_tracks, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Audio Busses:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (audio_buses, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Audio Auxes:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (audio_auxes, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Midi Busses:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (midi_buses, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Control Masters:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (control_masters, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Master (use /master instead):")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (master_type, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Monitor (use /monitor instead):")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (monitor_type, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Selected Tracks (use for selected tracks only):")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (selected_tracks, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Hidden Tracks:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (hidden_tracks, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;


	sttable->show_all ();
	append_page (*sttable, _("Default Strip Types"));


	// Feedback Calculate Page
	int fn = 0; // table row
	Table* fbtable = manage (new Table);
	fbtable->set_row_spacings (4);
	fbtable->set_col_spacings (6);
	fbtable->set_border_width (12);

	label = manage (new Gtk::Label(_("Select Desired Types of Feedback")));
	fbtable->attach (*label, 0, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	++fn;

	label = manage (new Gtk::Label(_("Feedback Value:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 15);
	calculate_feedback ();
	current_feedback.set_width_chars(10);
	fbtable->attach (current_feedback, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 15);
	++fn;

	label = manage (new Gtk::Label(_("Strip Buttons:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (strip_buttons_button, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Strip Controls:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (strip_control_button, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Use SSID as Path Extension:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (ssid_as_path, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Use Heart Beat:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (heart_beat, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Master Section:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (master_fb, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Play Head Position as Bar and Beat:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (bar_and_beat, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Play Head Position as SMPTE Time:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (smpte, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Metering as a Float:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (meter_float, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Metering as a LED Strip:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (meter_led, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Signal Present:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (signal_present, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Play Head Position as Samples:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (hp_samples, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Playhead Position as Minutes Seconds:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (hp_min_sec, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Playhead Position as per GUI Clock:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (hp_gui, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	hp_gui.set_sensitive (false); // we don't have this yet (Mixbus wants)
	++fn;

	label = manage (new Gtk::Label(_("Extra Select Only Feedback:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (select_fb, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	fbtable->show_all ();
	append_page (*fbtable, _("Default Feedback"));
	// set strips and feedback from loaded default values
	reshow_values ();
	// connect signals
	audio_tracks.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	midi_tracks.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	audio_buses.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	audio_auxes.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	midi_buses.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	control_masters.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	master_type.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	monitor_type.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	selected_tracks.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	hidden_tracks.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	strip_buttons_button.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	strip_control_button.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	ssid_as_path.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	heart_beat.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	master_fb.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	bar_and_beat.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	smpte.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	meter_float.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	meter_led.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	signal_present.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	hp_samples.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	hp_min_sec.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	hp_gui.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	select_fb.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::set_bitsets));
	preset_busy = false;

}

OSC_GUI::~OSC_GUI ()
{
}

// static directory and file handling stuff
static Searchpath
preset_search_path ()
{
	bool preset_path_defined = false;
        std::string spath_env (Glib::getenv (preset_env_variable_name, preset_path_defined));

	if (preset_path_defined) {
		return spath_env;
	}

	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(preset_dir_name);

	return spath;
}

static std::string
user_preset_directory ()
{
	return Glib::build_filename (user_config_directory(), preset_dir_name);
}

static bool
preset_filter (const std::string &str, void* /*arg*/)
{
	return (str.length() > strlen(preset_suffix) &&
		str.find (preset_suffix) == (str.length() - strlen (preset_suffix)));
}

static std::string
legalize_for_path (const std::string& str)
{
	std::string::size_type pos;
	std::string illegal_chars = "/\\"; /* DOS, POSIX. Yes, we're going to ignore HFS */
	std::string legal;

	legal = str;
	pos = 0;

	while ((pos = legal.find_first_of (illegal_chars, pos)) != std::string::npos) {
		legal.replace (pos, 1, "_");
		pos += 1;
	}

	return std::string (legal);
}

// end of static functions

void
OSC_GUI::debug_changed ()
{
	std::string str = debug_combo.get_active_text ();
	if (str == _("Off")) {
		cp.set_debug_mode (OSC::Off);
	}
	else if (str == _("Log invalid messages")) {
		cp.set_debug_mode (OSC::Unhandled);
	}
	else if (str == _("Log all messages")) {
		cp.set_debug_mode (OSC::All);
	}
	else {
		std::cerr << "Invalid OSC Debug Mode\n";
		assert (0);
	}
}

void
OSC_GUI::portmode_changed ()
{
	std::string str = portmode_combo.get_active_text ();
	if (str == _("Auto")) {
		cp.set_portmode (0);
		port_entry.set_sensitive (false);
	}
	else if (str == _("Manual")) {
		cp.set_portmode (1);
		port_entry.set_sensitive (true);
	}
	else {
		std::cerr << "Invalid OSC Port Mode\n";
		assert (0);
	}
	save_user ();
}

void
OSC_GUI::port_changed ()
{
	std::string str = port_entry.get_text ();
	if (port_entry.get_value() == 3819) {
		str = "8000";
		port_entry.set_value (8000);
	}
	cp.set_remote_port (str);
	save_user ();
}

void
OSC_GUI::bank_changed ()
{
	uint32_t bsize = bank_entry.get_value ();
	cp.set_banksize (bsize);
	save_user ();

}

void
OSC_GUI::gainmode_changed ()
{
	std::string str = gainmode_combo.get_active_text ();
	if (str == _("dB")) {
		cp.set_gainmode (0);
	}
	else if (str == _("Position")) {
		cp.set_gainmode (1);
	}
	else {
		std::cerr << "Invalid OSC Gain Mode\n";
		assert (0);
	}
	save_user ();
}

void
OSC_GUI::clear_device ()
{
	cp.clear_devices();
}

void
OSC_GUI::preset_changed ()
{
	preset_busy = true;
	std::string str = preset_combo.get_active_text ();
	if (str == "Last Loaded Session") {
		restore_sesn_values ();
	}
	else if (str == "Ardour Factory Setting") {
		factory_reset ();
	}
	else if (str == "User") {
		load_preset ("User");
	}
	else {
		load_preset (str);
	}
	preset_busy = false;
}

void
OSC_GUI::factory_reset ()
{
	cp.set_banksize (0);
	bank_entry.set_value (0);
	cp.set_defaultstrip (159);
	cp.set_defaultfeedback (0);
	reshow_values ();
	cp.set_gainmode (0);
	gainmode_combo.set_active (0);
	cp.set_portmode (0);
	portmode_combo.set_active (0);
	cp.set_remote_port ("8000");
	port_entry.set_value (8000);
	cp.clear_devices ();
	cp.gui_changed ();
}

void
OSC_GUI::reshow_values ()
{
	def_strip = cp.get_defaultstrip();
	audio_tracks.set_active(def_strip & 1);
	midi_tracks.set_active(def_strip & 2);
	audio_buses.set_active(def_strip & 4);
	midi_buses.set_active(def_strip & 8);
	control_masters.set_active(def_strip & 16);
	master_type.set_active(def_strip & 32);
	monitor_type.set_active(def_strip & 64);
	audio_auxes.set_active(def_strip & 128);
	selected_tracks.set_active(def_strip & 256);
	hidden_tracks.set_active(def_strip & 512);
	def_feedback = cp.get_defaultfeedback();
	strip_buttons_button.set_active(def_feedback & 1);
	strip_control_button.set_active(def_feedback & 2);
	ssid_as_path.set_active(def_feedback & 4);
	heart_beat.set_active(def_feedback & 8);
	master_fb.set_active(def_feedback & 16);
	bar_and_beat.set_active(def_feedback & 32);
	smpte.set_active(def_feedback & 64);
	meter_float.set_active(def_feedback & 128);
	meter_led.set_active(def_feedback & 256);
	signal_present.set_active(def_feedback & 512);
	hp_samples.set_active(def_feedback & 1024);
	hp_min_sec.set_active (def_feedback & 2048);
	//hp_gui.set_active (false); // we don't have this yet (Mixbus wants)
	select_fb.set_active(def_feedback & 8192);

	calculate_strip_types ();
	calculate_feedback ();
}

void
OSC_GUI::calculate_feedback ()
{
	fbvalue = 0;
	if (strip_buttons_button.get_active()) {
		fbvalue += 1;
	}
	if (strip_control_button.get_active()) {
		fbvalue += 2;
	}
	if (ssid_as_path.get_active()) {
		fbvalue += 4;
	}
	if (heart_beat.get_active()) {
		fbvalue += 8;
	}
	if (master_fb.get_active()) {
		fbvalue += 16;
	}
	if (bar_and_beat.get_active()) {
		fbvalue += 32;
	}
	if (smpte.get_active()) {
		fbvalue += 64;
	}
	if (meter_float.get_active()) {
		fbvalue += 128;
	}
	if (meter_led.get_active()) {
		fbvalue += 256;
	}
	if (signal_present.get_active()) {
		fbvalue += 512;
	}
	if (hp_samples.get_active()) {
		fbvalue += 1024;
	}
	if (hp_min_sec.get_active()) {
		fbvalue += 2048;
	}
	if (hp_gui.get_active()) {
		fbvalue += 4096;
	}
	if (select_fb.get_active()) {
		fbvalue += 8192;
	}

	current_feedback.set_text(string_compose("%1", fbvalue));
}

void
OSC_GUI::calculate_strip_types ()
{
	stvalue = 0;
	if (audio_tracks.get_active()) {
		stvalue += 1;
	}
	if (midi_tracks.get_active()) {
		stvalue += 2;
	}
	if (audio_buses.get_active()) {
		stvalue += 4;
	}
	if (midi_buses.get_active()) {
		stvalue += 8;
	}
	if (control_masters.get_active()) {
		stvalue += 16;
	}
	if (master_type.get_active()) {
		stvalue += 32;
	}
	if (monitor_type.get_active()) {
		stvalue += 64;
	}
	if (audio_auxes.get_active()) {
		stvalue += 128;
	}
	if (selected_tracks.get_active()) {
		stvalue += 256;
	}
	if (hidden_tracks.get_active()) {
		stvalue += 512;
	}

	current_strip_types.set_text(string_compose("%1", stvalue));
}

void
OSC_GUI::set_bitsets ()
{
	if (preset_busy) {
		return;
	}
	calculate_strip_types ();
	calculate_feedback ();
	cp.set_defaultstrip (stvalue);
	cp.set_defaultfeedback (fbvalue);
	save_user ();
}

void
OSC_GUI::scan_preset_files ()
{
	std::vector<std::string> presets;
	Searchpath spath (preset_search_path());

	find_files_matching_filter (presets, spath, preset_filter, 0, false, true);
	//device_profiles.clear ();preset_list.clear // first two entries already there

	if (presets.empty()) {
		error << "No OSC preset files found using " << spath.to_string() << endmsg;
		return;
	}

	for (std::vector<std::string>::iterator i = presets.begin(); i != presets.end(); ++i) {
		std::string fullpath = *i;
		//DeviceProfile dp; // has to be initial every loop or info from last added.

		XMLTree tree;

		if (!tree.read (fullpath.c_str())) {
			continue;
		}

		XMLNode* root = tree.root ();
		if (!root) {
			continue;
		}
		const XMLProperty* prop;
		const XMLNode* child;

		if (root->name() != "OSCPreset") {
			continue;
		}

		if ((child = root->child ("Name")) == 0 || (prop = child->property ("value")) == 0) {
			continue;
		} else {
			if (prop->value() == "User") {
				// We already added user but no file name
				preset_files[prop->value()] = fullpath;
			} else if (preset_files.find(prop->value()) == preset_files.end()) {
				preset_options.push_back (prop->value());
				preset_files[prop->value()] = fullpath;
			}
		}

	}
}

void
OSC_GUI::save_user ()
{
	if (preset_busy) {
		return;
	}
	std::string fullpath = user_preset_directory();

	if (g_mkdir_with_parents (fullpath.c_str(), 0755) < 0) {
		error << string_compose(_("Session: cannot create user MCP profile folder \"%1\" (%2)"), fullpath, strerror (errno)) << endmsg;
		return;
	}

	fullpath = Glib::build_filename (fullpath, string_compose ("%1%2", legalize_for_path ("user"), preset_suffix));

	XMLNode* node = new XMLNode ("OSCPreset");
	XMLNode* child = new XMLNode ("Name");

	child->set_property ("value", "User");
	node->add_child_nocopy (*child);

	child = new XMLNode ("PortMode");
	child->set_property ("value", cp.get_portmode());
	node->add_child_nocopy (*child);

	child = new XMLNode ("Remote-Port");
	child->set_property ("value", cp.get_remote_port());
	node->add_child_nocopy (*child);

	child = new XMLNode ("Bank-Size");
	child->set_property ("value", cp.get_banksize());
	node->add_child_nocopy (*child);

	child = new XMLNode ("Strip-Types");
	child->set_property ("value", cp.get_defaultstrip());
	node->add_child_nocopy (*child);

	child = new XMLNode ("Feedback");
	child->set_property ("value", cp.get_defaultfeedback());
	node->add_child_nocopy (*child);

	child = new XMLNode ("Gain-Mode");
	child->set_property ("value", cp.get_gainmode());
	node->add_child_nocopy (*child);

	XMLTree tree;
	tree.set_root (node);

	if (!tree.write (fullpath)) {
		error << string_compose ("MCP profile not saved to %1", fullpath) << endmsg;
	}
	preset_combo.set_active (2);
	cp.gui_changed();

}

void
OSC_GUI::load_preset (std::string preset)
{
	if (preset == "User" && preset_files["User"] == "") {
		restore_sesn_values ();
	} else if (preset_files.find(preset) != preset_files.end()) {
		XMLTree tree;

		if (!tree.read (preset_files[preset])) {
			std::cerr << "preset file not found " << preset_files[preset] << "\n";
			return;
		}

		XMLNode* root = tree.root ();
		if (!root) {
			std::cerr << "invalid preset file " << preset_files[preset] << "\n";
			return;
		}
		const XMLProperty* prop;
		const XMLNode* child;

		if (root->name() != "OSCPreset") {
			std::cerr << "invalid preset file " << preset_files[preset] << "\n";
			return;
		}

		if ((child = root->child ("Name")) == 0 || (prop = child->property ("value")) == 0) {
			std::cerr << "preset file missing Name " << preset_files[preset] << "\n";
			return;
		}
		if ((child = root->child ("PortMode")) == 0 || (prop = child->property ("value")) == 0) {
			cp.set_portmode (sesn_portmode);
			portmode_combo.set_active (sesn_portmode);
		} else {
			cp.set_portmode (atoi (prop->value().c_str()));
			portmode_combo.set_active (atoi (prop->value().c_str()));
		}
		if ((child = root->child ("Remote-Port")) == 0 || (prop = child->property ("value")) == 0) {
			cp.set_remote_port (sesn_port);
			port_entry.set_text (sesn_port);
		} else {
			cp.set_remote_port (prop->value());
			port_entry.set_text (prop->value());
		}
		if ((child = root->child ("Bank-Size")) == 0 || (prop = child->property ("value")) == 0) {
			cp.set_banksize (sesn_bank);
			bank_entry.set_value (sesn_bank);
		} else {
			cp.set_banksize (atoi (prop->value().c_str()));
			bank_entry.set_value (atoi (prop->value().c_str()));
		}
		if ((child = root->child ("Strip-Types")) == 0 || (prop = child->property ("value")) == 0) {
			cp.set_defaultstrip (sesn_strips);
		} else {
			cp.set_defaultstrip (atoi (prop->value().c_str()));
		}
		if ((child = root->child ("Feedback")) == 0 || (prop = child->property ("value")) == 0) {
			cp.set_defaultfeedback (sesn_feedback);
		} else {
			cp.set_defaultfeedback (atoi (prop->value().c_str()));
		}
		reshow_values (); // show strip types and feed back in GUI

		if ((child = root->child ("Gain-Mode")) == 0 || (prop = child->property ("value")) == 0) {
			cp.set_gainmode (sesn_gainmode);
			gainmode_combo.set_active (sesn_gainmode);
		} else {
			cp.set_gainmode (atoi (prop->value().c_str()));
			gainmode_combo.set_active (atoi (prop->value().c_str()));
		}
		cp.gui_changed();

	}
}

void
OSC_GUI::get_session ()
{
	sesn_portmode = cp.get_portmode ();
	sesn_port = cp.get_remote_port ();
	sesn_bank = cp.get_banksize ();
	sesn_strips = cp.get_defaultstrip ();
	sesn_feedback = cp.get_defaultfeedback ();
	sesn_gainmode = cp.get_gainmode ();
}

void
OSC_GUI::restore_sesn_values ()
{
	cp.set_portmode (sesn_portmode);
	portmode_combo.set_active (sesn_portmode);
	cp.set_remote_port (sesn_port);
	port_entry.set_text (sesn_port);
	cp.set_banksize (sesn_bank);
	bank_entry.set_value (sesn_bank);
	cp.set_defaultstrip (sesn_strips);
	cp.set_defaultfeedback (sesn_feedback);
	reshow_values ();
	cp.set_gainmode (sesn_gainmode);
	gainmode_combo.set_active (sesn_gainmode);
}
