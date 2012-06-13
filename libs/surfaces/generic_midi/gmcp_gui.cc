/*
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

#include <gtkmm/comboboxtext.h>
#include <gtkmm/label.h>
#include <gtkmm/box.h>
#include <gtkmm/adjustment.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>

#include "gtkmm2ext/utils.h"

#include "generic_midi_control_protocol.h"

#include "i18n.h"

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
	Gtk::CheckButton motorised_button;
	Gtk::Adjustment threshold_adjustment;
	Gtk::SpinButton threshold_spinner;

	void binding_changed ();
	void bank_changed ();
	void motorised_changed ();
	void threshold_changed ();
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
	return gui;
}

void
GenericMidiControlProtocol::tear_down_gui ()
{
	delete (GMCPGUI*) gui;
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
	, motorised_button ("Motorised")
	, threshold_adjustment (1, 1, 127, 1, 10)
	, threshold_spinner (threshold_adjustment)
{
	vector<string> popdowns;
	popdowns.push_back (_("Reset All"));

	for (list<GenericMidiControlProtocol::MapInfo>::iterator x = cp.map_info.begin(); x != cp.map_info.end(); ++x) {
		popdowns.push_back (x->name);
	}

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

	Label* label = manage (new Label (_("MIDI Bindings:")));
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

	motorised_button.signal_toggled().connect (sigc::mem_fun (*this, &GMCPGUI::motorised_changed));
	table->attach (motorised_button, 0, 2, n, n + 1);
	++n;

	motorised_button.show ();

	threshold_adjustment.signal_value_changed().connect (sigc::mem_fun (*this, &GMCPGUI::threshold_changed));

	label = manage (new Label (_("Threshold:")));
	label->set_alignment (0, 0.5);
	table->attach (*label, 0, 1, n, n + 1);
	table->attach (threshold_spinner, 1, 2, n, n + 1);
	++n;

	threshold_spinner.show ();
	label->show ();

	pack_start (*table, false, false);

	binding_changed ();
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
GMCPGUI::motorised_changed ()
{
	cp.set_motorised (motorised_button.get_active ());
}

void
GMCPGUI::threshold_changed ()
{
	cp.set_threshold (threshold_adjustment.get_value());
}
