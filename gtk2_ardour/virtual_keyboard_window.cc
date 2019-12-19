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
	, _send_panic (_("Panic"), ArdourButton::default_elements)
	, _piano_key_velocity (*manage (new Adjustment (100, 1, 127, 1, 16)))
	, _piano_octave_key (*manage (new Adjustment (4, -1, 7, 1, 1)))
	, _piano_octave_range (*manage (new Adjustment (7, 2, 11, 1, 1)))
	, _pitch_adjustment (8192, 0, 16383, 1, 256)
	, _modwheel_adjustment (0, 0, 127, 1, 8)
{
	UIConfiguration::instance().ParameterChanged.connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::parameter_changed));

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

	_midi_channel.set_active ("1");
	_transpose_output.set_active ("0");

	_send_panic.set_can_focus (false);

	_pitchbend            = boost::shared_ptr<VKBDControl> (new VKBDControl ("PB", 8192, 16383));
	_pitch_slider         = manage (new VSliderController (&_pitch_adjustment, _pitchbend, 0, PX_SCALE (15)));
	_pitch_slider_tooltip = new Gtkmm2ext::PersistentTooltip (_pitch_slider);

	_modwheel         = boost::shared_ptr<VKBDControl> (new VKBDControl ("MW", 0, 127));
	_modwheel_slider  = manage (new VSliderController (&_modwheel_adjustment, _modwheel, 0, PX_SCALE (15)));
	_modwheel_tooltip = new Gtkmm2ext::PersistentTooltip (_modwheel_slider);

	_pitch_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::pitch_slider_adjusted));
	_pitchbend->ValueChanged.connect_same_thread (_cc_connections, boost::bind (&VirtualKeyboardWindow::pitch_bend_event_handler, this, _1));
	_pitch_slider->StopGesture.connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::pitch_bend_release));

	_modwheel_adjustment.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::modwheel_slider_adjusted));
	_modwheel->ValueChanged.connect_same_thread (_cc_connections, boost::bind (&VirtualKeyboardWindow::control_change_event_handler, this, 1, _1));

	_piano_key_velocity.set_numeric (true);
	_piano_octave_key.set_numeric (true);
	_piano_octave_range.set_numeric (true);

	set_tooltip (_piano_octave_key, _("The center octave, and lowest octave for keyboard control. Change with Arrow left/right."));
	set_tooltip (_piano_octave_range, _("Available octave range, centered around the key-octave."));
	set_tooltip (_piano_key_velocity, _("The default velocity to use with keyboard control, and when y-axis click-position is disabled."));

	set_tooltip (_send_panic, _("Send MIDI Panic message for current channel"));

	pitch_bend_update_tooltip (8192);
	_pitch_slider->set_can_focus (false);

	/* settings */
	Table* tbl = manage (new Table);
	tbl->attach (_midi_channel, 0, 1, 0, 1, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new Label (_("Channel"))), 0, 1, 1, 2, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new ArdourVSpacer), 1, 2, 0, 2, SHRINK, FILL, 4, 0);
	tbl->attach (*_pitch_slider, 2, 3, 0, 2, SHRINK, FILL, 4, 0);
	tbl->attach (*_modwheel_slider, 3, 4, 0, 2, SHRINK, FILL, 4, 0);

	const int default_cc[VKBD_NCTRLS] = { 7, 8, 91, 93};

	int col = 4;
	for (size_t i = 0; i < VKBD_NCTRLS; ++i, ++col) {
		_cc[i]      = boost::shared_ptr<VKBDControl> (new VKBDControl ("CC"));
		_cc_knob[i] = manage (new ArdourKnob (ArdourKnob::default_elements, ArdourKnob::Flags (0)));
		_cc_knob[i]->set_controllable (_cc[i]);
		_cc_knob[i]->set_size_request (PX_SCALE (21), PX_SCALE (21));
		_cc_knob[i]->set_name ("monitor section knob");

		for (int c = 2; c < 120; ++c) {
			if (c == 32) {
				continue;
			}
			char key[32];
			sprintf (key, "%d", c);
			_cc_key[i].append_text_item (key);
		}
		update_cc (i, default_cc[i]);

		tbl->attach (*_cc_knob[i], col, col + 1, 0, 1, SHRINK, SHRINK, 4, 2);
		tbl->attach (_cc_key[i],   col, col + 1, 1, 2, SHRINK, SHRINK, 4, 2);

		_cc_key[i].StateChanged.connect (sigc::bind (sigc::mem_fun (*this, &VirtualKeyboardWindow::cc_key_changed), i));
		_cc[i]->ValueChanged.connect_same_thread (_cc_connections,
		                                          boost::bind (&VirtualKeyboardWindow::control_change_knob_event_handler, this, i, _1));
	}

	tbl->attach (*manage (new ArdourVSpacer),       col, col + 1, 0, 2, SHRINK, FILL, 4, 0);
	++col;
	tbl->attach (_piano_octave_key,                 col, col + 1, 0, 1, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new Label (_("Octave"))), col, col + 1, 1, 2, SHRINK, SHRINK, 4, 0);
	++col;
	tbl->attach (_piano_octave_range,               col, col + 1, 0, 1, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new Label (_("Range"))),  col, col + 1, 1, 2, SHRINK, SHRINK, 4, 0);
	++col;

	tbl->attach (*manage (new ArdourVSpacer),     col, col + 1, 0, 2, SHRINK, FILL, 4, 0);
	++col;
	tbl->attach (_piano_key_velocity,             col, col + 1, 0, 1, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new Label (_("Vel."))), col, col + 1, 1, 2, SHRINK, SHRINK, 4, 0);
	++col;

	tbl->attach (*manage (new ArdourVSpacer),          col, col + 1, 0, 2, SHRINK, FILL, 4, 0);
	++col;
	tbl->attach (_transpose_output,                    col, col + 1, 0, 1, SHRINK, SHRINK, 4, 0);
	tbl->attach (*manage (new Label (_("Transpose"))), col, col + 1, 1, 2, SHRINK, SHRINK, 4, 0);
	++col;
	tbl->attach (_send_panic,                      col, col + 1, 0, 2, SHRINK, SHRINK, 4, 0);

	/* main layout */
	Box* box1 = manage (new HBox ());
	box1->pack_start (*tbl, true, false);

	VBox* vbox = manage (new VBox);
	vbox->pack_start (*box1, false, false, 4);
	vbox->pack_start (_piano, true, true);
	add (*vbox);

	_piano_key_velocity.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_velocity_settings));

	_piano_octave_key.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_octave_key));
	_piano_octave_range.signal_value_changed ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::update_octave_range));

	_send_panic.signal_button_release_event ().connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::send_panic_message), false);


	_piano.NoteOn.connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::note_on_event_handler));
	_piano.NoteOff.connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::note_off_event_handler));
	_piano.SwitchOctave.connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::octave_key_event_handler));
	_piano.PitchBend.connect (sigc::mem_fun (*this, &VirtualKeyboardWindow::pitch_bend_key_event_handler));

	update_velocity_settings ();
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
void
VirtualKeyboardWindow::parameter_changed (std::string const& p)
{
	if (p == "vkeybd-layout") {
		select_keyboard_layout (UIConfiguration::instance().get_vkeybd_layout ());
	}
}

