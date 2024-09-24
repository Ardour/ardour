/*
 * Copyright (C) 2005-2024 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2024 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2024 Ben Loftis <ben@harrisonconsoles.com>
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

#pragma once

#include <gtkmm/box.h>
#include <gtkmm/label.h>
#include <gtkmm/table.h>

#include "ardour/ardour.h"
#include "ardour/types.h"
#include "ardour/session_handle.h"

#include "widgets/ardour_button.h"
#include "widgets/ardour_dropdown.h"
#include "widgets/ardour_spacer.h"

#include "main_clock.h"
#include "mini_timeline.h"
#include "shuttle_control.h"
#include "startup_fsm.h"
#include "transport_control.h"
#include "transport_control_ui.h"
#include "visibility_group.h"
#include "window_manager.h"

class BasicUI;
class TimeInfoBox;
class LevelMeterHBox;

namespace ARDOUR {
	class Route;
	class RouteGroup;
}

class ApplicationBar : public Gtk::HBox, public ARDOUR::SessionHandlePtr
{
public:
	ApplicationBar ();
	~ApplicationBar();

	void set_session (ARDOUR::Session *);

private:
	void on_parent_changed (Gtk::Widget*);

	bool               _have_layout;
	BasicUI*           _basic_ui;
	TransportControlUI _transport_ctrl;
};
