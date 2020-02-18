/*
 * Copyright (C) 2007-2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#ifndef PBD_FILE_UTILS_INCLUDED
#define PBD_FILE_UTILS_INCLUDED

#include <string>
#include <vector>

#include <glibmm/pattern.h>

#include "pbd/libpbd_visibility.h"
#include "pbd/search_path.h"

namespace PBD {

/**
 * Get a list of path entries in a directory or within a directory tree
 * if recursing.
 * @note paths in result will be absolute
 *
 * @param result A vector of absolute paths to the directory entries in filename
 * encoding.
 * @param paths A Searchpath
 * @param files_only Only include file entries in result
 * @param recurse Recurse into child directories
 */
LIBPBD_API void
get_paths (std::vector<std::string>& result,
           const Searchpath& paths,
           bool files_only = true,
           bool recurse = false);

/**
 * Get a list of files in a Searchpath.
 *
 * @param paths A Searchpath
 * @param result A vector of absolute paths to files.
 */
LIBPBD_API void
get_files (std::vector<std::string>& result,
           const Searchpath& paths);

/**
 * Takes a Searchpath and returns all the files contained in the
 * directory paths that match a particular pattern.
 *
 * @param result A vector in which to place the resulting matches.
 * @param paths A Searchpath
 * @param pattern A Glib::PatternSpec used to match the files.
 */
LIBPBD_API void
find_files_matching_pattern (std::vector<std::string>& result,
                             const Searchpath& paths,
                             const Glib::PatternSpec& pattern);

/**
 * Takes a Searchpath and returns all the files contained in the
 * directory paths that match a particular pattern.
 *
 * This is a convenience method to avoid explicitly declaring
 * temporary variables such as:
 * find_files_matching_pattern (result, paths, string("*.ext"))
 *
 * @param result A vector in which to place the resulting matches.
 * @param paths A Searchpath
 * @param pattern A string representing the Glib::PatternSpec used
 * to match the files.
 */
LIBPBD_API void
find_files_matching_pattern (std::vector<std::string>& result,
                             const Searchpath& paths,
                             const std::string& pattern);

/**
 * Takes a search path and a file name and places the full path
 * to that file in result if it is found within the search path.
 *
 * @note the parameter order of this function doesn't match the
 * others. At the time of writing it is the most widely used file
 * utility function so I didn't change it but it may be worth
 * doing at some point if it causes any confusion.
 *
 * @return true If file is found within the search path.
 */
LIBPBD_API bool
find_file (const Searchpath& search_path,
           const std::string& filename,
           std::string& result);


/**
 * Find files in paths that match a regular expression
 *
 * @param results A vector in which to place the resulting matches.
 * @param paths A Searchpath
 * @param regexp A regular expression
 * @param recurse Search directories recursively
 */
LIBPBD_API void
find_files_matching_regex (std::vector<std::string>& results,
                           const Searchpath& paths,
                           const std::string& regexp,
                           bool recurse = false);

/**
 * Find paths in a Searchpath that match a supplied filter(functor)
 * @note results include files and directories.
 *
 * @param results A vector in which to place the resulting matches.
 * @param paths A Searchpath
 * @param filter A functor to use to filter paths
 * @param arg additonal argument to filter if required
 * @param pass_fullpath pass the full path to the filter or just the basename
 * @param return_fullpath put the full path in results or just the basename
 * @param recurse Recurse into child directories to find paths.
 */
LIBPBD_API void
find_paths_matching_filter (std::vector<std::string>& results,
                            const Searchpath& paths,
                            bool (*filter)(const std::string &, void *),
                            void *arg,
                            bool pass_fullpath,
                            bool return_fullpath,
                            bool recurse = false);

/**
 * Find paths in a Searchpath that match a supplied filter(functor)
 * @note results include only files.
 *
 * @param results A vector in which to place the resulting matches.
 * @param paths A Searchpath
 * @param filter A functor to use to filter paths
 * @param arg additonal argument to filter if required
 * @param pass_fullpath pass the full path to the filter or just the basename
 * @param return_fullpath put the full path in results or just the basename
 * @param recurse Recurse into child directories to find files.
 */
