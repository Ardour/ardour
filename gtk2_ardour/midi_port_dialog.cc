/*
    Copyright (C) 2012 Paul Davis 

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

#include <string>
#include <sigc++/bind.h>
#include <gtkmm/stock.h>

#include "pbd/convert.h"
#include <gtkmm2ext/utils.h>

#include "midi_port_dialog.h"

#include "i18n.h"

using namespace std;
using namespace PBD;
using namespace Gtk;
using namespace Gtkmm2ext;

static const char* mode_strings[] = { "duplex", "output", "input",  (char*) 0 };

MidiPortDialog::MidiPortDialog ()
	: ArdourDialog (_("Add MIDI Port"))
	, port_label (_("Port name:"))
{
	set_modal (true);
	set_skip_taskbar_hint (true);
	set_resizable (false);
	set_position (Gtk::WIN_POS_MOUSE);
	set_name (N_("MidiPortDialog"));

	vector<string> str = internationalize (PACKAGE, mode_strings);
	set_popdown_strings (port_mode_combo, str);
	port_mode_combo.set_active_text (str.front());

	hpacker.set_spacing (6);
	hpacker.set_border_width (5);

	hpacker.pack_start (port_label);
	hpacker.pack_start (port_name);
	hpacker.pack_start (port_mode_combo);

	get_vbox()->pack_start (hpacker);

	port_name.signal_activate().connect (sigc::mem_fun (*this, &MidiPortDialog::entry_activated));

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::ADD, RESPONSE_ACCEPT);

	show_all_children ();
}

void
MidiPortDialog::entry_activated ()
{
	response (RESPONSE_ACCEPT);
}

MidiPortDialog::~MidiPortDialog ()
{

}
