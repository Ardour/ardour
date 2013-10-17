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

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

class LIBARDOUR_API SessionDirectory
{
public:

	/**
	 * @param session_path An absolute path to a session directory.
	 */
	SessionDirectory (const std::string& session_path);

	/**
	 * Change the root path of this SessionDirectory object
	 */
	SessionDirectory& operator= (const std::string& path);

	/**
	 * @return the absolute path to the root directory of the session
	 */
	const std::string root_path() const { return m_root_path; }

	/**
	 * @return the absolute path to the directory in which
	 * the session stores audio files.
	 *
	 * If the session is an older session with an existing
	 * "sounds" directory then it will return a path to that
	 * directory otherwise it will return the new location
	 * of root_path()/interchange/session_name/audiofiles
	 */
	const std::string sound_path () const;

	/**
	 * @return the absolute path to the directory in which
	 * the session stores audio files for Ardour 2.X.
	 *
	 * If the session is an older session with an existing
	 * "sounds" directory then it will return a path to that
	 * directory otherwise it will return the new location
	 * of root_path()/interchange/session_name/audiofiles
	 */
	const std::string sound_path_2X () const;

	/**
	 * @return the absolute path to the directory in which
	 * the session stores MIDI files, ie
	 * root_path()/interchange/session_name/midifiles
	 */
	const std::string midi_path () const;

	/**
	 * @return the absolute path to the directory in which
	 * the session stores MIDNAM patch files, ie
	 * root_path()/interchange/session_name/patchfiles
	 */
	const std::string midi_patch_path () const;

	/**
	 * @return The absolute path to the directory in which all
	 * peak files are stored for a session.
	 */
	const std::string peak_path () const;

	/**
	 * @return The absolute path to the directory in which all
	 * video files are stored for a session.
	 */
	const std::string video_path () const;

	/**
	 * @return The absolute path to the directory that source
	 * files are moved to when they are no longer part of the
	 * session.
	 */
	const std::string dead_path () const;

	/**
	 * @return The absolute path to the directory that audio
	 * files are created in by default when exporting.
	 */
	const std::string export_path () const;

	/**
	 * @return true if session directory and all the required
	 * subdirectories exist.
	 */
	bool is_valid () const;

	/**
	 * Create the session directory and all the subdirectories.
	 *
	 * @return true If a new session directory and subdirectories were
	 * created, otherwise false.
	 *
	 * @post is_valid ()
	 */
	bool create ();

	/**
	 * @return The path to the directory under which source directories
	 * are created for different source types.
	 * i.e root_path()/interchange/session_name
	 */
	const std::string sources_root() const;

	/**
	 * @return The path to the directory under which source directories
	 * are created for different source types in Ardour 2.X
	 * i.e root_path()/interchange/session_name
	 */
	const std::string sources_root_2X() const;

private:

	/**
	 * @return The path to the old style sound directory.
	 * It isn't created by create().
	 */
	const std::string old_sound_path () const;

	/**
	 * @return a vector containing the fullpath of all subdirectories.
	 */
	const std::vector<std::string> sub_directories () const;

	/// The path to the root of the session directory.
	std::string m_root_path;
};

} // namespace ARDOUR

#endif
