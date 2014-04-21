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

#include <algorithm>
#include <vector>

#include <glib.h>
#include <glib/gstdio.h>

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
#else
#include <unistd.h>
#endif

#include "pbd/compose.h"
#include "pbd/file_utils.h"
#include "pbd/error.h"
#include "pbd/pathscanner.h"
#include "pbd/stl_delete.h"

#include "i18n.h"

using namespace std;

namespace PBD {

void
get_files_in_directory (const std::string& directory_path, vector<string>& result)
{
	if (!Glib::file_test (directory_path, Glib::FILE_TEST_IS_DIR)) return;

	try
	{
		Glib::Dir dir(directory_path);
		std::copy(dir.begin(), dir.end(), std::back_inserter(result));
	}
	catch (Glib::FileError& err)
	{
		warning << err.what() << endmsg;
	}
}

void
find_matching_files_in_directory (const std::string& directory,
                                  const Glib::PatternSpec& pattern,
                                  vector<std::string>& result)
{
	vector<string> tmp_files;

	get_files_in_directory (directory, tmp_files);
	result.reserve(tmp_files.size());

	for (vector<string>::iterator file_iter = tmp_files.begin();
			file_iter != tmp_files.end();
			++file_iter)
	{
		if (!pattern.match(*file_iter)) continue;

		std::string full_path(directory);
		full_path = Glib::build_filename (full_path, *file_iter);

		result.push_back(full_path);
	}
}

void
find_matching_files_in_directories (const vector<std::string>& paths,
                                    const Glib::PatternSpec& pattern,
                                    vector<std::string>& result)
{
	for (vector<std::string>::const_iterator path_iter = paths.begin();
			path_iter != paths.end();
			++path_iter)
	{
		find_matching_files_in_directory (*path_iter, pattern, result);
	}		
}

void
find_matching_files_in_search_path (const SearchPath& search_path,
                                    const Glib::PatternSpec& pattern,
                                    vector<std::string>& result)
{
	find_matching_files_in_directories (search_path, pattern, result);    
}

bool
find_file_in_search_path(const SearchPath& search_path,
                         const string& filename,
                         std::string& result)
{
	vector<std::string> tmp;
	Glib::PatternSpec tmp_pattern(filename);

	find_matching_files_in_search_path (search_path, tmp_pattern, tmp);

	if (tmp.size() == 0)
	{
		return false;
	}

#if 0
	if (tmp.size() != 1)
	{
		info << string_compose
			(
			 "Found more than one file matching %1 in search path %2",
			 filename,
			 search_path ()
			)
			<< endmsg;
	}
#endif

	result = tmp.front();

	return true;
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
	PathScanner scanner;
	vector<string*>* files = scanner (from_path, accept_all_files, 0, true, false);

	if (files) {
		for (vector<string*>::iterator i = files->begin(); i != files->end(); ++i) {
			std::string from = Glib::build_filename (from_path, **i);
			std::string to = Glib::build_filename (to_dir, **i);
			copy_file (from, to);
		}
		vector_delete (files);
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
	struct stat bA;
	int const rA = g_stat (a.c_str(), &bA);
	struct stat bB;
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

	struct stat statbuf;

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
