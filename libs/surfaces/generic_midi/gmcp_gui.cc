/*
 * Copyright (C) 2009-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2012 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2014-2018 Robin Gareus <robin@gareus.org>
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

#include <iostream>
#include <list>
#include <string>
#include <vector>
#include <algorithm>

#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>
#include <gtkmm/liststore.h>

#include "pbd/unwind.h"

#include "ardour/audioengine.h"
#include "ardour/port.h"
#include "ardour/midi_port.h"

#include "gtkmm2ext/gtk_ui.h"
#include "gtkmm2ext/gui_thread.h"
#include "gtkmm2ext/utils.h"

#include "generic_midi_control_protocol.h"

#include "pbd/i18n.h"

class GMCPGUI : public Gtk::VBox
{
public:
	GMCPGUI (GenericMidiControlProtocol&);
	~GMCPGUI ();

private:
	GenericMidiControlProtocol& cp;
	Gtk::ComboBoxText map_combo;
	Gtk::Adjustment bank_adjustment;
	Gtk::SpinButton bank_spinner;
	Gtk::CheckButton feedback_enable;
	Gtk::CheckButton motorised_button;
	Gtk::Adjustment threshold_adjustment;
	Gtk::SpinButton threshold_spinner;

	Gtk::ComboBox input_combo;
	Gtk::ComboBox output_combo;

	void binding_changed ();
	void bank_changed ();
	void motorised_changed ();
	void threshold_changed ();
	void toggle_feedback_enable ();

	void update_port_combos ();
	PBD::ScopedConnectionList _port_connections;
	void connection_handler ();

	struct MidiPortColumns : public Gtk::TreeModel::ColumnRecord {
		MidiPortColumns() {
			add (short_name);
			add (full_name);
		}
		Gtk::TreeModelColumn<std::string> short_name;
		Gtk::TreeModelColumn<std::string> full_name;
	};

	MidiPortColumns midi_port_columns;
	bool ignore_active_change;

	Glib::RefPtr<Gtk::ListStore> build_midi_port_list (std::vector<std::string> const & ports, bool for_input);
	void active_port_changed (Gtk::ComboBox*,bool for_input);
};

using namespace PBD;
using namespace ARDOUR;
using namespace std;
using namespace Gtk;
using namespace Gtkmm2ext;

void*
GenericMidiControlProtocol::get_gui () const
{
	if (!gui) {
		const_cast<GenericMidiControlProtocol*>(this)->build_gui ();
	}
	static_cast<Gtk::VBox*>(gui)->show_all();
	return gui;
}

void
GenericMidiControlProtocol::tear_down_gui ()
{
	if (gui) {
		Gtk::Widget *w = static_cast<Gtk::VBox*>(gui)->get_parent();
		if (w) {
			w->hide();
			delete w;
		}
	}
	delete (GMCPGUI*) gui;
	gui = 0;
}

void
GenericMidiControlProtocol::build_gui ()
{
	gui = (void*) new GMCPGUI (*this);
}

/*--------------------*/

