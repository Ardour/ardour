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

#include "pbd/convert.h"
#include "ardour/async_midi_port.h"
#include "widgets/tooltips.h"

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
	, _piano_channel (*manage (new Adjustment (1, 1, 16, 1, 1)))
	, _bank_msb (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _bank_lsb (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _patchpgm (*manage (new Adjustment (1, 1, 128, 1, 16)))
	, _cfg_display ("Config", ArdourButton::led_default_elements)
	, _pgm_display ("Bank/Patch", ArdourButton::led_default_elements)
	, _yaxis_velocity ("Y-Axis Click Velocity", ArdourButton::led_default_elements)
	, _send_panic ("Panic", ArdourButton::default_elements)
	, _piano_key_velocity (*manage (new Adjustment (100, 1, 127, 1, 16)))
	, _piano_min_velocity (*manage (new Adjustment (1  , 1, 127, 1, 16)))
	, _piano_max_velocity (*manage (new Adjustment (127, 1, 127, 1, 16)))
	, _pitch_adjustment (8192, 0, 16383, 1, 256)
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

	_cfg_display.set_active (false);
	_pgm_display.set_active (false);
	_yaxis_velocity.set_active (false);

	_pitchbend = boost::shared_ptr<VKBDControl> (new VKBDControl ("PB", 8192, 16383));
	_pitch_slider = manage (new VSliderController(&_pitch_adjustment, _pitchbend, 0, PX_SCALE (15)));
	_pitch_slider_tooltip = new Gtkmm2ext::PersistentTooltip (_pitch_slider);
	_pitch_adjustment.signal_value_changed().connect (
			sigc::mem_fun (*this, &VirtualKeyboardWindow::pitch_slider_adjusted));
	_pitchbend->ValueChanged.connect_same_thread (_cc_connections,
			boost::bind (&VirtualKeyboardWindow::pitch_bend_event_handler, this, _1));

	set_tooltip (_send_panic, "Send MIDI Panic message for current channel");
	_pitch_slider_tooltip->set_tip ("Pitchbend: 8192");

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
	cfg_box->show_all ();

	/* bank/patch */
	Table* pgm_tbl = manage (new Table);

	Label* lbl = manage (new Label (_("Note: Prefer\nTrack-controls")));
	lbl->set_justify (JUSTIFY_CENTER);

	pgm_tbl->attach (*lbl, 0, 1, 0, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new ArdourVSpacer), 1, 2, 0, 2, SHRINK, FILL, 4, 0);
	pgm_tbl->attach (_bank_msb, 2, 3, 0, 1, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (_bank_lsb, 3, 4, 0, 1, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (_patchpgm, 4, 5, 0, 1, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new Label (_("MSB"))), 2, 3, 1, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new Label (_("LSB"))), 3, 4, 1, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new Label (_("PGM"))), 4, 5, 1, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new ArdourVSpacer), 5, 6, 0, 2, SHRINK, FILL, 4, 0);
	pgm_tbl->attach (_send_panic, 6, 7, 0, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->show_all ();

	/* settings */
	Table* tbl = manage (new Table);
	tbl->attach (_piano_channel, 0, 1, 0, 1, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new Label (_("Channel"))), 0, 1, 1, 2, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new ArdourVSpacer), 1, 2, 0, 2, SHRINK, FILL, 4, 0);
	tbl->attach (*_pitch_slider, 2, 3, 0, 2, SHRINK, FILL, 4, 0);

	const char* default_cc[VKBD_NCTRLS] = { "7", "8", "1", "11", "91", "92", "93", "94" };

	for (int i = 0; i < VKBD_NCTRLS; ++i) {
		_cc[i] = boost::shared_ptr<VKBDControl> (new VKBDControl ("CC"));
		_cc_knob[i] = manage(new ArdourKnob (ArdourKnob::default_elements, ArdourKnob::Flags (0)));
		_cc_knob[i]->set_controllable (_cc[i]);
		_cc_knob[i]->set_size_request (PX_SCALE(21), PX_SCALE(21));
		_cc_knob[i]->set_tooltip_prefix (_("CC: "));
		_cc_knob[i]->set_name ("monitor section knob");

		for (int c = 1; c < 120; ++c) {
			if (c == 32) {
				continue;
			}
			char key[32];
			sprintf (key, "%d", c);
			_cc_key[i].append_text_item (key);
		}
		_cc_key[i].set_text (default_cc[i]);

		tbl->attach (*_cc_knob[i], i+3, i+4, 0, 1, SHRINK, SHRINK, 4, 2);
		tbl->attach (_cc_key[i]  , i+3, i+4, 1, 2, SHRINK, SHRINK, 4, 2);

		_cc[i]->ValueChanged.connect_same_thread (_cc_connections,
				boost::bind (&VirtualKeyboardWindow::control_change_event_handler, this, i, _1));
	}

	/* main layout */
	Box* box1 = manage (new HBox ());
	box1->pack_start (*tbl, true, false);

	Box* box2 = manage (new VBox ());
	box2->pack_start (_pgm_display, false, false);
	box2->pack_start (_cfg_display, false, false);
	box1->pack_start (*box2, false, false);

	_cfg_box = manage (new HBox ());
	_cfg_box->pack_start (*cfg_box, true, false);
	_cfg_box->set_no_show_all (true);

	_pgm_box = manage (new HBox ());
	_pgm_box->pack_start (*pgm_tbl, true, false);
	_pgm_box->set_no_show_all (true);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (*box1, false, false, 4);
	vbox->pack_start (*_pgm_box, false, false, 4);
	vbox->pack_start (*_cfg_box, false, false, 4);
	vbox->pack_start (*_pianomm, true, true);
	add (*vbox);

	_bank_msb.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::bank_patch));
	_bank_lsb.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::bank_patch));
	_patchpgm.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::bank_patch));

	_piano_key_velocity.signal_value_changed ().connect (sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_velocity_settings), 0));
	_piano_min_velocity.signal_value_changed ().connect (sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_velocity_settings), 1));
	_piano_max_velocity.signal_value_changed ().connect (sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_velocity_settings), 2));

	_cfg_display.signal_button_release_event().connect (sigc::mem_fun(*this, &VirtualKeyboardWindow::toggle_config), false);
	_pgm_display.signal_button_release_event().connect (sigc::mem_fun(*this, &VirtualKeyboardWindow::toggle_bankpatch), false);
	_yaxis_velocity.signal_button_release_event().connect (sigc::mem_fun(*this, &VirtualKeyboardWindow::toggle_yaxis_velocity), false);
	_send_panic.signal_button_release_event().connect (sigc::mem_fun(*this, &VirtualKeyboardWindow::send_panic_message), false);

	g_signal_connect (G_OBJECT (_piano), "note-on", G_CALLBACK (VirtualKeyboardWindow::_note_on_event_handler), this);
	g_signal_connect (G_OBJECT (_piano), "note-off", G_CALLBACK (VirtualKeyboardWindow::_note_off_event_handler), this);

	update_velocity_settings (0);

	set_keep_above (true);
	vbox->show_all();
}

