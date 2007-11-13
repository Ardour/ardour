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

#ifndef __CAIRO_EXCEPTION_H
#define __CAIRO_EXCEPTION_H

#include <cairomm/enums.h>
#include <stdexcept>

namespace Cairo
{

/** 
 */
class logic_error: public std::logic_error
{
public:
  explicit logic_error(ErrorStatus status);
  virtual ~logic_error() throw();

  //virtual const char* what() const throw();

private:
  ErrorStatus m_status;
};

} // namespace Cairo

#endif // __CAIRO_EXCEPTION_H

// vim: ts=2 sw=2 et
