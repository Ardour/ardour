/*
  Copyright (C) 2014 Paul Davis
  Author: David Robillard

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

#include <gtkmm/stock.h>

#include "note_select_dialog.h"

#include "i18n.h"

static void
_note_on_event_handler(GtkWidget* /*widget*/, int note, gpointer arg)
{
	((NoteSelectDialog*)arg)->note_on_event_handler(note);
}

NoteSelectDialog::NoteSelectDialog()
	: ArdourDialog (_("Select Note"))
	, _piano((PianoKeyboard*)piano_keyboard_new())
	, _pianomm(Glib::wrap((GtkWidget*)_piano))
	, _note_number(60)
{
	_pianomm->set_flags(Gtk::CAN_FOCUS);
	_pianomm->show();
	g_signal_connect(G_OBJECT(_piano), "note-on", G_CALLBACK(_note_on_event_handler), this);
	piano_keyboard_set_monophonic(_piano, TRUE);
	piano_keyboard_sustain_press(_piano);

	get_vbox()->pack_start(*_pianomm);
	add_button(Gtk::Stock::CANCEL, Gtk::RESPONSE_CANCEL);
	add_button(Gtk::Stock::OK, Gtk::RESPONSE_ACCEPT);
	set_default_response(Gtk::RESPONSE_ACCEPT);
}

void
NoteSelectDialog::note_on_event_handler(int note)
{
	printf("NOTE: %d\n", note);
	_note_number = note;
}
