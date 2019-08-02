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

#include <gtkmm/box.h>

#include "ardour_ui.h"
#include "big_transport_window.h"
#include "utils.h"

#include "pbd/i18n.h"

using namespace Glib;
using namespace Gtk;

BigTransportWindow::BigTransportWindow ()
	: ArdourWindow (_("Transport Controls"))
{
	transport_ctrl.setup (ARDOUR_UI::instance ());
	transport_ctrl.map_actions ();

	set_keep_above (true);
	VBox* vbox = manage (new VBox);
	vbox->pack_start (transport_ctrl, true, true);
	add (*vbox);
	vbox->show_all();
}

void
BigTransportWindow::on_unmap ()
{
	ArdourWindow::on_unmap ();
	ARDOUR_UI::instance()->reset_focus (this);
}

bool
BigTransportWindow::on_key_press_event (GdkEventKey* ev)
{
	return ARDOUR_UI_UTILS::relay_key_press (ev, this);
}
