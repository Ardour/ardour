#include <iostream>
#include <string.h>
#include <pbd/basename.h>

char *
PBD::basename (const char *path)

{
	char *slash;

	if ((slash = strrchr (path, '/')) == 0) {
		return strdup (path);
	}
	
	if (*(slash+1) == '\0') {
		return strdup ("");
	}
	
	return strdup (slash+1);
}

std::string 
PBD::basename (const std::string str)
{
	std::string::size_type slash = str.find_last_of ('/');

	if (slash == std::string::npos) {
		return str;
	} 

	return str.substr (slash+1);
}

std::string 
PBD::basename_nosuffix (const std::string str)
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
