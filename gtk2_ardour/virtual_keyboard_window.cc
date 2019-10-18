/*
 * Copyright (C) 2019 Robin Gareus <robin@gareus.org>
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

#include <gtkmm/box.h>

#include "ardour/async_midi_port.h"

#include "ardour_ui.h"
#include "virtual_keyboard_window.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace Glib;
using namespace Gtk;

VirtualKeyboardWindow::VirtualKeyboardWindow ()
	: ArdourWindow (_("Virtual Keyboard"))
	, _piano_velocity (*manage (new Adjustment (100, 1, 127, 1, 16)))
	, _piano_channel (*manage (new Adjustment (0, 1, 16, 1, 1)))
{
	_piano = (PianoKeyboard*)piano_keyboard_new();
	_pianomm = Glib::wrap((GtkWidget*)_piano);
	_pianomm->set_flags(Gtk::CAN_FOCUS);
	_pianomm->add_events(Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);

	g_signal_connect (G_OBJECT (_piano), "note-on", G_CALLBACK (VirtualKeyboardWindow::_note_on_event_handler), this);
	g_signal_connect (G_OBJECT (_piano), "note-off", G_CALLBACK (VirtualKeyboardWindow::_note_off_event_handler), this);

	piano_keyboard_set_keyboard_layout (_piano, "QWERTY");

	HBox* box = manage (new HBox);
	box->pack_start (*manage (new Label (_("Channel:"))), false, false);
	box->pack_start (_piano_channel, false, false);
	box->pack_start (*manage (new Label (_("Velocity:"))), false, false);
	box->pack_start (_piano_velocity, false, false);

	Box* box2 = manage (new HBox ());
	box2->pack_start (*box, true, false);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (*box2, false, false, 4);
	vbox->pack_start (*_pianomm, true, true);
	add (*vbox);

	set_keep_above (true);
	vbox->show_all();
}

VirtualKeyboardWindow::~VirtualKeyboardWindow ()
{
	delete _pianomm;
}

void
VirtualKeyboardWindow::on_unmap ()
{
	ArdourWindow::on_unmap ();
	ARDOUR_UI::instance()->reset_focus (this);
}

bool
VirtualKeyboardWindow::on_key_press_event (GdkEventKey* ev)
{
	return ARDOUR_UI_UTILS::relay_key_press (ev, this);
}

void
VirtualKeyboardWindow::note_on_event_handler (int note)
{
	_pianomm->grab_focus ();
	if (!_session) {
		return;
	}
	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = (MIDI_CMD_NOTE_ON | channel);
	ev[1] = note;
	ev[2] = _piano_velocity.get_value_as_int ();
	boost::dynamic_pointer_cast<ARDOUR::AsyncMIDIPort>(_session->vkbd_output_port())->write (ev, 3, 0);
}

void
VirtualKeyboardWindow::note_off_event_handler (int note)
{
	if (!_session) {
		return;
	}

	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = (MIDI_CMD_NOTE_OFF | channel);
	ev[1] = note;
	ev[2] = 0;
	boost::dynamic_pointer_cast<ARDOUR::AsyncMIDIPort>(_session->vkbd_output_port())->write (ev, 3, 0);
}
