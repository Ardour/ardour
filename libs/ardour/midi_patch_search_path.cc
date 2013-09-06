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

#include "ardour/midi_patch_search_path.h"
#include "ardour/directory_names.h"
#include "ardour/filesystem_paths.h"

namespace {
	const char * const midi_patch_env_variable_name = "ARDOUR_MIDI_PATCH_PATH";
} // anonymous

using namespace PBD;

namespace ARDOUR {

Searchpath
midi_patch_search_path ()
{
	Searchpath spath (ardour_data_search_path());
	spath.add_subdirectory_to_paths(midi_patch_dir_name);

	bool midi_patch_path_defined = false;
	Searchpath spath_env (Glib::getenv(midi_patch_env_variable_name, midi_patch_path_defined));

	if (midi_patch_path_defined) {
		spath += spath_env;
	}

	return spath;
}

} // namespace ARDOUR
