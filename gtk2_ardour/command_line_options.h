/*
    Copyright (C) 2011 Paul Davis

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

*/

#ifndef __gtkardour_command_line_options_h__
#define __gtkardour_command_line_options_h__

#include <string>

#include <glibmm/optiongroup.h>

class CommandLineOptions : public Glib::OptionGroup
{
public:
	CommandLineOptions();

	virtual bool on_pre_parse (Glib::OptionContext&, Glib::OptionGroup&);
	virtual bool on_post_parse (Glib::OptionContext&, Glib::OptionGroup&);

	std::string    session_name;
	bool           show_key_actions;
	bool           check_announcements;
	bool           disable_plugins;
	bool           no_splash;
	bool           just_version;
	Glib::ustring  jack_client_name;
	bool           new_session;
	std::string    new_session_name;
	std::string    curvetest_file;
	bool           no_connect_ports;
	bool           use_gtk_theme;
	std::string    keybindings_path;
	std::string    menus_file;
	bool           finder_invoked_ardour;
	std::string    immediate_save;
	Glib::ustring  jack_session_uuid;
	std::string    load_template;
};


bool parse_cmdline_opts (int *argc, char ***argv);

// get_command_line_options().some_var seemed a bit long
CommandLineOptions& get_cmdline_opts ();

#endif
