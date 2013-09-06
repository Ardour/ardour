/*
    Copyright (C) 2011 Tim Mayberry

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

#ifndef ARDOUR_LADSPA_SEARCH_PATH_INCLUDED
#define ARDOUR_LADSPA_SEARCH_PATH_INCLUDED

#include "pbd/search_path.h"

namespace ARDOUR {

	/**
	 * return a Searchpath containing directories in which to look for
	 * LADSPA plugins.
	 *
	 * If LADSPA_PATH is defined then the Searchpath returned
	 * will contain the directories specified in it as well as the
	 * user and system directories.
	 */
	PBD::Searchpath ladspa_search_path ();

} // namespace ARDOUR

#endif
