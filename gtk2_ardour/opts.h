/*
    Copyright (C) 2001 Paul Davis

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

#ifndef __ardour_opts_h__
#define __ardour_opts_h__

#include <string>

namespace ARDOUR_COMMAND_LINE {

extern std::string session_name;
extern bool   show_key_actions;
extern bool   no_splash;
extern bool   just_version;
extern std::string backend_client_name;
extern std::string backend_session_uuid;
extern bool   use_vst;
extern bool   new_session;
extern char*  curvetest_file;
extern bool   try_hw_optimization;
extern bool no_connect_ports;
extern bool   use_gtk_theme;
extern std::string keybindings_path;
extern std::string menus_file;
extern bool   finder_invoked_ardour;
extern std::string immediate_save;
extern std::string load_template;
extern bool        check_announcements;

extern int32_t parse_opts (int argc, char *argv[]);

}

#endif /* __ardour_opts_h__ */
