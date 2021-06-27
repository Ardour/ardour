/*
 * Copyright (C) 2011 Carl Hetherington <carl@carlh.net>
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

#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>
#include "transpose_dialog.h"
#include "pbd/i18n.h"

#include <ardour/session.h>

using namespace Gtk;

TransposeDialog::TransposeDialog ()
	: ArdourDialog (_("Transpose MIDI"))
	, _octaves_adjustment (0.0, -4.0, 4.0, 1, 2.0)
	, _semitones_adjustment (0.0, -12.0, 12.0, 1.0, 4.0)
	, _octaves_spinner (_octaves_adjustment)
	, _semitones_spinner (_semitones_adjustment)
{
	Table* t = manage (new Table (2, 2));
	t->set_row_spacings (6);
	t->set_col_spacings (6);

	int r = 0;
	Label* l = manage (new Label (_("Octaves:"), ALIGN_LEFT, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_octaves_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	l = manage (new Label (_("Semitones:"), ALIGN_LEFT, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_semitones_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (*t, false, false);

	add_button (Stock::CANCEL, RESPONSE_CANCEL);
	add_button (_("Transpose"), RESPONSE_ACCEPT);

	show_all_children ();
}

int
TransposeDialog::semitones () const
{
	return _octaves_spinner.get_value () * 12 + _semitones_spinner.get_value ();
}





VarispeedDialog::VarispeedDialog ()
	: ArdourDialog (_("Varispeed"))
	, _octaves_adjustment (0.0, -4.0, 4.0, 1, 2.0)
	, _semitones_adjustment (0.0, -12.0, 12.0, 1.0, 4.0)
	, _cents_adjustment (0.0, -100.0, 100.0, 1.0, 10.0)
	, _octaves_spinner (_octaves_adjustment)
	, _semitones_spinner (_semitones_adjustment)
	, _cents_spinner (_cents_adjustment)
{
	set_modal(false);

	Table* t = manage (new Table (3, 2));
	t->set_row_spacings (6);
	t->set_col_spacings (6);

	int r = 0;
	Label* l = manage (new Label (_("Octaves:"), ALIGN_LEFT, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_octaves_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	l = manage (new Label (_("Semitones:"), ALIGN_LEFT, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_semitones_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	l = manage (new Label (_("Cents:"), ALIGN_LEFT, ALIGN_CENTER, false));
	t->attach (*l, 0, 1, r, r + 1, FILL, EXPAND, 0, 0);
	t->attach (_cents_spinner, 1, 2, r, r + 1, FILL, EXPAND & FILL, 0, 0);
	++r;

	get_vbox()->set_spacing (6);
	get_vbox()->pack_start (*t, false, false);

//	add_button (Stock::CANCEL, RESPONSE_CANCEL);
//	add_button (_("Transpose"), RESPONSE_ACCEPT);

	_octaves_spinner.signal_changed().connect (sigc::mem_fun (*this, &VarispeedDialog::apply_speed));
	_semitones_spinner.signal_changed().connect (sigc::mem_fun (*this, &VarispeedDialog::apply_speed));
	_cents_spinner.signal_changed().connect (sigc::mem_fun (*this, &VarispeedDialog::apply_speed));

	show_all_children ();
}

void
VarispeedDialog::reset ()
{
	_octaves_spinner.set_value(0);
	_semitones_spinner.set_value(0);
	_cents_spinner.set_value(0);
}

void
VarispeedDialog::apply_speed ()
{
	int cents = _octaves_spinner.get_value () * 12 * 100 + _semitones_spinner.get_value () * 100  + _cents_spinner.get_value ();

	double speed = pow (2.0, ((double)cents / (double)1200.0));

	if (_session) {
		_session->set_default_play_speed(speed);
	}
}

void
VarispeedDialog::on_hide ()
{
	_session->set_default_play_speed(1.0);
	ArdourDialog::on_hide();
}
