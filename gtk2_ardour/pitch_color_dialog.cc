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
#include <ytkmm/spinbutton.h>
#include <ytkmm/stock.h>

#include "ardour/parameter_descriptor.h"

#include "gtkmm2ext/colors.h"
#include "gtkmm2ext/utils.h"

#include "note_base.h"
#include "pitch_color_dialog.h"

#include "pbd/i18n.h"

using namespace Gtk;

PitchColorDialog::PitchColorDialog ()
	: ArdourDialog (_("Pitch Color Dialog"))
	, pitch_vpacker (nullptr)
	, cycle_adjust (12, 1, 56, 1, 2)
{
	Label* cycle_label = manage (new Label (_("Note Cycle")));
	SpinButton* cycle_spinner = manage (new SpinButton (cycle_adjust));
	HBox* cycle_hpacker = manage (new HBox);

	cycle_hpacker->pack_start (*cycle_label, true, false);
	cycle_hpacker->pack_start (*cycle_spinner, true, false);
	cycle_hpacker->show_all ();
	cycle_hpacker->set_border_width (12);
	cycle_hpacker->set_spacing (12);

	refill ();

	get_vbox()->pack_start (*cycle_hpacker, false, false);

	add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
	set_default_response(Gtk::RESPONSE_ACCEPT);

	cycle_adjust.signal_value_changed().connect ([this]() { refill (); });
}

void
PitchColorDialog::color_chosen (int n, ColorButton* cb)
{
	uint32_t c = Gtkmm2ext::gdk_color_to_rgba (cb->get_color ());

	if (n < (int) NoteBase::pitch_colors.size()) {
		NoteBase::pitch_colors.resize (n + 12);
	}

	NoteBase::pitch_colors[n] = c;
	ColorsChanged (); /* EMIT SIGNAL */
}

void
PitchColorDialog::refill ()
{
	if (pitch_vpacker) {
		pitch_vpacker->hide ();
		get_vbox()->remove (*pitch_vpacker);
		delete pitch_vpacker;
	}

	pitch_vpacker = new VBox;

	HBox* pitch_hpacker;
	Label* pitch_label;
	ColorButton* color_button;
	char buf[64];
	int pitch_cycle = cycle_adjust.get_value();

	for (int n = 0; n < pitch_cycle; ++n) {
		pitch_label = manage (new Label);
		snprintf (buf, sizeof (buf), "%d (%s)", n+1, ARDOUR::ParameterDescriptor::midi_note_name (n).c_str());
		pitch_label->set_text (buf);

		color_button = manage (new ColorButton);
		color_button->signal_color_set().connect ([this,n,color_button]() { color_chosen (n, color_button); });

		uint32_t col;

		if (n < (int) NoteBase::pitch_colors.size()) {
			col = NoteBase::pitch_colors[n];
		} else {
			col = Gtkmm2ext::random_color ();
		}

		color_button->set_color (Gtkmm2ext::gdk_color_from_rgba (col));

		pitch_hpacker = manage (new HBox);
		pitch_hpacker->pack_start (*color_button, false, false);
		pitch_hpacker->pack_start (*pitch_label, false, false);

		pitch_vpacker->pack_start (*pitch_hpacker);
	}

	pitch_vpacker->show_all ();
	get_vbox()->pack_end (*pitch_vpacker);
}
