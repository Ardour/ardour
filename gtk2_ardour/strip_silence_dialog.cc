/*
    Copyright (C) 2009 Paul Davis 

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

#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>
#include "strip_silence_dialog.h"
#include "i18n.h"

/** Construct Strip silence dialog box */
StripSilenceDialog::StripSilenceDialog ()
	: ArdourDialog (_("Strip silence"))
{
	Gtk::Table* table = Gtk::manage (new Gtk::Table (4, 3));
	table->set_spacings (4);

	Gtk::Label* l = Gtk::manage (new Gtk::Label (_("Threshold:")));
	l->set_alignment (1, 0.5);
	table->attach (*l, 0, 1, 0, 1, Gtk::FILL, Gtk::FILL);
	_threshold.set_digits (1);
	_threshold.set_increments (1, 10);
	_threshold.set_range (-120, 0);
	_threshold.set_value (-60);
	table->attach (_threshold, 1, 2, 0, 1, Gtk::FILL, Gtk::FILL);
	l = Gtk::manage (new Gtk::Label (_("dBFS")));
	l->set_alignment (0, 0.5);
	table->attach (*l, 2, 3, 0, 1, Gtk::FILL, Gtk::FILL);

	l = Gtk::manage (new Gtk::Label (_("Minimum length:")));
	l->set_alignment (1, 0.5);
	table->attach (*l, 0, 1, 1, 2, Gtk::FILL, Gtk::FILL);
	_minimum_length.set_digits (0);
	_minimum_length.set_increments (1, 10);
	_minimum_length.set_range (0, 65536);
	_minimum_length.set_value (256);
	table->attach (_minimum_length, 1, 2, 1, 2, Gtk::FILL, Gtk::FILL);
	l = Gtk::manage (new Gtk::Label (_("samples")));
	table->attach (*l, 2, 3, 1, 2, Gtk::FILL, Gtk::FILL);

	l = Gtk::manage (new Gtk::Label (_("Fade length:")));
	l->set_alignment (1, 0.5);
	table->attach (*l, 0, 1, 2, 3, Gtk::FILL, Gtk::FILL);
	_fade_length.set_digits (0);
	_fade_length.set_increments (1, 10);
	_fade_length.set_range (0, 1024);
	_fade_length.set_value (64);
	table->attach (_fade_length, 1, 2, 2, 3, Gtk::FILL, Gtk::FILL);
	l = Gtk::manage (new Gtk::Label (_("samples")));
	table->attach (*l, 2, 3, 2, 3, Gtk::FILL, Gtk::FILL);

	get_vbox()->add (*table);

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_OK);

	show_all ();
}
