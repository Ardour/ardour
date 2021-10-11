/*
 * Copyright (C) 2008 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
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

#include <shlobj.h>
#include <winreg.h>
#include <glibmm.h>

#include "pbd/windows_special_dirs.h"

std::string
PBD::get_win_special_folder_path (int csidl)
{
	wchar_t path[PATH_MAX+1];
	HRESULT hr;
	LPITEMIDLIST pidl = 0;
	char *utf8_folder_path = 0;

	if (S_OK == (hr = SHGetSpecialFolderLocation (0, csidl, &pidl))) {

		if (SHGetPathFromIDListW (pidl, path)) {
			utf8_folder_path = g_utf16_to_utf8 ((const gunichar2*)path, -1, 0, 0, 0);
		}
		CoTaskMemFree (pidl);
	}

	if (utf8_folder_path != NULL) {
		std::string folder_path(utf8_folder_path);
		g_free (utf8_folder_path);
		return folder_path;
	}
	return std::string();
}

bool
PBD::windows_query_registry (const char *regkey, const char *regval, std::string &rv, HKEY root)
{
	HKEY key;
	DWORD size = PATH_MAX;
	char tmp[PATH_MAX+1];

	if (ERROR_SUCCESS == RegOpenKeyExA (root, regkey, 0, KEY_READ, &key)) {
		if (ERROR_SUCCESS == RegQueryValueExA (key, regval, 0, NULL, reinterpret_cast<LPBYTE>(tmp), &size)) {
			rv = Glib::locale_to_utf8 (tmp);
			RegCloseKey (key);
			return true;
		}
		RegCloseKey (key);
	}

	if (ERROR_SUCCESS == RegOpenKeyExA (root, regkey, 0, KEY_READ | KEY_WOW64_32KEY, &key)) {
		if (ERROR_SUCCESS == RegQueryValueExA (key, regval, 0, NULL, reinterpret_cast<LPBYTE>(tmp), &size)) {
			rv = Glib::locale_to_utf8 (tmp);
			RegCloseKey (key);
			return true;
		}
		RegCloseKey (key);
	}

#ifndef _WIN64
	// If this is a 32-bit build (but we're running in Win64) the above
	// code will only search Win32 registry keys. So if we got this far
	// without finding anything, force Windows to search Win64 keys too
	if (ERROR_SUCCESS == RegOpenKeyExA (root, regkey, 0, KEY_READ | KEY_WOW64_64KEY, &key)) {
		if (ERROR_SUCCESS == RegQueryValueExA (key, regval, 0, NULL, reinterpret_cast<LPBYTE>(tmp), &size)) {
			rv = Glib::locale_to_utf8 (tmp);
			RegCloseKey (key);
			return true;
		}
		RegCloseKey (key);
	}
#endif

	return false;
}
