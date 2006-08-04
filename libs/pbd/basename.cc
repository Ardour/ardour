#include <iostream>
#include <string.h>
#include <pbd/basename.h>


// implement this using Glib::path_get_basename
std::string 
PBD::basename_nosuffix (const std::string& str)
{
	std::string::size_type slash = str.find_last_of ('/');
	std::string noslash;

	if (slash == std::string::npos) {
		noslash = str;
	} else {
		noslash = str.substr (slash+1);
	}

	return noslash.substr (0, noslash.find_last_of ('.'));
}
