/*
    Copyright (C) 2000-2007 Paul Davis 

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
	std::string copy = path;
	replace_all (copy, "~", Glib::get_home_dir());
	return copy;
}

