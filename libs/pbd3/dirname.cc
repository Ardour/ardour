#include <cstdio>
#include <cstdlib>
#include <string>
#include <pbd/dirname.h>


char *
PBD::dirname (const char *path)

{
	char *slash;
	size_t len;
	char *ret;
	
	if ((slash = strrchr (path, '/')) == 0) {
		return strdup (path);
	}
	
	if (*(slash+1) == '\0') {
		return strdup ("");
	}

	len = (size_t) (slash - path);
	ret = (char *) malloc (sizeof (char) * (len + 1));

	snprintf (ret, len, "%*s", (int)len, path);
	return ret;
}

std::string 
PBD::dirname (const std::string str)
{
	std::string::size_type slash = str.find_last_of ('/');
	std::string dir;

	if (slash == std::string::npos) {
		return str;
	}

	/* remove trailing multiple slashes (legal under POSIX) */

	dir = str.substr (0, slash);
	slash = dir.length();

	while (slash > 1 && dir[slash-1] == '/') {
		slash--;
		dir = dir.substr (0, slash);
	}

	return dir;
}
