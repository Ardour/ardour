/*
    Copyright (C) 2010 Paul Davis
    Copyright (C) 2010-2014 Robin Gareus <robin@gareus.org>

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

#include <glibmm/miscutils.h>
#include "pbd/file_utils.h"
#include "pbd/error.h"

#include "ardour/filesystem_paths.h"
#include "ardour/system_exec.h"

using namespace ARDOUR;

char * SystemExec::_vfork_exec_wrapper = NULL;

static char *vfork_exec_wrapper_path() {
#ifdef PLATFORM_WINDOWS
	return NULL;
#else
	std::string vfork_exec_wrapper;
	if (!PBD::find_file (
				PBD::Searchpath(
					ARDOUR::ardour_dll_directory() // deployed
					+ G_SEARCHPATH_SEPARATOR_S + Glib::build_filename(ARDOUR::ardour_dll_directory(), "vfork") // src, build (ardev, etc)
					),
				"ardour-exec-wrapper", vfork_exec_wrapper)) {
		PBD::fatal << "vfork exec wrapper 'ardour-exec-wrapper' was not found in $PATH." << endmsg;
		/* not reached */
		return NULL;
	}
	return strdup(vfork_exec_wrapper.c_str());
#endif
}

SystemExec::SystemExec (std::string c, char ** a)
	: PBD::SystemExec(c, a)
{
#ifndef PLATFORM_WINDOWS
	if (!_vfork_exec_wrapper) {
		_vfork_exec_wrapper = vfork_exec_wrapper_path();
	}
#endif
}

SystemExec::SystemExec (std::string c, std::string a)
	: PBD::SystemExec(c, a)
{
#ifndef PLATFORM_WINDOWS
	if (!_vfork_exec_wrapper) {
		_vfork_exec_wrapper = vfork_exec_wrapper_path();
	}
#endif
}

SystemExec::SystemExec (std::string c, const std::map<char, std::string> subs)
	: PBD::SystemExec(c, subs)
{
#ifndef PLATFORM_WINDOWS
	if (!_vfork_exec_wrapper) {
		_vfork_exec_wrapper = vfork_exec_wrapper_path();
	}
#endif
}

SystemExec::~SystemExec() { }
