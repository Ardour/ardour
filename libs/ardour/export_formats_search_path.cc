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

#include <iostream>
#include <glibmm/miscutils.h>

#include "ardour/export_formats_search_path.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"

namespace {
	const char * const export_env_variable_name = "ARDOUR_EXPORT_FORMATS_PATH";
} // anonymous

using namespace PBD;

namespace ARDOUR {

Searchpath
export_formats_search_path ()
{
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths (export_formats_dir_name);

	bool export_formats_path_defined = false;
	Searchpath spath_env (Glib::getenv(export_env_variable_name, export_formats_path_defined));

	if (export_formats_path_defined) {
		spath += spath_env;
	}

	return spath;
}

} // namespace ARDOUR
