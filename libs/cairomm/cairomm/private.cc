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

#include <cairomm/private.h>
#include <cairomm/exception.h>
#include <stdexcept>
#include <iostream>

namespace Cairo
{

void throw_exception(ErrorStatus status)
{
  switch(status)
  {
    case CAIRO_STATUS_SUCCESS:
      // we should never get here, but just in case
      break;

    case CAIRO_STATUS_NO_MEMORY:
      throw std::bad_alloc();
      break;

    // Programmer error
    case CAIRO_STATUS_INVALID_RESTORE:
    case CAIRO_STATUS_INVALID_POP_GROUP:
    case CAIRO_STATUS_NO_CURRENT_POINT:
    case CAIRO_STATUS_INVALID_MATRIX:
    //No longer in API?: case CAIRO_STATUS_NO_TARGET_SURFACE:
    case CAIRO_STATUS_INVALID_STRING:
    case CAIRO_STATUS_SURFACE_FINISHED:
    //No longer in API?: case CAIRO_STATUS_BAD_NESTING:
      throw Cairo::logic_error(status);
      break;

    // Language binding implementation:
    case CAIRO_STATUS_NULL_POINTER:
    case CAIRO_STATUS_INVALID_PATH_DATA:
    case CAIRO_STATUS_SURFACE_TYPE_MISMATCH:
      throw Cairo::logic_error(status);
      break;

    // Other      
    case CAIRO_STATUS_READ_ERROR:
    case CAIRO_STATUS_WRITE_ERROR:
    {
      //The Cairo language binding advice suggests that these are stream errors 
      //that should be mapped to their C++ equivalents.
      const char* error_message = cairo_status_to_string(status);
      throw std::ios_base::failure( error_message ? error_message : std::string() );
    }
    
    default:
      throw Cairo::logic_error(status);
      break;
  }
}

} //namespace Cairo

// vim: ts=2 sw=2 et
