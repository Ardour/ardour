/*
 * Copyright (C) 2005-2016 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2005 Taybin Rutkin <taybin@taybin.com>
 * Copyright (C) 2006-2012 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2012 David Robillard <d@drobilla.net>
 * Copyright (C) 2015 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2015 Robin Gareus <robin@gareus.org>
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

#include <cstring>
#include <cerrno>
#include <sstream>
#include <algorithm>

#include "pbd/gstdio_compat.h"
#include <glibmm/miscutils.h>

#include "pbd/error.h"

#include "ardour/rc_configuration.h"
#include "ardour/filesystem_paths.h"
#include "ardour/recent_sessions.h"

#include "pbd/i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace {

	const char * const recent_file_name = "recent";
	const char * const recent_templates_file_name = "recent_templates";

} // anonymous

int
ARDOUR::read_recent_sessions (RecentSessions& rs)
{
	std::string path = Glib::build_filename (user_config_directory(), recent_file_name);
	FILE* fin = g_fopen (path.c_str(), "rb");

	if (!fin) {
		if (errno != ENOENT) {
			error << string_compose (_("cannot open recent session file %1 (%2)"), path, strerror (errno)) << endmsg;
			return -1;
		} else {
			return 1;
		}
	}

	// Read the file into a std::string
	std::stringstream recent;
	while (!feof (fin)) {
		char buf[1024];
		size_t charsRead = fread (buf, sizeof(char), 1024, fin);
		if (ferror (fin)) {
			error << string_compose (_("Error reading recent session file %1 (%2)"), path, strerror (errno)) << endmsg;
			fclose(fin);
			return -1;
		}
		if (charsRead == 0) {
			break;
		}
		recent.write (buf, charsRead);
	}

	while (true) {

		pair<string,string> newpair;

		getline(recent, newpair.first);

		if (!recent.good()) {
			break;
		}

		getline(recent, newpair.second);

		if (!recent.good()) {
			break;
		}

		rs.push_back (newpair);
	}

	/* display sorting should be done in the GUI, otherwise the
	 * natural order will be broken
	 */

	fclose (fin);
	return 0;
}

int
ARDOUR::read_recent_templates (std::deque<std::string>& rt)
{
	std::string path = Glib::build_filename (user_config_directory(), recent_templates_file_name);
	FILE* fin = g_fopen (path.c_str(), "rb");

	if (!fin) {
		if (errno != ENOENT) {
			error << string_compose (_("Cannot open recent template file %1 (%2)"), path, strerror (errno)) << endmsg;
			return -1;
		} else {
			return 1;
		}
	}

	// Copy the file contents into a std::stringstream
	std::stringstream recent;
	while (!feof (fin)) {
		char buf[1024];
		size_t charsRead = fread (buf, sizeof(char), 1024, fin);
		if (ferror (fin)) {
			error << string_compose (_("Error reading recent session file %1 (%2)"), path, strerror (errno)) << endmsg;
			fclose(fin);
			return -1;
		}
		if (charsRead == 0) {
			break;
		}
		recent.write (buf, charsRead);
	}

	while (true) {

		std::string session_template_full_name;

		getline(recent, session_template_full_name);

		if (!recent.good()) {
			break;
		}

		rt.push_back (session_template_full_name);
	}

	fclose (fin);
	return 0;
}

int
ARDOUR::write_recent_sessions (RecentSessions& rs)
{
	FILE* fout = g_fopen (Glib::build_filename (user_config_directory(), recent_file_name).c_str(), "wb");

	if (!fout) {
		return -1;
	}

	{
		stringstream recent;

		for (RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
			recent << (*i).first << '\n' << (*i).second << endl;
		}

		string recentString = recent.str();
		size_t writeSize = recentString.length();

		fwrite(recentString.c_str(), sizeof(char), writeSize, fout);

		if (ferror(fout))
		{
			error << string_compose (_("Error writing recent sessions file %1 (%2)"), recent_file_name, strerror (errno)) << endmsg;
			fclose(fout);
			return -1;
		}
	}



	fclose (fout);

	return 0;
}

int
ARDOUR::write_recent_templates (std::deque<std::string>& rt)
{
	FILE* fout = g_fopen (Glib::build_filename (user_config_directory(), recent_templates_file_name).c_str(), "wb");

	if (!fout) {
		return -1;
	}

	stringstream recent;

	for (std::deque<std::string>::const_iterator i = rt.begin(); i != rt.end(); ++i) {
		recent << (*i) << std::endl;
	}

	string recentString = recent.str();
	size_t writeSize = recentString.length();

	fwrite(recentString.c_str(), sizeof(char), writeSize, fout);

	if (ferror(fout))
	{
		error << string_compose (_("Error writing saved template file %1 (%2)"), recent_templates_file_name, strerror (errno)) << endmsg;
		fclose(fout);
		return -1;
	}

	fclose (fout);

	return 0;
}

int
ARDOUR::store_recent_sessions (string name, string path)
{
	RecentSessions rs;

	if (ARDOUR::read_recent_sessions (rs) < 0) {
		return -1;
	}

	pair<string,string> newpair;

	newpair.first = name;
	newpair.second = path;

	rs.erase(remove(rs.begin(), rs.end(), newpair), rs.end());

	rs.push_front (newpair);

	uint32_t max_recent_sessions = Config->get_max_recent_sessions();

	if (rs.size() > max_recent_sessions) {
		rs.erase(rs.begin()+max_recent_sessions, rs.end());
	}

	return ARDOUR::write_recent_sessions (rs);
}

int
ARDOUR::store_recent_templates (const std::string& session_template_full_name)
{
	std::deque<std::string> rt;

	if (ARDOUR::read_recent_templates (rt) < 0) {
		return -1;
	}

	rt.erase(remove (rt.begin(), rt.end(), session_template_full_name), rt.end());

	rt.push_front (session_template_full_name);

	uint32_t max_recent_templates = Config->get_max_recent_templates ();

	if (rt.size() > max_recent_templates) {
		rt.erase( rt.begin() + max_recent_templates, rt.end ());
	}

	return ARDOUR::write_recent_templates (rt);
}

int
ARDOUR::remove_recent_sessions (const string& path)
{
	RecentSessions rs;
	bool write = false;

	if (ARDOUR::read_recent_sessions (rs) < 0) {
		return -1;
	}

	for (RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		if (i->second == path) {
			rs.erase (i);
			write = true;
			break;
		}
	}

	if (write) {
		return ARDOUR::write_recent_sessions (rs);
	} else {
		return 1;
	}
}
