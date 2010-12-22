/*
    Copyright (C) 2010 Paul Davis
    Author: Carl Hetherington <cth@carlh.net>

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
#include <gtkmm/table.h>
#include "program_change_dialog.h"

using namespace Gtk;

ProgramChangeDialog::ProgramChangeDialog ()
	: ArdourDialog (_("Add Program Change"), true)
	, _channel (*manage (new Adjustment (1, 1, 16, 1, 2)))
	, _program (*manage (new Adjustment (1, 1, 128, 1, 16)))
{
	Table* t = manage (new Table (2, 2));
	t->set_spacings (6);

	Label* l = manage (new Label (_("Channel")));
	l->set_alignment (0, 0.5);
	t->attach (*l, 0, 1, 0, 1);

	t->attach (_channel, 1, 2, 0, 1);

	l = manage (new Label (_("Program")));
	l->set_alignment (0, 0.5);
	t->attach (*l, 0, 1, 1, 2);
	t->attach (_program, 1, 2, 1, 2);

	get_vbox()->add (*t);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (Stock::ADD, RESPONSE_ACCEPT);
	set_default_response (RESPONSE_ACCEPT);

	show_all ();
}

/** @return Channel, counted from 0 */
uint8_t
ProgramChangeDialog::channel () const
{
	return _channel.get_value_as_int () - 1;
}

/** @return Program change number, counted from 0 */
uint8_t
ProgramChangeDialog::program () const
{
	return _program.get_value_as_int () - 1;
}
