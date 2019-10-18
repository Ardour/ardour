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
#include "ui_config.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace Glib;
using namespace Gtk;
using namespace ArdourWidgets;

#define PX_SCALE(px) std::max((float)px, rintf((float)px * UIConfiguration::instance().get_ui_scale()))

VirtualKeyboardWindow::VirtualKeyboardWindow ()
	: ArdourWindow (_("Virtual Keyboard"))
	, _piano_channel (*manage (new Adjustment (0, 1, 16, 1, 1)))
	, _yaxis_velocity ("Y-Axis Click Velocity", ArdourButton::led_default_elements)
	, _piano_key_velocity (*manage (new Adjustment (100, 1, 127, 1, 16)))
	, _piano_min_velocity (*manage (new Adjustment (1  , 1, 127, 1, 16)))
	, _piano_max_velocity (*manage (new Adjustment (127, 1, 127, 1, 16)))
	, _cc7 (new VKBDControl ("CC7", 127))
	, _cc7_knob (ArdourKnob::default_elements, ArdourKnob::Flags (0))
{
	_piano = (PianoKeyboard*)piano_keyboard_new();
	_pianomm = Glib::wrap((GtkWidget*)_piano);
	_pianomm->set_flags(Gtk::CAN_FOCUS);
	_pianomm->add_events(Gdk::KEY_PRESS_MASK|Gdk::KEY_RELEASE_MASK);
	piano_keyboard_set_keyboard_layout (_piano, "QWERTY");

	using namespace Menu_Helpers;
	_keyboard_layout.AddMenuElem (MenuElem (_("QWERTY"),
				sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), 0)));
	_keyboard_layout.AddMenuElem (MenuElem (_("QWERTZ"),
				sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), 1)));
	_keyboard_layout.AddMenuElem (MenuElem (_("AZERTY"),
				sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), 2)));
	_keyboard_layout.set_text (_("QWERTY"));

	_yaxis_velocity.set_active (false);

	_cc7_knob.set_controllable (_cc7);
	_cc7_knob.set_size_request (PX_SCALE(21), PX_SCALE(21));
	_cc7_knob.set_tooltip_prefix (_("CC7: "));
	_cc7_knob.set_name ("monitor section knob");

	// TODO allow to hide config panel
	// save/restore settings

	/* config */
	HBox* cfg_box = manage (new HBox);
	cfg_box->set_spacing (4);
	cfg_box->pack_start (*manage (new Label (_("Key Velocity:"))), false, false);
	cfg_box->pack_start (_piano_key_velocity, false, false);
	cfg_box->pack_start (_yaxis_velocity, false, false, 8);
	cfg_box->pack_start (*manage (new Label (_("Min Velocity:"))), false, false);
	cfg_box->pack_start (_piano_min_velocity, false, false);
	cfg_box->pack_start (*manage (new Label (_("Max Velocity:"))), false, false);
	cfg_box->pack_start (_piano_max_velocity, false, false);
	cfg_box->pack_start (_keyboard_layout, false, false, 8);

	/* settings */
	HBox* set_box = manage (new HBox);
	set_box->set_spacing (4);
	set_box->pack_start (*manage (new Label (_("Channel:"))), false, false);
	set_box->pack_start (_piano_channel, false, false, 8);
	set_box->pack_start (*manage (new Label (_("CC7:"))), false, false);
	set_box->pack_start (_cc7_knob, false, false);

	/* layout */
	Box* box1 = manage (new HBox ());
	box1->pack_start (*cfg_box, true, false);
	Box* box2 = manage (new HBox ());
	box2->pack_start (*set_box, true, false);
	VBox* vbox = manage (new VBox);
	vbox->pack_start (*box1, false, false, 4);
	vbox->pack_start (*box2, false, false, 4);
	vbox->pack_start (*_pianomm, true, true);
	add (*vbox);

	_piano_key_velocity.signal_value_changed ().connect (sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_velocity_settings), 0));
	_piano_min_velocity.signal_value_changed ().connect (sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_velocity_settings), 1));
	_piano_max_velocity.signal_value_changed ().connect (sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_velocity_settings), 2));

	_yaxis_velocity.signal_button_release_event().connect (sigc::mem_fun(*this, &VirtualKeyboardWindow::yaxis_velocity_button_release), false);

	g_signal_connect (G_OBJECT (_piano), "note-on", G_CALLBACK (VirtualKeyboardWindow::_note_on_event_handler), this);
	g_signal_connect (G_OBJECT (_piano), "note-off", G_CALLBACK (VirtualKeyboardWindow::_note_off_event_handler), this);

	_cc7->ValueChanged.connect_same_thread (_cc_connections, boost::bind (&VirtualKeyboardWindow::control_change_event_handler, this, 7, _1));

	update_velocity_settings (0);

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
VirtualKeyboardWindow::select_keyboard_layout (int l)
{
	switch (l) {
		default:
		case 0:
			piano_keyboard_set_keyboard_layout (_piano, "QWERTY");
			_keyboard_layout.set_text (_("QWERTY"));
			break;
		case 1:
			piano_keyboard_set_keyboard_layout (_piano, "QWERTZ");
			_keyboard_layout.set_text (_("QWERTZ"));
			break;
		case 2:
			piano_keyboard_set_keyboard_layout (_piano, "AZERTY");
			_keyboard_layout.set_text (_("AZERTY"));
			break;
	}
}

bool
VirtualKeyboardWindow::yaxis_velocity_button_release (GdkEventButton* ev)
{
	_yaxis_velocity.set_active (!_yaxis_velocity.get_active ());
	update_velocity_settings (0);
	return false;
}

void
VirtualKeyboardWindow::update_velocity_settings (int ctrl)
{
	if (_piano_min_velocity.get_value_as_int () > _piano_max_velocity.get_value_as_int ()) {
		if (ctrl == 2) {
			_piano_min_velocity.set_value (_piano_max_velocity.get_value_as_int ());
			return;
		} else {
			_piano_max_velocity.set_value (_piano_min_velocity.get_value_as_int ());
			return;
		}
	}

	if (_yaxis_velocity.get_active ()) {
		piano_keyboard_set_velocities (_piano,
				_piano_min_velocity.get_value_as_int (),
				_piano_max_velocity.get_value_as_int (),
				_piano_key_velocity.get_value_as_int ()
				);
	} else {
		piano_keyboard_set_velocities (_piano,
				_piano_key_velocity.get_value_as_int (),
				_piano_key_velocity.get_value_as_int (),
				_piano_key_velocity.get_value_as_int ()
				);
	}
	update_sensitivity ();
}

void
VirtualKeyboardWindow::update_sensitivity ()
{
	bool c = _yaxis_velocity.get_active ();
	_piano_min_velocity.set_sensitive (c);
	_piano_max_velocity.set_sensitive (c);
}

void
VirtualKeyboardWindow::note_on_event_handler (int note, int velocity)
{
	_pianomm->grab_focus ();
	if (!_session) {
		return;
	}
	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = (MIDI_CMD_NOTE_ON | channel);
	ev[1] = note;
	ev[2] = velocity;
	_session->vkbd_output_port()->write (ev, 3, 0);
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
	_session->vkbd_output_port()->write (ev, 3, 0);
}

void
VirtualKeyboardWindow::control_change_event_handler (int key, int val)
{
	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = (MIDI_CMD_CONTROL | channel);
	ev[1] = key;
	ev[2] = val;
	_session->vkbd_output_port()->write (ev, 3, 0);
}
