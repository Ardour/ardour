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

#include <string>

#include <glib.h>
#include <glibmm/miscutils.h>

#include "pbd/tokenizer.h"
#include "pbd/search_path.h"
#include "pbd/error.h"

using namespace std;

namespace PBD {

Searchpath::Searchpath ()
{

}

Searchpath::Searchpath (const string& path)
{
	vector<std::string> tmp;

	if (tokenize (path, string(G_SEARCHPATH_SEPARATOR_S), std::back_inserter (tmp))) {
		add_directories (tmp);
	}
}

Searchpath::Searchpath (const vector<std::string>& paths)
{
	add_directories (paths);
}

void
Searchpath::remove_directory (const std::string& directory_path)
{
	if (directory_path.empty()) {
		return;
	}

	for (vector<std::string>::iterator i = begin(); i != end();) {
		if (*i == directory_path) {
			i = erase (i);
		} else {
			++i;
		}
	}
}

void
Searchpath::remove_directories (const vector<std::string>& paths)
{
	for(vector<std::string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		remove_directory (*i);
	}
}

void
Searchpath::add_directory (const std::string& directory_path)
{
	if (directory_path.empty()) {
		return;
	}
	for (vector<std::string>::const_iterator i = begin(); i != end(); ++i) {
		if (*i == directory_path) {
			return;
		}
	}
	push_back(directory_path);
}

void
Searchpath::add_directories (const vector<std::string>& paths)
{
	for(vector<std::string>::const_iterator i = paths.begin(); i != paths.end(); ++i) {
		add_directory (*i);
	}
}

const string
Searchpath::to_string () const
{
	string path;

	for (vector<std::string>::const_iterator i = begin(); i != end(); ++i) {
		path += *i;
		path += G_SEARCHPATH_SEPARATOR;
	}

	path = path.substr (0, path.length() - 1); // drop final separator

	return path;
}

Searchpath&
Searchpath::operator+= (const Searchpath& spath)
{
	insert(end(), spath.begin(), spath.end());
	return *this;
}

Searchpath&
Searchpath::operator+= (const std::string& directory_path)
{
	add_directory (directory_path);
	return *this;
}

Searchpath&
Searchpath::operator+ (const std::string& directory_path)
{
	add_directory (directory_path);
	return *this;
}

Searchpath&
Searchpath::operator+ (const Searchpath& spath)
{
	// concatenate paths into new Searchpath
	insert(end(), spath.begin(), spath.end());
	return *this;
}

Searchpath&
Searchpath::operator-= (const Searchpath& spath)
{
	remove_directories (spath);
	return *this;
}

Searchpath&
Searchpath::operator-= (const std::string& directory_path)
{
	remove_directory (directory_path);
	return *this;
}


Searchpath&
Searchpath::add_subdirectory_to_paths (const string& subdir)
{
	for (vector<std::string>::iterator i = begin(); i != end(); ++i) {
		// should these new paths just be added to the end of 
		// the search path rather than replace?
		*i = Glib::build_filename (*i, subdir);
	}
	
	return *this;
}

/* This is not part of the Searchpath object, but is closely related to the
 * whole idea, and we put it here for convenience.
 */

void 
export_search_path (const string& base_dir, const char* varname, const char* dir)
{
	string path;
	const char * cstr = g_getenv (varname);

	if (cstr) {
		path = cstr;
		path += G_SEARCHPATH_SEPARATOR;
	} else {
		path = "";
	}
	path += base_dir;
	path += dir;

	g_setenv (varname, path.c_str(), 1);
}

} // namespace PBD
