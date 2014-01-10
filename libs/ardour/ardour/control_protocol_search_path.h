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

#ifndef ARDOUR_CONTROL_PROTOCOL_SEARCH_PATH_INCLUDED
#define ARDOUR_CONTROL_PROTOCOL_SEARCH_PATH_INCLUDED

#include "pbd/search_path.h"

namespace ARDOUR {

	/**
	 * return a Searchpath containing directories in which to look for
	 * control surface plugins.
	 *
	 * If ARDOUR_SURFACES_PATH is defined then the Searchpath returned
	 * will contain only those directories specified in it, otherwise it will
	 * contain the user and system directories which may contain control
	 * surface plugins.
	 */
	PBD::Searchpath control_protocol_search_path ();

} // namespace ARDOUR

#endif
