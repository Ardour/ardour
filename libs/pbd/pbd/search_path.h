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

#ifndef PBD_SEARCH_PATH_INCLUDED
#define PBD_SEARCH_PATH_INCLUDED

#include <string>
#include <vector>

#include "pbd/libpbd_visibility.h"

namespace PBD {

/**
 * @class SearchPath
 *
 * The SearchPath class is a helper class for getting a
 * vector of paths contained in a search path string where a 
 * "search path string" contains absolute directory paths 
 * separated by a colon(:) or a semi-colon(;) on windows.
 *
 * The SearchPath class does not test whether the paths exist
 * or are directories. It is basically just a container.
 */
class LIBPBD_API SearchPath : public std::vector<std::string>
{
public:
	/**
	 * Create an empty SearchPath.
	 */
	SearchPath ();

	/**
	 * Initialize SearchPath from a string where the string contains
	 * one or more absolute paths to directories which are delimited 
	 * by a path separation character. The path delimeter is a 
	 * colon(:) on unix and a semi-colon(;) on windows.
	 *
	 * Each path contained in the search path may or may not resolve to
	 * an existing directory in the filesystem.
	 * 
	 * @param search_path A path string.
	 */
	SearchPath (const std::string& search_path);

	/**
	 * Initialize SearchPath from a vector of paths that may or may
	 * not exist.
	 *
	 * @param paths A vector of paths.
	 */
	SearchPath (const std::vector<std::string>& paths);

	/**
	 * @return a search path string.
	 *
	 * The string that is returned contains the platform specific
	 * path separator.
	 */
	const std::string to_string () const;

	/**
	 * Add all the directories in path to this.
	 */
	SearchPath& operator+= (const SearchPath& spath);

	/**
	 * Add another directory path to the search path.
	 */
	SearchPath& operator+= (const std::string& directory_path);
	
	/**
	 * Concatenate another SearchPath onto this.
	 */
	SearchPath& operator+ (const SearchPath& other);
	
	/**
	 * Add another path to the search path.
	 */
	SearchPath& operator+ (const std::string& directory_path);

	/**
	 * Add a sub-directory to each path in the search path.
	 * @param subdir The directory name, it should not contain 
	 * any path separating tokens.
	 */
	SearchPath& add_subdirectory_to_paths (const std::string& subdir);

protected:

	void add_directory (const std::string& directory_path);
	void add_directories (const std::vector<std::string>& paths);
};

} // namespace PBD

#endif
