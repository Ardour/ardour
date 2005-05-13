// -*- c++ -*-
#ifndef _GLIBMM_SARRAY_H
#define _GLIBMM_SARRAY_H

/* $Id$ */

/* array.h
 *
 * Copyright (C) 2002 The gtkmm Development Team
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


#include <glibmm/arrayhandle.h>
#include <glibmm/ustring.h>

namespace Glib { typedef Glib::ArrayHandle<Glib::ustring> SArray; }

#if 0

namespace Glib
{

template <>
inline void cpp_type_to_c_type(const ustring& cpp_value, type_constpch& ref_c_value)
{
  ref_c_value = cpp_value.c_str();
}

template <>
inline void cpp_type_to_c_type(const std::string& cpp_value, type_constpch& ref_c_value)
{
  ref_c_value = cpp_value.c_str();
}

typedef Array<Glib::ustring, const char*> SArray;

/*
class SArray: public Array<nstring, const char*>
{
public:
  typedef const char* T_c;
  typedef Array<nstring, const char*> type_base;

  SArray(const SArray& src);

  // copy other containers
  template <typename T_container>
  SArray(const T_container& t)
  {
    owned_ = Array_Helpers::Traits<T_container, pointer>::get_owned();
    size_ = Array_Helpers::Traits<T_container, pointer>::get_size(t);
    pData_ = Array_Helpers::Traits<T_container, pointer>::get_data(t);
  }

  SArray(const T_c* pValues, size_type size);

  // copy a sequence
  template <typename Iterator>
  SArray(Iterator b, Iterator e);

  operator std::vector<nstring>() const;
  operator std::vector<ustring>() const;
  operator std::vector<std::string>() const;

  operator std::deque<nstring>() const;
  operator std::deque<ustring>() const;
  operator std::deque<std::string>() const;

  operator std::list<nstring>() const;
  operator std::list<ustring>() const;
  operator std::list<std::string>() const;
};


//template <typename T_container>
//SArray::SArray(const T_container& t)
//: type_base(t)
//{
//}


template <typename Iterator>
SArray::SArray(Iterator b, Iterator e)
: type_base(b, e)
{
}
*/

} // namespace Glib

#endif /* #if 0 */

#endif // _GLIBMM_SARRAY_H

