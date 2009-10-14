/*
    Copyright (C) 2000-2008 Paul Davis

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

#include "control_point_dialog.h"
#include "control_point.h"
#include "automation_line.h"
#include "i18n.h"
#include <gtkmm/stock.h>

/**
 *    ControlPointDialog constructor.
 *
 *    @param p ControlPoint to edit.
 */

ControlPointDialog::ControlPointDialog (ControlPoint* p)
	: ArdourDialog (_("Control point")),
	  point_ (p)
{
	assert (point_);

	double const y_fraction = 1.0 - (p->get_y () / p->line().height ());

	value_.set_text (p->line().fraction_to_string (y_fraction));

	Gtk::HBox* b = Gtk::manage (new Gtk::HBox ());

	b->pack_start (*Gtk::manage (new Gtk::Label (_("Value"))));
	b->pack_start (value_);

	if (p->line ().get_uses_gain_mapping ()) {
		b->pack_start (*Gtk::manage (new Gtk::Label (_("dB"))));
	}

	get_vbox ()->pack_end (*b);
	b->show_all ();

	add_button (Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button (Gtk::Stock::APPLY, Gtk::RESPONSE_ACCEPT);
	set_default_response (Gtk::RESPONSE_ACCEPT);
}

double
ControlPointDialog::get_y_fraction () const
{
	return point_->line().string_to_fraction (value_.get_text ());
}