XMLNode&
VirtualKeyboardWindow::get_state ()
{
	XMLNode* node = new XMLNode (X_("VirtualKeyboard"));
	node->set_property (X_("Channel"), _midi_channel.get_text ());
	node->set_property (X_("Transpose"), _transpose_output.get_text ());
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

	for (int i = 0; i < VKBD_NCTRLS; ++i) {
		char buf[16];
		sprintf (buf, "CC-%d", i);
		std::string cckey;
		if (node->get_property (buf, cckey)) {
			update_cc (i, PBD::atoi (cckey));
		}
	}

	int v;
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
	if (node->get_property (X_("KeyVelocity"), v)) {
		_piano_key_velocity.set_value (v);
	}
	if (node->get_property (X_("Octave"), v)) {
		_piano_octave_key.set_value (v);
	}
	if (node->get_property (X_("Range"), v)) {
		_piano_octave_range.set_value (v);
	}

	update_velocity_settings ();
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

	return ArdourWindow::on_key_release_event (ev);
}

void
VirtualKeyboardWindow::select_keyboard_layout (std::string const& l)
{
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
	}
	_piano.grab_focus ();
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
VirtualKeyboardWindow::update_velocity_settings ()
{
	_piano.set_velocities (_piano_key_velocity.get_value_as_int (),
	                       _piano_key_velocity.get_value_as_int (),
	                       _piano_key_velocity.get_value_as_int ());
}

