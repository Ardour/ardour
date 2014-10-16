/*
    Copyright (C) 2008 John Emmas

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

#include <shlobj.h>
#include <winreg.h>
#include <glib.h>

#include "pbd/windows_special_dirs.h"

//***************************************************************
//
//	get_win_special_folder()
//
//  Gets the full path name that corresponds of one of the Windows
//  special folders, such as "My Documents" and the like. The input
//  parameter must be one of the corresponding CSIDL values, such
//  as CSIDL_SYSTEM etc.
//  
//	Returns:
//
//    On Success: A pointer to a newly allocated string containing
//                the name of the special folder (must later be freed).
//    On Failure: NULL
//

char *
PBD::get_win_special_folder (int csidl)
{
	wchar_t path[PATH_MAX+1];
	HRESULT hr;
	LPITEMIDLIST pidl = 0;
	char *retval = 0;
	
	if (S_OK == (hr = SHGetSpecialFolderLocation (0, csidl, &pidl))) {

		if (SHGetPathFromIDListW (pidl, path)) {
			retval = g_utf16_to_utf8 ((const gunichar2*)path, -1, 0, 0, 0);
		}
		CoTaskMemFree (pidl);
	}

	return retval;
}

