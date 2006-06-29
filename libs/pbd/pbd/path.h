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

#ifndef PBD_PATH
#define PBD_PATH

#include <string>
#include <vector>

namespace PBD {

using std::string;
using std::vector;

/**
	The Path class is a helper class for getting a vector of absolute 
	paths contained in a path string where a path string contains
	absolute directory paths separated by a colon(:) or a semi-colon(;)
	on windows.
  */
class Path {
public:

	/**
	  Create an empty Path.
	  */
	Path ();

	/**
	  Initialize Path from a string, each absolute path contained
	  in the "path" will be accessed to ensure it exists and is 
	  readable.
	  \param path A path string.
	  */
	Path (const string& path);

	/**
	  Initialize Path from a vector of path strings, each absolute 
	  path contained in paths will be accessed to ensure it 
	  exists and is readable.
	  \param path A path string.
	  */
	Path (const vector<string>& paths);
	
	Path(const Path& path);

	/**
	  Indicate whether there are any directories in m_dirs, if Path is
	  initialized with an empty string as the result of for instance
	  calling Glib::getenv where the environment variable doesn't 
	  exist or if none of the directories in the path string are 
	  accessible then false is returned.
	  
	  \return true if there are any paths in m_paths.
	  */
	//operator bool () const { return !m_dirs.empty(); }
		
	/**
	  \return vector containing the absolute paths to the directories
	  contained
	  */
	operator const vector<string>& () const { return m_dirs; }

	/**
	  \return vector containing the absolute paths to the directories
	  contained
	  */
	const vector<string>& dirs () const { return m_dirs; }

	const string path_string() const;
	
	const Path& operator= (const Path& path);
	
	const Path& operator+= (const string& directory_path);
	
	Path& add_subdirectory_to_path (const string& subdirectory);

protected:
	
	friend const Path operator+ (const Path&, const Path&);

	bool readable_directory (const string& directory_path);

	void add_readable_directory (const string& directory_path);

	void add_readable_directories (const vector<string>& paths);
	
	vector<string> m_dirs;

};

bool find_file_in_path (const Path& path, const string& filename, string& resulting_path_to_file);

} // namespace PBD

#endif // PBD_PATH


