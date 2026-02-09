/*
 * Copyright (C) 2026 Paul Davis <paul@linuxaudiosystems.com>
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

#include "scale_dialog.h"

#include "gtkmm2ext/utils.h"

#include "pbd/i18n.h"

ScaleDialog::ScaleDialog ()
	: ArdourDialog (_("Scale Editor"))
	, _key (440.0, ARDOUR::MusicalMode (ARDOUR::MusicalMode::IonianMajor))
	, name_label (_("Name"))
	, step_adjustment (7, 1, 56, 1, 8)
	, step_spinner (step_adjustment)
{
}

ScaleDialog::~ScaleDialog ()
{
}

void
ScaleDialog ::set (ARDOUR::MusicalKey & key)
{
}

ARDOUR::MusicalKey
ScaleDialog::get() const
{
	return ARDOUR::MusicalKey (1.0, ARDOUR::MusicalMode (ARDOUR::MusicalMode::Dorian));
}

void
ScaleDialog::pack ()
{
	Gtk::VBox* vbox (get_vbox());

	name_packer.pack_start (name_label, false, false);
	name_packer.pack_start (name_entry, true, true);

	vbox->pack_start (name_packer, false, false);
	vbox->pack_start (step_spinner, false, false);

	for (int n = 0; n < _key.size(); ++n) {
		StepEntry* se = new StepEntry (n);
		Gtkmm2ext::set_size_request_to_display_given_text (*se, "abcdef", 2, 6);
		step_packer.pack_start (*se, false, false);
	}

	vbox->show_all ();
}
