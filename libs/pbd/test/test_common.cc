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

#include "test_common.h"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <sstream>

using namespace std;

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

std::string
test_output_directory (std::string prefix)
{
	std::string tmp_dir = Glib::build_filename (g_get_tmp_dir(), "pbd_test");
	std::string dir_name;
	std::string new_test_dir;
	do {
		ostringstream oss;
		oss << prefix;
		oss << g_random_int ();
		dir_name = oss.str();
		new_test_dir = Glib::build_filename (tmp_dir, dir_name);
		if (Glib::file_test (new_test_dir, Glib::FILE_TEST_EXISTS)) continue;
	} while (g_mkdir_with_parents (new_test_dir.c_str(), 0755) != 0);
	return new_test_dir;
}
