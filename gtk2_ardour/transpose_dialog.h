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

#include <gtkmm/spinbutton.h>
#include "ardour_dialog.h"

/** A dialog box to select a transposition to apply to a MIDI region.
 *  It asks for octaves and semitones, with the transposition being
 *  the sum of the two.
 */

class TransposeDialog : public ArdourDialog
{
public:
	TransposeDialog ();

	int semitones () const;

private:
	Gtk::Adjustment _octaves_adjustment;
	Gtk::Adjustment _semitones_adjustment;
	Gtk::SpinButton _octaves_spinner;
	Gtk::SpinButton _semitones_spinner;
};
