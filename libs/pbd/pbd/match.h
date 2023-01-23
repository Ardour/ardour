/*
 * Copyright (C) 2020 Robin Gareus <robin@gareus.org>
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

#ifndef __libpbd_match_h__
#define __libpbd_match_h__

#include <string>
#include <boost/tokenizer.hpp>

namespace PBD {

inline static bool
match_search_strings (std::string const& haystack, std::string const& needle)
{
	boost::char_separator<char> sep (" ");
	typedef boost::tokenizer<boost::char_separator<char> > tokenizer;
	tokenizer t (needle, sep);

	for (tokenizer::iterator ti = t.begin (); ti != t.end (); ++ti) {
		if (haystack.find (*ti) == std::string::npos) {
			return false;
		}
	}
	return true;
}

}

#endif /* __libpbd_match_h__ */
