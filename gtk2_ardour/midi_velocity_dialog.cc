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

#include <gtkmm/stock.h>

#include "midi_velocity_dialog.h"

#include "i18n.h"

using namespace Gtk;

MidiVelocityDialog::MidiVelocityDialog (uint8_t current_velocity)
	: ArdourDialog (X_("Note Velocity"), true)
	, adjustment (current_velocity, 0, 127, 1, 16)
	, spinner (adjustment)
	, label (_("New velocity"))
{
	spinner.show ();
	label.show ();
	packer.show ();

	packer.pack_start (label, false, false);
	packer.pack_start (spinner, false, false);

	get_vbox()->pack_start (packer);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::OK, RESPONSE_OK);
	
	spinner.signal_activate().connect (sigc::bind (sigc::mem_fun (*this, &MidiVelocityDialog::response), Gtk::RESPONSE_OK));
}

uint8_t 
MidiVelocityDialog::velocity () const
{
	return (uint8_t) adjustment.get_value();
}
