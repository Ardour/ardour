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

#include "osc.h"

#include "pbd/i18n.h"

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
	Gtk::SpinButton striptypes_spin;
	Gtk::SpinButton feedback_spin;
	Gtk::ComboBoxText gainmode_combo;
	void debug_changed ();
	void portmode_changed ();
	void gainmode_changed ();
	void clear_device ();
	void port_changed ();
	void bank_changed ();
	void strips_changed ();
	void feedback_changed ();
	// Strip types calculator
	void calculate_strip_types ();
	void push_strip_types ();
	Gtk::Label current_strip_types;
	Gtk::CheckButton audio_tracks;
	Gtk::CheckButton midi_tracks;
	Gtk::CheckButton audio_buses;
	Gtk::CheckButton midi_buses;
	Gtk::CheckButton control_masters;
	Gtk::CheckButton master_type;
	Gtk::CheckButton monitor_type;
	Gtk::CheckButton selected_tracks;
	Gtk::CheckButton hidden_tracks;
	int stvalue;
	// feedback calculator
	void calculate_feedback ();
	void push_feedback ();
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
	int fbvalue;
	OSC& cp;
};


void*
OSC::get_gui () const
{
	if (!gui) {
		const_cast<OSC*>(this)->build_gui ();
	}
	//static_cast<Gtk::VBox*>(gui)->show_all();
	static_cast<Gtk::Notebook*>(gui)->show_all();
	return gui;
}

