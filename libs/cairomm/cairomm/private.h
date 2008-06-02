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

#ifndef __CAIROMM_PRIVATE_H
#define __CAIROMM_PRIVATE_H

#include <cairomm/enums.h>
#include <cairomm/exception.h>
#include <string>

#ifndef DOXYGEN_IGNORE_THIS
namespace Cairo
{

/// Throws the appropriate exception, if exceptions are enabled.
void throw_exception(ErrorStatus status);

//We inline this because it is called so often.
inline void check_status_and_throw_exception(ErrorStatus status)
{
  if(status != CAIRO_STATUS_SUCCESS)
    throw_exception(status); //This part doesn't need to be inline because it would rarely be called.
}

template<class T>
void check_object_status_and_throw_exception(const T& object)
{
  //get_status() is normally an inlined member method.
  check_status_and_throw_exception(object.get_status());
}

} // namespace Cairo
#endif //DOXYGEN_IGNORE_THIS

#endif //__CAIROMM_PRIVATE_H

// vim: ts=2 sw=2 et
