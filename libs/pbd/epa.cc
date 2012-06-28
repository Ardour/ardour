/*
    Copyright (C) 2010 Paul Davis 

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

#include <cstdlib>

#include "pbd/epa.h"
#include "pbd/strsplit.h"

extern char** environ;

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

                const char* estr = getenv (_envname.c_str());

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
                setenv (i->first.c_str(), i->second.c_str(), 1);
        }
} 

void
EnvironmentalProtectionAgency::clear () const
{
        char** the_environ = environ;

        for (size_t i = 0; the_environ[i]; ++i) {
			
                string estring = the_environ[i];
                string::size_type equal = estring.find_first_of ('=');
			
                if (equal == string::npos) {
                        /* say what? an environ value without = ? */
                        continue;
                }
			
                string before = estring.substr (0, equal);
                unsetenv(before.c_str());
        }
}                        
