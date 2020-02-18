/*
 * Copyright (C) 2007-2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2007-2012 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2008 Hans Baier <hansfbaier@googlemail.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2013-2019 Robin Gareus <robin@gareus.org>
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

#include <cerrno>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/compose.h"
#include "pbd/file_utils.h"
#include "pbd/openuri.h"

#include "ardour/directory_names.h"
#include "ardour/session_directory.h"
#include "ardour/utils.h"

#include "pbd/i18n.h"

namespace ARDOUR {

using namespace std;
using namespace PBD::sys;


/* keep a static cache because SessionDirectory is used in various places. */
std::map<std::string,std::string> SessionDirectory::root_cache;

SessionDirectory::SessionDirectory (const std::string& session_path)
	: m_root_path(session_path)
{

}

SessionDirectory&
SessionDirectory::operator= (const std::string& newpath)
{
	m_root_path = newpath;
	root_cache.clear ();
	return *this;
}

bool
SessionDirectory::create ()
{
	vector<std::string> sub_dirs = sub_directories ();
	for (vector<std::string>::const_iterator i = sub_dirs.begin(); i != sub_dirs.end(); ++i)
	{
		if (g_mkdir_with_parents (i->c_str(), 0755) != 0) {
			PBD::error << string_compose(_("Cannot create Session directory at path %1 Error: %2"), *i, g_strerror(errno)) << endmsg;
			return false;
		}
	}

	return true;
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
	if (root_cache.find (m_root_path) != root_cache.end()) {
		return root_cache[m_root_path];
	}

	root_cache.clear ();

	std::string p = m_root_path;

	// TODO ideally we'd use the session's name() here, and not the containing folder's name.
	std::string filename = Glib::path_get_basename(p);

	if (filename == ".") {
		p = PBD::get_absolute_path (m_root_path);
	}

	const string legalized_root (legalize_for_path (Glib::path_get_basename(p)));

	std::string sources_root_path = Glib::build_filename (m_root_path, interchange_dir_name);

	/* check the interchange folder:
	 *
	 * 1) if a single subdir exists, use it, regardless of the name
	 * 2) if more than one dir is in interchange: abort, blame the user
	 * 3) if interchange does not exist or no subdir is present,
	 *    use the session-name to create one.
	 *
	 *    We use the name of the containing folder, not the actual
	 *    session name. The latter would require some API changes and
	 *    careful libardour updates:
	 *
	 *    The session object is created with the "snapshot-name", only
	 *    when loading the .ardour session file, the actual name is set.
	 *
	 *    SessionDirectory is created with the session itself
	 *    and picks up the wrong inital name.
	 *
	 *    SessionDirectory is also used directly by the AudioRegionImporter,
	 *    and the peak-file background thread (session.cc).
	 *
	 *    There is no actual benefit to use the session-name instead of
	 *    the folder-name. Under normal circumstances they are always
	 *    identical.  But it would be consistent to prefer the name.
	 */
	try {
		Glib::Dir dir(sources_root_path);

		std::list<std::string> entries;

		for (Glib::DirIterator di = dir.begin(); di != dir.end(); di++) {
			// ignore hidden files (eg. OS X ".DS_Store")
			if ((*di).at(0) == '.') {
				continue;
			}
			// and skip regular files (eg. Win Thumbs.db)
			string fullpath = Glib::build_filename (sources_root_path, *di);
			if (!Glib::file_test (fullpath, Glib::FILE_TEST_IS_DIR)) {
				continue;
			}
			entries.push_back(*di);
		}

		if (entries.size() == 1) {
			if (entries.front() != legalized_root) {
				PBD::info << _("session-dir and session-name mismatch. Please use 'Menu > Session > Rename' in the future to rename sessions.") << endmsg;
			}
			root_cache[m_root_path] = Glib::build_filename (sources_root_path, entries.front());
		}
		else if (entries.size() > 1) {
			PBD::open_folder (sources_root_path);
			PBD::fatal << string_compose (_("The session's interchange dir is tainted.\nThere is more than one folder in '%1'.\nPlease remove extra subdirs to reduce possible filename ambiguties."), sources_root_path) << endmsg;
			assert (0); // not reached
		}
	} catch (Glib::FileError const&) {
		;
	}

	if (root_cache.find (m_root_path) == root_cache.end()) {
		root_cache[m_root_path] = Glib::build_filename (sources_root_path, legalized_root);
	}

	return root_cache[m_root_path];
}

const std::string
SessionDirectory::sources_root_2X () const
{
	std::string p = m_root_path;
	std::string filename = Glib::path_get_basename(p);

	if (filename == ".") {
		p = PBD::get_absolute_path (m_root_path);
	}

	const string legalized_root (legalize_for_path_2X (Glib::path_get_basename(p)));

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
SessionDirectory::sound_path_2X () const
{
	return Glib::build_filename (sources_root_2X(), sound_dir_name);
}

const std::string
SessionDirectory::midi_path () const
{
	return Glib::build_filename (sources_root(), midi_dir_name);
}

const std::string
SessionDirectory::video_path () const
{
	return Glib::build_filename (sources_root(), video_dir_name);
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

const std::string
SessionDirectory::backup_path () const
{
	return Glib::build_filename (m_root_path, backup_dir_name);
}

const vector<std::string>
SessionDirectory::sub_directories () const
{
	vector<std::string> tmp_paths;

	tmp_paths.push_back (sound_path ());
	tmp_paths.push_back (midi_path ());
	tmp_paths.push_back (video_path ());
	tmp_paths.push_back (peak_path ());
	tmp_paths.push_back (dead_path ());
	tmp_paths.push_back (export_path ());
	tmp_paths.push_back (backup_path ());

	return tmp_paths;
}

} // namespace ARDOUR
