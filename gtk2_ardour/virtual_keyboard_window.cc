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
#include "ardour/session.h"
#include "pbd/convert.h"
#include "widgets/tooltips.h"

#include "ardour_ui.h"
#include "ui_config.h"
#include "utils.h"
#include "virtual_keyboard_window.h"

#include "pbd/i18n.h"

using namespace Glib;
using namespace Gtk;
using namespace ArdourWidgets;

#define PX_SCALE(px) std::max ((float)px, rintf ((float)px* UIConfiguration::instance ().get_ui_scale ()))

VirtualKeyboardWindow::VirtualKeyboardWindow ()
	: ArdourWindow (_("Virtual MIDI Keyboard"))
	, _piano_channel (*manage (new Adjustment (1, 1, 16, 1, 1)))
	, _transpose_output (*manage (new Adjustment (0, -12, 12, 1, 1)))
	, _bank_msb (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _bank_lsb (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _patchpgm (*manage (new Adjustment (1, 1, 128, 1, 16)))
	, _cfg_display ("Config", ArdourButton::led_default_elements)
	, _pgm_display ("Bank/Patch", ArdourButton::led_default_elements)
	, _yaxis_velocity ("Y-Axis", ArdourButton::led_default_elements)
	, _highlight_grand_piano ("Grand Piano", ArdourButton::led_default_elements)
	, _highlight_key_range ("Key Bindings", ArdourButton::led_default_elements)
	, _show_note_label ("Label C Key", ArdourButton::led_default_elements)
	, _send_panic ("Panic", ArdourButton::default_elements)
	, _piano_key_velocity (*manage (new Adjustment (100, 1, 127, 1, 16)))
	, _piano_min_velocity (*manage (new Adjustment (  1, 1, 127, 1, 16)))
	, _piano_max_velocity (*manage (new Adjustment (127, 1, 127, 1, 16)))
	, _piano_octave_key (*manage (new Adjustment (4, -1, 7, 1, 1)))
	, _piano_octave_range (*manage (new Adjustment (7, 2, 11, 1, 1)))
	, _pitch_adjustment (8192, 0, 16383, 1, 256)
{
	_piano   = (PianoKeyboard*)piano_keyboard_new ();
	_pianomm = Glib::wrap ((GtkWidget*)_piano);
	_pianomm->set_flags (Gtk::CAN_FOCUS);
	_pianomm->add_events (Gdk::KEY_PRESS_MASK | Gdk::KEY_RELEASE_MASK);
	piano_keyboard_set_keyboard_layout (_piano, "QWERTY");
	piano_keyboard_show_note_label (_piano, true);

	using namespace Menu_Helpers;
	_keyboard_layout.AddMenuElem (MenuElem ("QWERTY",
	                                        sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "QWERTY")));
	_keyboard_layout.AddMenuElem (MenuElem ("QWERTZ",
	                                        sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "QWERTZ")));
	_keyboard_layout.AddMenuElem (MenuElem ("AZERTY",
	                                        sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "AZERTY")));
	_keyboard_layout.AddMenuElem (MenuElem ("DVORAK",
	                                        sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "DVORAK")));
	_keyboard_layout.set_active ("QWERTY");

	_cfg_display.set_active (false);
	_pgm_display.set_active (false);
	_yaxis_velocity.set_active (false);
	_highlight_grand_piano.set_active (false);
	_highlight_key_range.set_active (false);
	_show_note_label.set_active (true);

	_pitchbend            = boost::shared_ptr<VKBDControl> (new VKBDControl ("PB", 8192, 16383));
	_pitch_slider         = manage (new VSliderController (&_pitch_adjustment, _pitchbend, 0, PX_SCALE (15)));
	_pitch_slider_tooltip = new Gtkmm2ext::PersistentTooltip (_pitch_slider);

	_pitch_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::pitch_slider_adjusted));
	_pitchbend->ValueChanged.connect_same_thread (_cc_connections, boost::bind (&VirtualKeyboardWindow::pitch_bend_event_handler, this, _1));

	set_tooltip (_highlight_grand_piano, "Shade keys outside the range of a Grand Piano (A0-C8).");
	set_tooltip (_highlight_key_range, "Indicate which notes can be controlled by keyboard-shortcuts.");
	set_tooltip (_show_note_label, "When enabled, print octave number on C-Keys");
	set_tooltip (_yaxis_velocity, "When enabled, mouse-click y-axis position defines the velocity.");

	set_tooltip (_piano_octave_key, "The center octave, and lowest octave for keyboard control.");
	set_tooltip (_piano_octave_range, "Available octave range, centered around the key-octave.");
	set_tooltip (_keyboard_layout, "Keyboard layout to use for keyboard control.");

	set_tooltip (_piano_key_velocity, "The default velocity to use with keyboard control, and when y-axis click-position is disabled.");
	set_tooltip (_piano_min_velocity, "Velocity to use when clicking at the top-end of a key.");
	set_tooltip (_piano_max_velocity, "Velocity to use when clicking at the bottom-end of a key.");

	set_tooltip (_send_panic, "Send MIDI Panic message for current channel");

	_pitch_slider_tooltip->set_tip ("Pitchbend: 8192");

	/* config */
	Table* cfg_tbl = manage (new Table);
	cfg_tbl->attach (_yaxis_velocity,                      0,  1, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Velocity:"))), 0,  1, 1, 2, SHRINK, SHRINK, 4, 0);

	cfg_tbl->attach (_piano_min_velocity,                  1,  2, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Min"))),       1,  2, 1, 2, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (_piano_max_velocity,                  2,  3, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Max"))),       2,  3, 1, 2, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (_piano_key_velocity,                  3,  4, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Key"))),       3,  4, 1, 2, SHRINK, SHRINK, 4, 0);

	cfg_tbl->attach (*manage (new ArdourVSpacer),          4,  5, 0, 2, SHRINK, FILL,   4, 0);

	cfg_tbl->attach (_piano_octave_key,                    5,  6, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Octave"))),    5,  6, 1, 2, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (_piano_octave_range,                  6,  7, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Range"))),     6,  7, 1, 2, SHRINK, SHRINK, 4, 0);

	cfg_tbl->attach (*manage (new ArdourVSpacer),          7,  8, 0, 2, SHRINK, FILL,   4, 0);

	cfg_tbl->attach (_highlight_grand_piano,               8,  9, 0, 1, FILL,   SHRINK, 4, 1);
	cfg_tbl->attach (_highlight_key_range,                 8,  9, 1, 2, FILL,   SHRINK, 4, 1);

	cfg_tbl->attach (*manage (new ArdourVSpacer),          9, 10, 0, 2, SHRINK, FILL,   4, 0);

	cfg_tbl->attach (_show_note_label,                    10, 11, 0, 1, FILL,   SHRINK, 4, 1);
	cfg_tbl->attach (_keyboard_layout,                    10, 11, 1, 2, FILL,   SHRINK, 4, 1);
	cfg_tbl->show_all ();

	/* bank/patch */
	Table* pgm_tbl = manage (new Table);

	Label* lbl = manage (new Label (_("Note: Prefer\nTrack-controls")));
	lbl->set_justify (JUSTIFY_CENTER);

	pgm_tbl->attach (*lbl,                             0, 1, 0, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new ArdourVSpacer),      1, 2, 0, 2, SHRINK, FILL,   4, 0);
	pgm_tbl->attach (_bank_msb,                        2, 3, 0, 1, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (_bank_lsb,                        3, 4, 0, 1, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (_patchpgm,                        4, 5, 0, 1, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new Label (_("MSB"))),   2, 3, 1, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new Label (_("LSB"))),   3, 4, 1, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new Label (_("PGM"))),   4, 5, 1, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->attach (*manage (new ArdourVSpacer),      5, 6, 0, 2, SHRINK, FILL,   4, 0);
	pgm_tbl->attach (_send_panic,                      6, 7, 0, 2, SHRINK, SHRINK, 4, 0);
	pgm_tbl->show_all ();

	/* settings */
	Table* tbl = manage (new Table);
	tbl->attach (_piano_channel, 0, 1, 0, 1, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new Label (_("Channel"))), 0, 1, 1, 2, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new ArdourVSpacer), 1, 2, 0, 2, SHRINK, FILL, 4, 0);
	tbl->attach (*_pitch_slider, 2, 3, 0, 2, SHRINK, FILL, 4, 0);

	const char* default_cc[VKBD_NCTRLS] = { "7", "8", "1", "11", "91", "92", "93", "94" };

	int col = 3;
	for (int i = 0; i < VKBD_NCTRLS; ++i, ++col) {
		_cc[i]      = boost::shared_ptr<VKBDControl> (new VKBDControl ("CC"));
		_cc_knob[i] = manage (new ArdourKnob (ArdourKnob::default_elements, ArdourKnob::Flags (0)));
		_cc_knob[i]->set_controllable (_cc[i]);
		_cc_knob[i]->set_size_request (PX_SCALE (21), PX_SCALE (21));
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
		_cc_key[i].set_active (default_cc[i]);

		tbl->attach (*_cc_knob[i], col, col + 1, 0, 1, SHRINK, SHRINK, 4, 2);
		tbl->attach (_cc_key[i],   col, col + 1, 1, 2, SHRINK, SHRINK, 4, 2);

		_cc[i]->ValueChanged.connect_same_thread (_cc_connections,
		                                          boost::bind (&VirtualKeyboardWindow::control_change_event_handler, this, i, _1));
	}

	tbl->attach (*manage (new ArdourVSpacer), col, col + 1, 0, 2, SHRINK, FILL, 4, 0);
	++col;
	tbl->attach (_transpose_output,                      col, col + 1, 0, 1, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new Label (_("Transpose"))),   col, col + 1, 1, 2, SHRINK, SHRINK, 4, 0);

	/* main layout */
	Box* box1 = manage (new HBox ());
	box1->pack_start (*tbl, true, false);

	Box* box2 = manage (new VBox ());
	box2->pack_start (_pgm_display, false, false, 1);
	box2->pack_start (_cfg_display, false, false, 1);
	box1->pack_start (*box2, false, false);

	_cfg_box = manage (new HBox ());
	_cfg_box->pack_start (*cfg_tbl, true, false);
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

	_piano_octave_key.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_octave_key));
	_piano_octave_range.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_octave_range));

	_cfg_display.signal_button_release_event ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::toggle_config), false);
	_pgm_display.signal_button_release_event ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::toggle_bankpatch), false);
	_yaxis_velocity.signal_button_release_event ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::toggle_yaxis_velocity), false);
	_highlight_grand_piano.signal_button_release_event ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::toggle_highlight_piano), false);
	_highlight_key_range.signal_button_release_event ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::toggle_highlight_key), false);
	_show_note_label.signal_button_release_event ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::toggle_note_label), false);
	_send_panic.signal_button_release_event ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::send_panic_message), false);

	g_signal_connect (G_OBJECT (_piano), "note-on", G_CALLBACK (VirtualKeyboardWindow::_note_on_event_handler), this);
	g_signal_connect (G_OBJECT (_piano), "note-off", G_CALLBACK (VirtualKeyboardWindow::_note_off_event_handler), this);

	update_velocity_settings (0);
	update_octave_range ();

	set_keep_above (true);
	vbox->show_all ();
}

