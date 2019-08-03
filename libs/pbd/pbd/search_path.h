/*
 * Copyright (C) 2007-2015 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2015 Robin Gareus <robin@gareus.org>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#ifndef __libpbd_search_path_h__
#define __libpbd_search_path_h__

#include <string>
#include <vector>

#include "pbd/libpbd_visibility.h"

namespace PBD {

/**
 * @class Searchpath
 *
 * The Searchpath class is a helper class for getting a
 * vector of paths contained in a search path string where a
 * "search path string" contains absolute directory paths
 * separated by a colon(:) or a semi-colon(;) on windows.
 *
 * The Searchpath class does not test whether the paths exist
 * or are directories. It is basically just a container.
 */
class LIBPBD_TEMPLATE_API Searchpath : public std::vector<std::string>
{
public:
	/**
	 * Create an empty Searchpath.
	 */
	LIBPBD_TEMPLATE_MEMBER_API Searchpath ();

	/**
	 * Initialize Searchpath from a string where the string contains
	 * one or more absolute paths to directories which are delimited
	 * by a path separation character. The path delimeter is a
	 * colon(:) on unix and a semi-colon(;) on windows.
	 *
	 * Each path contained in the search path may or may not resolve to
	 * an existing directory in the filesystem.
	 *
	 * @param search_path A path string.
	 */
	LIBPBD_TEMPLATE_MEMBER_API Searchpath (const std::string& search_path);

	/**
	 * Initialize Searchpath from a vector of paths that may or may
	 * not exist.
	 *
	 * @param paths A vector of paths.
	 */
	LIBPBD_TEMPLATE_MEMBER_API Searchpath (const std::vector<std::string>& paths);

	LIBPBD_TEMPLATE_MEMBER_API ~Searchpath () {};

	/**
	 * @return a search path string.
	 *
	 * The string that is returned contains the platform specific
	 * path separator.
	 */
	LIBPBD_TEMPLATE_MEMBER_API const std::string to_string () const;

	/**
	 * Add all the directories in path to this.
	 */
	LIBPBD_TEMPLATE_MEMBER_API Searchpath& operator+= (const Searchpath& spath);

	/**
	 * Add another directory path to the search path.
	 */
	LIBPBD_TEMPLATE_MEMBER_API Searchpath& operator+= (const std::string& directory_path);

	/**
	 * Concatenate another Searchpath onto this.
	 */
	LIBPBD_TEMPLATE_MEMBER_API const Searchpath operator+ (const Searchpath& other);

	/**
	 * Add another path to the search path.
	 */
	LIBPBD_TEMPLATE_MEMBER_API const Searchpath operator+ (const std::string& directory_path);

	/**
	 * Remove all the directories in path from this.
	 */
	LIBPBD_TEMPLATE_MEMBER_API Searchpath& operator-= (const Searchpath& spath);

	/**
	 * Remove a directory path from the search path.
	 */
	LIBPBD_TEMPLATE_MEMBER_API Searchpath& operator-= (const std::string& directory_path);

	/**
	 * Add a sub-directory to each path in the search path.
	 * @param subdir The directory name, it should not contain
	 * any path separating tokens.
	 */
	LIBPBD_TEMPLATE_MEMBER_API Searchpath& add_subdirectory_to_paths (const std::string& subdir);

	/**
	 * Add directory_path to this Searchpath.
	 */
	LIBPBD_TEMPLATE_MEMBER_API void add_directory (const std::string& directory_path);

	/**
	 * Add directories in paths to this Searchpath.
	 */
	LIBPBD_TEMPLATE_MEMBER_API void add_directories (const std::vector<std::string>& paths);

	/**
	 * Remove directory_path from this Searchpath.
	 */
	LIBPBD_TEMPLATE_MEMBER_API void remove_directory (const std::string& directory_path);

	/**
	 * Remove all the directories in paths from this Searchpath.
	 */
	LIBPBD_TEMPLATE_MEMBER_API void remove_directories (const std::vector<std::string>& paths);

	/**
	 * @return true if Searchpath already contains path
	 */
	LIBPBD_TEMPLATE_MEMBER_API bool contains (const std::string& path) const;
};

LIBPBD_API void export_search_path (const std::string& base_dir, const char* varname, const char* dir);


} // namespace PBD

#endif /* __libpbd_search_path_h__ */
