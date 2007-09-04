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

#include <glibmm/fileutils.h>
#include <glibmm/pattern.h>

#include <pbd/compose.h>
#include <pbd/file_utils.h>

#include <pbd/error.h>

namespace PBD {

void
get_files_in_directory (const sys::path& directory_path, vector<string>& result)
{
	if (!is_directory(directory_path)) return;

	try
	{
		Glib::Dir dir(directory_path.to_string());
		std::copy(dir.begin(), dir.end(), std::back_inserter(result));
	}
	catch (Glib::FileError& err)
	{
		warning << err.what() << endmsg;
	}
}

void
find_matching_files_in_directory (const sys::path& directory,
                                  const Glib::PatternSpec& pattern,
                                  vector<sys::path>& result)
{
	vector<string> tmp_files;

	get_files_in_directory (directory, tmp_files);

	for (vector<string>::iterator file_iter = tmp_files.begin();
			file_iter != tmp_files.end();
			++file_iter)
	{
		if (!pattern.match(*file_iter)) continue;

		sys::path full_path(directory);
		full_path /= *file_iter;

		result.push_back(full_path);
	}
}

void
find_matching_files_in_directories (const vector<sys::path>& paths,
                                    const Glib::PatternSpec& pattern,
                                    vector<sys::path>& result)
{
	for (vector<sys::path>::const_iterator path_iter = paths.begin();
			path_iter != paths.end();
			++path_iter)
	{
		find_matching_files_in_directory (*path_iter, pattern, result);
	}		
}

void
find_matching_files_in_search_path (const SearchPath& search_path,
                                    const Glib::PatternSpec& pattern,
                                    vector<sys::path>& result)
{
	vector<sys::path> dirs;
	std::copy(search_path.begin(), search_path.end(), std::back_inserter(dirs));
	find_matching_files_in_directories (dirs, pattern, result);    
}

bool
find_file_in_search_path(const SearchPath& search_path,
                         const string& filename,
                         sys::path& result)
{
	vector<sys::path> tmp;
	Glib::PatternSpec tmp_pattern(filename);

	find_matching_files_in_search_path (search_path, tmp_pattern, tmp);

	if (tmp.size() == 0)
	{
		info << string_compose
			(
			 "Found no file named %1 in search path %2",
			 filename,
			 search_path.to_string ()
			)
			<< endmsg;

		return false;
	}

#if 0
	if (tmp.size() != 1)
	{
		info << string_compose
			(
			 "Found more than one file matching %1 in search path %2",
			 filename,
			 search_path.to_string ()
			)
			<< endmsg;
	}
#endif

	result = tmp.front();

	return true;
}

} // namespace PBD
