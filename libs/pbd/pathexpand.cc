/*
    Copyright (C) 2013 Paul Davis

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

#include <vector>
#include <iostream>
#include <climits>
#include <cerrno>
#include <cstdlib>

#include <regex.h>

#include <glibmm/miscutils.h>

#include "pbd/pathexpand.h"
#include "pbd/strsplit.h"

using std::string;
using std::vector;

#ifdef COMPILER_MINGW

#include <WTypes.h>
#include <glibmm.h>

/****************************************************************
 * Emulate POSIX realpath() using Win32 _fullpath() since realpath()
 * is not available.
 *
 * Returns:
 *    On Success: A pointer to the resolved (absolute) path
 *    On Failure: 0 (NULL)
 */

static char* 
realpath (const char *original_path, char resolved_path[_MAX_PATH+1])
{
	char *rpath = 0;
	bool bIsSymLink = false; // We'll probably need to test the incoming path
	                         // to find out if it points to a Windows shortcut
	                         // (or a hard link) and set this appropriately.

	if (bIsSymLink) {
		// At the moment I'm not sure if Windows '_fullpath()' is directly
		// equivalent to POSIX 'realpath()' - in as much as the latter will
		// resolve the supplied path if it happens to point to a symbolic
		// link ('_fullpath()' probably DOESN'T do this but I'm not really
		// sure if Ardour needs such functionality anyway). Therefore we'll
		// possibly need to add that functionality here at a later date.
	} else {
		char temp[(MAX_PATH+1)*6]; // Allow for maximum length of a path in wchar characters
		
		// POSIX 'realpath()' requires that the buffer size is at
		// least PATH_MAX+1, so assume that the user knew this !!

		rpath = _fullpath (temp, Glib::locale_from_utf8 (original_path).c_str(), _MAX_PATH);

		if (0 != rpath) {
			snprintf (resolved_path, _MAX_PATH+1, Glib::locale_to_utf8 (temp).c_str());
		}
			
	}
	
	return (rpath);
}

#endif  // COMPILER_MINGW

string
PBD::canonical_path (const std::string& path)
{
	char buf[PATH_MAX+1];

	if (!realpath (path.c_str(), buf) && (errno != ENOENT)) {
		return path;
	}

	return string (buf);
}

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

	/* now do $VAR substitution, since wordexp isn't reliable */

	regex_t compiled_pattern;
	const int nmatches = 100;
	regmatch_t matches[nmatches];
	
	if (regcomp (&compiled_pattern, "\\$([a-zA-Z_][a-zA-Z0-9_]*|\\{[a-zA-Z_][a-zA-Z0-9_]*\\})", REG_EXTENDED)) {
		std::cerr << "bad regcomp\n";
                return path;
        }

	while (true) { 

		if (regexec (&compiled_pattern, path.c_str(), nmatches, matches, 0)) {
			break;
		}
		
		/* matches[0] gives the entire match */
		
		string match = path.substr (matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so);
		
		/* try to get match from the environment */

                if (match[1] == '{') {
                        /* ${FOO} form */
                        match = match.substr (2, match.length() - 3);
                }

		char* matched_value = getenv (match.c_str());

		if (matched_value) {
			path.replace (matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so, matched_value);
		} else {
			path.replace (matches[0].rm_so, matches[0].rm_eo - matches[0].rm_so, string());
                }

		/* go back and do it again with whatever remains after the
		 * substitution 
		 */
	}

	regfree (&compiled_pattern);

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

	split (path, s, ':');

	for (vector<string>::iterator i = s.begin(); i != s.end(); ++i) {
		string exp = path_expand (*i);
		if (!exp.empty()) {
			n.push_back (exp);
		}
	}

	string r;

	for (vector<string>::iterator i = n.begin(); i != n.end(); ++i) {
		if (!r.empty()) {
			r += ':';
		}
		r += *i;
	}

	return r;
}
