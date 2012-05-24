/*
    Copyright (C) 2004 Paul Davis

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

#include <cstring>
#include <cerrno>
#include <fstream>
#include <algorithm>

#include "pbd/error.h"

#include "ardour/rc_configuration.h"
#include "ardour/filesystem_paths.h"
#include "ardour/recent_sessions.h"

#include "i18n.h"

using namespace std;
using namespace ARDOUR;
using namespace PBD;

namespace {

	const char * const recent_file_name = "recent";

} // anonymous

int
ARDOUR::read_recent_sessions (RecentSessions& rs)
{
	sys::path recent_file_path(user_config_directory());

	recent_file_path /= recent_file_name;

	const string path = recent_file_path.to_string();

	ifstream recent (path.c_str());

	if (!recent) {
		if (errno != ENOENT) {
			error << string_compose (_("cannot open recent session file %1 (%2)"), path, strerror (errno)) << endmsg;
			return -1;
		} else {
			return 1;
		}
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

	return 0;
}

int
ARDOUR::write_recent_sessions (RecentSessions& rs)
{
	sys::path recent_file_path(user_config_directory());

	recent_file_path /= recent_file_name;

	const string path = recent_file_path.to_string();

	ofstream recent (path.c_str());

	if (!recent) {
		return -1;
	}

	for (RecentSessions::iterator i = rs.begin(); i != rs.end(); ++i) {
		recent << (*i).first << '\n' << (*i).second << endl;
	}

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

