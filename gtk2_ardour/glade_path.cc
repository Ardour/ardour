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

#include <glibmm/fileutils.h>
#include <glibmm/miscutils.h>

#include "i18n.h"
#include "glade_path.h"

#include <iostream>

std::string
GladePath::path(const std::string& glade_file)
{
	std::string user_glade_dir = Glib::getenv(X_("ARDOUR_GLADE_PATH"));
	std::string full_path;
	
	if(user_glade_dir != "") {
		full_path = Glib::build_filename(user_glade_dir, glade_file);
		if(Glib::file_test(full_path, Glib::FILE_TEST_EXISTS)) return full_path;
	}
	
	// check if file ~/.ardour/glade/glade_file exists.
	std::vector<std::string> path;
	path.push_back(Glib::get_home_dir());
	path.push_back(X_(".ardour")); // define as a constant somewhere?
	path.push_back(X_("glade"));
	path.push_back(glade_file);
	full_path = Glib::build_filename(path);
	
	// temporary debugging
	std::cout << "Path to glade file" << full_path << std::endl;
	
	if(Glib::file_test(full_path, Glib::FILE_TEST_EXISTS)) return full_path;
	
	/*
	  If for some wierd reason the system wide glade file
	  doesn't exist libglademm will throw an exception 
	  so don't bother testing if it exists etc.
	*/
	return Glib::build_filename(GLADEPATH, glade_file);
}
