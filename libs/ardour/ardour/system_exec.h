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
#ifndef _ardour_system_exec_h_
#define _ardour_system_exec_h_

#include "ardour/libardour_visibility.h"
#include "pbd/system_exec.h"

namespace ARDOUR {

class LIBARDOUR_API SystemExec
	: public PBD::SystemExec
{

public:
	SystemExec (std::string c, std::string a = "");
	SystemExec (std::string c, char ** a);
	SystemExec (std::string c, const std::map<char, std::string> subs);
	~SystemExec ();

	int start (int stderr_mode = 1) {
		return PBD::SystemExec::start(stderr_mode, _vfork_exec_wrapper);
	}

private:
	static char * _vfork_exec_wrapper;

}; /* end class */

}; /* end namespace */

#endif /* _libpbd_system_exec_h_ */


