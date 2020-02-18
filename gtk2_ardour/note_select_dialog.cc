/*
 * Copyright (C) 2014 David Robillard <d@drobilla.net>
 * Copyright (C) 2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <gtkmm/stock.h>

#include "note_select_dialog.h"

#include "pbd/i18n.h"

NoteSelectDialog::NoteSelectDialog ()
	: ArdourDialog (_("Select Note"))
	, _note_number(60)
{
	_piano.set_flags(Gtk::CAN_FOCUS);
	_piano.show();
	_piano.NoteOn.connect (sigc::mem_fun (*this, &NoteSelectDialog::note_on_event_handler));

	_piano.set_monophonic (true);
	_piano.sustain_press ();

	get_vbox()->pack_start(_piano);

	add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
	set_default_response(Gtk::RESPONSE_ACCEPT);
}

void
NoteSelectDialog::note_on_event_handler(int note, int)
{
	_note_number = note;
}
