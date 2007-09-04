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

#include <cerrno>
#include <fstream>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <pbd/filesystem.h>
#include <pbd/error.h>
#include <pbd/compose.h>

#include "i18n.h"

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
copy_file(const path & from_path, const path & to_path)
{
	// this implementation could use mucho memory
	// for big files.
	std::ifstream in(from_path.to_string().c_str());
	std::ofstream out(to_path.to_string().c_str());
	
	if (!in || !out) {
		throw filesystem_error(string_compose(_("Could not open files %1 and %2 for copying"),
					from_path.to_string(), to_path.to_string()));
	}
	
	out << in.rdbuf();
	
	if (!in || !out) {
		throw filesystem_error(string_compose(_("Could not copy existing file %1 to %2"),
					from_path.to_string(), to_path.to_string()));
		remove (to_path);
	}
}

string
basename (const path & p)
{
	// I'm not sure if this works quite the same as boost::filesystem::basename
	return Glib::path_get_basename (p.to_string ());
}

} // namespace sys

} // namespace PBD
