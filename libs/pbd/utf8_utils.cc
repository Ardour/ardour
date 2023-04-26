/*
 * Copyright (C) 2023 Robin Gareus <robin@gareus.org>
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

#include <glibmm/miscutils.h>

#include "pbd/utf8_utils.h"

std::string
PBD::sanitize_utf8 (std::string const& s)
{
	std::string rv;
	char const* data = s.c_str ();
	for (char const *ptr = data, *pend = data; *pend != '\0'; ptr = pend + 1) {
		g_utf8_validate (ptr, -1, &pend);
		rv.append (ptr, pend);
	}
	return rv;
}