VirtualKeyboardWindow::~VirtualKeyboardWindow ()
{
	delete _pianomm;
	delete _pitch_slider_tooltip;
}

void
VirtualKeyboardWindow::set_session (ARDOUR::Session* s)
{
	ArdourWindow::set_session (s);

	if (!_session) {
		return;
	}

	XMLNode* node = _session->instant_xml (X_("VirtualKeyboard"));
	if (node) {
		set_state (*node);
	}
}

XMLNode&
VirtualKeyboardWindow::get_state ()
{
	XMLNode* node = new XMLNode (X_("VirtualKeyboard"));
	node->set_property (X_("YAxisVelocity"), _yaxis_velocity.get_active ());
	node->set_property (X_("HighlightGrandPiano"), _highlight_grand_piano.get_active ());
	node->set_property (X_("HighlightKeyRange"), _highlight_key_range.get_active ());
	node->set_property (X_("ShowNoteLabel"), _show_note_label.get_active ());
	node->set_property (X_("Layout"), _keyboard_layout.get_text ());
	node->set_property (X_("Channel"), _piano_channel.get_value_as_int ());
	node->set_property (X_("Transpose"), _transpose_output.get_value_as_int ());
	node->set_property (X_("MinVelocity"), _piano_min_velocity.get_value_as_int ());
	node->set_property (X_("MaxVelocity"), _piano_max_velocity.get_value_as_int ());
	node->set_property (X_("KeyVelocity"), _piano_key_velocity.get_value_as_int ());
	node->set_property (X_("Octave"), _piano_octave_key.get_value_as_int ());
	node->set_property (X_("Range"), _piano_octave_range.get_value_as_int ());
	for (int i = 0; i < VKBD_NCTRLS; ++i) {
		char buf[16];
		sprintf (buf, "CC-%d", i);
		node->set_property (buf, _cc_key[i].get_text ());
	}
	return *node;
}

