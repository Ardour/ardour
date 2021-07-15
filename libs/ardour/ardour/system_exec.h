/*
 * Copyright (C) 2014-2019 Robin Gareus <robin@gareus.org>
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
#ifndef _ardour_system_exec_h_
#define _ardour_system_exec_h_

#include <glibmm/threads.h>

#include "ardour/libardour_visibility.h"
#include "pbd/system_exec.h"

namespace ARDOUR {

class LIBARDOUR_API SystemExec : public PBD::SystemExec
{
public:
	SystemExec (std::string c, std::string a = "");
	SystemExec (std::string c, char** a);
	SystemExec (std::string c, const std::map<char, std::string> subs);
	~SystemExec ();

	int start (StdErrMode stderr_mode = IgnoreAndClose)
	{
		return PBD::SystemExec::start (stderr_mode, _vfork_exec.c_str ());
	}

private:
	static void initialize ();

	static bool                 _initialized;
	static Glib::Threads::Mutex _init_mutex;
	static std::string          _vfork_exec;

};

}; // namespace ARDOUR

#endif /* _libpbd_system_exec_h_ */