LIBPBD_API void
find_files_matching_filter (std::vector<std::string>& results,
                            const Searchpath& paths,
                            bool (*filter)(const std::string &, void *),
                            void *arg,
                            bool pass_fullpath,
                            bool return_fullpath,
                            bool recurse = false);

/**
 * Attempt to copy the contents of the file from_path to a new file
 * at path to_path. If to_path exists it is overwritten.
 *
 * @return true if file was successfully copied
 */
LIBPBD_API bool copy_file(const std::string & from_path, const std::string & to_path);

/**
 * Attempt to copy all regular files from from_path to a new directory.
 * This method does not recurse.
 */
LIBPBD_API void copy_files(const std::string & from_path, const std::string & to_dir);

/**
 * Attempt to copy all regular files from from_path to a new directory.
 */
LIBPBD_API void copy_recurse(const std::string & from_path, const std::string & to_dir);

/**
 * Update the access and modification times of file at path, creating file
 * if it doesn't already exist.
 * @param path file path to touch
 * @return true if file exists or was created and access time updated.
 */
LIBPBD_API bool touch_file (const std::string& path);

/** try hard-link a file
 * @return true if file was successfully linked
 */
LIBPBD_API bool hard_link (const std::string& existing_file, const std::string& new_path);

/**
 * Take a (possibly) relative path and make it absolute
 * @return An absolute path
 */
LIBPBD_API std::string get_absolute_path (const std::string &);

/**
 * The equivalent of realpath on POSIX systems, on Windows hard
 * links/junctions etc are not resolved.
 */
LIBPBD_API std::string canonical_path (const std::string& path);

/**
 * Take a path/filename and return the suffix (characters beyond the last '.'
 * @return A string containing the suffix, which will be empty
 * if there are no '.' characters in the path/filename.
 */
LIBPBD_API std::string get_suffix (const std::string &);

/**
 * Find out if `needle' is a file or directory within the
 * directory `haystack'.
 * @return true if it is.
 */
LIBPBD_API bool path_is_within (const std::string &, std::string);

/**
 * @return true if p1 and p2 both resolve to the same file
 * @param p1 a file path.
 * @param p2 a file path.
 *
 * Uses g_stat to check for identical st_dev and st_ino values.
 */
LIBPBD_API bool equivalent_paths (const std::string &p1, const std::string &p2);

/// @return true if path at p exists and is writable, false otherwise
LIBPBD_API bool exists_and_writable(const std::string & p);

/**
 * Remove all the files in a directory recursively leaving the directory
 * structure in place.
 * @note dir will not be removed
 *
 * @param dir The directory to clear of files.
 * @param size of removed files in bytes.
 * @param removed_files list of files that were removed.
 */
LIBPBD_API int clear_directory (const std::string& dir, size_t* size = 0,
                                std::vector<std::string>* removed_files = 0);

/**
 * Remove all the contents of a directory recursively.
 * including the dir itself (`rm -rf $dir`)
 *
 * @param dir The directory to remove recursively
 */
LIBPBD_API void remove_directory (const std::string& dir);

/**
 * Create a temporary writable directory in which to create
 * temporary files. The directory will be created under the
 * top level "domain" directory.
 *
 * For instance tmp_writable_directory ("pbd", "foo") on POSIX
 * systems may return a path to a new directory something like
 * to /tmp/pbd/foo-1423
 *
 * @param domain The top level directory
 * @param prefix A prefix to use when creating subdirectory name
 *
 * @return new temporary directory
 */
LIBPBD_API std::string tmp_writable_directory (const char* domain, const std::string& prefix);

/** If \p path exists, unlink it. If it doesn't exist, create it.
 *
 * @return zero if required action was successful, non-zero otherwise.
 */
LIBPBD_API int toggle_file_existence (std::string const & path);

} // namespace PBD

#endif
