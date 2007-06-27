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

#include <pbd/error.h>

#include <glibmm/miscutils.h>

#include <ardour/directory_names.h>
#include <ardour/filesystem_paths.h>

namespace ARDOUR {

using std::string;

sys::path
user_config_directory ()
{
	const string home_dir = Glib::get_home_dir ();

	if (home_dir.empty ())
	{
		const string error_msg = "Unable to determine home directory";

		// log the error
		error << error_msg << endmsg;

		throw sys::filesystem_error(error_msg);
	}

	sys::path p(home_dir);
	p /= user_config_dir_name;

	return p;
}

sys::path
ardour_module_directory ()
{
	sys::path module_directory(MODULE_DIR);
	module_directory /= "ardour2";
	return module_directory;
}

} // namespace ARDOUR
