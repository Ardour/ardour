/*
    Copyright (C) 2013 Paul Davis

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

#ifndef ARDOUR_LV2_BUNDLED_SEARCH_PATH_INCLUDED
#define ARDOUR_LV2_BUNDLED_SEARCH_PATH_INCLUDED

#include "pbd/search_path.h"

namespace ARDOUR {

	/**
	 * return a SearchPath containing directories in which to look for
	 * lv2 plugins.
	 */
	PBD::SearchPath lv2_bundled_search_path ();

} // namespace ARDOUR

#endif
