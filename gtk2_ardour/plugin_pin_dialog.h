/*
 * Copyright (C) 2016 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef __gtkardour_plugin_pin_dialog_h__
#define __gtkardour_plugin_pin_dialog_h__

#include <gtkmm/drawingarea.h>
#include <gtkmm/box.h>

#include "pbd/stateful.h"
#include "pbd/signals.h"

#include "ardour/plugin_insert.h"
#include "ardour/route.h"

#include "ardour_window.h"

class PluginPinDialog : public ArdourWindow
{
public:
	PluginPinDialog (boost::shared_ptr<ARDOUR::PluginInsert>);
	~PluginPinDialog ();

private:
	Gtk::DrawingArea darea;
	bool darea_expose_event (GdkEventExpose* event);

	void plugin_reconfigured ();

	void draw_io_pins (cairo_t*, double, double, uint32_t, uint32_t, bool);
	void draw_plugin_pins (cairo_t*, double, double, double, uint32_t, uint32_t, bool);
	void draw_connection (cairo_t*, double, double, double, double, bool);

	double pin_x_pos (uint32_t, double, double, uint32_t, uint32_t, bool);

	PBD::ScopedConnectionList _plugin_connections;
	boost::shared_ptr<ARDOUR::PluginInsert> _pi;
};

#endif
