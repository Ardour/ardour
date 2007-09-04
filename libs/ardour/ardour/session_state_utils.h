/*
	Copyright (C) 2007 Tim Mayberry 

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

#ifndef ARDOUR_SESSION_STATE_UTILS_INCLUDED
#define ARDOUR_SESSION_STATE_UTILS_INCLUDED

#include <pbd/filesystem.h>

namespace ARDOUR {

using namespace PBD;

/**
 * Attempt to create a backup copy of a file.
 *
 * A copy of the file is created in the same directory using 
 * the same filename with the backup suffix appended.
 *
 * @return true if successful, false otherwise.
 */
bool create_backup_file (const sys::path & file_path);

} // namespace ARDOUR

#endif
