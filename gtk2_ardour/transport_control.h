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

#ifndef _gtkardour_transport_control_h_
#define _gtkardour_transport_control_h_

#include <gtkmm/widget.h>
#include "pbd/controllable.h"

#include "ardour/session_handle.h"

/* This is an API implemented by AROUR_UI,
 * and made available to transport-control-UIs
 */
class TransportControlProvider
{
public:
	TransportControlProvider ();
	virtual ~TransportControlProvider () {}

	/* show metronome preferences */
	virtual bool click_button_clicked (GdkEventButton *) = 0;

	struct TransportControllable : public PBD::Controllable, public ARDOUR::SessionHandlePtr {
		enum ToggleType {
			Roll = 0,
			Stop,
			RecordEnable,
			GotoStart,
			GotoEnd,
			AutoLoop,
			PlaySelection,
		};

		TransportControllable (std::string name, ToggleType);
		void set_value (double, PBD::Controllable::GroupControlDisposition group_override);
		double get_value (void) const;

		ToggleType type;
	};

	boost::shared_ptr<TransportControllable> roll_controllable;
	boost::shared_ptr<TransportControllable> stop_controllable;
	boost::shared_ptr<TransportControllable> goto_start_controllable;
	boost::shared_ptr<TransportControllable> goto_end_controllable;
	boost::shared_ptr<TransportControllable> auto_loop_controllable;
	boost::shared_ptr<TransportControllable> play_selection_controllable;
	boost::shared_ptr<TransportControllable> rec_controllable;
};

#endif
