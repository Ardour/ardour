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

#include <cerrno>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include <pbd/error.h>
#include <pbd/compose.h>
#include <pbd/filesystem.h>

#include <ardour/directory_names.h>
#include <ardour/session_directory.h>

#include "i18n.h"

namespace ARDOUR {

using namespace PBD::sys;

SessionDirectory::SessionDirectory (const string& session_path)
	: m_root_path(session_path)
{

}

bool
SessionDirectory::create ()
{
	if (is_directory (m_root_path))
	{
		PBD::error << string_compose(_("Cannot create Session directory at path %1 as it already exists"), m_root_path.to_string()) << endmsg;
		return false;
	}

	vector<path> sub_dirs = sub_directories ();
	for (vector<path>::const_iterator i = sub_dirs.begin(); i != sub_dirs.end(); ++i)
	{
		if (!create_directories(*i))
		{
			PBD::error << string_compose(_("Cannot create Session directory at path %1 Error: %2"), (*i).to_string(), g_strerror (errno)) << endmsg;
			return false;
		}
	}

	return true;
}

bool
SessionDirectory::is_valid () const
{
	if (!is_directory (m_root_path)) return false;

	vector<path> sub_dirs = sub_directories ();

	for (vector<path>::iterator i = sub_dirs.begin(); i != sub_dirs.end(); ++i) {
		if (!is_directory (*i)) {
			PBD::warning << string_compose(_("Session subdirectory does not exist at path %1"), (*i).to_string()) << endmsg;
			return false;
		}
	}
	return true;
}

const path
SessionDirectory::old_sound_path () const
{
	return path(m_root_path) /= old_sound_dir_name;
}

const path
SessionDirectory::sound_path () const
{
	if(is_directory (old_sound_path ())) return old_sound_path();

	// the new style sound directory
	path l_sound_path(m_root_path);

	l_sound_path /= interchange_dir_name;
	l_sound_path /= basename(m_root_path);
	l_sound_path /= sound_dir_name;

	return l_sound_path;
}

const path
SessionDirectory::peak_path () const
{
	return path(m_root_path) /= peak_dir_name;
}

const path
SessionDirectory::dead_sound_path () const
{
	return path(m_root_path) /= dead_sound_dir_name;
}

const path
SessionDirectory::export_path () const
{
	return path(m_root_path) /= export_dir_name;
}

const vector<path>
SessionDirectory::sub_directories () const
{
	vector<path> tmp_paths; 

	tmp_paths.push_back ( sound_path () );
	tmp_paths.push_back ( peak_path () );
	tmp_paths.push_back ( dead_sound_path () );
	tmp_paths.push_back ( export_path () );

	return tmp_paths;
}

} // namespace ARDOUR
