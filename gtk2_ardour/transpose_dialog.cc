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

#include "transpose_dialog.h"

#include <gtkmm/table.h>
#include <gtkmm/label.h>
#include <gtkmm/stock.h>

#include "pbd/i18n.h"

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
