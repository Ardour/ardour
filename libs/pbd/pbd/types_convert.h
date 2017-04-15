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

#ifndef PBD_TYPES_CONVERT_H
#define PBD_TYPES_CONVERT_H

#include "pbd/enum_convert.h"

#include "pbd/controllable.h"
#include "pbd/id.h"

namespace PBD {

DEFINE_ENUM_CONVERT(Controllable::Flag)

template <>
inline bool to_string (ID val, std::string& str)
{
	str = val.to_s();
	return true;
}

template <>
inline bool string_to (const std::string& str, ID& val)
{
	val = str;
	return true;
}

template <>
inline std::string to_string (ID val)
{
	return val.to_s();
}

template <>
inline ID string_to (const std::string& str)
{
	return ID(str);
}

} // namespace PBD

#endif // PBD_TYPES_CONVERT_H