void
OSC::tear_down_gui ()
{
	if (gui) {
		Gtk::Widget *w = static_cast<Gtk::VBox*>(gui)->get_parent();
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

///////////////////////////////////////////////////////////////////////////////

using namespace PBD;
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
	Button* fbbutton;
	Button* stbutton;
	table->set_row_spacings (4);
	table->set_col_spacings (6);
	table->set_border_width (12);

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
	label = manage (new Gtk::Label(_("Manual Port:")));
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

	// Default strip types
	label = manage (new Gtk::Label(_("Strip Types:")));
	label->set_alignment(1, .5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (striptypes_spin, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	striptypes_spin.set_range (0, 0x3ff);
	striptypes_spin.set_increments (1, 10);
	striptypes_spin.set_value (cp.get_defaultstrip());

	++n;

	// default feedback settings
	label = manage (new Gtk::Label(_("Feedback:")));
	label->set_alignment(1, .5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (feedback_spin, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	feedback_spin.set_range (0, 0x3fff);
	feedback_spin.set_increments (1, 10);
	feedback_spin.set_value (cp.get_defaultfeedback());
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

	// refresh button
	button = manage (new Gtk::Button(_("Clear OSC Devices")));
	table->attach (*button, 0, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));

	table->show_all ();
	append_page (*table, _("OSC Setup"));

	debug_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::debug_changed));
	portmode_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::portmode_changed));
	gainmode_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::gainmode_changed));
	button->signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::clear_device));
	port_entry.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::port_changed));
	bank_entry.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::bank_changed));
	striptypes_spin.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::strips_changed));
	feedback_spin.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::feedback_changed));


	// Strip Types Calculate Page
	int stn = 0; // table row
	Table* sttable = manage (new Table);
	sttable->set_row_spacings (4);
	sttable->set_col_spacings (6);
	sttable->set_border_width (12);

	// show our url
	label = manage (new Gtk::Label(_("Select Desired Types of Tracks")));
	sttable->attach (*label, 0, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	++stn;

	label = manage (new Gtk::Label(_("Strip Types Value:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	calculate_strip_types ();
	current_strip_types.set_width_chars(10);
	sttable->attach (current_strip_types, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++stn;

	label = manage (new Gtk::Label(_("Audio Tracks:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (audio_tracks, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	audio_tracks.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_strip_types));
	++stn;

	label = manage (new Gtk::Label(_("Midi Tracks:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (midi_tracks, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	midi_tracks.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_strip_types));
	++stn;

	label = manage (new Gtk::Label(_("Audio Buses:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (audio_buses, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	audio_buses.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_strip_types));
	++stn;

	label = manage (new Gtk::Label(_("Midi Buses:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (midi_buses, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	midi_buses.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_strip_types));
	++stn;

	label = manage (new Gtk::Label(_("Control Masters:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (control_masters, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	control_masters.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_strip_types));
	++stn;

	label = manage (new Gtk::Label(_("Master (use /master instead):")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (master_type, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	master_type.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_strip_types));
	++stn;

	label = manage (new Gtk::Label(_("Monitor (use /monitor instead):")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (monitor_type, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	monitor_type.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_strip_types));
	++stn;

	label = manage (new Gtk::Label(_("Selected Tracks (use for selected tracks only):")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (selected_tracks, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	selected_tracks.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_strip_types));
	++stn;

	label = manage (new Gtk::Label(_("Hidden Tracks:")));
	label->set_alignment(1, .5);
	sttable->attach (*label, 0, 1, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	sttable->attach (hidden_tracks, 1, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	hidden_tracks.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_strip_types));
	++stn;

	stbutton = manage (new Gtk::Button(_("Use Value as Strip Types Default")));
	sttable->attach (*stbutton, 0, 2, stn, stn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	stbutton->signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::push_strip_types));


	sttable->show_all ();
	append_page (*sttable, _("Calculate Strip Types"));


	// Feedback Calculate Page
	int fn = 0; // table row
	Table* fbtable = manage (new Table);
	fbtable->set_row_spacings (4);
	fbtable->set_col_spacings (6);
	fbtable->set_border_width (12);

	// show our url
	label = manage (new Gtk::Label(_("Select Desired Types of Feedback")));
	fbtable->attach (*label, 0, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	++fn;

	label = manage (new Gtk::Label(_("Feedback Value:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	calculate_feedback ();
	current_feedback.set_width_chars(10);
	fbtable->attach (current_feedback, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	++fn;

	label = manage (new Gtk::Label(_("Strip Buttons:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (strip_buttons_button, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	strip_buttons_button.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Strip Controls:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (strip_control_button, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	strip_control_button.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Use SSID as Path Extension:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (ssid_as_path, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	ssid_as_path.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Use Heart Beat:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (heart_beat, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	heart_beat.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Master Section:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (master_fb, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	master_fb.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Play Head Position as Bar and Beat:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (bar_and_beat, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	bar_and_beat.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Play Head Position as SMPTE Time:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (smpte, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	smpte.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Metering as a Float:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (meter_float, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	meter_float.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Metering as a LED Strip:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (meter_led, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	meter_led.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Signal Present:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (signal_present, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	signal_present.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Play Head Position as Samples:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (hp_samples, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	hp_samples.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Playhead Position as Minutes Seconds:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (hp_min_sec, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	hp_min_sec.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	label = manage (new Gtk::Label(_("Playhead Position as per GUI Clock:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (hp_gui, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	hp_gui.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	hp_gui.set_sensitive (false); // we don't have this yet (Mixbus wants)
	++fn;

	label = manage (new Gtk::Label(_("Extra Select Only Feedback:")));
	label->set_alignment(1, .5);
	fbtable->attach (*label, 0, 1, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbtable->attach (select_fb, 1, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	select_fb.signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::calculate_feedback));
	++fn;

	fbbutton = manage (new Gtk::Button(_("Use Value as Feedback Default")));
	fbtable->attach (*fbbutton, 0, 2, fn, fn+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	fbbutton->signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::push_feedback));


	fbtable->show_all ();
	append_page (*fbtable, _("Calculate Feedback"));

}

OSC_GUI::~OSC_GUI ()
{
}

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
}

void
OSC_GUI::bank_changed ()
{
	uint32_t bsize = bank_entry.get_value ();
	cp.set_banksize (bsize);

}

void
OSC_GUI::strips_changed ()
{
	uint32_t st = striptypes_spin.get_value ();
	cp.set_defaultstrip (st);
}

void
OSC_GUI::feedback_changed ()
{
	uint32_t fb = feedback_spin.get_value ();
	cp.set_defaultfeedback (fb);
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
}

void
OSC_GUI::clear_device ()
{
	cp.clear_devices();
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
OSC_GUI::push_feedback ()
{
	feedback_spin.set_value (fbvalue);
	feedback_changed ();
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
	/*if (Auditioner_type.get_active()) {
		stvalue += 128; // this one has no user accessable controls
	}*/
	if (selected_tracks.get_active()) {
		stvalue += 256;
	}
	if (hidden_tracks.get_active()) {
		stvalue += 512;
	}

	current_strip_types.set_text(string_compose("%1", stvalue));
}

void
OSC_GUI::push_strip_types ()
{
	striptypes_spin.set_value (stvalue);
	strips_changed ();
}
