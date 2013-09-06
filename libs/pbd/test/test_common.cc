/*
    Copyright (C) 2011 Tim Mayberry

    This program is free software; you can redistribute it and/or modify it
    under the terms of the GNU General Public License as published by the Free
    Software Foundation; either version 2 of the License, or (at your option)
    any later version.

    This program is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License
    for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include <glibmm/miscutils.h>

#include "test_common.h"

/**
 * This allows tests to find the data files they require by looking
 * in an installed location on windows or by setting an environment variable
 * on unix.
 */
PBD::Searchpath
test_search_path ()
{
#ifdef PLATFORM_WINDOWS
	std::string wsp(g_win32_get_package_installation_directory_of_module(NULL));
	return Glib::build_filename (wsp,  "pbd_testdata");
#else
	return Glib::getenv("PBD_TEST_PATH");
#endif
}
