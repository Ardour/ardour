/*
    Copyright (C) 2009 John Emmas

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

#ifdef __MINGW64__

#include <WTypes.h>
#include <glibmm.h>

//***************************************************************
//
//	realpath()
//
// Emulates POSIX realpath() using Win32 _fullpath().
//
//	Returns:
//
//    On Success: A pointer to the resolved (absolute) path
//    On Failure: NULL
//

extern "C" {
		
char* 
realpath (const char *original_path, char resolved_path[_MAX_PATH+1])
{
char *pRet = NULL;
bool bIsSymLink = 0; // We'll probably need to test the incoming path
                     // to find out if it points to a Windows shortcut
                     // (or a hard link) and set this appropriately.
	if (bIsSymLink)
	{
		// At the moment I'm not sure if Windows '_fullpath()' is directly
		// equivalent to POSIX 'realpath()' - in as much as the latter will
		// resolve the supplied path if it happens to point to a symbolic
		// link ('_fullpath()' probably DOESN'T do this but I'm not really
		// sure if Ardour needs such functionality anyway). Therefore we'll
		// possibly need to add that functionality here at a later date.
	}
	else
	{
		char temp[(MAX_PATH+1)*6]; // Allow for maximum length of a path in UTF8 characters

		// POSIX 'realpath()' requires that the buffer size is at
		// least PATH_MAX+1, so assume that the user knew this !!
		pRet = _fullpath(temp, Glib::locale_from_utf8(original_path).c_str(), _MAX_PATH);
		if (NULL != pRet)
			strcpy(resolved_path, Glib::locale_to_utf8(temp).c_str());
	}

	return (pRet);
}

}

#endif  // __MINGW64__
