/* 
    Copyright (C) 2006 Paul Davis

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

    $Id$
*/

#include <cctype>

#include <cstring>
#include <cstdlib>

#include "pbd/enumwriter.h"
#include "pbd/error.h"
#include "pbd/compose.h"

using namespace std;
using namespace PBD;

#include "i18n.h"

EnumWriter* EnumWriter::_instance = 0;
map<string,string> EnumWriter::hack_table;

static int 
nocase_cmp(const string & s1, const string& s2) 
{
	string::const_iterator it1 = s1.begin();
	string::const_iterator it2 = s2.begin();
	
	while ((it1 != s1.end()) && (it2 != s2.end())) { 
		if(::toupper(*it1) != ::toupper(*it2))  {//letters differ?
			// return -1 to indicate 'smaller than', 1 otherwise
			return (::toupper(*it1) < ::toupper(*it2)) ? -1 : 1; 
		}

		++it1;
		++it2;
	}

	string::size_type size1 = s1.size();
	string::size_type size2 = s2.size();

	//return -1,0 or 1 according to strings' lengths

	if (size1 == size2) {
		return 0;
	}

	return (size1 < size2) ? -1 : 1;
}

EnumWriter&
EnumWriter::instance() 
{
	if (_instance == 0) {
		_instance = new EnumWriter;
	} 

	return *_instance;
}

void
EnumWriter::destroy ()
{
	delete _instance;
	_instance = 0;
}

EnumWriter::EnumWriter ()
{
}

EnumWriter::~EnumWriter ()
{
}

void
EnumWriter::register_distinct (string type, vector<int> v, vector<string> s)
{
	pair<string,EnumRegistration> newpair;
	pair<Registry::iterator,bool> result;

	newpair.first = type;
	newpair.second = EnumRegistration (v, s, false);
	
	result = registry.insert (newpair);

	if (!result.second) {
		warning << string_compose (_("enum type \"%1\" already registered with the enum writer"), type) << endmsg;
	}
}

void
EnumWriter::register_bits (string type, vector<int> v, vector<string> s)
{
	pair<string,EnumRegistration> newpair;
	pair<Registry::iterator,bool> result;

	newpair.first = type;
	newpair.second = EnumRegistration (v, s, true);
	
	result = registry.insert (newpair);

	if (!result.second) {
		warning << _("enum type \"%1\" already registered with the enum writer") << endmsg;
	}
}

string
EnumWriter::write (string type, int value)
{
	Registry::iterator x = registry.find (type);

	if (x == registry.end()) {
		error << string_compose (_("EnumWriter: unknown enumeration type \"%1\""), type) << endmsg;
		throw unknown_enumeration (type);
	}

	if (x->second.bitwise) {
		return write_bits (x->second, value);
	} else {
		return write_distinct (x->second, value);
	}
}

int
EnumWriter::read (string type, string value)
{
	Registry::iterator x = registry.find (type);

	if (x == registry.end()) {
		error << string_compose (_("EnumWriter: unknown enumeration type \"%1\""), type) << endmsg;
		throw unknown_enumeration (type);
	}

	if (x->second.bitwise) {
		return read_bits (x->second, value);
	} else {
		return read_distinct (x->second, value);
	}
}	

string
EnumWriter::write_bits (EnumRegistration& er, int value)
{
	vector<int>::iterator i;
	vector<string>::iterator s;
	string result;

	for (i = er.values.begin(), s = er.names.begin(); i != er.values.end(); ++i, ++s) {
		if (value & (*i)) {
			if (!result.empty()) {
				result += ',';
			} 
			result += (*s);
		}
	}

	return result;
}

string
EnumWriter::write_distinct (EnumRegistration& er, int value)
{
	vector<int>::iterator i;
	vector<string>::iterator s;

	for (i = er.values.begin(), s = er.names.begin(); i != er.values.end(); ++i, ++s) {
		if (value == (*i)) {
			return (*s);
		}
	}

	return string();
}

int
EnumWriter::validate (EnumRegistration& er, int val)
{
        if (er.values.empty()) {
                return val;
        }

        if (val == 0) {
                /* zero is always a legal value for our enumerations, just about
                 */
                return val;
        }

        vector<int>::iterator i;
        string enum_name = _("unknown enumeration");
        
        for (Registry::iterator x = registry.begin(); x != registry.end(); ++x) {
                if (&er == &(*x).second) {
                        enum_name = (*x).first;
                }
        }
        

        for (i = er.values.begin(); i != er.values.end(); ++i) {
                if (*i == val) {
                        return val;
                }
        }
        
        warning << string_compose (_("Illegal value loaded for %1 (%2) - %3 used instead"),
                                   enum_name, val, er.names.front()) 
                << endmsg;
        return er.values.front();
}

int
EnumWriter::read_bits (EnumRegistration& er, string str)
{
	vector<int>::iterator i;
	vector<string>::iterator s;
	int result = 0;
	bool found = false;
	string::size_type comma;

	/* catch old-style hex numerics */

	if (str.length() > 2 && str[0] == '0' && str[1] == 'x') {
		int val = strtol (str.c_str(), (char **) 0, 16);
                return validate (er, val);
	}

	/* catch old style dec numerics */

	if (strspn (str.c_str(), "0123456789") == str.length()) {
		int val = strtol (str.c_str(), (char **) 0, 10);
                return validate (er, val);
        }

	do {
		
		comma = str.find_first_of (',');
		string segment = str.substr (0, comma);

		for (i = er.values.begin(), s = er.names.begin(); i != er.values.end(); ++i, ++s) {
			if (segment == *s || nocase_cmp (segment, *s) == 0) {
				result |= (*i);
				found = true;
			}
		}

		if (comma == string::npos) {
			break;
		}

		str = str.substr (comma+1);

	} while (true);

	if (!found) {
		throw unknown_enumeration (str);
	}

	return result;
}

int
EnumWriter::read_distinct (EnumRegistration& er, string str)
{
	vector<int>::iterator i;
	vector<string>::iterator s;

	/* catch old-style hex numerics */

	if (str.length() > 2 && str[0] == '0' && str[1] == 'x') {
		int val = strtol (str.c_str(), (char **) 0, 16);
                return validate (er, val);
	}

	/* catch old style dec numerics */

	if (strspn (str.c_str(), "0123456789") == str.length()) {
		int val = strtol (str.c_str(), (char **) 0, 10);
                return validate (er, val);
        }

	for (i = er.values.begin(), s = er.names.begin(); i != er.values.end(); ++i, ++s) {
		if (str == (*s) || nocase_cmp (str, *s) == 0) {
			return (*i);
		}
	}

	/* failed to find it as-is. check to see if there a hack for the name we're looking up */

	map<string,string>::iterator x;

	if ((x  = hack_table.find (str)) != hack_table.end()) {

		cerr << "found hack for " << str << " = " << x->second << endl;

		str = x->second;

		for (i = er.values.begin(), s = er.names.begin(); i != er.values.end(); ++i, ++s) {
			if (str == (*s) || nocase_cmp (str, *s) == 0) {
				return (*i);
			}
		}
	}

	throw unknown_enumeration(str);
}

void
EnumWriter::add_to_hack_table (string str, string hacked)
{
	hack_table[str] = hacked;
}
