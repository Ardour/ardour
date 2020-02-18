/*
 * Copyright (C) 2010-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2013 John Emmas <john@creativepost.co.uk>
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

#include <glib.h>

#include <cstdlib>

#include "pbd/epa.h"
#include "pbd/strsplit.h"

#ifdef COMPILER_MSVC
#define environ        _environ
_CRTIMP extern char ** _environ;
#else
extern char** environ;
#endif

using namespace PBD;
using namespace std;

EnvironmentalProtectionAgency* EnvironmentalProtectionAgency::_global_epa = 0;

EnvironmentalProtectionAgency::EnvironmentalProtectionAgency (bool arm, const std::string& envname)
        : _armed (arm)
        , _envname (envname)
{
        if (_armed) {
                save ();
        }
}

EnvironmentalProtectionAgency::~EnvironmentalProtectionAgency()
{
        if (_armed) {
                restore ();
        }
}

void
EnvironmentalProtectionAgency::arm ()
{
        _armed = true;
}

void
EnvironmentalProtectionAgency::save ()
{
        e.clear ();

        if (!_envname.empty()) {

                /* fetch environment from named environment variable, rather than "environ"
                 */

                const char* estr = g_getenv (_envname.c_str());

                if (!estr) {
                        return;
                }

                /* parse line by line, and save into "e"
                 */

                vector<string> lines;
                split (estr, lines, '\n');

                for (vector<string>::iterator i = lines.begin(); i != lines.end(); ++i) {

                        string estring = *i;
                        string::size_type equal = estring.find_first_of ('=');

                        if (equal == string::npos) {
                                /* say what? an environ value without = ? */
                                continue;
                        }

                        string before = estring.substr (0, equal);
                        string after = estring.substr (equal+1);

                        e.insert (pair<string,string>(before,after));
                }

        } else {

                /* fetch environment from "environ"
                 */

                for (size_t i = 0; environ[i]; ++i) {

                        string estring = environ[i];
                        string::size_type equal = estring.find_first_of ('=');

                        if (equal == string::npos) {
                                /* say what? an environ value without = ? */
                                continue;
                        }

                        string before = estring.substr (0, equal);
                        string after = estring.substr (equal+1);

                        e.insert (pair<string,string>(before,after));
                }
        }
}
void
EnvironmentalProtectionAgency::restore () const
{
		clear ();

        for (map<string,string>::const_iterator i = e.begin(); i != e.end(); ++i) {
                g_setenv (i->first.c_str(), i->second.c_str(), 1);
        }
}

void
EnvironmentalProtectionAgency::clear () const
{
	/* Copy the environment before using (g_)unsetenv() because on some
	   platforms (maybe all?) this directly modifies the environ array,
	   cause complications for iterating through it.
	*/

	vector<string> ecopy;

        for (size_t i = 0; environ[i]; ++i) {
		ecopy.push_back (environ[i]);
	}

	for (vector<string>::const_iterator e = ecopy.begin(); e != ecopy.end(); ++e) {
                string::size_type equal = (*e).find_first_of ('=');

                if (equal == string::npos) {
                        /* say what? an environ value without = ? */
                        continue;
                }

                string var_name = (*e).substr (0, equal);
                g_unsetenv(var_name.c_str());
        }
}
