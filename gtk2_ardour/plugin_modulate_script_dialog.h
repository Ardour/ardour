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

#ifndef __gtkardour_modulate_script_dialog_h__
#define __gtkardour_modulate_script_dialog_h__

#include "ardour/plugin_insert.h"

#include <gtkmm/box.h>
#include <gtkmm/scrolledwindow.h>

#include "ardour_button.h"
#include "ardour_window.h"
#include "window_manager.h"

class PluginModulateScriptDialog : public ArdourWindow
{
public:
	PluginModulateScriptDialog (boost::shared_ptr<ARDOUR::PluginInsert>);
	~PluginModulateScriptDialog ();

private:
	boost::shared_ptr<ARDOUR::PluginInsert> _pi;

	ArdourButton _set_button;
	ArdourButton _read_button;
	ArdourButton _clear_button;

	Gtk::TextView entry;
	Gtk::VBox vbox;
	Gtk::Label _status;

	PBD::ScopedConnection _plugin_connection;

	void script_changed ();
	void read_script ();
	void set_script ();
	void unload_script ();
};

class PluginModulateScriptProxy : public WM::ProxyBase
{
  public:
	PluginModulateScriptProxy (std::string const&, boost::weak_ptr<ARDOUR::PluginInsert>);
	~PluginModulateScriptProxy();

	Gtk::Window* get (bool create = false);
	ARDOUR::SessionHandlePtr* session_handle();

  private:
	boost::weak_ptr<ARDOUR::PluginInsert> _pi;

	void processor_going_away ();
	PBD::ScopedConnection going_away_connection;
};

#endif
