/*
 * Copyright (C) 2013-2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2013-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013-2016 Tim Mayberry <mojofunk@gmail.com>
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

#include <vector>
#include <iostream>
#include <climits>
#include <cerrno>
#include <cstdlib>
#include <regex>

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "pbd/compose.h"
#include "pbd/debug.h"
#include "pbd/pathexpand.h"
#include "pbd/strsplit.h"
#include "pbd/tokenizer.h"

using std::string;
using std::vector;

string
PBD::path_expand (string path)
{
	if (path.empty()) {
		return path;
	}

	/* tilde expansion */

	if (path[0] == '~') {
		if (path.length() == 1) {
			return Glib::get_home_dir();
		}

		if (path[1] == '/') {
			path.replace (0, 1, Glib::get_home_dir());
		} else {
			/* can't handle ~roger, so just leave it */
		}
	}

	/* now do $VAR or ${VAR} substitution, since wordexp isn't reliable */

	static const std::regex var_regex (R"(\$([A-Za-z_][A-Za-z0-9_]*|\{[A-Za-z_][A-Za-z0-9_]*\}))");

	std::smatch m;

	while (std::regex_search (path, m, var_regex)) {
		std::string var = m[1].str(); // "$FOO" or "${FOO}"

		if (var[0] == '{') {
			var = var.substr(1, var.size() - 2);
		}

		const char* val = g_getenv (var.c_str());

		path.replace (m.position(0), m.length(0), val ? val : "");
	}

	/* canonicalize */
	return canonical_path (path);
}

string
PBD::search_path_expand (string path)
{
        if (path.empty()) {
                return path;
        }

	vector<string> s;
	vector<string> n;

	split (path, s, G_SEARCHPATH_SEPARATOR);

	for (vector<string>::iterator i = s.begin(); i != s.end(); ++i) {
		string exp = path_expand (*i);
		if (!exp.empty()) {
			n.push_back (exp);
		}
	}

	string r;

	for (vector<string>::iterator i = n.begin(); i != n.end(); ++i) {
		if (!r.empty()) {
			r += G_SEARCHPATH_SEPARATOR;
		}
		r += *i;
	}

	return r;
}

std::vector <std::string>
PBD::parse_path(std::string path, bool check_if_exists)
{
	vector <std::string> pathlist;
	vector <std::string> tmp;
	PBD::tokenize (path, string(G_SEARCHPATH_SEPARATOR_S), std::back_inserter (tmp));

	for(vector<std::string>::const_iterator i = tmp.begin(); i != tmp.end(); ++i) {
		if ((*i).empty()) continue;
		std::string dir;
#ifndef PLATFORM_WINDOWS
		if ((*i).substr(0,1) == "~") {
			dir = Glib::build_filename(Glib::get_home_dir(), (*i).substr(1));
		}
		else
#endif
		{
			dir = *i;
		}
		if (!check_if_exists || Glib::file_test (dir, Glib::FILE_TEST_IS_DIR)) {
			pathlist.push_back(dir);
		}
	}
  return pathlist;
}