void
VirtualKeyboardWindow::set_state (const XMLNode& root)
{
	if (root.name () != "VirtualKeyboard") {
		return;
	}

	XMLNode const* node = &root;

	std::string layout;
	if (node->get_property (X_("Layout"), layout)) {
		piano_keyboard_set_keyboard_layout (_piano, layout.c_str ());
		_keyboard_layout.set_active (layout);
	}

	for (int i = 0; i < VKBD_NCTRLS; ++i) {
		char buf[16];
		sprintf (buf, "CC-%d", i);
		std::string cckey;
		if (node->get_property (buf, cckey)) {
			_cc_key[i].set_active (cckey);
		}
	}

	bool a;
	if (node->get_property (X_("YAxisVelocity"), a)) {
		_yaxis_velocity.set_active (a);
	}
	if (node->get_property (X_("HighlightGrandPiano"), a)) {
		_highlight_grand_piano.set_active (a);
		piano_keyboard_set_grand_piano_highlight (_piano, a);
	}
	if (node->get_property (X_("HighlightKeyRange"), a)) {
		_highlight_key_range.set_active (a);
		piano_keyboard_set_keyboard_cue (_piano, a);
	}
	if (node->get_property (X_("ShowNoteLabel"), a)) {
		_show_note_label.set_active (a);
		piano_keyboard_show_note_label (_piano, a);
	}

	int v;
	if (node->get_property (X_("Channel"), v)) {
		_piano_channel.set_value (v);
	}
	if (node->get_property (X_("Transpose"), v)) {
		_transpose_output.set_value (v);
	}
	if (node->get_property (X_("MinVelocity"), v)) {
		_piano_min_velocity.set_value (v);
	}
	if (node->get_property (X_("MaxVelocity"), v)) {
		_piano_max_velocity.set_value (v);
	}
	if (node->get_property (X_("KeyVelocity"), v)) {
		_piano_key_velocity.set_value (v);
	}
	if (node->get_property (X_("Octave"), v)) {
		_piano_octave_key.set_value (v);
	}
	if (node->get_property (X_("Range"), v)) {
		_piano_octave_range.set_value (v);
	}

	update_velocity_settings (0);
	update_octave_range ();
	update_octave_key ();
}

