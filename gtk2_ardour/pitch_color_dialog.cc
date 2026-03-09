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

#include <ytkmm/label.h>
#include <ytkmm/box.h>
#include <ytkmm/colorbutton.h>
#include <ytkmm/stock.h>

#include "gtkmm2ext/utils.h"

#include "pitch_color_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;

PitchColorDialog::PitchColorDialog ()
	: ArdourDialog (_("Pitch Color Dialog"))
	, pitch_cycle (12)
{
	refill ();

	get_vbox()->pack_start (pitch_vpacker);

	add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
	set_default_response(Gtk::RESPONSE_ACCEPT);
}

void
PitchColorDialog::refill ()
{
	Gtkmm2ext::container_clear (pitch_vpacker);

	HBox* pitch_hpacker;
	Label* pitch_label;
	ColorButton* color_button;
	char buf[64];

	for (int n = 0; n < pitch_cycle; ++n) {
		pitch_label = manage (new Label);
		snprintf (buf, sizeof (buf), "%d", n+1);
		pitch_label->set_text (buf);
		pitch_hpacker = manage (new HBox);

		color_button = manage (new ColorButton);

		pitch_hpacker->pack_start (*color_button, false, false);
		pitch_hpacker->pack_start (*pitch_label, false, false);

		pitch_vpacker.pack_start (*pitch_hpacker);
	}

	pitch_vpacker.show_all ();
}