void
VirtualKeyboardWindow::cc_key_changed (size_t i)
{
	int ctrl = PBD::atoi (_cc_key[i].get_text ());
	update_cc (i, ctrl);
}

void
VirtualKeyboardWindow::update_cc (size_t i, int cc)
{
	assert (i < VKBD_NCTRLS);
	if (cc < 0 || cc > 120) {
		return;
	}
	char buf[16];
	sprintf (buf, "%d", cc);
	_cc_key[i].set_active (buf);
	_cc_knob[i]->set_tooltip_prefix (string_compose (_("CC-%1: "), cc));
	// TODO update _cc[i]->normal
}

void
VirtualKeyboardWindow::octave_key_event_handler (bool up)
{
	if (up) {
		_piano_octave_key.set_value (_piano_octave_key.get_value_as_int () + 1);
	} else {
		_piano_octave_key.set_value (_piano_octave_key.get_value_as_int () - 1);
	}
}

void
VirtualKeyboardWindow::pitch_bend_key_event_handler (int target, bool interpolate)
{
	int cur = _pitch_adjustment.get_value();
	if (cur == target) {
		return;
	}
	if (interpolate) {
		_pitch_bend_target = target;
		if (!_bender_connection.connected ()) {
			float tc = _pitch_bend_target == 8192 ? .35 : .51;
			cur = rintf (cur + tc * (_pitch_bend_target - cur));
			_pitch_adjustment.set_value (cur);
			_bender_connection =  Glib::signal_timeout().connect (sigc::mem_fun(*this, &VirtualKeyboardWindow::pitch_bend_timeout), 20 /*ms*/);
		}
		return;
	}
		_bender_connection.disconnect ();
	_pitch_adjustment.set_value (target);
	_pitch_bend_target = target;
}

bool
VirtualKeyboardWindow::pitch_bend_timeout ()
{
	int cur = _pitch_adjustment.get_value();

	/* a spring would be 2nd order with overshoot,
	 * but we assume it's critically damped */
	float tc = _pitch_bend_target == 8192 ? .35 : .51;
	cur = rintf (cur + tc * (_pitch_bend_target - cur));
	if (abs (cur - _pitch_bend_target) < 2) {
		cur = _pitch_bend_target;
	}
	_pitch_adjustment.set_value (cur);
	return _pitch_bend_target != cur;
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
VirtualKeyboardWindow::modwheel_slider_adjusted ()
{
	_modwheel->set_value (_modwheel_adjustment.get_value (), PBD::Controllable::NoGroup);
	modwheel_update_tooltip (_modwheel_adjustment.get_value ());
}

void
VirtualKeyboardWindow::modwheel_update_tooltip (int value)
{
	_modwheel_tooltip->set_tip (string_compose (_("Modulation: %1"), value));
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
VirtualKeyboardWindow::control_change_knob_event_handler (int key, int val)
{
	assert (key >= 0 && key < VKBD_NCTRLS);
	int ctrl = PBD::atoi (_cc_key[key].get_text ());
	assert (ctrl > 0 && ctrl < 127);
	control_change_event_handler (ctrl, val);
}

void
VirtualKeyboardWindow::control_change_event_handler (int ctrl, int val)
{
	if (!_session) {
		return;
	}
	uint8_t channel = PBD::atoi (_midi_channel.get_text ()) - 1;
	uint8_t ev[3];
	ev[0] = MIDI_CMD_CONTROL | channel;
	ev[1] = ctrl & 0x7f;
	ev[2] = val & 0x7f;
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
