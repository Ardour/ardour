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

class OSC_GUI : public Gtk::VBox
{
	public:
		OSC_GUI (OSC&);
		~OSC_GUI ();

private:
	Gtk::ComboBoxText debug_combo;
	Gtk::ComboBoxText portmode_combo;
	Gtk::SpinButton port_entry;
	Gtk::SpinButton bank_entry;
	Gtk::SpinButton striptypes_spin; // dropdown would be nicer
	Gtk::SpinButton feedback_spin; // dropdown would be nicer
	Gtk::ComboBoxText gainmode_combo;
	void debug_changed ();
	void portmode_changed ();
	void gainmode_changed ();
	void clear_device ();
	void port_changed ();
	void bank_changed ();
	void strips_changed ();
	void feedback_changed ();
	OSC& cp;
};


void*
OSC::get_gui () const
{
	if (!gui) {
		const_cast<OSC*>(this)->build_gui ();
	}
	static_cast<Gtk::VBox*>(gui)->show_all();
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

	// show our url
	label = manage (new Gtk::Label(_("Connection:")));
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	label = manage (new Gtk::Label(cp.get_server_url()));
	table->attach (*label, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	++n;

	// show and set port to auto (default) or manual (one surface only)
	label = manage (new Gtk::Label(_("Port Mode:")));
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
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (bank_entry, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	bank_entry.set_range (0, 0xffff);
	bank_entry.set_increments (1, 8);
	bank_entry.set_value (cp.get_banksize());

	++n;

	// Default strip types
	label = manage (new Gtk::Label(_("Strip Types:")));
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (striptypes_spin, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	striptypes_spin.set_range (0, 0x3ff);
	striptypes_spin.set_increments (1, 10);
	striptypes_spin.set_value (cp.get_defaultstrip());

/*	std::vector<std::string> strip_options;
	strip_options.push_back (_("Audio Tracks"));
	strip_options.push_back (_("Midi Tracks"));
	strip_options.push_back (_("Audio Buses"));
	strip_options.push_back (_("Midi Buses"));
	strip_options.push_back (_("Control Masters"));
	strip_options.push_back (_("Master"));
	strip_options.push_back (_("Monitor"));
	strip_options.push_back (_("Selected"));
	strip_options.push_back (_("Hidden"));*/

	++n;

	// default feedback settings
	label = manage (new Gtk::Label(_("Feedback:")));
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (feedback_spin, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	feedback_spin.set_range (0, 0x3fff);
	feedback_spin.set_increments (1, 10);
	feedback_spin.set_value (cp.get_defaultfeedback());
	/*std::vector<std::string> feedback_options;
	feedback_options.push_back (_("Strip Buttons"));
	feedback_options.push_back (_("Strip Controls"));
	feedback_options.push_back (_("Use SSID as Path Extension"));
	feedback_options.push_back (_("Send heart beat"));
	feedback_options.push_back (_("Master and Transport Feedback"));
	feedback_options.push_back (_("Send Bar and Beat"));
	feedback_options.push_back (_("Send SMPTE Time"));
	feedback_options.push_back (_("Send metering"));
	feedback_options.push_back (_("Send  LED strip metering"));
	feedback_options.push_back (_("Signal Present"));
	feedback_options.push_back (_("Strip Controls"));
	feedback_options.push_back (_("Playhead Position as Samples"));
	feedback_options.push_back (_("Playhead Position as Minutes Seconds"));
	feedback_options.push_back (_("Playhead Position as per GUI Clock"));
	feedback_options.push_back (_("Extra Select Channel Controls"));*/

	++n;

	// Gain Mode
	label = manage (new Gtk::Label(_("Gain Mode:")));
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
	pack_start (*table, false, false);

	debug_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::debug_changed));
	portmode_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::portmode_changed));
	gainmode_combo.signal_changed().connect (sigc::mem_fun (*this, &OSC_GUI::gainmode_changed));
	button->signal_clicked().connect (sigc::mem_fun (*this, &OSC_GUI::clear_device));
	port_entry.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::port_changed));
	bank_entry.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::bank_changed));
	striptypes_spin.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::strips_changed));
	feedback_spin.signal_activate().connect (sigc::mem_fun (*this, &OSC_GUI::feedback_changed));
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
