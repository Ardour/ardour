/*
    Copyright (C) 2010 Paul Davis

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

#include <gtkmm/label.h>
#include <gtkmm/spinbutton.h>
#include <gtkmm/radiobutton.h>
#include <gtkmm/stock.h>
#include <gtkmm/progressbar.h>
#include "normalize_dialog.h"

using namespace Gtk;

double NormalizeDialog::_last_normalization_value = 0;

NormalizeDialog::NormalizeDialog (bool more_than_one)
	: ArdourDialog (more_than_one ? _("Normalize regions") : _("Normalize region"))
	, _normalize_individually (0)
{
	get_vbox()->set_spacing (12);
	
	HBox* hbox = manage (new HBox);
	hbox->set_spacing (6);
	hbox->set_border_width (6);
	hbox->pack_start (*manage (new Label (_("Normalize to:"))), false, false);
	_spin = manage (new SpinButton (0.2, 2));
	_spin->set_range (-112, 0);
	_spin->set_increments (0.1, 1);
	_spin->set_value (_last_normalization_value);
	hbox->pack_start (*_spin, false, false);
	hbox->pack_start (*manage (new Label (_("dbFS"))), false, false);
	get_vbox()->pack_start (*hbox);

	if (more_than_one) {
		RadioButtonGroup group;
		VBox* vbox = manage (new VBox);

		_normalize_individually = manage (new RadioButton (group, _("Normalize each region using its own peak value")));
		vbox->pack_start (*_normalize_individually);
		RadioButton* b = manage (new RadioButton (group, _("Normalize each region using the peak value of all regions")));
		vbox->pack_start (*b);

		get_vbox()->pack_start (*vbox);

		_progress_bar = manage (new ProgressBar);
		get_vbox()->pack_start (*_progress_bar);
	}

	show_all ();
	
	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (_("Normalize"), RESPONSE_ACCEPT);
}

bool
NormalizeDialog::normalize_individually () const
{
	if (_normalize_individually == 0) {
		return false;
	}

	return _normalize_individually->get_active ();
}

double
NormalizeDialog::target () const
{
	return _spin->get_value ();
}

void
NormalizeDialog::set_progress (double p)
{
	_progress_bar->set_fraction (p);

	while (gtk_events_pending()) {
		gtk_main_iteration ();
	}
}
