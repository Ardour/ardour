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

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/file_utils.h"

#include "ardour/directory_names.h"
#include "ardour/session_directory.h"
#include "ardour/utils.h"

#include "i18n.h"

namespace ARDOUR {

using namespace std;
using namespace PBD::sys;

SessionDirectory::SessionDirectory (const std::string& session_path)
	: m_root_path(session_path)
{

}

SessionDirectory& 
SessionDirectory::operator= (const std::string& newpath)
{
	m_root_path = newpath;
	return *this;
}

bool
SessionDirectory::create ()
{
	bool is_new = false;

	vector<std::string> sub_dirs = sub_directories ();
	for (vector<std::string>::const_iterator i = sub_dirs.begin(); i != sub_dirs.end(); ++i)
	{
		if (Glib::file_test (*i, Glib::FILE_TEST_EXISTS)) {
			is_new = false;
		}

		if (g_mkdir_with_parents (i->c_str(), 0755) != 0) {
			PBD::error << string_compose(_("Cannot create Session directory at path %1 Error: %2"), *i, g_strerror(errno)) << endmsg;

		}
	}

	return is_new;
}

bool
SessionDirectory::is_valid () const
{
	if (!Glib::file_test (m_root_path, Glib::FILE_TEST_IS_DIR)) return false;

	vector<std::string> sub_dirs = sub_directories ();

	for (vector<std::string>::iterator i = sub_dirs.begin(); i != sub_dirs.end(); ++i) {
		if (!Glib::file_test (*i, Glib::FILE_TEST_IS_DIR)) {
			PBD::warning << string_compose(_("Session subdirectory does not exist at path %1"), *i) << endmsg;
			return false;
		}
	}
	return true;
}

const std::string
SessionDirectory::old_sound_path () const
{
	return Glib::build_filename (m_root_path, old_sound_dir_name);
}

const std::string
SessionDirectory::sources_root () const
{
	std::string p = m_root_path;
	std::string filename = Glib::path_get_basename(p);

	if (filename == ".") {
		p = PBD::get_absolute_path (m_root_path);
	}

	const string legalized_root (legalize_for_path (Glib::path_get_basename(p)));

	std::string sources_root_path = Glib::build_filename (m_root_path, interchange_dir_name);
	return Glib::build_filename (sources_root_path, legalized_root);
}

const std::string
SessionDirectory::sound_path () const
{
	if (Glib::file_test (old_sound_path (), Glib::FILE_TEST_IS_DIR)) return old_sound_path();

	// the new style sound directory
	return Glib::build_filename (sources_root(), sound_dir_name);
}

const std::string
SessionDirectory::midi_path () const
{
	return Glib::build_filename (sources_root(), midi_dir_name);
}

const std::string
SessionDirectory::midi_patch_path () const
{
	return Glib::build_filename (sources_root(), midi_patch_dir_name);
}

const std::string
SessionDirectory::peak_path () const
{
	return Glib::build_filename (m_root_path, peak_dir_name);
}

const std::string
SessionDirectory::dead_path () const
{
	return Glib::build_filename (m_root_path, dead_dir_name);
}

const std::string
SessionDirectory::export_path () const
{
	return Glib::build_filename (m_root_path, export_dir_name);
}

const vector<std::string>
SessionDirectory::sub_directories () const
{
	vector<std::string> tmp_paths;

	tmp_paths.push_back (sound_path ());
	tmp_paths.push_back (midi_path ());
	tmp_paths.push_back (peak_path ());
	tmp_paths.push_back (dead_path ());
	tmp_paths.push_back (export_path ());

	return tmp_paths;
}

} // namespace ARDOUR
