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

#include <sys/stat.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <giomm/file.h>

#include <cerrno>
#include <fstream>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/filesystem.h"
#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/pathscanner.h"

#include "i18n.h"

using namespace std;

namespace PBD {

namespace sys {
	
path&
path::operator/=(const path& rhs)
{
	m_path = Glib::build_filename(m_path, rhs.m_path);
	return *this;
}

path&
path::operator/=(const string& rhs)
{
	m_path = Glib::build_filename(m_path, rhs);
	return *this;
}

path&
path::operator/=(const char* rhs)
{
	m_path = Glib::build_filename(m_path, rhs);
	return *this;
}

string
path::leaf () const
{
	return Glib::path_get_basename(m_path);
}

path
path::branch_path () const
{
	string dir = Glib::path_get_dirname (m_path);

	/*
	 * glib returns "." to signify that the path
	 * has no directory components(branch path)
	 * whereas boost::filesystem returns an empty
	 * string
	 */
	if(dir == ".")
	{
		return "";
	}
	return dir;
}

bool
exists (const path & p)
{
	return Glib::file_test (p.to_string(), Glib::FILE_TEST_EXISTS);
}

bool
exists_and_writable (const path & p)
{
	/* writable() really reflects the whole folder, but if for any
	   reason the session state file can't be written to, still
	   make us unwritable.
	*/

	struct stat statbuf;

	if (g_stat (p.to_string().c_str(), &statbuf) != 0) {
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
		if (g_access (p.to_string().c_str(), W_OK) != 0) {
			return false;
		}
	}

	return true;
}


bool
is_directory (const path & p)
{
	return Glib::file_test (p.to_string(), Glib::FILE_TEST_IS_DIR);
}

bool
create_directory(const path & p)
{
	if(is_directory(p)) return false;

	int error = g_mkdir (p.to_string().c_str(), S_IRWXU|S_IRWXG|S_IRWXO);

	if(error == -1)
	{
		throw filesystem_error(g_strerror(errno), errno);
	}
	return true;
}

bool
create_directories(const path & p)
{
	if(is_directory(p)) return false;

	int error = g_mkdir_with_parents (p.to_string().c_str(), S_IRWXU|S_IRWXG|S_IRWXO);

	if(error == -1)
	{
		throw filesystem_error(g_strerror(errno), errno);
	}
	return true;
}

bool
remove(const path & p)
{
	if(!exists(p)) return false;

	int error = g_unlink (p.to_string().c_str());

	if(error == -1)
	{
		throw filesystem_error(g_strerror(errno), errno);
	}
	return true;
}

void
rename (const path & from_path, const path & to_path)
{
	// g_rename is a macro that evaluates to ::rename on
	// POSIX systems, without the global namespace qualifier
	// it would evaluate to a recursive call(if it compiled)
	if ( ::g_rename( from_path.to_string().c_str(),
				to_path.to_string().c_str() ) == -1 )
	{
		throw filesystem_error(g_strerror(errno), errno);
	}
}
	
string
basename (const path & p)
{
	string base(p.leaf());

	string::size_type n = base.rfind ('.');

	return base.substr (0, n);
}

string
extension (const path & p)
{
	string base(p.leaf());

	string::size_type n = base.rfind ('.');

	if (n != string::npos)
	{
		return base.substr(n);
	}

	return string();

}

path
get_absolute_path (const path & p)
{
	Glib::RefPtr<Gio::File> f = Gio::File::create_for_path (p.to_string ());
	return f->get_path ();
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

} // namespace sys

} // namespace PBD
