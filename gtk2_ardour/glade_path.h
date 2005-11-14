/*
    Copyright (C) 2005 Paul Davis 

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

    $Id$
*/

#ifndef GLADE_PATH_H
#define GLADE_PATH_H

#include <string>

struct GladePath {

	/**
	   @return Path to glade file.
	   
	   XXX subject to change upon discussion.

	   glade files are currently looked for in
	   three possible directories in this order.

	   In the directory defined in the environment
	   variable ARDOUR_GLADE_PATH

	   In the users .ardour/glade directory.
	   
	   In the system defined glade path.
	*/
	static std::string
	path(const std::string& glade_filename);

};

#endif // GLADE_PATH_H

