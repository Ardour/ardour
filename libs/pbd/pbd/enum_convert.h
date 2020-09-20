/*
 * Copyright (C) 2016 Tim Mayberry <mojofunk@gmail.com>
 * Copyright (C) 2017 Robin Gareus <robin@gareus.org>
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

#ifndef PBD_ENUM_CONVERT_H
#define PBD_ENUM_CONVERT_H

#include <typeinfo>

#include "pbd/enumwriter.h"
#include "pbd/string_convert.h"

#define TO_STRING_FULL(Type)                                                  \
  template <>                                                                 \
  inline bool to_string (Type val, std::string& str)                          \
  {                                                                           \
    str = enum_2_string (val);                                                \
    return true;                                                              \
  }

#define STRING_TO_FULL(Type)                                                  \
  template <>                                                                 \
  inline bool string_to (const std::string& str, Type& val)                   \
  {                                                                           \
    val = (Type)string_2_enum (str, val);                                     \
    return true;                                                              \
  }

#define TO_STRING(Type) \
  template<> inline std::string to_string (Type val)                          \
  { return enum_2_string (val); }

#define STRING_TO(Type) \
  template<> inline Type string_to (const std::string& str)                   \
  { Type val; return (Type) string_2_enum (str, val); }

#define DEFINE_ENUM_CONVERT(Type)                                             \
  TO_STRING_FULL (Type)                                                       \
  STRING_TO_FULL (Type)                                                       \
  TO_STRING (Type)                                                            \
  STRING_TO (Type)

#endif // PBD_ENUM_CONVERT_H
