/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifndef __gtk_ardour_recorder_ui_h__
#define __gtk_ardour_recorder_ui_h__

#include <gtkmm/box.h>

#include "ardour/session_handle.h"
#include "gtkmm2ext/bindings.h"
#include "widgets/tabbable.h"

class RecorderUI : public ArdourWidgets::Tabbable, public ARDOUR::SessionHandlePtr, public PBD::ScopedConnectionList
{
public:
	RecorderUI ();
	~RecorderUI ();

	void set_session (ARDOUR::Session*);
	void cleanup ();

	XMLNode& get_state ();
	int set_state (const XMLNode&, int /* version */);

	Gtk::Window* use_own_window (bool and_fill_it);

private:
	void load_bindings ();
	void register_actions ();
	void update_title ();
	void session_going_away ();
	void parameter_changed (std::string const&);

	Gtkmm2ext::Bindings* bindings;

	Gtk::VBox _content;
};

#endif /* __gtk_ardour_recorder_ui_h__ */
