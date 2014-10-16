/*
    Copyright (C) 2004 Paul Davis

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

#ifndef __ardour_recent_sessions_h__
#define __ardour_recent_sessions_h__

#include <deque>
#include <utility>
#include <string>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {
	typedef std::deque<std::pair<std::string,std::string> > RecentSessions;

	LIBARDOUR_API int read_recent_sessions (RecentSessions& rs);
	LIBARDOUR_API int store_recent_sessions (std::string name, std::string path);
	LIBARDOUR_API int write_recent_sessions (RecentSessions& rs);
	LIBARDOUR_API int remove_recent_sessions (const std::string& path);
}; // namespace ARDOUR

#endif // __ardour_recent_sessions_h__
