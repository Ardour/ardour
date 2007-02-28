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

using std::string;

namespace GTK_ARDOUR {

extern string session_name;
extern bool   show_key_actions;
extern bool   no_splash;
extern bool   just_version;
extern string jack_client_name;
extern bool   use_vst;
extern bool   new_session;
extern char*  curvetest_file;
extern bool   try_hw_optimization;
extern bool   use_gtk_theme;
extern string keybindings_path;

extern int32_t parse_opts (int argc, char *argv[]);

}

#endif /* __ardour_opts_h__ */