void
VirtualKeyboardWindow::on_unmap ()
{
	ArdourWindow::on_unmap ();
	ARDOUR_UI::instance ()->reset_focus (this);
}

bool
VirtualKeyboardWindow::on_key_press_event (GdkEventKey* ev)
{
	return ARDOUR_UI_UTILS::relay_key_press (ev, this);
}

void
VirtualKeyboardWindow::select_keyboard_layout (std::string const& l)
{
	piano_keyboard_set_keyboard_layout (_piano, l.c_str ());
	_keyboard_layout.set_active (l);
}

bool
VirtualKeyboardWindow::toggle_config (GdkEventButton* ev)
{
	bool a = !_cfg_display.get_active ();
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
	bool a = !_pgm_display.get_active ();
	_pgm_display.set_active (a);
	if (a) {
		_pgm_box->show ();
	} else {
		_pgm_box->hide ();
	}
	return false;
}

void
VirtualKeyboardWindow::update_octave_key ()
{
	piano_keyboard_set_octave (_piano, _piano_octave_key.get_value_as_int ());
}

void
VirtualKeyboardWindow::update_octave_range ()
{
	piano_keyboard_set_octave_range (_piano, _piano_octave_range.get_value_as_int ());
}

bool
VirtualKeyboardWindow::toggle_yaxis_velocity (GdkEventButton*)
{
	_yaxis_velocity.set_active (!_yaxis_velocity.get_active ());
	update_velocity_settings (0);
	return false;
}

