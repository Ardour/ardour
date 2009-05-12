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

#include <glib.h>

#include "pbd/filesystem_paths.h"

namespace PBD {

std::vector<sys::path>
system_data_directories ()
{
	std::vector<sys::path> tmp;
	const char * const * dirs;

	dirs = g_get_system_data_dirs ();

	if (dirs == NULL) return tmp;

	for (int i = 0; dirs[i] != NULL; ++i)
	{
		tmp.push_back(dirs[i]);
	}

	return tmp;
}

std::vector<sys::path>
system_config_directories ()
{
	std::vector<sys::path> tmp;
	const char * const * dirs;

	dirs = g_get_system_config_dirs ();

	if (dirs == NULL) return tmp;

	for (int i = 0; dirs[i] != NULL; ++i)
	{
		tmp.push_back(dirs[i]);
	}

	return tmp;
}

} // namespace PBD
