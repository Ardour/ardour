/*
	Copyright (C) 2010 Paul Davis

	This program is free software; you can redistribute it and/or modify
	it under the terms of the GNU General Public License as published by
	the Free Software Foundation; either version 2 of the License, or
	(at your option) any later version.

	This program is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
	GNU General Public License for more details.

	You should have received a copy of the GNU General Public License
	along with this program; if not, write to the Free Software
	Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <gtkmm/comboboxtext.h>
#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/table.h>
#include "gtkmm2ext/utils.h"
#include "ardour/rc_configuration.h"
#include "mackie_control_protocol.h"
#include "i18n.h"

using namespace std;

class MackieControlProtocolGUI : public Gtk::VBox
{
public:
	MackieControlProtocolGUI (MackieControlProtocol &);

private:

	void surface_combo_changed ();
	void extenders_changed ();
	
	MackieControlProtocol& _cp;
	Gtk::ComboBoxText _surface_combo;
	Gtk::SpinButton _extenders;
};

void*
MackieControlProtocol::get_gui () const
{
	if (!_gui) {
		const_cast<MackieControlProtocol*>(this)->build_gui ();
	}

	return _gui;
}

void
MackieControlProtocol::tear_down_gui ()
{
	delete (MackieControlProtocolGUI*) _gui;
}

void
MackieControlProtocol::build_gui ()
{
	_gui = (void *) new MackieControlProtocolGUI (*this);
}

MackieControlProtocolGUI::MackieControlProtocolGUI (MackieControlProtocol& p)
	: _cp (p)
{
	Gtk::Table* table = Gtk::manage (new Gtk::Table (2, 2));
	table->set_spacings (4);
	
	table->attach (*manage (new Gtk::Label (_("Surface type:"))), 0, 1, 0, 1);
	table->attach (_surface_combo, 1, 2, 0, 1);

	vector<string> surfaces = p.get_possible_devices ();
	Gtkmm2ext::set_popdown_strings (_surface_combo, surfaces);
	_surface_combo.set_active_text (p.device_name());

	_extenders.set_range (0, 8);
	_extenders.set_increments (1, 4);

	Gtk::Label* l = manage (new Gtk::Label (_("Extenders:")));
	l->set_alignment (0, 0.5);
	table->attach (*l, 0, 1, 1, 2);
	table->attach (_extenders, 1, 2, 1, 2);

	pack_start (*table);

	Gtk::Label* cop_out = manage (new Gtk::Label (_("<i>You must restart Ardour for changes\nto these settings to take effect.</i>")));
	cop_out->set_use_markup (true);
	pack_start (*cop_out);

	set_spacing (4);
	show_all ();

	_surface_combo.signal_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::surface_combo_changed));
	_extenders.signal_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::extenders_changed));
}

void
MackieControlProtocolGUI::surface_combo_changed ()
{
	if (_surface_combo.get_active_text() == _("Mackie Control")) {
		ARDOUR::Config->set_mackie_emulation (X_("mcu"));
	} else {
		ARDOUR::Config->set_mackie_emulation (X_("bcf"));
	}
}

void
MackieControlProtocolGUI::extenders_changed ()
{
	ARDOUR::Config->set_mackie_extenders (_extenders.get_value_as_int ());
}