bool
VirtualKeyboardWindow::toggle_highlight_piano (GdkEventButton*)
{
	bool a = !_highlight_grand_piano.get_active ();
	_highlight_grand_piano.set_active (a);
	piano_keyboard_set_grand_piano_highlight (_piano, a);
	return false;
}

bool
VirtualKeyboardWindow::toggle_highlight_key (GdkEventButton*)
{
	bool a = !_highlight_key_range.get_active ();
	_highlight_key_range.set_active (a);
	piano_keyboard_set_keyboard_cue (_piano, a);
	return false;
}

bool
VirtualKeyboardWindow::toggle_note_label (GdkEventButton*)
{
	bool a = !_show_note_label.get_active ();
	_show_note_label.set_active (a);
	piano_keyboard_show_note_label (_piano, a);
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
	_session->vkbd_output_port ()->write (ev, 3, 0);
	ev[1] = MIDI_CTL_ALL_NOTES_OFF;
	_session->vkbd_output_port ()->write (ev, 3, 0);
	ev[1] = MIDI_CTL_RESET_CONTROLLERS;
	_session->vkbd_output_port ()->write (ev, 3, 0);
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
	_session->vkbd_output_port ()->write (ev, 3, 0);
	ev[1] = MIDI_CTL_LSB_BANK | channel;
	ev[2] = lsb & 0x7f;
	_session->vkbd_output_port ()->write (ev, 3, 0);
	ev[0] = MIDI_CMD_PGM_CHANGE | channel;
	ev[1] = pgm & 0x7f;
	_session->vkbd_output_port ()->write (ev, 2, 0);
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
		                               _piano_key_velocity.get_value_as_int ());
	} else {
		piano_keyboard_set_velocities (_piano,
		                               _piano_key_velocity.get_value_as_int (),
		                               _piano_key_velocity.get_value_as_int (),
		                               _piano_key_velocity.get_value_as_int ());
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
	snprintf (buf, sizeof (buf), "Pitchbend: %.0f", _pitch_adjustment.get_value ());
	_pitch_slider_tooltip->set_tip (buf);
}

void
VirtualKeyboardWindow::note_on_event_handler (int note, int velocity)
{
	_pianomm->grab_focus ();
	if (!_session) {
		return;
	}
	note += _transpose_output.get_value_as_int ();
	if (note < 0 || note > 127) {
		return;
	}
	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = MIDI_CMD_NOTE_ON | channel;
	ev[1] = note;
	ev[2] = velocity;
	_session->vkbd_output_port ()->write (ev, 3, 0);
}

void
VirtualKeyboardWindow::note_off_event_handler (int note)
{
	if (!_session) {
		return;
	}
	note += _transpose_output.get_value_as_int ();
	if (note < 0 || note > 127) {
		return;
	}
	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = MIDI_CMD_NOTE_OFF | channel;
	ev[1] = note;
	ev[2] = 0;
	_session->vkbd_output_port ()->write (ev, 3, 0);
}

void
VirtualKeyboardWindow::control_change_event_handler (int key, int val)
{
	if (!_session) {
		return;
	}
	assert (key >= 0 && key < VKBD_NCTRLS);
	int ctrl = PBD::atoi (_cc_key[key].get_text ());
	assert (ctrl > 0 && ctrl < 127);
	uint8_t channel = _piano_channel.get_value_as_int () - 1;
	uint8_t ev[3];
	ev[0] = MIDI_CMD_CONTROL | channel;
	ev[1] = ctrl;
	ev[2] = val;
	_session->vkbd_output_port ()->write (ev, 3, 0);
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
	_session->vkbd_output_port ()->write (ev, 3, 0);
}
