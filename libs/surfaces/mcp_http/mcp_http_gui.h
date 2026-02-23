/*
 * Copyright (C) 2026
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

#ifndef _ardour_surface_mcp_http_gui_h_
#define _ardour_surface_mcp_http_gui_h_

#include <ytkmm/box.h>
#include <ytkmm/comboboxtext.h>
#include <ytkmm/label.h>
#include <ytkmm/spinbutton.h>
#include <ytkmm/table.h>

namespace ArdourSurface {

class MCPHttp;

class MCPHttpGUI : public Gtk::VBox
{
public:
	explicit MCPHttpGUI (MCPHttp&);
	~MCPHttpGUI ();

private:
	MCPHttp& _cp;
	Gtk::Table _table;
	Gtk::Label _protocol_label;
	Gtk::Label _endpoint_label;
	Gtk::SpinButton _port_entry;
	Gtk::ComboBoxText _debug_combo;
	Gtk::Label _hint_label;

	void refresh_connection_info ();
	void commit_port ();
	void port_activate ();
	bool port_focus_out (GdkEventFocus*);
	void debug_changed ();
};

} // namespace ArdourSurface

#endif // _ardour_surface_mcp_http_gui_h_
