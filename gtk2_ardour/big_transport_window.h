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

#ifndef _big_transport_window_h_
#define _big_transport_window_h_

#include "ardour_window.h"
#include "transport_control_ui.h"

namespace ARDOUR {
	class Session;
}

class BigTransportWindow : public ArdourWindow
{
public:
	BigTransportWindow ();

	void set_session (ARDOUR::Session *s) {
		transport_ctrl.set_session (s);
	}

protected:
	void on_unmap ();
	bool on_key_press_event (GdkEventKey*);

private:
	TransportControlUI transport_ctrl;
};

#endif

