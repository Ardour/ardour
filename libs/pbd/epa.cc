#include <cstdlib>

#include "pbd/epa.h"

extern char** environ;

using namespace PBD;
using namespace std;

EnvironmentalProtectionAgency::EnvironmentalProtectionAgency ()
{
        save ();
}

EnvironmentalProtectionAgency::~EnvironmentalProtectionAgency()
{
        restore ();
}

void
EnvironmentalProtectionAgency::save ()
{
        e.clear ();

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
void
EnvironmentalProtectionAgency::restore ()
{
        for (map<string,string>::iterator i = e.begin(); i != e.end(); ++i) {
                setenv (i->first.c_str(), i->second.c_str(), 1);
        }
}                         
