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
#include "pbd/compose.h"

#include "ardour/async_midi_port.h"
#include "ardour/session.h"

#include "gtkmm2ext/utils.h"
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
	, _bank_msb (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _bank_lsb (*manage (new Adjustment (0, 0, 127, 1, 16)))
	, _patchpgm (*manage (new Adjustment (1, 1, 128, 1, 16)))
	, _cfg_display (S_("Virtual keyboard|Config"), ArdourButton::led_default_elements)
	, _pgm_display (_("Bank/Patch"), ArdourButton::led_default_elements)
	, _yaxis_velocity (_("Y-Axis"), ArdourButton::led_default_elements)
	, _send_panic (_("Panic"), ArdourButton::default_elements)
	, _piano_key_velocity (*manage (new Adjustment (100, 1, 127, 1, 16)))
	, _piano_min_velocity (*manage (new Adjustment (  1, 1, 127, 1, 16)))
	, _piano_max_velocity (*manage (new Adjustment (127, 1, 127, 1, 16)))
	, _piano_octave_key (*manage (new Adjustment (4, -1, 7, 1, 1)))
	, _piano_octave_range (*manage (new Adjustment (7, 2, 11, 1, 1)))
	, _pitch_adjustment (8192, 0, 16383, 1, 256)
{
	_piano.set_flags (Gtk::CAN_FOCUS);

	_piano.set_keyboard_layout (APianoKeyboard::S_QWERTY);
	_piano.set_annotate_octave (true);
	_piano.set_grand_piano_highlight (false);
	_piano.set_annotate_layout (true);
	_piano.set_annotate_octave (true);

	for (int c = 0; c < 16; ++c) {
		char buf[16];
		sprintf (buf, "%d", c + 1);
		_midi_channel.append_text_item (buf);
	}
	for (int t = -12; t < 13; ++t) {
		char buf[16];
		sprintf (buf, "%d", t);
		_transpose_output.append_text_item (buf);
	}

	using namespace Menu_Helpers;
	_keyboard_layout.AddMenuElem (MenuElem (_("QWERTY"), sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "QWERTY")));
	_keyboard_layout.AddMenuElem (MenuElem (_("QWERTZ"), sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "QWERTZ")));
	_keyboard_layout.AddMenuElem (MenuElem (_("AZERTY"), sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "AZERTY")));
	_keyboard_layout.AddMenuElem (MenuElem (_("DVORAK"), sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "DVORAK")));
	_keyboard_layout.AddMenuElem (MenuElem (_("QWERTY Single"), sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "QWERTY Single")));
	_keyboard_layout.AddMenuElem (MenuElem (_("QWERTZ Single"), sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::select_keyboard_layout), "QWERTZ Single")));

	Gtkmm2ext::set_size_request_to_display_given_text_width (_keyboard_layout, _("QWERTZ Single"), 2, 0); // Longest Text
	_keyboard_layout.set_active (_("QWERTY Single"));
	_midi_channel.set_active ("1");
	_transpose_output.set_active ("0");

	_cfg_display.set_active (false);
	_pgm_display.set_active (false);
	_yaxis_velocity.set_active (false);

	_send_panic.set_can_focus (false);

	_pitchbend            = boost::shared_ptr<VKBDControl> (new VKBDControl ("PB", 8192, 16383));
	_pitch_slider         = manage (new VSliderController (&_pitch_adjustment, _pitchbend, 0, PX_SCALE (15)));
	_pitch_slider_tooltip = new Gtkmm2ext::PersistentTooltip (_pitch_slider);

	_pitch_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::pitch_slider_adjusted));
	_pitchbend->ValueChanged.connect_same_thread (_cc_connections, boost::bind (&VirtualKeyboardWindow::pitch_bend_event_handler, this, _1));
	_pitch_slider->StopGesture.connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::pitch_bend_release));

	set_tooltip (_yaxis_velocity, _("When enabled, mouse-click y-axis position defines the velocity."));

	set_tooltip (_piano_octave_key, _("The center octave, and lowest octave for keyboard control. Change with Arrow left/right."));
	set_tooltip (_piano_octave_range, _("Available octave range, centered around the key-octave."));
	set_tooltip (_keyboard_layout, _("Keyboard layout to use for keyboard control."));

	set_tooltip (_piano_key_velocity, _("The default velocity to use with keyboard control, and when y-axis click-position is disabled."));
	set_tooltip (_piano_min_velocity, _("Velocity to use when clicking at the top-end of a key."));
	set_tooltip (_piano_max_velocity, _("Velocity to use when clicking at the bottom-end of a key."));

	set_tooltip (_send_panic, _("Send MIDI Panic message for current channel"));

	pitch_bend_update_tooltip (8192);
	_pitch_slider->set_can_focus (false);

	/* config */
	Table* cfg_tbl = manage (new Table);
	cfg_tbl->attach (_yaxis_velocity,                      0, 1, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Velocity:"))), 0, 1, 1, 2, SHRINK, SHRINK, 4, 0);

	cfg_tbl->attach (_piano_min_velocity,                  1, 2, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Min"))),       1, 2, 1, 2, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (_piano_max_velocity,                  2, 3, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Max"))),       2, 3, 1, 2, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (_piano_key_velocity,                  3, 4, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Key"))),       3, 4, 1, 2, SHRINK, SHRINK, 4, 0);

	cfg_tbl->attach (*manage (new ArdourVSpacer),          4, 5, 0, 2, SHRINK, FILL,   4, 0);

	cfg_tbl->attach (_piano_octave_key,                    5, 6, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Octave"))),    5, 6, 1, 2, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (_piano_octave_range,                  6, 7, 0, 1, SHRINK, SHRINK, 4, 0);
	cfg_tbl->attach (*manage (new Label (_("Range"))),     6, 7, 1, 2, SHRINK, SHRINK, 4, 0);

	cfg_tbl->attach (*manage (new ArdourVSpacer),          7, 8, 0, 2, SHRINK, FILL,   4, 0);

	cfg_tbl->attach (_keyboard_layout,                     8, 9, 0, 2, FILL,   SHRINK, 4, 1);
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
	tbl->attach (_midi_channel, 0, 1, 0, 1, SHRINK, SHRINK, 4, 0);
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
	vbox->pack_start (_piano, true, true);
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
	_send_panic.signal_button_release_event ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::send_panic_message), false);


	_piano.NoteOn.connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::note_on_event_handler));
	_piano.NoteOff.connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::note_off_event_handler));

	update_velocity_settings (0);
	update_octave_range ();

	set_keep_above (true);
	vbox->show_all ();
}

