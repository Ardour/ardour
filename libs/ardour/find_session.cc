/*
 * Copyright (C) 2008-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015-2017 Robin Gareus <robin@gareus.org>
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

#include <unistd.h>

#include <cstring>
#include <climits>
#include <cerrno>

#include "pbd/gstdio_compat.h"

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/file_archive.h"
#include "pbd/pathexpand.h"
#include "pbd/error.h"

#include "ardour/filename_extensions.h"
#include "ardour/utils.h"
#include "ardour/session_utils.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;

namespace ARDOUR {

int
find_session (string str, string& path, string& snapshot, bool& isnew)
{
	GStatBuf statbuf;

	isnew = false;

	str = canonical_path (str);

	/* check to see if it exists, and what it is */

	if (g_stat (str.c_str(), &statbuf)) {
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

				if (g_stat (tmp.c_str(), &statbuf)) {
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

			const string::size_type start_pos_of_extension = snapshot.size () - strlen (statefile_suffix);
			// we should check the start of extension position
			// because files '*.ardour.bak' are possible
			if (suffix != start_pos_of_extension) {
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

/* check if zip is a session-archive,
 * return > 0 if file is not an archive
 * return < 0 if unzip failed
 * return 0 on success.  path and snapshot are set.
 */
int
inflate_session (const std::string& zipfile, const std::string& target_dir, string& path, string& snapshot)
{
	if (zipfile.find (session_archive_suffix) == string::npos) {
		return 1;
	}

	try {
		PBD::FileArchive ar (zipfile);
		std::vector<std::string> files = ar.contents ();

		if (files.size () == 0) {
			error << _("Archive is empty") << endmsg;
			return -2;
		}

		/* session archives are expected to be named after the archive */
		std::string bn = Glib::path_get_dirname (files.front());
		if (bn.empty ()) {
			error << _("Archive does not contain a session folder") << endmsg;
			return -3;
		}

		size_t sep = bn.find_first_of ('/');

		if (sep != string::npos) {
			bn = bn.substr (0, sep);
		}

		if (bn.empty ()) {
			error << _("Archive does not contain a valid session structure") << endmsg;
			return -4;
		}

		string sn = bn + "/" + bn + statefile_suffix;
		if (std::find (files.begin(), files.end(), sn) == files.end()) {
			error << _("Archive does not contain a session file") << endmsg;
			return -5;
		}

		/* check if target folder exists */
		string dest = Glib::build_filename (target_dir, bn);
		if (Glib::file_test (dest, Glib::FILE_TEST_EXISTS)) {
			error << string_compose (_("Destination '%1' already exists."), dest) << endmsg;
			return -1;
		}

		if (0 == ar.inflate (target_dir)) {
			info << string_compose (_("Extracted session-archive to '%1'."), dest) << endmsg;
			path = dest;
			snapshot = bn;
			return 0;
		}

	} catch (...) {
		error << _("Error reading file-archive") << endmsg;
		return -6;
	}

	error << _("Error extracting file-archive") << endmsg;
	return -7;
}

string inflate_error (int e) {
	switch (e) {
		case 0:
			return _("No Error");
		case 1:
			return string_compose (_("File extension is not %1"), session_archive_suffix);
		case -2:
			return _("Archive is empty");
		case -3:
			return _("Archive does not contain a session folder");
		case -4:
			return _("Archive does not contain a valid session structure");
		case -5:
			return _("Archive does not contain a session file");
		case -6:
			return _("Error reading file-archive");
		case -7:
			return _("Error extracting file-archive");
		case -1:
			return _("Destination folder already exists.");
		default:
			assert (0);
			break;
	}
	return _("Unknown Error");
}

}  // namespace ARDOUR
