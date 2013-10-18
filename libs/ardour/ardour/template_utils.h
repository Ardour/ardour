/*
    Copyright (C) 2012 Paul Davis 

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


#ifndef TEMPLATE_UTILS_INCLUDED
#define TEMPLATE_UTILS_INCLUDED

#include <string>
#include <vector>

#include "ardour/libardour_visibility.h"

namespace ARDOUR {

	LIBARDOUR_API std::string system_template_directory ();
	LIBARDOUR_API std::string system_route_template_directory ();

	LIBARDOUR_API std::string user_template_directory ();
	LIBARDOUR_API std::string user_route_template_directory ();

	struct LIBARDOUR_API TemplateInfo {
		std::string name;
		std::string path;
	};

	LIBARDOUR_API void find_route_templates (std::vector<TemplateInfo>& template_names);
	LIBARDOUR_API void find_session_templates (std::vector<TemplateInfo>& template_names);

	LIBARDOUR_API std::string session_template_dir_to_file (std::string const &);

} // namespace ARDOUR

#endif
