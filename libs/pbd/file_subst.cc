/*
    Copyright (C) 2011 Paul Davis 

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

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstdio> /* for ::rename() */
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "pbd/file_subst.h"
#include "pbd/error.h"
#include "pbd/replace_all.h"
#include "pbd/compose.h"

#include "i18n.h"

using namespace PBD;
using namespace std;

/* this implementation assumes that the contents of the file are small enough to fit reasonably into 
   memory.
*/

int
PBD::file_subst (const string& path, const map<string,string>& dict)
{
        ifstream in (path.c_str());
        int replacements = 0;

        if (!in) {
                return -1;
        }

        /* Get the size of the file */
        in.seekg(0,std::ios::end);
        std::streampos length = in.tellg();
        in.seekg(0,std::ios::beg);

        /* allocate a string to hold the data */

        string str;

        try {
                str.reserve (length);
        } catch (exception& e) {
		cerr << "reserve failed\n";
                error << string_compose (_("could not reserve space to read substitution data from %1 (err: %2"),
                                         path, e.what()) << endmsg;
                in.close ();
                return -1;
        }

        /* read the file contents into the string */

        while (true) {

                str += in.get();

		if (in.eof()) {
			break;
		}

		if (in.fail()) {
			cerr << "input file has failed after " << str.size() << "chars\n";
			error << string_compose (_("could not read data for substitution from %1 (err: %2)"),
						 path, strerror (errno)) << endmsg;
			in.close ();
			return -1;
		}

        }

        in.close ();

        /* do the replacement */

        for (map<string,string>::const_iterator i = dict.begin(); i != dict.end(); ++i) {
                replacements += replace_all (str, i->first, i->second);
        }

        if (replacements) {
                char suffix[64];
                snprintf (suffix, sizeof (suffix), ".fs_%d", getpid());
                string s = path + suffix;

                ofstream out (s.c_str());

                if (out) {

                        out << str;

                        if (!out) {
				cerr << "output failed\n";
                                /* ignore error since we're failing anyway, although
                                   it will leave the file around.
                                */
                                out.close ();
                                (void) ::unlink (s.c_str());
                                return -1;
                        } else {
                                if (::rename (s.c_str(), path.c_str())) {
                                        error << string_compose (_("Could not rename substituted file %1 to %2 (err: %3)"), 
                                                                 s, path, strerror (errno)) << endmsg;
                                        (void) ::unlink (s.c_str());
                                        out.close ();
                                        return -1;
                                }
                        }

                        out.close ();

                } else {
                        return -1;
                }

        }

        return 0;
}

