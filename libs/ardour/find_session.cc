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

#include <unistd.h>
#include <sys/stat.h>

#include <cstring>
#include <climits>
#include <cerrno>

#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "pbd/pathexpand.h"
#include "pbd/error.h"

#include "ardour/filename_extensions.h"
#include "ardour/utils.h"

#include "i18n.h"

using namespace std;
using namespace PBD;

namespace ARDOUR {

int
find_session (string str, string& path, string& snapshot, bool& isnew)
{
	struct stat statbuf;

	isnew = false;

	str = canonical_path (str);

	/* check to see if it exists, and what it is */

	if (stat (str.c_str(), &statbuf)) {
		if (errno == ENOENT) {
			isnew = true;
		} else {
			error << string_compose (_("cannot check session path %1 (%2)"), str, strerror (errno))
			      << endmsg;
			return -1;
		}
	}

	if (!isnew) {

		/* it exists, so it must either be the name
		   of the directory, or the name of the statefile
		   within it.
		*/

		if (S_ISDIR (statbuf.st_mode)) {

			string::size_type slash = str.find_last_of (G_DIR_SEPARATOR);

			if (slash == string::npos) {

				/* a subdirectory of cwd, so statefile should be ... */

				string tmp = Glib::build_filename (str, str+statefile_suffix);

				/* is it there ? */

				if (stat (tmp.c_str(), &statbuf)) {
					error << string_compose (_("cannot check statefile %1 (%2)"), tmp, strerror (errno))
					      << endmsg;
					return -1;
				}

				path = str;
				snapshot = str;

			} else {

				/* some directory someplace in the filesystem.
				   the snapshot name is the directory name
				   itself.
				*/

				path = str;
				snapshot = str.substr (slash+1);

			}

		} else if (S_ISREG (statbuf.st_mode)) {

			string::size_type slash = str.find_last_of (G_DIR_SEPARATOR);
			string::size_type suffix;

			/* remove the suffix */

			if (slash != string::npos) {
				snapshot = str.substr (slash+1);
			} else {
				snapshot = str;
			}

			suffix = snapshot.find (statefile_suffix);

			if (suffix == string::npos) {
				error << string_compose (_("%1 is not a snapshot file"), str) << endmsg;
				return -1;
			}

			/* remove suffix */

			snapshot = snapshot.substr (0, suffix);

			if (slash == string::npos) {

				/* we must be in the directory where the
				   statefile lives. get it using cwd().
				*/

				char cwd[PATH_MAX+1];

				if (getcwd (cwd, sizeof (cwd)) == 0) {
					error << string_compose (_("cannot determine current working directory (%1)"), strerror (errno))
					      << endmsg;
					return -1;
				}

				path = cwd;

			} else {

				/* full path to the statefile */

				path = str.substr (0, slash);
			}

		} else {

			/* what type of file is it? */
			error << string_compose (_("unknown file type for session %1"), str) << endmsg;
			return -1;
		}

	} else {

		/* its the name of a new directory. get the name
		   as "dirname" does.
		*/

		string::size_type slash = str.find_last_of (G_DIR_SEPARATOR);

		if (slash == string::npos) {

			/* no slash, just use the name, but clean it up */

			path = legalize_for_path (str);
			snapshot = path;

		} else {

			path = str;
			snapshot = str.substr (slash+1);
		}
	}

	return 0;
}

}  // namespace ARDOUR
