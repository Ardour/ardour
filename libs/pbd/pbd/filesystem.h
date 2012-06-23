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

/**
 * @namespace PBD::sys
 *
 * The API in this file is intended to be as close as possible to the
 * boost::filesystem API but implementing only the subset of it that is required
 * by ardour using the glib/glibmm file utility functions in the implementation.
 *
 * More information about boost::filesystem and the TR2 proposal at
 *
 * http://www.boost.org/libs/filesystem/doc/tr2_proposal.html
 *
 * Hopefully the boost::filesystem API will pass TR2 review etc and become part
 * of the C++ standard and this code can be removed, or we just end up using
 * the boost filesystem library when it matures a bit more.
 * 
 * My reasons for writing this thin wrapper instead of using glib directly or
 * using boost::filesystem immediately are:
 *
 *  - Using sys::path instead of strings and Glib::build_filename is more
 *    convenient, terse and forces correct platform agnostic path building.
 *
 *  - Using boost::filesystem on windows would mean converting between any UTF-8
 *    encoded strings(such as when selecting a file/directory in the gtk file
 *    chooser) and the native file encoding (UTF-16). It would take some time
 *    and testing to find out when this is required and the glib functions already
 *    do this if necessary.
 *
 *  - Using exceptions to indicate errors is more likely to uncover situations 
 *    where error conditions are being silently ignored(I've already encounted
 *    a few examples of this in the ardour code).
 *
 *  - Many of the glib file utility functions are not wrapped by glibmm so this
 *    also provides what I think is a better API.
 *
 *  - Using boost::filesystem directly means another library dependence and would
 *    require more testing on windows because of the character encoding issue.
 *  
 *  - The boost::filesystem API changes a bit as part of the TR2 review process etc.
 */

#ifndef __filesystem_h__
#define __filesystem_h__

#include <stdexcept>
#include <string>

namespace PBD {

namespace sys {

class path
{
public:
	path() : m_path("") { }
	path(const path & p) : m_path(p.m_path) { }
	path(const std::string & s) : m_path(s) { }
	path(const char* s) : m_path(s) { }
	
	path& operator=(const path& p) { m_path = p.m_path; return *this;}
	path& operator=(const std::string& s) { m_path = s; return *this; }
	path& operator=(const char* s) { m_path = s; return *this; }

	path& operator/=(const path& rhs);
	path& operator/=(const std::string& s);
	path& operator/=(const char* s);

	const std::string to_string() const { return m_path; }

	/**
	 * @return the last component of the path, if the path refers to
	 * a file then it will be the entire filename including any extension.
	 */
	std::string leaf () const; 

	/**
	 * @returns the directory component of a path without any trailing
	 * path separator or an empty string if the path has no directory
	 * component(branch path).
	 */
	path branch_path () const;

private:

	std::string m_path;
};

class filesystem_error : public std::runtime_error
{
	const int m_error_code;

public:
	explicit filesystem_error(const std::string & what, int error_code=0)
		: std::runtime_error(what), m_error_code(error_code) { }

	int system_error() const { return m_error_code; }
};

inline path operator/ (const path& lhs, const path& rhs)
{ return path(lhs) /= rhs; }

/// @return true if path at p exists
bool exists(const path & p);


/// @return true if path at p exists and is writable, false otherwise
bool exists_and_writable(const path & p);

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
 * Renames from_path to to_path as if by the glib function g_rename.
 */
void rename (const path& from_path, const path& to_path);

/**
 * @return The substring of the filename component of the path, starting
 * at the beginning of the filename up to but not including the last dot.
 *
 * boost::filesystem::path::basename differs from g_path_get_basename and
 * ::basename and most other forms of basename in that it removes the
 * extension from the filename if the filename has one.
 */ 
std::string basename (const path& p);

/**
 * @return If the filename contains a dot, return a substring of the
 * filename starting the rightmost dot to the end of the string, otherwise
 * an empty string.
 *
 * @param p a file path.
 */
std::string extension (const path& p);

path get_absolute_path (const path &);

bool path_is_within (const std::string &, std::string);

/**
 * @return true if p1 and p2 both resolve to the same file
 * @param p1 a file path.
 * @param p2 a file path.
 *
 * Uses g_stat to check for identical st_dev and st_ino values.
 */
bool equivalent_paths (const std::string &p1, const std::string &p2);

} // namespace sys

} // namespace PBD

#endif
