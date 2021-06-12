/*
 * Copyright (C) 2021 Paul Davis <paul@linuxaudiosystems.com>
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

#ifndef _gtkardour_dspstats_ui_h_
#define _gtkardour_dspstats_ui_h_

#include <gtkmm/widget.h>
#include <gtkmm/table.h>
#include <gtkmm/label.h>

#include "ardour/session_handle.h"

class DspStatisticsGUI : public Gtk::Table, public ARDOUR::SessionHandlePtr
{
public:
	DspStatisticsGUI (ARDOUR::Session* s);

	void start_updating ();
	void stop_updating ();

private:
	void update ();

	sigc::connection update_connection;

	Gtk::Label** labels;

};

#endif