VirtualKeyboardWindow::~VirtualKeyboardWindow ()
{
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
	node->set_property (X_("Layout"), _keyboard_layout.get_text ());
	node->set_property (X_("Channel"), _midi_channel.get_text ());
	node->set_property (X_("Transpose"), _transpose_output.get_text ());
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
		select_keyboard_layout (layout);
	}

	for (int i = 0; i < VKBD_NCTRLS; ++i) {
		char buf[16];
		sprintf (buf, "CC-%d", i);
		std::string cckey;
		if (node->get_property (buf, cckey)) {
			_cc_key[i].set_active (cckey);
		}
	}

	std::string s;
	if (node->get_property (X_("Channel"), s)) {
		uint8_t channel = PBD::atoi (_midi_channel.get_text ());
		if (channel > 0 && channel < 17) {
			_midi_channel.set_active (s);
		}
	}
	if (node->get_property (X_("Transpose"), s)) {
		_transpose_output.set_active (s);
	}

	bool a;
	if (node->get_property (X_("YAxisVelocity"), a)) {
		_yaxis_velocity.set_active (a);
	}

	int v;
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

bool
VirtualKeyboardWindow::on_focus_in_event (GdkEventFocus* ev)
{
	_piano.grab_focus ();
	return ArdourWindow::on_focus_in_event (ev);
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
	/* try propagate unmodified events first */
	if ((ev->state & 0xf) == 0) {
		if (gtk_window_propagate_key_event (gobj(), ev)) {
			return true;
		}
	}

	_piano.grab_focus ();

	/* handle up/down */
	// XXX consider to handle these in APianoKeyboard::on_key_press_event
	// and use signals. -- also subscribe SustainChanged, indicate sustain.
	// TODO: pitch-bend shortcuts
	if (ev->type == GDK_KEY_PRESS) {
		switch (ev->keyval) {
			case GDK_KEY_Left:
				_piano_octave_key.set_value (_piano_octave_key.get_value_as_int () - 1);
				return true;
			case GDK_KEY_Right:
				_piano_octave_key.set_value (_piano_octave_key.get_value_as_int () + 1);
				return true;
			case GDK_KEY_F1:
				_pitch_adjustment.set_value (0);
				return true;
			case GDK_KEY_F2:
				_pitch_adjustment.set_value (4096);
				return true;
			case GDK_KEY_F3:
				_pitch_adjustment.set_value (12288);
				return true;
			case GDK_KEY_F4:
				_pitch_adjustment.set_value (16383);
				return true;
			case GDK_KEY_Down:
				_pitch_adjustment.set_value (std::max(0., _pitch_adjustment.get_value() - 1024));
				return true;
			case GDK_KEY_Up:
				_pitch_adjustment.set_value (std::min(16383., _pitch_adjustment.get_value() + 1024));
				return true;
			default:
				break;
		}
	}

	return ARDOUR_UI_UTILS::relay_key_press (ev, this);
}

