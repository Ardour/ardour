/* $Id: quark.cc 779 2009-01-19 17:58:50Z murrayc $ */

/* quark.cc
 *
 * Copyright 2002 The gtkmm Development Team
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free
 * Software Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <glibmm/quark.h>

namespace Glib
{

QueryQuark::QueryQuark(const GQuark& q)
  : quark_(q) 
{}

QueryQuark::QueryQuark(const ustring& s)
: quark_(g_quark_try_string(s.c_str()))
{}

QueryQuark::QueryQuark(const char* s)
: quark_(g_quark_try_string(s))
{}

QueryQuark& QueryQuark::operator=(const QueryQuark& q)
{ quark_=q.quark_;
  return *this;
}

QueryQuark::operator ustring() const
{
  return ustring(g_quark_to_string(quark_));
}


Quark::Quark(const ustring& s)
: QueryQuark(g_quark_from_string(s.c_str()))
{}

Quark::Quark(const char* s)
: QueryQuark(g_quark_from_string(s))
{}

Quark::~Quark()
{}


GQuark quark_ = 0;
GQuark quark_cpp_wrapper_deleted_ = 0;

} /* namespace Glib */
