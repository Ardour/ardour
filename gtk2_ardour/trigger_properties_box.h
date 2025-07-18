/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2021 Ben Loftis <ben@harrisonconsoles.com>
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

#include <ytkmm/label.h>
#include <ytkmm/table.h>

#include "pbd/signals.h"

#include "ardour/session_handle.h"

#include "trigger_ui.h"

class TriggerPropertiesBox : public Gtk::Table, public ARDOUR::SessionHandlePtr, public TriggerUI
{
public:
	TriggerPropertiesBox () {}
	~TriggerPropertiesBox () {}

protected:
	Gtk::Label _header_label;

	PBD::ScopedConnection _state_connection;
};

