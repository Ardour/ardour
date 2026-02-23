/*
 * Copyright (C) 2026 Frank Povazanj <frank.povazanj@gmail.com>
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

#include "mcp_http_gui.h"

#include "mcp_http.h"

#include "pbd/i18n.h"

using namespace ArdourSurface;

MCPHttpGUI::MCPHttpGUI (MCPHttp& cp)
	: Gtk::VBox ()
	, _cp (cp)
	, _table (4, 2, false)
	, _hint_label ("")
{
	set_border_width (12);
	set_spacing (10);
	set_size_request (500, 280);
	_table.set_row_spacings (6);
	_table.set_col_spacings (10);

	Gtk::Label* label = 0;
	int row = 0;

	label = manage (new Gtk::Label (_("MCP Protocol:")));
	label->set_alignment (0.0f, 0.5f);
	_table.attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::AttachOptions (0), 4, 2);
	_protocol_label.set_alignment (0.0f, 0.5f);
	_protocol_label.set_text (_cp.protocol_name ());
	_table.attach (_protocol_label, 1, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::AttachOptions (0), 4, 2);
	++row;

	label = manage (new Gtk::Label (_("MCP URL:")));
	label->set_alignment (0.0f, 0.5f);
	_table.attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::AttachOptions (0), 4, 2);
	_endpoint_label.set_alignment (0.0f, 0.5f);
	_endpoint_label.set_selectable (true);
	_endpoint_label.set_text (_cp.endpoint_url ());
	_table.attach (_endpoint_label, 1, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::AttachOptions (0), 4, 2);
	++row;

	label = manage (new Gtk::Label (_("Listen Port:")));
	label->set_alignment (0.0f, 0.5f);
	_table.attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::AttachOptions (0), 4, 2);
	_port_entry.set_range (1.0, 65535.0);
	_port_entry.set_increments (1.0, 100.0);
	_port_entry.set_numeric (true);
	_port_entry.set_value ((double) _cp.port ());
	_table.attach (_port_entry, 1, 2, row, row + 1, Gtk::FILL, Gtk::AttachOptions (0), 4, 2);
	++row;

	label = manage (new Gtk::Label (_("Debug:")));
	label->set_alignment (0.0f, 0.5f);
	_table.attach (*label, 0, 1, row, row + 1, Gtk::FILL, Gtk::AttachOptions (0), 4, 2);
	_debug_combo.append_text (_("Off"));
	_debug_combo.append_text (_("Basic (requests/errors)"));
	_debug_combo.append_text (_("Verbose (payloads)"));
	_debug_combo.set_active (_cp.debug_level ());
	_table.attach (_debug_combo, 1, 2, row, row + 1, Gtk::FILL | Gtk::EXPAND, Gtk::AttachOptions (0), 4, 2);

	_hint_label.set_alignment (0.0f, 0.5f);
	_hint_label.set_line_wrap (true);
	_hint_label.set_width_chars (90);
	_hint_label.set_max_width_chars (100);
	_hint_label.set_selectable (true);
	_hint_label.set_text (
		_("Client setup:\n"
		  "Transport: HTTP (Streamable HTTP)\n"
		  "Endpoint: ") + _cp.endpoint_url () + "\n"
		+ _("[mcp_servers.ardour_http]\nurl = \"") + _cp.endpoint_url () + "\"");

	pack_start (_table, false, false, 0);
	pack_start (_hint_label, false, false, 0);

	_port_entry.signal_activate ().connect (sigc::mem_fun (*this, &MCPHttpGUI::port_activate));
	_port_entry.signal_focus_out_event ().connect (sigc::mem_fun (*this, &MCPHttpGUI::port_focus_out));
	_debug_combo.signal_changed ().connect (sigc::mem_fun (*this, &MCPHttpGUI::debug_changed));
}

MCPHttpGUI::~MCPHttpGUI ()
{
}

void
MCPHttpGUI::refresh_connection_info ()
{
	_endpoint_label.set_text (_cp.endpoint_url ());
	_hint_label.set_text (
		_("Client setup:\n"
		  "Transport: HTTP (Streamable HTTP)\n"
		  "Endpoint: ") + _cp.endpoint_url () + "\n"
		+ _("[mcp_servers.ardour_http]\nurl = \"") + _cp.endpoint_url () + "\"");
}

void
MCPHttpGUI::commit_port ()
{
	_port_entry.update ();

	const int port = _port_entry.get_value_as_int ();
	if (port < 1 || port > 65535) {
		_port_entry.set_value ((double) _cp.port ());
		return;
	}

	_cp.set_port ((uint16_t) port);
	_port_entry.set_value ((double) _cp.port ());
	refresh_connection_info ();
}

void
MCPHttpGUI::port_activate ()
{
	commit_port ();
}

bool
MCPHttpGUI::port_focus_out (GdkEventFocus*)
{
	commit_port ();
	return false;
}

void
MCPHttpGUI::debug_changed ()
{
	const int level = _debug_combo.get_active_row_number ();
	if (level < 0) {
		return;
	}
	_cp.set_debug_level (level);
}

void*
MCPHttp::get_gui () const
{
	if (!_gui) {
		const_cast<MCPHttp*> (this)->build_gui ();
	}
	static_cast<Gtk::VBox*> (_gui)->show_all ();
	return _gui;
}

void
MCPHttp::tear_down_gui ()
{
	if (_gui) {
		Gtk::Widget* w = static_cast<Gtk::VBox*> (_gui)->get_parent ();
		if (w) {
			w->hide ();
			delete w;
		}
		delete static_cast<MCPHttpGUI*> (_gui);
		_gui = 0;
	}
}

void
MCPHttp::build_gui ()
{
	_gui = (void*) new MCPHttpGUI (*this);
}
