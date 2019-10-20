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

#ifndef _virtual_keyboard_window_h_
#define _virtual_keyboard_window_h_

#include <gtkmm/box.h>
#include <gtkmm/spinbutton.h>

#include "pbd/signals.h"
#include "pbd/controllable.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_knob.h"

#include "ardour_window.h"
#include "gtk_pianokeyboard.h"

namespace ARDOUR {
	class Session;
}

class VKBDControl : public PBD::Controllable {
public:
	VKBDControl (const std::string& name, double normal = 127)
		: PBD::Controllable (name, Flag(0))
		, _lower (0)
		, _upper (127)
		, _normal (normal)
		, _value (normal)
	{}

	/* Controllable API */
	void set_value (double v, PBD::Controllable::GroupControlDisposition gcd) {
		if (v != _value) {
			_value = std::max (_lower, std::min (_upper, v));
			Changed (true, gcd); /* EMIT SIGNAL */
			ValueChanged ((int)_value); /* EMIT SIGNAL */
		}
	}

	double get_value () const {
		return _value;
	}

	std::string get_user_string () const
	{
		char buf[32];
		sprintf (buf, "%.0f", get_value());
		return std::string(buf);
	}

	double lower () const { return _lower; }
	double upper () const { return _upper; }
	double normal () const { return _normal; }

	PBD::Signal1<void, int> ValueChanged;

protected:
	double _lower;
	double _upper;
	double _normal;
	double _value;
};


class VirtualKeyboardWindow : public ArdourWindow
{
public:
	VirtualKeyboardWindow ();
	~VirtualKeyboardWindow ();

private:
	static void _note_on_event_handler (GtkWidget*, int note, int vel, gpointer arg)
	{
		static_cast<VirtualKeyboardWindow*>(arg)->note_on_event_handler(note, vel);
	}

	static void _note_off_event_handler (GtkWidget*, int note, gpointer arg) 
	{
		static_cast<VirtualKeyboardWindow*>(arg)->note_off_event_handler(note);
	}

	void on_unmap ();
	bool on_key_press_event (GdkEventKey*);

	void note_on_event_handler  (int, int);
	void note_off_event_handler (int);
	void control_change_event_handler (int, int);

	void select_keyboard_layout (int);
	void update_velocity_settings (int);
	void bank_patch ();
	void update_sensitivity ();
	bool toggle_config (GdkEventButton*);
	bool toggle_bankpatch (GdkEventButton*);
	bool toggle_yaxis_velocity (GdkEventButton*);
	bool send_panic_message (GdkEventButton*);

	PianoKeyboard*  _piano;
	Gtk::Widget*    _pianomm;
	Gtk::SpinButton _piano_channel;

	Gtk::SpinButton _bank_msb;
	Gtk::SpinButton _bank_lsb;
	Gtk::SpinButton _patchpgm;

	Gtk::HBox* _cfg_box;
	Gtk::HBox* _pgm_box;

	ArdourWidgets::ArdourButton   _cfg_display;
	ArdourWidgets::ArdourButton   _pgm_display;
	ArdourWidgets::ArdourButton   _yaxis_velocity;
	ArdourWidgets::ArdourButton   _send_panic;
	ArdourWidgets::ArdourDropdown _keyboard_layout;

	Gtk::SpinButton _piano_key_velocity;
	Gtk::SpinButton _piano_min_velocity;
	Gtk::SpinButton _piano_max_velocity;

#define VKBD_NCTRLS 8

	boost::shared_ptr<VKBDControl> _cc[VKBD_NCTRLS];
	ArdourWidgets::ArdourKnob*     _cc_knob[VKBD_NCTRLS];
	ArdourWidgets::ArdourDropdown  _cc_key[VKBD_NCTRLS];

	PBD::ScopedConnectionList _cc_connections;
};

#endif
