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
run_functor_for_paths (vector<string>& result,
                       const Searchpath& paths,
                       bool (*functor)(const string &, void *),
                       void *arg,
                       bool pass_files_only,
                       bool pass_fullpath, bool return_fullpath,
                       bool recurse)
{
	for (vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		string expanded_path = path_expand (*i);
		DEBUG_TRACE (DEBUG::FileUtils,
				string_compose("Find files in expanded path: %1\n", expanded_path));

		if (!Glib::file_test (expanded_path, Glib::FILE_TEST_IS_DIR)) continue;

		try
		{
			Glib::Dir dir(expanded_path);

			for (Glib::DirIterator di = dir.begin(); di != dir.end(); di++) {

				string fullpath = Glib::build_filename (expanded_path, *di);
				string basename = *di;

				bool is_dir = Glib::file_test (fullpath, Glib::FILE_TEST_IS_DIR);

				if (is_dir && recurse) {
					DEBUG_TRACE (DEBUG::FileUtils,
							string_compose("Descending into directory:  %1\n",
								fullpath));
					run_functor_for_paths (result, fullpath, functor, arg, pass_files_only,
					                       pass_fullpath, return_fullpath, recurse);
				}

				if (is_dir && pass_files_only) {
					continue;
				}

				string functor_str;

				if (pass_fullpath) {
					functor_str = fullpath;
				} else {
					functor_str = basename;
				}

				DEBUG_TRACE (DEBUG::FileUtils,
						string_compose("Run Functor using string: %1\n", functor_str));

				if (!functor(functor_str, arg)) {
					continue;
				}

				DEBUG_TRACE (DEBUG::FileUtils,
						string_compose("Found file %1 matching functor\n", functor_str));

				if (return_fullpath) {
					result.push_back(fullpath);
				} else {
					result.push_back(basename);
				}
			}
		}
		catch (Glib::FileError& err)
		{
			warning << err.what() << endmsg;
		}
	}
}

static
bool accept_all_files (string const &, void *)
{
	return true;
}

void
get_paths (vector<string>& result,
           const Searchpath& paths,
           bool files_only,
           bool recurse)
{
	run_functor_for_paths (result, paths, accept_all_files, 0,
	                       files_only, true, true, recurse);
}

void
get_files (vector<string>& result, const Searchpath& paths)
{
	return get_paths (result, paths, true, false);
}

static
bool
pattern_filter (const string& str, void *arg)
{
	Glib::PatternSpec* pattern = (Glib::PatternSpec*)arg;
	return pattern->match(str);
}

void
find_files_matching_pattern (vector<string>& result,
                             const Searchpath& paths,
                             const Glib::PatternSpec& pattern)
{
	run_functor_for_paths (result, paths, pattern_filter,
	                       const_cast<Glib::PatternSpec*>(&pattern),
	                       true, false, true, false);
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
find_file (const Searchpath& search_path,
           const string& filename,
           std::string& result)
{
	vector<std::string> tmp;

	find_files_matching_pattern (tmp, search_path, filename);

	if (tmp.size() == 0) {
		DEBUG_TRACE (DEBUG::FileUtils,
		             string_compose("No file matching %1 found in Path: %2\n",
		             filename, search_path.to_string()));
		return false;
	}

	if (tmp.size() != 1) {
		DEBUG_TRACE (DEBUG::FileUtils,
		             string_compose("Found more that one file matching %1 in Path: %2\n",
		             filename, search_path.to_string()));
	}

	result = tmp.front();

	DEBUG_TRACE (DEBUG::FileUtils,
	             string_compose("Found file %1 in Path: %2\n",
	             filename, search_path.to_string()));

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

	DEBUG_TRACE (DEBUG::FileUtils,
			string_compose("Matching files using regexp: %1\n", regexp));

	find_files_matching_filter (result, paths,
	                            regexp_filter, &compiled_pattern,
	                            true, true, false);

	regfree (&compiled_pattern);
}

void
find_paths_matching_filter (vector<string>& result,
                            const Searchpath& paths,
                            bool (*filter)(const string &, void *),
                            void *arg,
                            bool pass_fullpath, bool return_fullpath,
                            bool recurse)
{
	run_functor_for_paths (result, paths, filter, arg, false, pass_fullpath, return_fullpath, recurse);
}

void
find_files_matching_filter (vector<string>& result,
                            const Searchpath& paths,
                            bool (*filter)(const string &, void *),
                            void *arg,
                            bool pass_fullpath, bool return_fullpath,
                            bool recurse)
{
	run_functor_for_paths (result, paths, filter, arg, true, pass_fullpath, return_fullpath, recurse);
}

#ifdef PLATFORM_WINDOWS
#define WRITE_FLAGS O_RDWR | O_CREAT | O_BINARY
#define READ_FLAGS O_RDONLY | O_BINARY
#else
#define WRITE_FLAGS O_RDWR | O_CREAT
#define READ_FLAGS O_RDONLY
#endif

bool
copy_file(const std::string & from_path, const std::string & to_path)
{
	if (!Glib::file_test (from_path, Glib::FILE_TEST_EXISTS)) return false;

	int fd_from = -1;
	int fd_to = -1;
	char buf[4096]; // BUFSIZ  ??
	ssize_t nread;

	fd_from = g_open(from_path.c_str(), READ_FLAGS, 0444);
	if (fd_from < 0) {
		goto copy_error;
	}

	fd_to = g_open(to_path.c_str(), WRITE_FLAGS, 0666);
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

int
remove_directory_internal (const string& dir, size_t* size, vector<string>* paths,
                           bool just_remove_files)
{
	vector<string> tmp_paths;
	struct stat statbuf;
	int ret = 0;

	get_paths (tmp_paths, dir, just_remove_files, true);

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
clear_directory (const string& dir, size_t* size, vector<string>* paths)
{
	return remove_directory_internal (dir, size, paths, true);
}

// rm -rf <dir> -- used to remove saved plugin state
void
remove_directory (const std::string& dir)
{
	remove_directory_internal (dir, 0, 0, false);
}

} // namespace PBD
