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

#include "pbd/controllable.h"
#include "pbd/signals.h"

#include "gtkmm2ext/persistent_tooltip.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_knob.h"
#include "widgets/slider_controller.h"

#include "ardour_window.h"
#include "pianokeyboard.h"

namespace ARDOUR {
	class Session;
}

class VKBDControl : public PBD::Controllable
{
public:
	VKBDControl (const std::string& name, double normal = 127, double upper = 127)
		: PBD::Controllable (name, Flag (0))
		, _lower (0)
		, _upper (upper)
		, _normal (normal)
		, _value (normal)
	{}

	/* Controllable API */
	void set_value (double v, PBD::Controllable::GroupControlDisposition gcd)
	{
		if (v != _value) {
			_value = std::max (_lower, std::min (_upper, v));
			Changed (true, gcd);        /* EMIT SIGNAL */
			ValueChanged ((int)_value); /* EMIT SIGNAL */
		}
	}

	std::string get_user_string () const
	{
		char buf[32];
		sprintf (buf, "%.0f", get_value ());
		return std::string (buf);
	}

	double get_value () const { return _value; }
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

	void set_session (ARDOUR::Session*);

	XMLNode& get_state ();
	void     set_state (const XMLNode&);

protected:
	bool on_focus_in_event (GdkEventFocus*);

private:
	void on_unmap ();
	bool on_key_press_event (GdkEventKey*);
	bool on_key_release_event (GdkEventKey*);

	void parameter_changed (std::string const&);

	void note_on_event_handler (int, int);
	void note_off_event_handler (int);
	void control_change_event_handler (int, int);
	void control_change_knob_event_handler (int, int);

	void modwheel_update_tooltip (int);
	void modwheel_slider_adjusted ();

	void octave_key_event_handler (bool);
	void pitch_bend_key_event_handler (int, bool);
	bool pitch_bend_timeout ();

	void pitch_bend_event_handler (int);
	void pitch_bend_release ();
	void pitch_bend_update_tooltip (int);
	void pitch_slider_adjusted ();

	void select_keyboard_layout (std::string const&);
	void update_velocity_settings ();
	void update_octave_key ();
	void update_octave_range ();
	void cc_key_changed (size_t);
	void update_cc (size_t, int);
	bool send_panic_message (GdkEventButton*);
	bool on_velocity_scroll_event (GdkEventScroll*);

	APianoKeyboard  _piano;

	ArdourWidgets::ArdourDropdown  _midi_channel;
	ArdourWidgets::ArdourDropdown  _piano_velocity;
	ArdourWidgets::ArdourDropdown  _piano_octave_key;
	ArdourWidgets::ArdourDropdown  _piano_octave_range;
	ArdourWidgets::ArdourDropdown  _transpose_output;
	ArdourWidgets::ArdourButton    _send_panic;

	boost::shared_ptr<VKBDControl>    _pitchbend;
	Gtk::Adjustment                   _pitch_adjustment;
	ArdourWidgets::VSliderController* _pitch_slider;
	Gtkmm2ext::PersistentTooltip*     _pitch_slider_tooltip;

	boost::shared_ptr<VKBDControl>    _modwheel;
	Gtk::Adjustment                   _modwheel_adjustment;
	ArdourWidgets::VSliderController* _modwheel_slider;
	Gtkmm2ext::PersistentTooltip*     _modwheel_tooltip;
#define VKBD_NCTRLS 4

	boost::shared_ptr<VKBDControl> _cc[VKBD_NCTRLS];
	ArdourWidgets::ArdourKnob*     _cc_knob[VKBD_NCTRLS];
	ArdourWidgets::ArdourDropdown  _cc_key[VKBD_NCTRLS];

	PBD::ScopedConnectionList _cc_connections;

	sigc::connection _bender_connection;
	int              _pitch_bend_target;
};

#endif
