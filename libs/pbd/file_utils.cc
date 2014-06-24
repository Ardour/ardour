/*
    Copyright (C) 2007-2014 Tim Mayberry
    Copyright (C) 1998-2014 Paul Davis

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

#include <algorithm>
#include <vector>

#include <glib.h>
#include <glib/gstdio.h>

#ifdef COMPILER_MINGW
#include <io.h> // For W_OK
#define strtok_r strtok_s
#endif

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>
#include <glibmm/pattern.h>

#include <errno.h>
#include <string.h> /* strerror */

/* open() */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

/* close(), read(), write() */
#ifdef COMPILER_MSVC
#include <io.h> // Microsoft's nearest equivalent to <unistd.h>
#include <ardourext/misc.h>
#else
#include <unistd.h>
#include <regex.h>
#endif

#include "pbd/compose.h"
#include "pbd/file_utils.h"
#include "pbd/debug.h"
#include "pbd/error.h"
#include "pbd/pathexpand.h"
#include "pbd/stl_delete.h"

#include "i18n.h"

using namespace std;

namespace PBD {

void
get_directory_contents (const std::string& directory_path,
                       vector<string>& result,
                       bool files_only,
                       bool recurse)
{
	// perhaps we don't need this check assuming an exception is thrown
	// as it would save checking that the path is a directory twice when
	// recursing
	if (!Glib::file_test (directory_path, Glib::FILE_TEST_IS_DIR)) return;

	try
	{
		Glib::Dir dir(directory_path);
		Glib::DirIterator i = dir.begin();

		while (i != dir.end()) {

			string fullpath = Glib::build_filename (directory_path, *i);

			bool is_dir = Glib::file_test (fullpath, Glib::FILE_TEST_IS_DIR);

			if (is_dir && recurse) {
				get_directory_contents (fullpath, result, files_only, recurse);
			}

			i++;

			if (is_dir && files_only) {
				continue;
			}
			result.push_back (fullpath);
		}
	}
	catch (Glib::FileError& err)
	{
		warning << err.what() << endmsg;
	}
}

void
get_files_in_directory (const std::string& directory_path, vector<string>& result)
{
	return get_directory_contents (directory_path, result, true, false);
}

void
find_files_matching_pattern (vector<string>& result,
                             const Searchpath& paths,
                             const Glib::PatternSpec& pattern)
{
	vector<string> tmp_files;

	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		get_files_in_directory (*i, tmp_files);
	}

	for (vector<string>::iterator file_iter = tmp_files.begin();
			file_iter != tmp_files.end();
			++file_iter)
	{
		string filename = Glib::path_get_basename (*file_iter);
		if (!pattern.match(filename)) continue;

		DEBUG_TRACE (
			DEBUG::FileUtils,
			string_compose("Found file %1\n", *file_iter)
			);

		result.push_back(*file_iter);
	}

}

void
find_files_matching_pattern (vector<string>& result,
                             const Searchpath& paths,
                             const string& pattern)
{
	Glib::PatternSpec tmp(pattern);
	find_files_matching_pattern (result, paths, tmp);
}

bool
find_file_in_search_path(const Searchpath& search_path,
                         const string& filename,
                         std::string& result)
{
	vector<std::string> tmp;

	find_files_matching_pattern (tmp, search_path, filename);

	if (tmp.size() == 0)
	{
		DEBUG_TRACE (
			DEBUG::FileUtils,
			string_compose("No file matching %1 found in Path: %2\n", filename, search_path.to_string())
			    );
		return false;
	}

	if (tmp.size() != 1)
	{
		DEBUG_TRACE (
			DEBUG::FileUtils,
			string_compose("Found more that one file matching %1 in Path: %2\n", filename, search_path.to_string())
			    );
	}

	result = tmp.front();

	DEBUG_TRACE (
		DEBUG::FileUtils,
		string_compose("Found file %1 in Path: %2\n", filename, search_path.to_string())
		    );

	return true;
}

static
bool
regexp_filter (const string& str, void *arg)
{
	regex_t* pattern = (regex_t*)arg;
	return regexec (pattern, str.c_str(), 0, 0, 0) == 0;
}

void
find_files_matching_regex (vector<string>& result,
                           const Searchpath& paths,
                           const std::string& regexp)
{
	int err;
	char msg[256];
	regex_t compiled_pattern;

	if ((err = regcomp (&compiled_pattern, regexp.c_str(),
			    REG_EXTENDED|REG_NOSUB))) {

		regerror (err, &compiled_pattern,
			  msg, sizeof (msg));

		error << "Cannot compile soundfile regexp for use ("
		      << msg
		      << ")"
		      << endmsg;

		return;
	}

	find_files_matching_filter (result, paths,
	                            regexp_filter, &compiled_pattern,
	                            true, true, false);

	regfree (&compiled_pattern);
}

