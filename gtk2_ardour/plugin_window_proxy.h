/*
 * Copyright (C) 2022,2024 Robin Gareus <robin@gareus.org>
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

#ifndef _gtkardour_plugin_window_proxy_h_
#define _gtkardour_plugin_window_proxy_h_

#include "ardour_window.h"
#include "window_manager.h"

#include "pbd/signals.h"

namespace Gtk
{
	class Window;
}

namespace ARDOUR
{
	class PlugInsertBase;
}

class PluginWindowProxy : public WM::ProxyBase, public PBD::ScopedConnectionList
{
public:
	PluginWindowProxy (std::string const&, std::string const&, std::weak_ptr<ARDOUR::PlugInsertBase>);
	~PluginWindowProxy ();

	Gtk::Window* get (bool create = false);

	void show_the_right_window ();

	ARDOUR::SessionHandlePtr* session_handle ()
	{
		return 0;
	}

	void set_custom_ui_mode (bool use_custom)
	{
		_want_custom = use_custom;
	}

	int      set_state (const XMLNode&, int);
	XMLNode& get_state () const;

	std::string generate_processor_title (std::shared_ptr<ARDOUR::PlugInsertBase>);

private:
	void plugin_going_away ();

	std::weak_ptr<ARDOUR::PlugInsertBase> _pib;

	std::string _title;
	bool        _is_custom;
	bool        _want_custom;
};

#endif