GMCPGUI::GMCPGUI (GenericMidiControlProtocol& p)
	: cp (p)
	, bank_adjustment (1, 1, 100, 1, 10)
	, bank_spinner (bank_adjustment)
	, feedback_enable (_("Enable Feedback"))
	, motorised_button (_("Motorised"))
	, threshold_adjustment (p.threshold(), 1, 127, 1, 10)
	, threshold_spinner (threshold_adjustment)
	, ignore_active_change (false)
{
	vector<string> popdowns;

	for (list<GenericMidiControlProtocol::MapInfo>::iterator x = cp.map_info.begin(); x != cp.map_info.end(); ++x) {
		popdowns.push_back (x->name);
	}

	sort (popdowns.begin(), popdowns.end(), less<string>());

	popdowns.insert (popdowns.begin(), _("Reset All"));
	popdowns.insert (popdowns.begin(), _("Drop Bindings"));

	set_popdown_strings (map_combo, popdowns);

	if (cp.current_binding().empty()) {
		map_combo.set_active_text (popdowns[0]);
	} else {
		map_combo.set_active_text (cp.current_binding());
	}

	map_combo.signal_changed().connect (sigc::mem_fun (*this, &GMCPGUI::binding_changed));

	set_spacing (6);
	set_border_width (6);

	Table* table = manage (new Table);
	table->set_row_spacings (6);
	table->set_col_spacings (6);
	table->show ();

	int n = 0;

	// MIDI input and output selectors
	input_combo.pack_start (midi_port_columns.short_name);
	output_combo.pack_start (midi_port_columns.short_name);

	input_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &GMCPGUI::active_port_changed), &input_combo, true));
	output_combo.signal_changed().connect (sigc::bind (sigc::mem_fun (*this, &GMCPGUI::active_port_changed), &output_combo, false));

	Label* label = manage (new Gtk::Label);
	label->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Incoming MIDI on:")));
	label->set_alignment (1.0, 0.5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (input_combo, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	n++;

	label = manage (new Gtk::Label);
	label->set_markup (string_compose ("<span weight=\"bold\">%1</span>", _("Outgoing MIDI on:")));
	label->set_alignment (1.0, 0.5);
	table->attach (*label, 0, 1, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0));
	table->attach (output_combo, 1, 2, n, n+1, AttachOptions(FILL|EXPAND), AttachOptions(0), 0, 0);
	n++;

	//MIDI binding file selector...
	label = manage (new Label (_("MIDI Bindings:")));
	label->set_alignment (0, 0.5);
	table->attach (*label, 0, 1, n, n + 1);
	table->attach (map_combo, 1, 2, n, n + 1);
	++n;

	map_combo.show ();
	label->show ();

	bank_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &GMCPGUI::bank_changed));

	label = manage (new Label (_("Current Bank:")));
	label->set_alignment (0, 0.5);
	table->attach (*label, 0, 1, n, n + 1);
	table->attach (bank_spinner, 1, 2, n, n + 1);
	++n;

	bank_spinner.show ();
	label->show ();

	feedback_enable.signal_toggled().connect (sigc::mem_fun (*this, &GMCPGUI::toggle_feedback_enable));
	table->attach (feedback_enable, 0, 2, n, n + 1);
	++n;
	feedback_enable.show ();
	feedback_enable.set_active (p.get_feedback ());

	motorised_button.signal_toggled().connect (sigc::mem_fun (*this, &GMCPGUI::motorised_changed));
	table->attach (motorised_button, 0, 2, n, n + 1);
	++n;

	motorised_button.show ();
	motorised_button.set_active (p.motorised ());

	threshold_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &GMCPGUI::threshold_changed));

	Gtkmm2ext::UI::instance()->set_tip (threshold_spinner,
					    string_compose (_("Controls how %1 behaves if the MIDI controller sends discontinuous values"), PROGRAM_NAME));

	label = manage (new Label (_("Smoothing:")));
	label->set_alignment (0, 0.5);
	table->attach (*label, 0, 1, n, n + 1);
	table->attach (threshold_spinner, 1, 2, n, n + 1);
	++n;

	threshold_spinner.show ();
	label->show ();

	pack_start (*table, false, false);

	binding_changed ();

	/* update the port connection combos */

	update_port_combos ();

	/* catch future changes to connection state */
	ARDOUR::AudioEngine::instance()->PortRegisteredOrUnregistered.connect (_port_connections, invalidator (*this), boost::bind (&GMCPGUI::connection_handler, this), gui_context());
	ARDOUR::AudioEngine::instance()->PortPrettyNameChanged.connect (_port_connections, invalidator (*this), boost::bind (&GMCPGUI::connection_handler, this), gui_context());
	cp.ConnectionChange.connect (_port_connections, invalidator (*this), boost::bind (&GMCPGUI::connection_handler, this), gui_context());
}

GMCPGUI::~GMCPGUI ()
{
}

void
GMCPGUI::bank_changed ()
{
	int new_bank = bank_adjustment.get_value() - 1;
	cp.set_current_bank (new_bank);
}

void
GMCPGUI::binding_changed ()
{
	string str = map_combo.get_active_text ();

	if (str == _("Reset All")) {
		cp.drop_all ();
	} else if (str == _("Drop Bindings")) {
		cp.drop_bindings ();
	} else {
		for (list<GenericMidiControlProtocol::MapInfo>::iterator x = cp.map_info.begin(); x != cp.map_info.end(); ++x) {
			if (str == x->name) {
				cp.load_bindings (x->path);
				motorised_button.set_active (cp.motorised ());
				threshold_adjustment.set_value (cp.threshold ());
				break;
			}
		}
	}
}

