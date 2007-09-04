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

#ifndef __filesystem_h__
#define __filesystem_h__

#include <stdexcept>
#include <string>

namespace PBD {

namespace sys {

using std::string;

class path
{
public:
	path() : m_path("") { }
	path(const path & p) : m_path(p.m_path) { }
	path(const string & s) : m_path(s) { }
	path(const char* s) : m_path(s) { }
	
	path& operator=(const path& p) { m_path = p.m_path; return *this;}
	path& operator=(const string& s) { m_path = s; return *this; }
	path& operator=(const char* s) { m_path = s; return *this; }

	path& operator/=(const path& rhs);
	path& operator/=(const string& s);
	path& operator/=(const char* s);

	const string to_string() const { return m_path; }

private:

	string m_path;
};

class filesystem_error : public std::runtime_error
{
	const int m_error_code;

public:
	explicit filesystem_error(const std::string & what, int error_code=0)
		: std::runtime_error(what), m_error_code(error_code) { }

	int system_error() const { return m_error_code; }
};

/// @return true if path at p exists
bool exists(const path & p);

/// @return true if path at p is a directory.
bool is_directory(const path & p);

/**
 * Attempt to create a directory at p as if by the glib function g_mkdir 
 * with a second argument of S_IRWXU|S_IRWXG|S_IRWXO
 * 
 * @throw filesystem_error if mkdir fails for any other reason other than
 * the directory already exists.
 *
 * @return true If the directory p was created, otherwise false
 *
 * @post is_directory(p)
 */
bool create_directory(const path & p);

/**
 * Attempt to create a directory at p as if by the glib function 
 * g_mkdir_with_parents with a second argument of S_IRWXU|S_IRWXG|S_IRWXO
 * 
 * @throw filesystem_error if g_mkdir_with_parents fails for any other 
 * reason other than the directory already exists.
 *
 * @return true If the directory at p was created, otherwise false
 *
 * @post is_directory(p)
 */
bool create_directories(const path & p);

/**
 * Attempt to delete the file at path p as if by the glib function
 * g_unlink.
 *
 * @return true if file existed prior to removing it, false if file
 * at p did not exist.
 *
 * @throw filesystem_error if removing the file failed for any other
 * reason other than the file did not exist.
 */
bool remove(const path & p);

/**
 * Attempt to copy the contents of the file from_path to a new file 
 * at path to_path.
 *
 * @throw filesystem_error if from_path.empty() || to_path.empty() ||
 * !exists(from_path) || !is_regular(from_path) || exists(to_path)
 */
void copy_file(const path & from_path, const path & to_path);


string basename (const path& p);

} // namespace sys

} // namespace PBD

#endif
