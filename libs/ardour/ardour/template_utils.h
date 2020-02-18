/*
 * Copyright (C) 2007-2012 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2008-2015 Paul Davis <paul@linuxaudiosystems.com>
 * Copyright (C) 2009-2011 Carl Hetherington <carl@carlh.net>
 * Copyright (C) 2009 David Robillard <d@drobilla.net>
 * Copyright (C) 2016-2018 Robin Gareus <robin@gareus.org>
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


#ifndef TEMPLATE_UTILS_INCLUDED
#define TEMPLATE_UTILS_INCLUDED

#include <string>
#include <vector>

#include "ardour/utils.h"
#include "ardour/libardour_visibility.h"

namespace ARDOUR {

	LIBARDOUR_API std::string system_template_directory ();
	LIBARDOUR_API std::string system_route_template_directory ();

	LIBARDOUR_API std::string user_template_directory ();
	LIBARDOUR_API std::string user_route_template_directory ();

	struct LIBARDOUR_API TemplateInfo {
		std::string name;
		std::string path;
		std::string description;
		std::string modified_with;

		bool operator < (const TemplateInfo& other) const {
			return cmp_nocase_utf8 (name, other.name) < 0;
		}
	};

	LIBARDOUR_API void find_route_templates (std::vector<TemplateInfo>& template_names);
	LIBARDOUR_API void find_session_templates (std::vector<TemplateInfo>& template_names, bool read_xml = false);

	LIBARDOUR_API std::string session_template_dir_to_file (std::string const &);

} // namespace ARDOUR

#endif