bool
VirtualKeyboardWindow::on_key_release_event (GdkEventKey* ev)
{
	/* try propagate unmodified events first */
	if ((ev->state & 0xf) == 0) {
		if (gtk_window_propagate_key_event (gobj(), ev)) {
			return true;
		}
	}

	_piano.grab_focus ();

	if (ev->type == GDK_KEY_RELEASE) {
		switch (ev->keyval) {
			case GDK_KEY_F1:
				/* fallthrough */
			case GDK_KEY_F2:
				/* fallthrough */
			case GDK_KEY_F3:
				/* fallthrough */
			case GDK_KEY_F4:
				/* fallthrough */
			case GDK_KEY_Up:
				/* fallthrough */
			case GDK_KEY_Down:
				_pitch_adjustment.set_value (8192);
				return true;
			default:
				break;
		}
	}

	return ArdourWindow::on_key_release_event (ev);
}

void
VirtualKeyboardWindow::select_keyboard_layout (std::string const& l)
{
	_keyboard_layout.set_active (l);
	if (l == "QWERTY") {
		_piano.set_keyboard_layout (APianoKeyboard::QWERTY);
	} else if (l == "QWERTZ") {
		_piano.set_keyboard_layout (APianoKeyboard::QWERTZ);
	} else if (l == "AZERTY") {
		_piano.set_keyboard_layout (APianoKeyboard::AZERTY);
	} else if (l == "DVORAK") {
		_piano.set_keyboard_layout (APianoKeyboard::DVORAK);
	} else if (l == "QWERTY Single") {
		_piano.set_keyboard_layout (APianoKeyboard::S_QWERTY);
	} else if (l == "QWERTZ Single") {
		_piano.set_keyboard_layout (APianoKeyboard::S_QWERTZ);
	} else {
	_keyboard_layout.set_active ("QWERTY");
	}
	_piano.grab_focus ();
}

bool
VirtualKeyboardWindow::toggle_config (GdkEventButton* ev)
{
	bool a = !_cfg_display.get_active ();
	_cfg_display.set_active (a);
	if (a) {
		_cfg_box->show ();
	} else {
		const int child_height = _cfg_box->get_height ();
		_cfg_box->hide ();
		Gtk::Requisition wr;
		get_size (wr.width, wr.height);
		wr.height -= child_height;
		resize (wr.width, wr.height);
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
		const int child_height = _pgm_box->get_height ();
		_pgm_box->hide ();
		Gtk::Requisition wr;
		get_size (wr.width, wr.height);
		wr.height -= child_height;
		resize (wr.width, wr.height);
	}
	return false;
}

void
VirtualKeyboardWindow::update_octave_key ()
{
	_piano.set_octave (_piano_octave_key.get_value_as_int ());
	_piano.grab_focus ();
}

void
VirtualKeyboardWindow::update_octave_range ()
{
	_piano.set_octave_range (_piano_octave_range.get_value_as_int ());
	_piano.set_grand_piano_highlight (_piano_octave_range.get_value_as_int () > 3);
	_piano.grab_focus ();
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
	uint8_t channel = PBD::atoi (_midi_channel.get_text ()) - 1;
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

	uint8_t channel = PBD::atoi (_midi_channel.get_text ()) - 1;
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
		_piano.set_velocities (_piano_min_velocity.get_value_as_int (),
		                       _piano_max_velocity.get_value_as_int (),
		                       _piano_key_velocity.get_value_as_int ());
	} else {
		_piano.set_velocities (_piano_key_velocity.get_value_as_int (),
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
	_piano.grab_focus ();
}

void
VirtualKeyboardWindow::pitch_slider_adjusted ()
{
	_pitchbend->set_value (_pitch_adjustment.get_value (), PBD::Controllable::NoGroup);
	pitch_bend_update_tooltip (_pitch_adjustment.get_value ());
}

void
VirtualKeyboardWindow::pitch_bend_update_tooltip (int value)
{
	_pitch_slider_tooltip->set_tip (string_compose (
	      _("Pitchbend: %1\n"
	        "Use mouse-drag for sprung mode,\n"
	        "mouse-wheel for presisent bends.\n"
	        "F1-F4 and arrow-up/down keys jump\n"
	        "to select values."), value));
}


void
VirtualKeyboardWindow::note_on_event_handler (int note, int velocity)
{
	_piano.grab_focus ();
	if (!_session) {
		return;
	}
	note += PBD::atoi (_transpose_output.get_text ());
	if (note < 0 || note > 127) {
		return;
	}
	uint8_t channel = PBD::atoi (_midi_channel.get_text ()) - 1;
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
	note += PBD::atoi (_transpose_output.get_text ());
	if (note < 0 || note > 127) {
		return;
	}
	uint8_t channel = PBD::atoi (_midi_channel.get_text ()) - 1;
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
	uint8_t channel = PBD::atoi (_midi_channel.get_text ()) - 1;
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
	uint8_t channel = PBD::atoi (_midi_channel.get_text ()) - 1;
	uint8_t ev[3];
	ev[0] = MIDI_CMD_BENDER | channel;
	ev[1] = val & 0x7f;
	ev[2] = (val >> 7) & 0x7f;
	_session->vkbd_output_port ()->write (ev, 3, 0);
}

void
VirtualKeyboardWindow::pitch_bend_release ()
{
	_pitch_adjustment.set_value (8192);
}
