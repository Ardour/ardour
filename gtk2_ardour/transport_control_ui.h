/*
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef _transport_control_ui_h_
#define _transport_control_ui_h_

#include <gtkmm/box.h>

#include "pbd/signals.h"
#include "ardour/session_handle.h"
#include "widgets/ardour_button.h"

namespace ARDOUR {
	class Session;
}

class TransportControlProvider;

class TransportControlUI : public ARDOUR::SessionHandlePtr, public Gtk::HBox
{
public:
	TransportControlUI ();

	void setup (TransportControlProvider*);
	void map_actions ();
	void set_session (ARDOUR::Session *s);

	ArdourWidgets::ArdourButton& size_button () { return stop_button; }

protected:

	void parameter_changed (std::string p);

	void blink_rec_enable (bool onoff);
	void set_loop_sensitivity ();
	void set_transport_sensitivity (bool);
	void map_transport_state ();
	void step_edit_status_change (bool yn);

	bool click_button_scroll (GdkEventScroll* ev);

	ArdourWidgets::ArdourButton roll_button;
	ArdourWidgets::ArdourButton stop_button;
	ArdourWidgets::ArdourButton goto_start_button;
	ArdourWidgets::ArdourButton goto_end_button;
	ArdourWidgets::ArdourButton auto_loop_button;
	ArdourWidgets::ArdourButton play_selection_button;
	ArdourWidgets::ArdourButton rec_button;
	ArdourWidgets::ArdourButton midi_panic_button;
	ArdourWidgets::ArdourButton click_button;

private:
	PBD::ScopedConnection config_connection;
};

#endif
