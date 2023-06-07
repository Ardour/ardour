/*
 * Copyright (C) 2007-2012 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2013 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
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

#ifndef ARDOUR_SESSION_STATE_UTILS_INCLUDED
#define ARDOUR_SESSION_STATE_UTILS_INCLUDED

#include <vector>
#include <string>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

/**
 * Attempt to create a backup copy of a file.
 *
 * A copy of the file is created in the same directory using
 * the same filename with the backup suffix appended.
 *
 * @return true if successful, false otherwise.
 */
LIBARDOUR_API bool create_backup_file (const std::string & file_path);

} // namespace ARDOUR

#endif
