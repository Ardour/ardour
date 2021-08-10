/*
 * Copyright (C) 2006-2019 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2009 Carl Hetherington <carl@carlh.net>
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

#ifndef __ardour_opts_h__
#define __ardour_opts_h__

#include <string>

namespace ARDOUR_COMMAND_LINE {

extern std::string session_name;
extern bool        show_key_actions;
extern bool        show_actions;
extern bool        no_splash;
extern bool        just_version;
extern std::string backend_client_name;
extern bool        new_session;
extern bool        try_hw_optimization;
extern bool        no_connect_ports;
extern bool        use_gtk_theme;
extern std::string keybindings_path;
extern std::string menus_file;
extern bool        finder_invoked_ardour;
extern std::string load_template;
extern bool        check_announcements;

extern int32_t parse_opts (int argc, char *argv[]);

}

#endif /* __ardour_opts_h__ */
