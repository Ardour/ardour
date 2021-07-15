/*
 * Copyright (C) 2014-2015 Robin Gareus <robin@gareus.org>
 * Copyright (C) 2014-2015 Tim Mayberry <mojofunk@gmail.com>
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

#include <glibmm/miscutils.h>

#include "pbd/error.h"
#include "pbd/file_utils.h"

#include "ardour/filesystem_paths.h"
#include "ardour/system_exec.h"

namespace ARDOUR {

bool                 SystemExec::_initialized = false;
Glib::Threads::Mutex SystemExec::_init_mutex;
std::string          SystemExec::_vfork_exec;

void
SystemExec::initialize ()
{
	if (_initialized) {
		return;
	}
#ifndef PLATFORM_WINDOWS
	Glib::Threads::Mutex::Lock lk (_init_mutex);
	if (_initialized) {
		return;
	}
	PBD::Searchpath vfsp (
	    ARDOUR::ardour_dll_directory () //< deployed
	    + G_SEARCHPATH_SEPARATOR_S + Glib::build_filename (ARDOUR::ardour_dll_directory (), "vfork") //< src-tree (ardev, etc)
	);

	if (!PBD::find_file (vfsp, "ardour-exec-wrapper", _vfork_exec)) {
		PBD::fatal << "child process app 'ardour-exec-wrapper' was not found in search path:\n"
		           << vfsp.to_string () << endmsg;
		abort (); /*NOTREACHED*/
	}
#endif
	_initialized = true;
}

SystemExec::SystemExec (std::string c, char** a)
	: PBD::SystemExec (c, a)
{
	initialize ();
}

SystemExec::SystemExec (std::string c, std::string a)
	: PBD::SystemExec (c, a)
{
	initialize ();
}

SystemExec::SystemExec (std::string c, const std::map<char, std::string> subs)
	: PBD::SystemExec (c, subs)
{
	initialize ();
}

SystemExec::~SystemExec () {}

} // namespace ARDOUR
