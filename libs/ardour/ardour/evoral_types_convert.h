/*
    Copyright (C) 2015 Tim Mayberry

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

#ifndef ARDOUR_EVORAL_TYPES_CONVERT_H
#define ARDOUR_EVORAL_TYPES_CONVERT_H

#include "pbd/enum_convert.h"

#include "evoral/Beats.hpp"
#include "evoral/ControlList.hpp"

namespace PBD {

DEFINE_ENUM_CONVERT(Evoral::ControlList::InterpolationStyle)

template <>
inline bool to_string (Evoral::Beats beats, std::string& str)
{
	return double_to_string (beats.to_double (), str);
}

template <>
inline bool string_to (const std::string& str, Evoral::Beats& beats)
{
	double tmp;
	if (!string_to_double (str, tmp)) {
		return false;
	}
	beats = Evoral::Beats(tmp);
	return true;
}

template <>
inline std::string to_string (Evoral::Beats beats)
{
	std::string tmp;
	double_to_string (beats.to_double (), tmp);
	return tmp;
}

template <>
inline Evoral::Beats string_to (const std::string& str)
{
	double tmp;
	string_to_double (str, tmp);
	return Evoral::Beats (tmp);
}

} // namespace PBD

#endif // ARDOUR_EVORAL_TYPES_CONVERT_H
