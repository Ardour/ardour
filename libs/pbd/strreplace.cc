/*
 * Copyright (C) 2000-2015 Paul Davis <paul@linuxaudiosystems.com>
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

#include "pbd/replace_all.h"
#include "glibmm/miscutils.h"

int
replace_all (std::string& str,
	     std::string const& target,
	     std::string const& replacement)
{
	std::string::size_type start = str.find (target, 0);
	int cnt = 0;

	while (start != std::string::npos) {
		str.replace (start, target.size(), replacement);
		start = str.find (target, start+replacement.size());
		++cnt;
	}

	return cnt;
}

std::string
poor_mans_glob (std::string path)
{
	if (path.find ('~') == 0) {
		path.replace (0, 1, Glib::get_home_dir());
	}
	return path;
}

