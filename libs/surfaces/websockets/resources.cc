/*
 * Copyright (C) 2020 Luciano Iam <oss@lucianoiam.com>
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
 * You should have received a copy of the/GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
 */

#include <iostream>
#include <sstream>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "ardour/filesystem_paths.h"
#include "pbd/file_utils.h"

#include "resources.h"
#include "json.h"

using namespace ArdourSurface;

static const char* const data_dir_env_var = "ARDOUR_WEBSURFACES_PATH";
static const char* const data_dir_name = "web_surfaces";
static const char* const builtin_dir_name = "builtin";
static const char* const user_dir_name = "user";

static bool
dir_filter (const std::string &str, void* /*arg*/)
{
	return Glib::file_test (str, Glib::FILE_TEST_IS_DIR);
}

ServerResources::ServerResources ()
    : _index_dir ("")
    , _builtin_dir ("")
    , _user_dir ("")
{
}

const std::string&
ServerResources::index_dir ()
{
	if (_index_dir.empty ()) {
		_index_dir = server_data_dir ();
	}

	return _index_dir;
}

const std::string&
ServerResources::builtin_dir ()
{
	if (_builtin_dir.empty ()) {
		_builtin_dir = Glib::build_filename (server_data_dir (), builtin_dir_name);
	}

	return _builtin_dir;
}

const std::string&
ServerResources::user_dir ()
{
	if (_user_dir.empty ()) {
		_user_dir = Glib::build_filename (ARDOUR::user_config_directory (), data_dir_name);
	}

	return _user_dir;
}

std::string
ServerResources::scan ()
{
	std::stringstream ss;

	std::string builtin_dir_str   = PBD::canonical_path (builtin_dir ());
	SurfaceManifestVector builtin = read_manifests (builtin_dir_str);

	ss << "[{"
		<< "\"filesystemPath\":\"" << WebSocketsJSON::escape (builtin_dir_str) << "\""
		<< ",\"path\":\"" << WebSocketsJSON::escape (builtin_dir_name) << "\""
		<< ",\"surfaces\":"
		<< "[";

	for (SurfaceManifestVector::iterator it = builtin.begin (); it != builtin.end (); ) {
		ss << it->to_json ();
		if (++it != builtin.end()) {
			ss << ",";
		}
	}

	std::string user_dir_str   = PBD::canonical_path (user_dir ());
	SurfaceManifestVector user = read_manifests (user_dir_str);

	ss << "]},{" 
		<< "\"filesystemPath\":\"" << WebSocketsJSON::escape (user_dir_str) << "\""
		<< ",\"path\":\"" << WebSocketsJSON::escape (user_dir_name) << "\"" 
		<< ",\"surfaces\":" 
		<< "[";

	for (SurfaceManifestVector::iterator it = user.begin (); it != user.end (); ) {
		ss << it->to_json ();
		if (++it != user.end()) {
			ss << ",";
		}
	}

	ss << "]}]";

	return ss.str ();
}

std::string
ServerResources::server_data_dir ()
{
	std::string data_dir;

	bool defined = false;
	std::string env_dir (Glib::getenv (data_dir_env_var, defined));

	if (defined && !env_dir.empty ()) {
		/* useful for development */
		data_dir = env_dir;
	} else {
		/* use reverse iterator, since ardour_data_search_path() prefixes the user-data dir */
		PBD::Searchpath s (ARDOUR::ardour_data_search_path ());
		for (PBD::Searchpath::reverse_iterator i = s.rbegin (); i != s.rend(); ++i) {
			data_dir = Glib::build_filename (*i, data_dir_name);
			if (Glib::file_test(data_dir, Glib::FILE_TEST_EXISTS | Glib::FILE_TEST_IS_DIR)) {
				break;
			}
		}
	}

	return data_dir;
}

SurfaceManifestVector
ServerResources::read_manifests (std::string dir)
{
	SurfaceManifestVector    result;
	std::vector<std::string> subdirs;
	PBD::Searchpath          spath (dir);
	
	find_paths_matching_filter (subdirs, spath, dir_filter,
		0 /*arg*/, true /*pass_fullpath*/, true /*return_fullpath*/, false /*recurse*/);

	for (std::vector<std::string>::const_iterator it = subdirs.begin (); it != subdirs.end (); ++it) {
		if (!SurfaceManifest::exists_at_path (*it)) {
			continue;
		}

		SurfaceManifest manifest (*it);

		if (manifest.valid ()) {
			result.push_back (manifest);
		}
	}

	return result;
}
