/*
    Copyright (C) 2011 Tim Mayberry

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

#include <glibmm/miscutils.h>

#include "ardour/ladspa_search_path.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"

namespace {
	const char * const ladspa_env_variable_name = "LADSPA_PATH";
} // anonymous

using namespace PBD;

namespace ARDOUR {

SearchPath
ladspa_search_path ()
{
	SearchPath spath_env (Glib::getenv(ladspa_env_variable_name));

	SearchPath spath (user_config_directory ());

	spath += ardour_dll_directory ();
	spath.add_subdirectory_to_paths (ladspa_dir_name);

#ifndef WIN32
	spath.push_back ("/usr/local/lib64/ladspa");
	spath.push_back ("/usr/local/lib/ladspa");
	spath.push_back ("/usr/lib64/ladspa");
	spath.push_back ("/usr/lib/ladspa");
	spath.push_back ("/Library/Audio/Plug-Ins/LADSPA");
#endif

	return spath_env + spath;
}

} // namespace ARDOUR
