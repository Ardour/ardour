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

#ifndef __ardour_session_directory_h__
#define __ardour_session_directory_h__

#include <string>
#include <vector>

#include <pbd/filesystem.h>

namespace ARDOUR {

using std::string;
using std::vector;
using PBD::sys::path;

class SessionDirectory
{
public:
	/**
	 * @param session_path An absolute path to a session directory.
	 */
	SessionDirectory (const string& session_path);

	/**
	 * @return the absolute path to the root directory of the session
	 */
	const path root_path() const { return m_root_path; }

	/**
	 * @return the absolute path to the directory in which 
	 * the session stores audio files.
	 *
	 * If the session is an older session with an existing
	 * "sounds" directory then it will return a path to that
	 * directory otherwise it will return the new location
	 * of root_path()/interchange/session_name/audiofiles
	 */
	const path sound_path () const;

	/**
	 * @return The absolute path to the directory in which all
	 * peak files are stored for a session.
	 */
	const path peak_path () const;

	/**
	 * @return The absolute path to the directory that audio
	 * files are moved to when they are no longer part of the
	 * session.
	 */
	const path dead_sound_path () const;

	/**
	 * @return The absolute path to the directory that audio
	 * files are created in by default when exporting.
	 */
	const path export_path () const;

	/**
	 * @return true if session directory and all the required 
	 * subdirectories exist.
	 */
	bool is_valid () const;

	/**
	 * @return true If a new session directory and all the 
	 * subdirectories were created, otherwise false.
	 */
	bool create ();

protected:

	/**
	 * @return The path to the old style sound directory.
	 * It isn't created by create().
	 */
	const path old_sound_path () const;

	/**
	 * @return a vector containing the fullpath of all subdirectories.
	 */
	const vector<PBD::sys::path> sub_directories () const;

	/// The path to the root of the session directory.
	const path m_root_path;
};

} // namespace ARDOUR

#endif