void
find_files_matching_filter (vector<string>& result,
                            const Searchpath& paths,
                            bool (*filter)(const string &, void *),
                            void *arg,
                            bool match_fullpath, bool return_fullpath,
                            bool recurse)
{
	vector<string> all_files;

	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		string expanded_path = path_expand (*i);
		get_directory_contents (expanded_path, all_files, true, recurse);
	}

	for (vector<string>::iterator i = all_files.begin(); i != all_files.end(); ++i) {

		string fullpath = *i;
		string filename = Glib::path_get_basename (*i);
		string search_str;

		if (match_fullpath) {
			search_str = *i;
		} else {
			search_str = filename;
		}

		if (!filter(search_str, arg)) {
			continue;
		}

		DEBUG_TRACE (DEBUG::FileUtils,
		             string_compose("Found file %1 matching filter\n", search_str));

		if (return_fullpath) {
			result.push_back(fullpath);
		} else {
			result.push_back(filename);
		}
	}
}

bool
copy_file(const std::string & from_path, const std::string & to_path)
{
	if (!Glib::file_test (from_path, Glib::FILE_TEST_EXISTS)) return false;

	int fd_from = -1;
	int fd_to = -1;
	char buf[4096]; // BUFSIZ  ??
	ssize_t nread;

	fd_from = ::open(from_path.c_str(), O_RDONLY);
	if (fd_from < 0) {
		goto copy_error;
	}

	fd_to = ::open(to_path.c_str(), O_WRONLY | O_CREAT, 0666);
	if (fd_to < 0) {
		goto copy_error;
	}

	while (nread = ::read(fd_from, buf, sizeof(buf)), nread > 0) {
		char *out_ptr = buf;
		do {
			ssize_t nwritten = ::write(fd_to, out_ptr, nread);
			if (nwritten >= 0) {
				nread -= nwritten;
				out_ptr += nwritten;
			} else if (errno != EINTR) {
				goto copy_error;
			}
		} while (nread > 0);
	}

	if (nread == 0) {
		if (::close(fd_to)) {
			fd_to = -1;
			goto copy_error;
		}
		::close(fd_from);
		return true;
	}

copy_error:
	int saved_errno = errno;

	if (fd_from >= 0) {
		::close(fd_from);
	}
	if (fd_to >= 0) {
		::close(fd_to);
	}

	error << string_compose (_("Unable to Copy file %1 to %2 (%3)"),
			from_path, to_path, strerror(saved_errno))
		<< endmsg;
	return false;
}

static
bool accept_all_files (string const &, void *)
{
	return true;
}

void
copy_files(const std::string & from_path, const std::string & to_dir)
{
	vector<string> files;
	find_files_matching_filter (files, from_path, accept_all_files, 0, true, false);

	for (vector<string>::iterator i = files.begin(); i != files.end(); ++i) {
		std::string from = Glib::build_filename (from_path, *i);
		std::string to = Glib::build_filename (to_dir, *i);
		copy_file (from, to);
	}
}

std::string
get_absolute_path (const std::string & p)
{
	if (Glib::path_is_absolute(p)) return p;
	return Glib::build_filename (Glib::get_current_dir(), p);
}

bool
equivalent_paths (const std::string& a, const std::string& b)
{
	GStatBuf bA;
	int const rA = g_stat (a.c_str(), &bA);
	GStatBuf bB;
	int const rB = g_stat (b.c_str(), &bB);

	return (rA == 0 && rB == 0 && bA.st_dev == bB.st_dev && bA.st_ino == bB.st_ino);
}

bool
path_is_within (std::string const & haystack, std::string needle)
{
	while (1) {
		if (equivalent_paths (haystack, needle)) {
			return true;
		}

		needle = Glib::path_get_dirname (needle);
		if (needle == "." || needle == "/") {
			break;
		}
	}

	return false;
}

bool
exists_and_writable (const std::string & p)
{
	/* writable() really reflects the whole folder, but if for any
	   reason the session state file can't be written to, still
	   make us unwritable.
	*/

	GStatBuf statbuf;

	if (g_stat (p.c_str(), &statbuf) != 0) {
		/* doesn't exist - not writable */
		return false;
	} else {
		if (!(statbuf.st_mode & S_IWUSR)) {
			/* exists and is not writable */
			return false;
		}
		/* filesystem may be mounted read-only, so even though file
		 * permissions permit access, the mount status does not.
		 * access(2) seems like the best test for this.
		 */
		if (g_access (p.c_str(), W_OK) != 0) {
			return false;
		}
	}

	return true;
}

} // namespace PBD
