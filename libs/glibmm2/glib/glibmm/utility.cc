// -*- c++ -*-

/* $Id: utility.cc 2 2003-01-07 16:59:16Z murrayc $ */

/* Copyright (C) 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/utility.h>
#include <glib/gstrfuncs.h>


void Glib::append_canonical_typename(std::string& dest, const char* type_name)
{
  const std::string::size_type offset = dest.size();
  dest += type_name;

  std::string::iterator p = dest.begin() + offset;
  const std::string::iterator pend = dest.end();

  for(; p != pend; ++p)
  {
    if(!(g_ascii_isalnum(*p) || *p == '_' || *p == '-'))
      *p = '+';
  }
}

