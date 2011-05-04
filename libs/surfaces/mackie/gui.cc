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
	
	MackieControlProtocol& _cp;
	Gtk::ComboBoxText _surface_combo;
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
	Gtk::HBox* hbox = Gtk::manage (new Gtk::HBox);
	hbox->set_spacing (4);
	hbox->pack_start (*manage (new Gtk::Label (_("Surface to support:"))));
	hbox->pack_start (_surface_combo);

	vector<string> surfaces;
	surfaces.push_back (_("Mackie Control"));
	surfaces.push_back (_("Behringer BCF2000"));
	Gtkmm2ext::set_popdown_strings (_surface_combo, surfaces);

	if (ARDOUR::Config->get_mackie_emulation () == X_("mcu")) {
		_surface_combo.set_active_text (surfaces.front ());
	} else {
		_surface_combo.set_active_text (surfaces.back ());
	}

	pack_start (*hbox);

	show_all ();

	_surface_combo.signal_changed().connect (sigc::mem_fun (*this, &MackieControlProtocolGUI::surface_combo_changed));
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