VirtualKeyboardWindow::~VirtualKeyboardWindow ()
{
	delete _pianomm;
	delete _pitch_slider_tooltip;
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
VirtualKeyboardWindow::toggle_config (GdkEventButton* ev)
{
	bool a = ! _cfg_display.get_active ();
	_cfg_display.set_active (a);
	if (a) {
		_cfg_box->show ();
	} else {
		_cfg_box->hide ();
	}
	return false;
}

bool
VirtualKeyboardWindow::toggle_bankpatch (GdkEventButton*)
{
	bool a = ! _pgm_display.get_active ();
	_pgm_display.set_active (a);
	if (a) {
		_pgm_box->show ();
	} else {
		_pgm_box->hide ();
	}
	return false;
}

bool
VirtualKeyboardWindow::toggle_yaxis_velocity (GdkEventButton*)
{
	_yaxis_velocity.set_active (!_yaxis_velocity.get_active ());
	update_velocity_settings (0);
	return false;
}

bool
VirtualKeyboardWindow::send_panic_message (GdkEventButton*)
{
	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = MIDI_CMD_CONTROL | channel;
	ev[1] = MIDI_CTL_SUSTAIN;
	ev[2] = 0;
	_session->vkbd_output_port()->write (ev, 3, 0);
	ev[1] = MIDI_CTL_ALL_NOTES_OFF;
	_session->vkbd_output_port()->write (ev, 3, 0);
	ev[1] = MIDI_CTL_RESET_CONTROLLERS;
	_session->vkbd_output_port()->write (ev, 3, 0);
	return false;
}

void
VirtualKeyboardWindow::bank_patch ()
{
	int msb = _bank_msb.get_value_as_int ();
	int lsb = _bank_lsb.get_value_as_int ();
	int pgm = _patchpgm.get_value_as_int () - 1;

	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = MIDI_CMD_CONTROL | channel;
	ev[1] = MIDI_CTL_MSB_BANK;
	ev[2] = (msb >> 7) & 0x7f;
	_session->vkbd_output_port()->write (ev, 3, 0);
	ev[1] = MIDI_CTL_LSB_BANK | channel;
	ev[2] = lsb & 0x7f;
	_session->vkbd_output_port()->write (ev, 3, 0);
	ev[0] = MIDI_CMD_PGM_CHANGE | channel;
	ev[1] = pgm & 0x7f;
	_session->vkbd_output_port()->write (ev, 2, 0);
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
VirtualKeyboardWindow::pitch_slider_adjusted ()
{
	_pitchbend->set_value (_pitch_adjustment.get_value (), PBD::Controllable::NoGroup);
	char buf[64];
	snprintf(buf, sizeof(buf), "Pitchbend: %.0f", _pitch_adjustment.get_value());
	_pitch_slider_tooltip->set_tip (buf);
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
	ev[0] = MIDI_CMD_NOTE_ON | channel;
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
	ev[0] = MIDI_CMD_NOTE_OFF | channel;
	ev[1] = note;
	ev[2] = 0;
	_session->vkbd_output_port()->write (ev, 3, 0);
}

void
VirtualKeyboardWindow::control_change_event_handler (int key, int val)
{
	if (!_session) {
		return;
	}
	assert (key >= 0 && key < VKBD_NCTRLS);
	int ctrl = PBD::atoi (_cc_key[key].get_text());
	assert (ctrl > 0 && ctrl < 127);
	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = MIDI_CMD_CONTROL | channel;
	ev[1] = ctrl;
	ev[2] = val;
	_session->vkbd_output_port()->write (ev, 3, 0);
}

void
VirtualKeyboardWindow::pitch_bend_event_handler (int val)
{
	if (!_session) {
		return;
	}
	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = MIDI_CMD_BENDER | channel;
	ev[1] = val & 0x7f;
	ev[2] = (val >> 7) & 0x7f;
	_session->vkbd_output_port()->write (ev, 3, 0);
}