void
GMCPGUI::toggle_feedback_enable ()
{
	cp.set_feedback (feedback_enable.get_active ());
}

void
GMCPGUI::motorised_changed ()
{
	cp.set_motorised (motorised_button.get_active ());
}

void
GMCPGUI::threshold_changed ()
{
	cp.set_threshold (threshold_adjustment.get_value());
}

void
GMCPGUI::connection_handler ()
{
	/* ignore all changes to combobox active strings here, because we're
	   updating them to match a new ("external") reality - we were called
	   because port connections have changed.
	*/

	PBD::Unwinder<bool> ici (ignore_active_change, true);

	update_port_combos ();
}

void
GMCPGUI::update_port_combos ()
{
	vector<string> midi_inputs;
	vector<string> midi_outputs;

	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsOutput|ARDOUR::IsTerminal), midi_inputs);
	ARDOUR::AudioEngine::instance()->get_ports ("", ARDOUR::DataType::MIDI, ARDOUR::PortFlags (ARDOUR::IsInput|ARDOUR::IsTerminal), midi_outputs);

	Glib::RefPtr<Gtk::ListStore> input = build_midi_port_list (midi_inputs, true);
	Glib::RefPtr<Gtk::ListStore> output = build_midi_port_list (midi_outputs, false);
	bool input_found = false;
	bool output_found = false;
	int n;

	input_combo.set_model (input);
	output_combo.set_model (output);

	Gtk::TreeModel::Children children = input->children();
	Gtk::TreeModel::Children::iterator i;
	i = children.begin();
	++i; /* skip "Disconnected" */


	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (cp.input_port()->connected_to (port_name)) {
			input_combo.set_active (n);
			input_found = true;
			break;
		}
	}

	if (!input_found) {
		input_combo.set_active (0); /* disconnected */
	}

	children = output->children();
	i = children.begin();
	++i; /* skip "Disconnected" */

	for (n = 1;  i != children.end(); ++i, ++n) {
		string port_name = (*i)[midi_port_columns.full_name];
		if (cp.output_port()->connected_to (port_name)) {
			output_combo.set_active (n);
			output_found = true;
			break;
		}
	}

	if (!output_found) {
		output_combo.set_active (0); /* disconnected */
	}
}

Glib::RefPtr<Gtk::ListStore>
GMCPGUI::build_midi_port_list (vector<string> const & ports, bool for_input)
{
	Glib::RefPtr<Gtk::ListStore> store = ListStore::create (midi_port_columns);
	TreeModel::Row row;

	row = *store->append ();
	row[midi_port_columns.full_name] = string();
	row[midi_port_columns.short_name] = _("Disconnected");

	for (vector<string>::const_iterator p = ports.begin(); p != ports.end(); ++p) {
		row = *store->append ();
		row[midi_port_columns.full_name] = *p;
		std::string pn = ARDOUR::AudioEngine::instance()->get_pretty_name_by_name (*p);
		if (pn.empty ()) {
			pn = (*p).substr ((*p).find (':') + 1);
		}
		row[midi_port_columns.short_name] = pn;
	}

	return store;
}

void
GMCPGUI::active_port_changed (Gtk::ComboBox* combo, bool for_input)
{
	if (ignore_active_change) {
		return;
	}

	TreeModel::iterator active = combo->get_active ();
	string new_port = (*active)[midi_port_columns.full_name];

	if (new_port.empty()) {
		if (for_input) {
			cp.input_port()->disconnect_all ();
		} else {
			cp.output_port()->disconnect_all ();
		}

		return;
	}

	if (for_input) {
		if (!cp.input_port()->connected_to (new_port)) {
			cp.input_port()->disconnect_all ();
			cp.input_port()->connect (new_port);
		}
	} else {
		if (!cp.output_port()->connected_to (new_port)) {
			cp.output_port()->disconnect_all ();
			cp.output_port()->connect (new_port);
		}
	}
}
