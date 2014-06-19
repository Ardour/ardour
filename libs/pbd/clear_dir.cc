/*
    Copyright (C) 2012 Paul Davis 

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

#ifdef COMPILER_MSVC
#include <io.h>      // Microsoft's nearest equivalent to <unistd.h>
using PBD::readdir;
using PBD::opendir;
using PBD::closedir;
#else
#include <dirent.h>
#include <unistd.h>
#endif

#include <string>
#include <sys/stat.h>
#include <errno.h>
#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/clear_dir.h"
#include "pbd/file_utils.h"

#include "i18n.h"

using namespace PBD;
using namespace std;

int
remove_directory_internal (const string& dir, size_t* size, vector<string>* paths,
                           bool just_remove_files)
{
	vector<string> tmp_paths;
	struct stat statbuf;
	int ret = 0;

	get_directory_contents (dir, tmp_paths, just_remove_files, true);

	for (vector<string>::const_iterator i = tmp_paths.begin();
	     i != tmp_paths.end(); ++i) {

		if (g_stat (i->c_str(), &statbuf)) {
			continue;
		}

                if (::g_remove (i->c_str())) {
                        error << string_compose (_("cannot remove path %1 (%2)"), *i, strerror (errno))
                              << endmsg;
                        ret = 1;
                }

                if (paths) {
                        paths->push_back (Glib::path_get_basename(*i));
                }

                if (size) {
                        *size += statbuf.st_size;
                }

	}

        return ret;
}

int
PBD::clear_directory (const string& dir, size_t* size, vector<string>* paths)
{
	return remove_directory_internal (dir, size, paths, true);
}

// rm -rf <dir> -- used to remove saved plugin state
void
PBD::remove_directory (const std::string& dir)
{
	remove_directory_internal (dir, 0, 0, false);
}
