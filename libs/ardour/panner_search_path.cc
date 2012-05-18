/*
    Copyright (C) 2007 Tim Mayberry

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

#include "ardour/panner_search_path.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"

namespace {
	const char * const panner_env_variable_name = "ARDOUR_PANNER_PATH";
} // anonymous

using namespace PBD;

namespace ARDOUR {

SearchPath
panner_search_path ()
{
	SearchPath spath (user_config_directory ());

	spath += ardour_dll_directory ();
	spath.add_subdirectory_to_paths(panner_dir_name);

	bool panner_path_defined = false;
	SearchPath spath_env (Glib::getenv(panner_env_variable_name, panner_path_defined));

	if (panner_path_defined) {
		spath += spath_env;
	}

	return spath;
}

} // namespace ARDOUR
