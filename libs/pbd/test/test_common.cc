/*
 * Copyright (C) 2013-2014 Tim Mayberry <mojofunk@gmail.com>
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

#include "test_common.h"

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/file_utils.h"

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
	if (!getenv("PBD_TEST_PATH")) {
		std::string wsp(g_win32_get_package_installation_directory_of_module(NULL));
		return Glib::build_filename (wsp,  "pbd_testdata");
	}
#endif
	return Glib::getenv("PBD_TEST_PATH");
}

std::string
test_output_directory (std::string prefix)
{
	return PBD::tmp_writable_directory (PACKAGE, prefix);
}

void
get_utf8_test_strings (std::vector<std::string>& result)
{
	// These are all translations of "Ardour" from google translate
	result.push_back ("Ardour"); // Reference
	result.push_back ("\320\277\321\213\320\273"); // Russian
	result.push_back ("\305\276ar"); // Croatian
	result.push_back ("\340\270\204\340\270\247\340\270\262\340\270\241\340\270\201\340\270\243\340\270\260\340\270\225\340\270\267\340\270\255\340\270\243\340\270\267\340\270\255\340\270\243\340\271\211\340\270\231"); // Thai
	result.push_back ("\325\245\325\274\325\241\325\266\325\244"); // Armenian
	result.push_back ("\340\246\254\340\247\215\340\246\257\340\246\227\340\247\215\340\246\260\340\246\244\340\246\276"); // Bengali
	result.push_back ("\346\203\205\347\206\261"); // Japanese
	result.push_back ("\347\203\255\346\203\205"); // Chinese (Simplified)
}
