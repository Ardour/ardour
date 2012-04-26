/*
   Copyright (C) 2006  Paul Davis

   This program is free software; you can redistribute it and/or
   modify it under the terms of the GNU General Public License
   as published by the Free Software Foundation; either version 2
   of the License, or (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include <cerrno>
#include <unistd.h>

#include <glib.h>
#include <glib/gstdio.h>

#include <glibmm/miscutils.h>
#include <glibmm/fileutils.h>

#include <pbd/path.h>
#include <pbd/tokenizer.h>

namespace PBD {

Path::Path ()
{

}

Path::Path (const string& path)
{
	vector<string> tmp;

	if(!tokenize ( path, string(":;"), std::back_inserter (tmp))) {
		g_warning ("%s : %s\n", G_STRLOC, G_STRFUNC);
		return;
	}

	add_readable_directories (tmp);
}

Path::Path (const vector<string>& paths)
{
	add_readable_directories (paths);
}

Path::Path (const Path& other)
	: m_dirs(other.m_dirs)
{

}

bool
Path::readable_directory (const string& directory_path)
{
	if (g_access (directory_path.c_str(), R_OK) == 0) {
		if (Glib::file_test(directory_path, Glib::FILE_TEST_IS_DIR)) {
			return true;
		} else {
			g_warning (" %s : Path exists but is not a directory\n", G_STRLOC); 
		}
	} else {
		g_warning ("%s : %s : %s\n", G_STRLOC, directory_path.c_str(), g_strerror(errno));
	}
	return false;
}

void
Path::add_readable_directory (const string& directory_path)
{
	if(readable_directory(directory_path)) {
		m_dirs.push_back(directory_path);
	}
}

void
Path::add_readable_directories (const vector<string>& paths)
{
	for(vector<string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		add_readable_directory (*i);
	}
}

const string
Path::path_string() const
{
	string path;

	for (vector<string>::const_iterator i = m_dirs.begin(); i != m_dirs.end(); ++i) {
		path += (*i);
		path += G_SEARCHPATH_SEPARATOR;
	}

	g_message ("%s : %s", G_STRLOC, path.c_str());

	return path.substr (0, path.length() - 1); // drop final colon
}

const Path&
Path::operator= (const Path& path)
{
	m_dirs = path.m_dirs;
	return *this;
}

const Path& 
Path::operator+= (const string& directory_path)
{
	add_readable_directory (directory_path);
	return *this;
}

const Path 
operator+ (const Path& lhs_path, const Path& rhs_path)
{
	Path tmp_path(lhs_path); // does this do what I think it does.
	// concatenate paths into new Path
	tmp_path.m_dirs.insert(tmp_path.m_dirs.end(), rhs_path.m_dirs.begin(), rhs_path.m_dirs.end());
	return tmp_path;
}

Path&
Path::add_subdirectory_to_path (const string& subdir)
{
	vector<string> tmp;
	string directory_path;

	for (vector<string>::iterator i = m_dirs.begin(); i != m_dirs.end(); ++i) {
		directory_path = Glib::build_filename (*i, subdir);
		if(readable_directory(directory_path)) {
			tmp.push_back(directory_path);
		}
	}
	m_dirs = tmp;
	return *this;
}

bool
find_file_in_path (const Path& path, const string& filename, string& resulting_path)
{
	for (vector<string>::const_iterator i = path.dirs().begin(); i != path.dirs().end(); ++i) {
		resulting_path = Glib::build_filename ((*i), filename);
		if (g_access (resulting_path.c_str(), R_OK) == 0) {
			g_message ("File %s found in Path : %s\n", resulting_path.c_str(),
					path.path_string().c_str());
			return true;
		}
	}

	g_warning ("%s : Could not locate file %s in path %s\n", G_STRLOC, filename.c_str(),
			path.path_string().c_str());
	
	return false;
}

} // namespace PBD

