/*
 * Copyright (C) 2007-2014 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2016 Paul Davis <paul@linuxaudiosystems.com>
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

#include <algorithm>

#include <glibmm/fileutils.h>

#include <giomm/file.h>

#include "pbd/basename.h"
#include "pbd/compose.h"
#include "pbd/error.h"
#include "pbd/file_utils.h"

#include "ardour/session_state_utils.h"
#include "ardour/filename_extensions.h"

#include "pbd/i18n.h"

using namespace std;
using namespace PBD;

namespace ARDOUR {

bool
create_backup_file (const std::string & file_path)
{
	return copy_file (file_path, file_path + backup_suffix);
}

} // namespace ARDOUR
