/* Copyright (C) 2005 The cairomm Development Team
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
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include <cairomm/path.h>
#include <cairomm/private.h>
#include <iostream>

namespace Cairo
{

/*
Path::Path()
: m_cobject(0)
{
  m_cobject = cairo_path_create();
}
*/

Path::Path(cairo_path_t* cobject, bool take_ownership)
: m_cobject(0)
{
  if(take_ownership)
    m_cobject = cobject;
  else
  {
    std::cerr << "cairomm: Path::Path(): copying of the underlying cairo_path_t* is not yet implemented." << std::endl;
    //m_cobject = cairo_path_copy(cobject);
  }
}

/*
Path::Path(const Path& src)
{
  //Reference-counting, instead of copying by value:
  if(!src.m_cobject)
    m_cobject = 0;
  else
    m_cobject = cairo_path_copy(src.m_cobject);
}
*/

Path::~Path()
{
  if(m_cobject)
    cairo_path_destroy(m_cobject);
}

/*
Path& Path::operator=(const Path& src)
{
  //Reference-counting, instead of copying by value:

  if(this == &src)
    return *this;

  if(m_cobject == src.m_cobject)
    return *this;

  if(m_cobject)
  {
    cairo_path_destroy(m_cobject);
    m_cobject = 0;
  }

  if(!src.m_cobject)
    return *this;

  m_cobject = cairo_path_copy(src.m_cobject);

  return *this;
}
*/

/*
bool Path::operator==(const Path& src) const
{
  return cairo_path_equal(m_cobject, src.cobj());
}
*/

} //namespace Cairo

// vim: ts=2 sw=2 et
