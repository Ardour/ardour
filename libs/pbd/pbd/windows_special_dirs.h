/*
 * Copyright (C) 2014 John Emmas <john@creativepost.co.uk>
 * Copyright (C) 2014 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2014 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2015 Tim Mayberry <mojofunk@gmail.com>
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

#ifndef __libpbd_windows_special_dirs_h__
#define __libpbd_windows_special_dirs_h__

#include <windows.h>
#include <string>

#include "pbd/libpbd_visibility.h"

namespace PBD {

/**
* Gets the full path that corresponds of one of the Windows special folders,
* such as "My Documents" and the like.
*
* @param csidl corresponds to CSIDL values, such as CSIDL_SYSTEM etc.
* @return A string containing the name of the special folder or an empty
* string on failure.
*/
LIBPBD_API std::string get_win_special_folder_path (int csidl);

/**
 * Convenience function to query registry keys. Test for both native,
 * as well as WindowsOnWindows 64/32 key and returns the value as UTF-8 string.
 */
LIBPBD_API bool windows_query_registry (const char* regkey, const char* regval, std::string &rv, HKEY root = HKEY_LOCAL_MACHINE);

}

#endif /* __libpbd_windows_special_dirs_h__ */
